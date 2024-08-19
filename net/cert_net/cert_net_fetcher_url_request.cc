// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Overview
//
// The main entry point is CertNetFetcherURLRequest. This is an implementation
// of CertNetFetcher that provides a service for fetching network requests.
//
// The interface for CertNetFetcher is synchronous, however allows
// overlapping requests. When starting a request CertNetFetcherURLRequest
// returns a CertNetFetcher::Request (CertNetFetcherRequestImpl) that the
// caller can use to cancel the fetch, or wait for it to complete
// (blocking).
//
// The CertNetFetcherURLRequest is shared between a network thread and a
// caller thread that waits for fetches to happen on the network thread.
//
// The classes are mainly organized based on their thread affinity:
//
// ---------------
// Straddles caller thread and network thread
// ---------------
//
// CertNetFetcherURLRequest (implements CertNetFetcher)
//   * Main entry point. Must be created and shutdown from the network thread.
//   * Provides a service to start/cancel/wait for URL fetches, to be
//     used on the caller thread.
//   * Returns callers a CertNetFetcher::Request as a handle
//   * Requests can run in parallel, however will block the current thread when
//     reading results.
//   * Posts tasks to network thread to coordinate actual work
//
// RequestCore
//   * Reference-counted bridge between CertNetFetcherRequestImpl and the
//     dependencies on the network thread
//   * Holds the result of the request, a WaitableEvent for signaling
//     completion, and pointers for canceling work on network thread.
//
// ---------------
// Lives on caller thread
// ---------------
//
// CertNetFetcherRequestImpl (implements CertNetFetcher::Request)
//   * Wrapper for cancelling events, or waiting for a request to complete
//   * Waits on a WaitableEvent to complete requests.
//
// ---------------
// Lives on network thread
// ---------------
//
// AsyncCertNetFetcherURLRequest
//   * Asynchronous manager for outstanding requests. Handles de-duplication,
//     timeouts, and actual integration with network stack. This is where the
//     majority of the logic lives.
//   * Signals completion of requests through RequestCore's WaitableEvent.
//   * Attaches requests to Jobs for the purpose of de-duplication

#include "net/cert_net/cert_net_fetcher_url_request.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_math.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/io_buffer.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "url/origin.h"

// TODO(eroman): Add support for POST parameters.
// TODO(eroman): Add controls for bypassing the cache.
// TODO(eroman): Add a maximum number of in-flight jobs/requests.
// TODO(eroman): Add NetLog integration.

namespace net {

namespace {

// The size of the buffer used for reading the response body of the URLRequest.
const int kReadBufferSizeInBytes = 4096;

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

}  // namespace

// AsyncCertNetFetcherURLRequest manages URLRequests in an async fashion on the
// URLRequestContexts's task runner thread.
//
//  * Schedules
//  * De-duplicates requests
//  * Handles timeouts
class CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest {
 public:
  // Initializes AsyncCertNetFetcherURLRequest using the specified
  // URLRequestContext for issuing requests. |context| must remain valid until
  // Shutdown() is called or the AsyncCertNetFetcherURLRequest is destroyed.
  explicit AsyncCertNetFetcherURLRequest(URLRequestContext* context);

  AsyncCertNetFetcherURLRequest(const AsyncCertNetFetcherURLRequest&) = delete;
  AsyncCertNetFetcherURLRequest& operator=(
      const AsyncCertNetFetcherURLRequest&) = delete;

  // The AsyncCertNetFetcherURLRequest is expected to be kept alive until all
  // requests have completed or Shutdown() is called.
  ~AsyncCertNetFetcherURLRequest();

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
  // Finds a job with a matching RequestPararms or returns nullptr if there was
  // no match.
  Job* FindJob(const RequestParams& params);

  // The in-progress jobs. This set does not contain the job which is actively
  // invoking callbacks (OnJobCompleted).
  JobSet jobs_;

  // Not owned. |context_| must outlive the AsyncCertNetFetcherURLRequest.
  raw_ptr<URLRequestContext> context_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

namespace {

// Policy for which URLs are allowed to be fetched. This is called both for the
// initial URL and for each redirect. Returns OK on success or a net error
// code on failure.
Error CanFetchUrl(const GURL& url) {
  if (!url.SchemeIs("http"))
    return ERR_DISALLOWED_URL_SCHEME;
  return OK;
}

base::TimeDelta GetTimeout(int timeout_milliseconds) {
  if (timeout_milliseconds == CertNetFetcher::DEFAULT)
    return base::Seconds(kTimeoutSeconds);
  return base::Milliseconds(timeout_milliseconds);
}

size_t GetMaxResponseBytes(int max_response_bytes,
                           size_t default_max_response_bytes) {
  if (max_response_bytes == CertNetFetcher::DEFAULT)
    return default_max_response_bytes;

  // Ensure that the specified limit is not negative, and cannot result in an
  // overflow while reading.
  base::CheckedNumeric<size_t> check(max_response_bytes);
  check += kReadBufferSizeInBytes;
  DCHECK(check.IsValid());

  return max_response_bytes;
}

enum HttpMethod {
  HTTP_METHOD_GET,
  HTTP_METHOD_POST,
};

}  // namespace

// RequestCore tracks an outstanding call to Fetch(). It is
// reference-counted for ease of sharing between threads.
class CertNetFetcherURLRequest::RequestCore
    : public base::RefCountedThreadSafe<RequestCore> {
 public:
  explicit RequestCore(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
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
    DCHECK_NE(error_, ERR_ABORTED);
    job_ = job;
  }

  void OnJobCompleted(Job* job,
                      Error error,
                      const std::vector<uint8_t>& response_body) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    DCHECK_EQ(job_, job);
    job_ = nullptr;

    error_ = error;
    bytes_ = response_body;
    completion_event_.Signal();
  }

  // Detaches this request from its job (if it is attached to any) and
  // signals completion with ERR_ABORTED. Can be called from any thread.
  void CancelJob();

  // Can be used to signal that an error was encountered before the request was
  // attached to a job. Can be called from any thread.
  void SignalImmediateError();

  // Should only be called once.
  void WaitForResult(Error* error, std::vector<uint8_t>* bytes) {
    DCHECK(!task_runner_->RunsTasksInCurrentSequence());

    completion_event_.Wait();
    *bytes = std::move(bytes_);
    *error = error_;

    error_ = ERR_UNEXPECTED;
  }

 private:
  friend class base::RefCountedThreadSafe<RequestCore>;

  ~RequestCore() {
    // Requests should have been cancelled prior to destruction.
    DCHECK(!job_);
  }

  // A non-owned pointer to the job that is executing the request.
  raw_ptr<Job> job_ = nullptr;

  // May be written to from network thread, or from the caller thread only when
  // there is no work that will be done on the network thread (e.g. when the
  // network thread has been shutdown before the request begins). See comment in
  // SignalImmediateError.
  Error error_ = OK;
  std::vector<uint8_t> bytes_;

  // Indicates when |error_| and |bytes_| have been written to.
  base::WaitableEvent completion_event_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

struct CertNetFetcherURLRequest::RequestParams {
  RequestParams();

  RequestParams(const RequestParams&) = delete;
  RequestParams& operator=(const RequestParams&) = delete;

  bool operator<(const RequestParams& other) const;

  GURL url;
  HttpMethod http_method = HTTP_METHOD_GET;
  size_t max_response_bytes = 0;

  // If set to a value <= 0 then means "no timeout".
  base::TimeDelta timeout;

  // IMPORTANT: When adding fields to this structure, update operator<().
};

CertNetFetcherURLRequest::RequestParams::RequestParams() = default;

bool CertNetFetcherURLRequest::RequestParams::operator<(
    const RequestParams& other) const {
  return std::tie(url, http_method, max_response_bytes, timeout) <
         std::tie(other.url, other.http_method, other.max_response_bytes,
                  other.timeout);
}

namespace {

// Job tracks an outstanding URLRequest as well as all of the pending requests
// for it.
class Job : public URLRequest::Delegate {
 public:
  Job(std::unique_ptr<CertNetFetcherURLRequest::RequestParams> request_params,
      CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest* parent);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job() override;

  const CertNetFetcherURLRequest::RequestParams& request_params() const {
    return *request_params_;
  }

  // Creates a request and attaches it to the job. When the job completes it
  // will notify the request of completion through OnJobCompleted.
  void AttachRequest(
      scoped_refptr<CertNetFetcherURLRequest::RequestCore> request);

  // Removes |request| from the job.
  void DetachRequest(CertNetFetcherURLRequest::RequestCore* request);

  // Creates and starts a URLRequest for the job. After the URLRequest has
  // completed, OnJobCompleted() will be invoked and all the registered requests
  // notified of completion.
  void StartURLRequest(URLRequestContext* context);

  // Cancels the request with an ERR_ABORTED error and invokes
  // RequestCore::OnJobCompleted() to notify the registered requests of the
  // cancellation. The job is *not* removed from the
  // AsyncCertNetFetcherURLRequest.
  void Cancel();

 private:
  // Implementation of URLRequest::Delegate
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnReadCompleted(URLRequest* request, int bytes_read) override;

  // Clears the URLRequest and timer. Helper for doing work common to
  // cancellation and job completion.
  void Stop();

  // Reads as much data as available from |request|.
  void ReadBody(URLRequest* request);

  // Helper to copy the partial bytes read from the read IOBuffer to an
  // aggregated buffer.
  bool ConsumeBytesRead(URLRequest* request, int num_bytes);

  // Called when the URLRequest has completed (either success or failure).
  void OnUrlRequestCompleted(int net_error);

  // Called when the Job has completed. The job may finish in response to a
  // timeout, an invalid URL, or the URLRequest completing. By the time this
  // method is called, the |response_body_| variable have been assigned.
  void OnJobCompleted(Error error);

  // Calls r->OnJobCompleted() for each RequestCore |r| currently attached
  // to this job, and then clears |requests_|.
  void CompleteAndClearRequests(Error error);

  // Cancels a request with a specified error code and calls
  // OnUrlRequestCompleted().
  void FailRequest(Error error);

  // The requests attached to this job.
  std::vector<scoped_refptr<CertNetFetcherURLRequest::RequestCore>> requests_;

  // The input parameters for starting a URLRequest.
  std::unique_ptr<CertNetFetcherURLRequest::RequestParams> request_params_;

  // The URLRequest response information.
  std::vector<uint8_t> response_body_;

  std::unique_ptr<URLRequest> url_request_;
  scoped_refptr<IOBuffer> read_buffer_;

  // Used to timeout the job when the URLRequest takes too long. This timer is
  // also used for notifying a failure to start the URLRequest.
  base::OneShotTimer timer_;

  // Non-owned pointer to the AsyncCertNetFetcherURLRequest that created this
  // job.
  raw_ptr<CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest> parent_;
};

}  // namespace

void CertNetFetcherURLRequest::RequestCore::CancelJob() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&RequestCore::CancelJob, this));
    return;
  }

  if (job_) {
    auto* job = job_.get();
    job_ = nullptr;
    job->DetachRequest(this);
  }

  SignalImmediateError();
}

void CertNetFetcherURLRequest::RequestCore::SignalImmediateError() {
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
  error_ = ERR_ABORTED;
  bytes_.clear();
  completion_event_.Signal();
}

namespace {

Job::Job(
    std::unique_ptr<CertNetFetcherURLRequest::RequestParams> request_params,
    CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest* parent)
    : request_params_(std::move(request_params)), parent_(parent) {}

Job::~Job() {
  DCHECK(requests_.empty());
  Stop();
}

void Job::AttachRequest(
    scoped_refptr<CertNetFetcherURLRequest::RequestCore> request) {
  request->AttachedToJob(this);
  requests_.push_back(std::move(request));
}

void Job::DetachRequest(CertNetFetcherURLRequest::RequestCore* request) {
  std::unique_ptr<Job> delete_this;

  auto it = base::ranges::find(requests_, request);
  CHECK(it != requests_.end(), base::NotFatalUntil::M130);
  requests_.erase(it);

  // If there are no longer any requests attached to the job then
  // cancel and delete it.
  if (requests_.empty())
    delete_this = parent_->RemoveJob(this);
}

void Job::StartURLRequest(URLRequestContext* context) {
  Error error = CanFetchUrl(request_params_->url);
  if (error != OK) {
    OnJobCompleted(error);
    return;
  }

  // Start the URLRequest.
  read_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSizeInBytes);
  NetworkTrafficAnnotationTag traffic_annotation =
      DefineNetworkTrafficAnnotation("certificate_verifier_url_request",
                                     R"(
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
            "NOTE: this path is being deprecated. Please see the"
            "certificate_verifier_url_loader annotation for the new path."
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
  url_request_ = context->CreateRequest(request_params_->url, DEFAULT_PRIORITY,
                                        this, traffic_annotation);
  if (request_params_->http_method == HTTP_METHOD_POST)
    url_request_->set_method("POST");
  url_request_->set_allow_credentials(false);

  // Disable secure DNS for hostname lookups triggered by certificate network
  // fetches to prevent deadlock.
  url_request_->SetSecureDnsPolicy(SecureDnsPolicy::kDisable);

  // Create IsolationInfo based on the origin of the requested URL.
  // TODO(crbug.com/40104280): Cert validation needs to either be
  // double-keyed or based on a static database, to protect it from being used
  // as a cross-site user tracking vector. For now, just treat it as if it were
  // a subresource request of the origin used for the request. This allows the
  // result to still be cached in the HTTP cache, and lets URLRequest DCHECK
  // that all requests have non-empty IsolationInfos.
  url::Origin origin = url::Origin::Create(request_params_->url);
  url_request_->set_isolation_info(IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, origin /* top_frame_origin */,
      origin /* frame_origin */, SiteForCookies()));

  // Ensure that we bypass HSTS for all requests sent through
  // CertNetFetcherURLRequest, since AIA/CRL/OCSP requests must be in HTTP to
  // avoid circular dependencies.
  url_request_->SetLoadFlags(url_request_->load_flags() |
                             net::LOAD_SHOULD_BYPASS_HSTS);

  url_request_->Start();

  // Start a timer to limit how long the job runs for.
  if (request_params_->timeout.is_positive()) {
    timer_.Start(FROM_HERE, request_params_->timeout,
                 base::BindOnce(&Job::FailRequest, base::Unretained(this),
                                ERR_TIMED_OUT));
  }
}

void Job::Cancel() {
  // Stop the timer and clear the URLRequest.
  Stop();
  // Signal attached requests that they've been completed.
  CompleteAndClearRequests(static_cast<Error>(ERR_ABORTED));
}

void Job::OnReceivedRedirect(URLRequest* request,
                             const RedirectInfo& redirect_info,
                             bool* defer_redirect) {
  DCHECK_EQ(url_request_.get(), request);

  // Ensure that the new URL matches the policy.
  Error error = CanFetchUrl(redirect_info.new_url);
  if (error != OK) {
    FailRequest(error);
    return;
  }
}

void Job::OnResponseStarted(URLRequest* request, int net_error) {
  DCHECK_EQ(url_request_.get(), request);
  DCHECK_NE(ERR_IO_PENDING, net_error);

  if (net_error != OK) {
    OnUrlRequestCompleted(net_error);
    return;
  }

  if (request->GetResponseCode() != 200) {
    FailRequest(ERR_HTTP_RESPONSE_CODE_FAILURE);
    return;
  }

  ReadBody(request);
}

void Job::OnReadCompleted(URLRequest* request, int bytes_read) {
  DCHECK_EQ(url_request_.get(), request);
  DCHECK_NE(ERR_IO_PENDING, bytes_read);

  // Keep reading the response body.
  if (ConsumeBytesRead(request, bytes_read))
    ReadBody(request);
}

void Job::Stop() {
  timer_.Stop();
  url_request_.reset();
}

void Job::ReadBody(URLRequest* request) {
  // Read as many bytes as are available synchronously.
  int num_bytes = 0;
  while (num_bytes >= 0) {
    num_bytes = request->Read(read_buffer_.get(), kReadBufferSizeInBytes);
    if (num_bytes == ERR_IO_PENDING)
      return;
    if (!ConsumeBytesRead(request, num_bytes))
      return;
  }

  OnUrlRequestCompleted(num_bytes);
}

bool Job::ConsumeBytesRead(URLRequest* request, int num_bytes) {
  DCHECK_NE(ERR_IO_PENDING, num_bytes);
  if (num_bytes <= 0) {
    // Error while reading, or EOF.
    OnUrlRequestCompleted(num_bytes);
    return false;
  }

  // Enforce maximum size bound.
  if (num_bytes + response_body_.size() > request_params_->max_response_bytes) {
    FailRequest(ERR_FILE_TOO_BIG);
    return false;
  }

  // Append the data to |response_body_|.
  response_body_.reserve(response_body_.size() + num_bytes);
  base::Extend(response_body_, read_buffer_->span().subspan(0, num_bytes));
  return true;
}

void Job::OnUrlRequestCompleted(int net_error) {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  Error result = static_cast<Error>(net_error);
  OnJobCompleted(result);
}

void Job::OnJobCompleted(Error error) {
  DCHECK_NE(ERR_IO_PENDING, error);
  // Stop the timer and clear the URLRequest.
  Stop();

  std::unique_ptr<Job> delete_this = parent_->RemoveJob(this);
  CompleteAndClearRequests(error);
}

void Job::CompleteAndClearRequests(Error error) {
  for (const auto& request : requests_) {
    request->OnJobCompleted(this, error, response_body_);
  }

  requests_.clear();
}

void Job::FailRequest(Error error) {
  DCHECK_NE(ERR_IO_PENDING, error);
  int result = url_request_->CancelWithError(error);
  OnUrlRequestCompleted(result);
}

}  // namespace

CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest::
    AsyncCertNetFetcherURLRequest(URLRequestContext* context)
    : context_(context) {
  // Allow creation to happen from another thread.
  DETACH_FROM_THREAD(thread_checker_);
}

CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest::
    ~AsyncCertNetFetcherURLRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  jobs_.clear();
}

bool JobComparator::operator()(const Job* job1, const Job* job2) const {
  return job1->request_params() < job2->request_params();
}

void CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest::Fetch(
    std::unique_ptr<RequestParams> request_params,
    scoped_refptr<RequestCore> request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If there is an in-progress job that matches the request parameters use it.
  // Otherwise start a new job.
  Job* job = FindJob(*request_params);
  if (job) {
    job->AttachRequest(std::move(request));
    return;
  }

  auto new_job = std::make_unique<Job>(std::move(request_params), this);
  job = new_job.get();
  jobs_[job] = std::move(new_job);
  // Attach the request before calling StartURLRequest; this ensures that the
  // request will get signalled if StartURLRequest completes the job
  // synchronously.
  job->AttachRequest(std::move(request));
  job->StartURLRequest(context_);
}

void CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& job : jobs_) {
    job.first->Cancel();
  }
  jobs_.clear();
}

namespace {

struct JobToRequestParamsComparator {
  bool operator()(const JobSet::value_type& job,
                  const CertNetFetcherURLRequest::RequestParams& value) const {
    return job.first->request_params() < value;
  }
};

}  // namespace

Job* CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest::FindJob(
    const RequestParams& params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // The JobSet is kept in sorted order so items can be found using binary
  // search.
  auto it = std::lower_bound(jobs_.begin(), jobs_.end(), params,
                             JobToRequestParamsComparator());
  if (it != jobs_.end() && !(params < (*it).first->request_params()))
    return (*it).first;
  return nullptr;
}

std::unique_ptr<Job>
CertNetFetcherURLRequest::AsyncCertNetFetcherURLRequest::RemoveJob(Job* job) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = jobs_.find(job);
  CHECK(it != jobs_.end());
  std::unique_ptr<Job> owned_job = std::move(it->second);
  jobs_.erase(it);
  return owned_job;
}

namespace {

class CertNetFetcherRequestImpl : public CertNetFetcher::Request {
 public:
  explicit CertNetFetcherRequestImpl(
      scoped_refptr<CertNetFetcherURLRequest::RequestCore> core)
      : core_(std::move(core)) {
    DCHECK(core_);
  }

  void WaitForResult(Error* error, std::vector<uint8_t>* bytes) override {
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
  scoped_refptr<CertNetFetcherURLRequest::RequestCore> core_;
};

}  // namespace

CertNetFetcherURLRequest::CertNetFetcherURLRequest()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

CertNetFetcherURLRequest::~CertNetFetcherURLRequest() {
  // The fetcher must be shutdown (at which point |context_| will be set to
  // null) before destruction.
  DCHECK(!context_);
}

void CertNetFetcherURLRequest::SetURLRequestContext(
    URLRequestContext* context) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  context_ = context;
}

// static
base::TimeDelta CertNetFetcherURLRequest::GetDefaultTimeoutForTesting() {
  return GetTimeout(CertNetFetcher::DEFAULT);
}

void CertNetFetcherURLRequest::Shutdown() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (impl_) {
    impl_->Shutdown();
    impl_.reset();
  }
  context_ = nullptr;
}

std::unique_ptr<CertNetFetcher::Request>
CertNetFetcherURLRequest::FetchCaIssuers(const GURL& url,
                                         int timeout_milliseconds,
                                         int max_response_bytes) {
  auto request_params = std::make_unique<RequestParams>();

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForAia);

  return DoFetch(std::move(request_params));
}

std::unique_ptr<CertNetFetcher::Request> CertNetFetcherURLRequest::FetchCrl(
    const GURL& url,
    int timeout_milliseconds,
    int max_response_bytes) {
  auto request_params = std::make_unique<RequestParams>();

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForCrl);

  return DoFetch(std::move(request_params));
}

std::unique_ptr<CertNetFetcher::Request> CertNetFetcherURLRequest::FetchOcsp(
    const GURL& url,
    int timeout_milliseconds,
    int max_response_bytes) {
  auto request_params = std::make_unique<RequestParams>();

  request_params->url = url;
  request_params->http_method = HTTP_METHOD_GET;
  request_params->timeout = GetTimeout(timeout_milliseconds);
  request_params->max_response_bytes =
      GetMaxResponseBytes(max_response_bytes, kMaxResponseSizeInBytesForAia);

  return DoFetch(std::move(request_params));
}

void CertNetFetcherURLRequest::DoFetchOnNetworkSequence(
    std::unique_ptr<RequestParams> request_params,
    scoped_refptr<RequestCore> request) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!context_) {
    // The fetcher might have been shutdown between when this task was posted
    // and when it is running. In this case, signal the request and do not
    // start a network request.
    request->SignalImmediateError();
    return;
  }

  if (!impl_) {
    impl_ = std::make_unique<AsyncCertNetFetcherURLRequest>(context_);
  }

  impl_->Fetch(std::move(request_params), request);
}

std::unique_ptr<CertNetFetcherURLRequest::Request>
CertNetFetcherURLRequest::DoFetch(
    std::unique_ptr<RequestParams> request_params) {
  auto request_core = base::MakeRefCounted<RequestCore>(task_runner_);

  // If the fetcher has already been shutdown, DoFetchOnNetworkSequence will
  // signal the request with an error. However, if the fetcher shuts down
  // before DoFetchOnNetworkSequence runs and PostTask still returns true,
  // then the request will hang (that is, WaitForResult will not return).
  if (!task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CertNetFetcherURLRequest::DoFetchOnNetworkSequence,
                         this, std::move(request_params), request_core))) {
    request_core->SignalImmediateError();
  }

  return std::make_unique<CertNetFetcherRequestImpl>(std::move(request_core));
}

}  // namespace net
