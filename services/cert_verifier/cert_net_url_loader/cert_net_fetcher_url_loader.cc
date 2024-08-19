// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Overview
//
// The main entry point is CertNetFetcherURLLoader. This is an implementation of
// net::CertNetFetcher that provides a service for fetching network requests.
//
// The interface for net::CertNetFetcher is synchronous, however allows
// overlapping requests. When starting a request CertNetFetcherURLLoader
// returns a net::CertNetFetcher::Request (CertNetFetcherRequestImpl) that the
// caller can use to cancel the fetch, or wait for it to complete
// (blocking).
//
// The CertNetFetcherURLLoader is shared between a creation thread and a
// caller thread that waits for fetches to happen on the creation thread.
//
// The classes are mainly organized based on their thread affinity:
//
// ---------------
// Straddles caller thread and creation thread
// ---------------
//
// CertNetFetcherURLLoader (implements CertNetFetcher)
//   * Main entry point. Must be created and shutdown from the creation thread.
//   * Provides a service to start/cancel/wait for URL fetches, to be
//     used on the caller thread.
//   * Returns callers a net::CertNetFetcher::Request as a handle
//   * Requests can run in parallel, however will block the current thread when
//     reading results.
//   * Posts tasks to creation thread to coordinate actual work
//
// RequestCore
//   * Reference-counted bridge between CertNetFetcherRequestImpl and the
//     dependencies on the creation thread
//   * Holds the result of the request, a WaitableEvent for signaling
//     completion, and pointers for canceling work on creation thread.
//
// ---------------
// Lives on caller thread
// ---------------
//
// CertNetFetcherRequestImpl (implements net::CertNetFetcher::Request)
//   * Wrapper for cancelling events, or waiting for a request to complete
//   * Waits on a WaitableEvent to complete requests.
//
// ---------------
// Lives on creation thread
// ---------------
//
// AsyncCertNetFetcherURLLoader
//   * Asynchronous manager for outstanding requests. Handles de-duplication,
//     timeouts, and actual integration with network stack. This is where the
//     majority of the logic lives.
//   * Signals completion of requests through RequestCore's WaitableEvent.
//   * Attaches requests to Jobs for the purpose of de-duplication

#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_math.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

// TODO(eroman): Add support for POST parameters.
// TODO(eroman): Add controls for bypassing the cache.
// TODO(eroman): Add a maximum number of in-flight jobs/requests.
// TODO(eroman): Add NetLog integration.

namespace cert_verifier {

namespace {

// NOTE: This code uses the SimpleURLLoader interface, so the maximum response
// sizes must be smaller than the limits imposed on
// SimpleURLLoader::DownloadToString.

// The maximum size in bytes for the response body when fetching a CRL.
const int kMaxResponseSizeInBytesForCrl = 5 * 1024 * 1024;

// The maximum size in bytes for the response body when fetching an AIA URL
// (caIssuers/OCSP).
const int kMaxResponseSizeInBytesForAia = 64 * 1024;

// The default timeout in seconds for fetch requests.
const int kTimeoutSeconds = 15;

class Job;

struct JobToRequestParamsComparator;

struct JobComparator {
  bool operator()(const Job* job1, const Job* job2) const;
};

// Would be a set<unique_ptr> but extraction of owned objects from a set of
// owned types doesn't come until C++17.
using JobSet = std::map<Job*, std::unique_ptr<Job>, JobComparator>;

using BindNewURLLoaderFactoryCallback = base::RepeatingCallback<void(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>)>;

}  // namespace

// AsyncCertNetFetcherURLLoader manages URLLoaders in an async fashion on the
// creation thread.
//
//  * Schedules
//  * De-duplicates requests
class CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader {
 public:
  // Initializes AsyncCertNetFetcherURLLoader, binding the
  // PendingRemote<URLLoaderFactory> into a Remote<URLLoaderFactory> for
  // creating URLLoaders.
  explicit AsyncCertNetFetcherURLLoader(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          factory_pending_remote,
      BindNewURLLoaderFactoryCallback bind_new_url_loader_factory_cb);

  AsyncCertNetFetcherURLLoader(const AsyncCertNetFetcherURLLoader&) = delete;
  AsyncCertNetFetcherURLLoader& operator=(const AsyncCertNetFetcherURLLoader&) =
      delete;

  // The AsyncCertNetFetcherURLLoader is expected to be kept alive until all
  // requests have completed or Shutdown() is called.
  ~AsyncCertNetFetcherURLLoader();

  // Disconnects |factory_|, and calls FlushForTesting() in order to
  // synchronously disconnect.
  void DisconnectURLLoaderFactoryForTesting();

  // Starts an asynchronous request to fetch the given URL. On completion
  // request->OnJobCompleted() will be invoked.
  void Fetch(std::unique_ptr<RequestParams> request_params,
             scoped_refptr<RequestCore> request);

  // Removes |job| from the in progress jobs and transfers ownership to the
  // caller.
  std::unique_ptr<Job> RemoveJob(Job* job);

  // Cancels outstanding jobs, which stops network requests and signals the
  // corresponding RequestCores that the requests have completed.
  void Shutdown();

 private:
  // Attempts to use |bind_new_url_loader_factory_cb_| to reconnect |factory_|.
  // There's no guarantee that |bind_new_url_loader_factory_cb_| will succeed.
  void RebindURLLoaderFactory();

  // Finds a job with a matching RequestPararms or returns nullptr if there was
  // no match.
  Job* FindJob(const RequestParams& params);

  // The in-progress jobs. This set does not contain the job which is actively
  // invoking callbacks (OnJobCompleted).
  JobSet jobs_;

  // |factory_| may disconnect. On disconnection, immediately try to reconnect
  // in case the disconnection was deliberate. If the disconnection was due to a
  // crash, the CertNetFetcherURLLoader will likely be destroyed and recreated
  // for a new CertVerifier. It can then be reconnected with
  // |bind_new_url_loader_factory_cb_|.
  mojo::Remote<network::mojom::URLLoaderFactory> factory_;

  // A callback provided by the creator of the CertNetFetcherURLLoader that can
  // be used to bind a new URLLoaderFactory if the current one disconnects.
  BindNewURLLoaderFactoryCallback bind_new_url_loader_factory_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
};

namespace {

// Policy for which URLs are allowed to be fetched. This is called both for the
// initial URL and for each redirect. Returns OK on success or a net error
// code on failure.
net::Error CanFetchUrl(const GURL& url) {
  if (!url.SchemeIs("http"))
    return net::ERR_DISALLOWED_URL_SCHEME;
  return net::OK;
}

base::TimeDelta GetTimeout(int timeout_milliseconds) {
  if (timeout_milliseconds == net::CertNetFetcher::DEFAULT)
    return base::Seconds(kTimeoutSeconds);
  return base::Milliseconds(timeout_milliseconds);
}

size_t GetMaxResponseBytes(int max_response_bytes,
                           size_t default_max_response_bytes) {
  if (max_response_bytes == net::CertNetFetcher::DEFAULT)
    return default_max_response_bytes;
  return max_response_bytes;
}

enum HttpMethod {
  HTTP_METHOD_GET,
  HTTP_METHOD_POST,
};

}  // namespace

// RequestCore tracks an outstanding call to Fetch(). It is
// reference-counted for ease of sharing between threads.
class CertNetFetcherURLLoader::RequestCore
    : public base::RefCountedThreadSafe<RequestCore> {
 public:
  explicit RequestCore(scoped_refptr<base::SequencedTaskRunner> task_runner)
      : completion_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED),
        task_runner_(std::move(task_runner)) {}

  RequestCore(const RequestCore&) = delete;
  RequestCore& operator=(const RequestCore&) = delete;

  void AttachedToJob(Job* job) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!job_);
    // Requests should not be attached to jobs after they have been signalled
    // with a cancellation error (which happens via either Cancel() or
    // SignalImmediateError()).
    DCHECK_NE(error_, net::ERR_ABORTED);
    job_ = job;
  }

  void OnJobCompleted(Job* job,
                      net::Error error,
                      const std::string* response_body) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    DCHECK_EQ(job_, job);
    job_ = nullptr;

    error_ = error;
    bytes_ = base::ToVector(base::as_byte_span(*response_body));
    completion_event_.Signal();
  }

  // Can be used to signal that an error was encountered before the request was
  // attached to a job. Can be called from any thread.
  void SignalImmediateError();

  // Should only be called once.
  void WaitForResult(net::Error* error, std::vector<uint8_t>* bytes) {
    DCHECK(!task_runner_->RunsTasksInCurrentSequence());

    completion_event_.Wait();
    *bytes = std::move(bytes_);
    *error = error_;

    error_ = net::ERR_UNEXPECTED;
  }

  // Detaches this request from its job (if it is attached to any) and
  // signals completion with ERR_ABORTED. Can be called from any thread.
  void CancelJob();

 private:
  friend class base::RefCountedThreadSafe<RequestCore>;

  ~RequestCore() {
    // Requests should have been cancelled prior to destruction.
    DCHECK(!job_);
  }

  // See comments on CancelJob().
  void CancelJobOnTaskRunner();

  // A non-owned pointer to the job that is executing the request.
  raw_ptr<Job> job_ = nullptr;

  // May be written to from network thread, or from the caller thread only when
  // there is no work that will be done on the network thread (e.g. when the
  // network thread has been shutdown before the request begins). See comment in
  // SignalImmediateError.
  net::Error error_ = net::OK;
  std::vector<uint8_t> bytes_;

  // Indicates when |error_| and |bytes_| have been written to.
  base::WaitableEvent completion_event_;

  // The task runner of the creation thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

struct CertNetFetcherURLLoader::RequestParams {
  RequestParams();

  RequestParams(const RequestParams&) = delete;
  RequestParams& operator=(const RequestParams&) = delete;

  bool operator<(const RequestParams& other) const;

  GURL url;
  HttpMethod http_method;
  size_t max_response_bytes;

  // If set to a value == 0 then means "no timeout".
  base::TimeDelta timeout;

  // IMPORTANT: When adding fields to this structure, update operator<().
};

CertNetFetcherURLLoader::RequestParams::RequestParams()
    : http_method(HTTP_METHOD_GET), max_response_bytes(0) {}

bool CertNetFetcherURLLoader::RequestParams::operator<(
    const RequestParams& other) const {
  return std::tie(url, http_method, max_response_bytes, timeout) <
         std::tie(other.url, other.http_method, other.max_response_bytes,
                  other.timeout);
}

namespace {

// Job tracks an outstanding URLLoader as well as all of the pending requests
// for it.
class Job {
 public:
  Job(std::unique_ptr<CertNetFetcherURLLoader::RequestParams> request_params,
      CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader* parent);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job();

  const CertNetFetcherURLLoader::RequestParams& request_params() const {
    return *request_params_;
  }

  // Creates a request and attaches it to the job. When the job completes it
  // will notify the request of completion through OnJobCompleted.
  void AttachRequest(
      scoped_refptr<CertNetFetcherURLLoader::RequestCore> request);

  // Removes |request| from the job.
  void DetachRequest(CertNetFetcherURLLoader::RequestCore* request);

  // Creates and starts a URLLoader for the job. After the URLLoader has
  // completed, OnJobCompleted() will be invoked and all the registered requests
  // notified of completion.
  void StartURLLoader(network::mojom::URLLoaderFactory* factory);

  // Cancels the request with an ERR_ABORTED error and invokes
  // RequestCore::OnJobCompleted() to notify the registered requests of the
  // cancellation. The job is *not* removed from the
  // AsyncCertNetFetcherURLLoader.
  void Cancel();

 private:
  void OnReceivedRedirect(const GURL& url_before_redirect,
                          const net::RedirectInfo& redirect_info,
                          const network::mojom::URLResponseHead& response_head,
                          std::vector<std::string>* removed_headers);
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  // Callback for when |url_loader_| has finished.
  void OnUrlLoaderCompleted(std::unique_ptr<std::string> response_body);

  // Called when the Job has completed. The job may finish in response to a
  // timeout, an invalid URL, or the SimpleURLLoader completing.
  // This will delete the Job after calling |CompleteAndClearRequests()|.
  void OnJobCompleted(net::Error error,
                      std::unique_ptr<std::string> response_body);

  // Calls r->OnJobCompleted() for each RequestCore |r| currently attached
  // to this job, and then clears |requests_|.
  void CompleteAndClearRequests(net::Error error,
                                std::unique_ptr<std::string> response_body);

  // Cancels a request with a specified error code and calls
  // OnUrlRequestCompleted().
  void FailRequest(net::Error error);

  // The requests attached to this job.
  std::vector<scoped_refptr<CertNetFetcherURLLoader::RequestCore>> requests_;

  // The input parameters for starting a URLLoader.
  std::unique_ptr<CertNetFetcherURLLoader::RequestParams> request_params_;

  // SimpleURLLoader for the request. Can be deleted to cancel the request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Non-owned pointer to the AsyncCertNetFetcherURLLoader that created this
  // job.
  raw_ptr<CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader> parent_;
};

}  // namespace

void CertNetFetcherURLLoader::RequestCore::SignalImmediateError() {
  // These data members are normally only written on the network thread, but it
  // is safe to write here from either thread. This is because
  // SignalImmediateError is only to be called before this request is attached
  // to a job. In particular, if called from the caller thread, no work will be
  // done on the network thread for this request, so these variables will only
  // be written and read on the caller thread. If called from the network
  // thread, they will only be written to on the network thread and will not be
  // read on the caller thread until |completion_event_| is signalled (after
  // which it will be not be written on the network thread again).
  DCHECK(!job_);
  error_ = net::ERR_ABORTED;
  bytes_.clear();
  completion_event_.Signal();
}

void CertNetFetcherURLLoader::RequestCore::CancelJob() {
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RequestCore::CancelJobOnTaskRunner, this));
}

void CertNetFetcherURLLoader::RequestCore::CancelJobOnTaskRunner() {
  if (job_) {
    auto* job = job_.get();
    job_ = nullptr;
    job->DetachRequest(this);
  }

  SignalImmediateError();
}

namespace {

Job::Job(std::unique_ptr<CertNetFetcherURLLoader::RequestParams> request_params,
         CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader* parent)
    : request_params_(std::move(request_params)), parent_(parent) {}

Job::~Job() {
  DCHECK(requests_.empty());
}

void Job::AttachRequest(
    scoped_refptr<CertNetFetcherURLLoader::RequestCore> request) {
  request->AttachedToJob(this);
  requests_.push_back(std::move(request));
}

void Job::DetachRequest(CertNetFetcherURLLoader::RequestCore* request) {
  std::unique_ptr<Job> delete_this;

  auto it = base::ranges::find(requests_, request);
  CHECK(it != requests_.end(), base::NotFatalUntil::M130);
  requests_.erase(it);

  // If there are no longer any requests attached to the job then
  // cancel and delete it.
  if (requests_.empty())
    delete_this = parent_->RemoveJob(this);
}

void Job::StartURLLoader(network::mojom::URLLoaderFactory* factory) {
  net::Error error = CanFetchUrl(request_params_->url);
  if (error != net::OK) {
    FailRequest(error);
    return;
  }

  // Start the SimpleURLLoader.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("certificate_verifier_url_loader", R"(
        semantics {
          sender: "Certificate Verifier"
          description:
            "When verifying certificates, the browser may need to fetch "
            "additional URLs that are encoded in the server-provided "
            "certificate chain. This may be part of revocation checking ("
            "Online Certificate Status Protocol, Certificate Revocation List), "
            "or path building (Authority Information Access fetches). Please "
            "refer to the following for more on above protocols: "
            "https://tools.ietf.org/html/rfc6960, "
            "https://tools.ietf.org/html/rfc5280#section-4.2.1.13, and"
            "https://tools.ietf.org/html/rfc5280#section-5.2.7."
          trigger:
            "Verifying a certificate (likely in response to navigating to an "
            "'https://' website)."
          data:
            "In the case of OCSP this may divulge the website being viewed. No "
            "user data in other cases."
          destination: OTHER
          destination_other:
            "The URL specified in the certificate."
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = request_params_->url;
  if (request_params_->http_method == HTTP_METHOD_POST)
    request->method = net::HttpRequestHeaders::kPostMethod;
  // Disable secure DNS for hostname lookups triggered by certificate network
  // fetches to prevent deadlock.
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->disable_secure_dns = true;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Ensure that we bypass HSTS for all requests sent through
  // CertNetFetcherURLLoader, since AIA/CRL/OCSP requests must be in HTTP to
  // avoid circular dependencies.
  request->load_flags |= net::LOAD_SHOULD_BYPASS_HSTS;
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  // base::Unretained(this) is safe because |this| owns |url_loader_|, which
  // will not call the callback if it is deleted.
  url_loader_->SetOnRedirectCallback(
      base::BindRepeating(&Job::OnReceivedRedirect, base::Unretained(this)));
  url_loader_->SetOnResponseStartedCallback(
      base::BindOnce(&Job::OnResponseStarted, base::Unretained(this)));
  url_loader_->SetTimeoutDuration(request_params_->timeout);
  url_loader_->DownloadToString(
      factory,
      base::BindOnce(&Job::OnUrlLoaderCompleted, base::Unretained(this)),
      request_params_->max_response_bytes);
}

void Job::Cancel() {
  // Reset the SimpleURLLoader.
  url_loader_.reset();
  // Signal attached requests that they've been completed.
  CompleteAndClearRequests(net::ERR_ABORTED, nullptr);
}

void Job::OnReceivedRedirect(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  // Ensure that the new URL matches the policy.
  net::Error error = CanFetchUrl(redirect_info.new_url);
  if (error != net::OK) {
    FailRequest(error);
  }
}

void Job::OnResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  if (!response_head.headers || response_head.headers->response_code() != 200) {
    FailRequest(net::ERR_HTTP_RESPONSE_CODE_FAILURE);
  }
}

void Job::OnUrlLoaderCompleted(std::unique_ptr<std::string> response_body) {
  net::Error error = static_cast<net::Error>(url_loader_->NetError());
  OnJobCompleted(error, std::move(response_body));
}

void Job::OnJobCompleted(net::Error error,
                         std::unique_ptr<std::string> response_body) {
  // Reset the SimpleURLLoader.
  url_loader_.reset();

  std::unique_ptr<Job> delete_this = parent_->RemoveJob(this);
  CompleteAndClearRequests(error, std::move(response_body));
}

void Job::CompleteAndClearRequests(net::Error error,
                                   std::unique_ptr<std::string> response_body) {
  for (const auto& request : requests_) {
    std::string empty_str;
    request->OnJobCompleted(this, error,
                            response_body ? response_body.get() : &empty_str);
  }

  requests_.clear();
}

void Job::FailRequest(net::Error error) {
  OnJobCompleted(error, nullptr);
}

}  // namespace

CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader::
    AsyncCertNetFetcherURLLoader(
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            factory_pending_remote,
        BindNewURLLoaderFactoryCallback bind_new_url_loader_factory_cb)
    : factory_(std::move(factory_pending_remote)),
      bind_new_url_loader_factory_cb_(
          std::move(bind_new_url_loader_factory_cb)) {}

CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader::
    ~AsyncCertNetFetcherURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The fetcher must be shutdown (at which point |factory_| will be reset and
  // unbound) before destruction.
  DCHECK(!factory_);
  DCHECK(jobs_.empty());
}

void CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader::
    DisconnectURLLoaderFactoryForTesting() {
  // Sadly, there's no good way to disconnect a Mojo remote other than resetting
  // it, binding it to a new pipe, and dropping the PendingReceiver on the
  // floor.
  factory_.reset();
  std::ignore = factory_.BindNewPipeAndPassReceiver();
  factory_.FlushForTesting();
}

bool JobComparator::operator()(const Job* job1, const Job* job2) const {
  return job1->request_params() < job2->request_params();
}

void CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader::Fetch(
    std::unique_ptr<RequestParams> request_params,
    scoped_refptr<RequestCore> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there is an in-progress job that matches the request parameters use it.
  // Otherwise start a new job.
  Job* job = FindJob(*request_params);
  if (job) {
    job->AttachRequest(std::move(request));
    return;
  }

  if (!factory_.is_connected()) {
    RebindURLLoaderFactory();
  }

  job = new Job(std::move(request_params), this);
  jobs_[job] = base::WrapUnique(job);
  // Attach the request before calling |StartURLLoader()|; this ensures that the
  // request will get signalled if |StartURLLoader()| completes the job
  // synchronously.
  job->AttachRequest(std::move(request));
  job->StartURLLoader(factory_.get());
}

void CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& job : jobs_) {
    job.first->Cancel();
  }
  jobs_.clear();

  factory_.reset();
}

void CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader::
    RebindURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If Shutdown() was called, the AsyncCertNetFetcherURLLoader should not be
  // trying to reconnect a URLLoaderFactory.
  DCHECK(factory_);

  if (!bind_new_url_loader_factory_cb_) {
    return;
  }

  factory_.reset();
  bind_new_url_loader_factory_cb_.Run(factory_.BindNewPipeAndPassReceiver());
}

namespace {

struct JobToRequestParamsComparator {
  bool operator()(const JobSet::value_type& job,
                  const CertNetFetcherURLLoader::RequestParams& value) const {
    return job.first->request_params() < value;
  }
};

}  // namespace

Job* CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader::FindJob(
    const RequestParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The JobSet is kept in sorted order so items can be found using binary
  // search.
  auto it = std::lower_bound(jobs_.begin(), jobs_.end(), params,
                             JobToRequestParamsComparator());
  if (it != jobs_.end() && !(params < (*it).first->request_params()))
    return (*it).first;
  return nullptr;
}

std::unique_ptr<Job>
CertNetFetcherURLLoader::AsyncCertNetFetcherURLLoader::RemoveJob(Job* job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = jobs_.find(job);
  CHECK(it != jobs_.end());
  std::unique_ptr<Job> owned_job = std::move(it->second);
  jobs_.erase(it);
  return owned_job;
}

namespace {

class CertNetFetcherRequestImpl : public net::CertNetFetcher::Request {
 public:
  explicit CertNetFetcherRequestImpl(
      scoped_refptr<CertNetFetcherURLLoader::RequestCore> core)
      : core_(std::move(core)) {
    DCHECK(core_);
  }

  void WaitForResult(net::Error* error, std::vector<uint8_t>* bytes) override {
    // Should only be called a single time.
    DCHECK(core_);

    core_->WaitForResult(error, bytes);
    core_ = nullptr;
  }

  ~CertNetFetcherRequestImpl() override {
    if (core_)
      core_->CancelJob();
  }

 private:
  scoped_refptr<CertNetFetcherURLLoader::RequestCore> core_;
};

}  // namespace

CertNetFetcherURLLoader::CertNetFetcherURLLoader()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

CertNetFetcherURLLoader::~CertNetFetcherURLLoader() = default;

void CertNetFetcherURLLoader::SetURLLoaderFactoryAndReconnector(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory,
    base::RepeatingCallback<
        void(mojo::PendingReceiver<network::mojom::URLLoaderFactory>)>
        bind_new_url_loader_factory_cb) {
  DCHECK(!impl_);
  impl_ = std::make_unique<AsyncCertNetFetcherURLLoader>(
      std::move(factory), std::move(bind_new_url_loader_factory_cb));
}

// static
base::TimeDelta CertNetFetcherURLLoader::GetDefaultTimeoutForTesting() {
  return GetTimeout(CertNetFetcher::DEFAULT);
}

void CertNetFetcherURLLoader::DisconnectURLLoaderFactoryForTesting() {
  impl_->DisconnectURLLoaderFactoryForTesting();
}

void CertNetFetcherURLLoader::Shutdown() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (impl_) {
    impl_->Shutdown();
    impl_.reset();
  }
}

std::unique_ptr<net::CertNetFetcher::Request>
CertNetFetcherURLLoader::FetchCaIssuers(const GURL& url,
                                        int timeout_milliseconds,
                                        int max_response_bytes) {
  std::unique_ptr<RequestParams> request_params(new RequestParams);

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForAia);

  return DoFetch(std::move(request_params));
}

std::unique_ptr<net::CertNetFetcher::Request> CertNetFetcherURLLoader::FetchCrl(
    const GURL& url,
    int timeout_milliseconds,
    int max_response_bytes) {
  std::unique_ptr<RequestParams> request_params(new RequestParams);

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForCrl);

  return DoFetch(std::move(request_params));
}

std::unique_ptr<net::CertNetFetcher::Request>
CertNetFetcherURLLoader::FetchOcsp(const GURL& url,
                                   int timeout_milliseconds,
                                   int max_response_bytes) {
  std::unique_ptr<RequestParams> request_params(new RequestParams);

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForAia);

  return DoFetch(std::move(request_params));
}

void CertNetFetcherURLLoader::DoFetchOnTaskRunner(
    std::unique_ptr<RequestParams> request_params,
    scoped_refptr<RequestCore> request) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!impl_) {
    // |SetURLLoaderFactoryAndReconnector| may not have been called yet, or the
    // fetcher might have been shutdown between when this task was posted and
    // when it is running. In this case, signal the request and do not start a
    // network request.
    request->SignalImmediateError();
    return;
  }

  impl_->Fetch(std::move(request_params), request);
}

std::unique_ptr<CertNetFetcherURLLoader::Request>
CertNetFetcherURLLoader::DoFetch(
    std::unique_ptr<RequestParams> request_params) {
  // Caller thread cannot be the same as the creation thread.
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());

  auto request_core = base::MakeRefCounted<RequestCore>(task_runner_);

  // If the fetcher has already been shutdown, DoFetchOnTaskRunner will
  // signal the request with an error. However, if the fetcher shuts down
  // before DoFetchOnTaskRunner runs and PostTask still returns true,
  // then the request will hang (that is, WaitForResult will not return).
  if (!task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CertNetFetcherURLLoader::DoFetchOnTaskRunner, this,
                         std::move(request_params), request_core))) {
    request_core->SignalImmediateError();
  }

  return std::make_unique<CertNetFetcherRequestImpl>(std::move(request_core));
}

}  // namespace cert_verifier
