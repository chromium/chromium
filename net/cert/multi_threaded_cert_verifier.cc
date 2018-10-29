// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_threaded_cert_verifier.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/linked_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"

namespace net {

class NetLogCaptureMode;

// Allows DoVerifyOnWorkerThread to wait on a base::WaitableEvent.
// DoVerifyOnWorkerThread may wait on network operations done on a separate
// sequence. For instance when using the NSS-based implementation of certificate
// verification, the library requires a blocking callback for fetching OCSP and
// AIA responses.
class MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives
    : public base::ScopedAllowBaseSyncPrimitives {};

////////////////////////////////////////////////////////////////////////////
//
// MultiThreadedCertVerifier is a thread-unsafe object which lives, dies, and is
// operated on a single thread, henceforth referred to as the "origin" thread.
//
// When an incoming Verify() request is received, MultiThreadedCertVerifier
// checks if there is an outstanding "job" (CertVerifierJob) in progress that
// can service the request. If there is, the request is attached to that job.
// Otherwise a new job is started.
//
// A job (CertVerifierJob) is a way to de-duplicate requests that are
// fundamentally doing the same verification. CertVerifierJob is similarly
// thread-unsafe and lives on the origin thread.
//
// To do the actual work, CertVerifierJob posts a task to TaskScheduler
// (PostTaskAndReply), and on completion notifies all requests attached to it.
//
// Cancellation:
//
// There are two ways for a request to be cancelled.
//
// (1) When the caller explicitly frees the Request.
//
//     If the request was in-flight (attached to a job), then it is detached.
//     Note that no effort is made to reap jobs which have no attached requests.
//     (Because the worker task isn't cancelable).
//
// (2) When the MultiThreadedCertVerifier is deleted.
//
//     This automatically cancels all outstanding requests. This is accomplished
//     by deleting each of the jobs owned by the MultiThreadedCertVerifier,
//     whose destructor in turn marks each attached request as canceled.
//
// TODO(eroman): If the MultiThreadedCertVerifier is deleted from within a
// callback, the remaining requests in the completing job will NOT be cancelled.

namespace {

std::unique_ptr<base::Value> CertVerifyResultCallback(
    const CertVerifyResult& verify_result,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  results->SetBoolean("has_md5", verify_result.has_md5);
  results->SetBoolean("has_md2", verify_result.has_md2);
  results->SetBoolean("has_md4", verify_result.has_md4);
  results->SetBoolean("is_issued_by_known_root",
                      verify_result.is_issued_by_known_root);
  results->SetBoolean("is_issued_by_additional_trust_anchor",
                      verify_result.is_issued_by_additional_trust_anchor);
  results->SetInteger("cert_status", verify_result.cert_status);
  results->Set("verified_cert",
               NetLogX509CertificateCallback(verify_result.verified_cert.get(),
                                             capture_mode));

  std::unique_ptr<base::ListValue> hashes(new base::ListValue());
  for (auto it = verify_result.public_key_hashes.begin();
       it != verify_result.public_key_hashes.end(); ++it) {
    hashes->AppendString(it->ToString());
  }
  results->Set("public_key_hashes", std::move(hashes));

  return std::move(results);
}

// Used to pass the result of CertVerifierJob::DoVerifyOnWorkerThread() to
// CertVerifierJob::OnJobCompleted().
struct ResultHelper {
  int error;
  CertVerifyResult result;
};

int GetFlagsForConfig(const CertVerifier::Config& config) {
  int flags = 0;

  if (config.enable_rev_checking)
    flags |= CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  if (config.require_rev_checking_local_anchors)
    flags |= CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  if (config.enable_sha1_local_anchors)
    flags |= CertVerifyProc::VERIFY_ENABLE_SHA1_LOCAL_ANCHORS;
  if (config.disable_symantec_enforcement)
    flags |= CertVerifyProc::VERIFY_DISABLE_SYMANTEC_ENFORCEMENT;

  return flags;
}

}  // namespace

// Represents the output and result callback of a request. The
// CertVerifierRequest is owned by the caller that initiated the call to
// CertVerifier::Verify().
class CertVerifierRequest : public base::LinkNode<CertVerifierRequest>,
                            public CertVerifier::Request {
 public:
  CertVerifierRequest(CertVerifierJob* job,
                      CompletionOnceCallback callback,
                      CertVerifyResult* verify_result,
                      const NetLogWithSource& net_log)
      : job_(job),
        callback_(std::move(callback)),
        verify_result_(verify_result),
        net_log_(net_log) {
    net_log_.BeginEvent(NetLogEventType::CERT_VERIFIER_REQUEST);
  }

  // Cancels the request.
  ~CertVerifierRequest() override {
    if (job_) {
      // Cancel the outstanding request.
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_REQUEST);

      // Remove the request from the Job. No attempt is made to cancel the job
      // even though it may no longer have any requests attached to it. Because
      // it is running on a worker thread aborting it isn't feasible.
      RemoveFromList();
    }
  }

  // Copies the contents of |verify_result| to the caller's
  // CertVerifyResult and calls the callback.
  void Post(const ResultHelper& verify_result) {
    DCHECK(job_);
    job_ = nullptr;

    net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_REQUEST);
    *verify_result_ = verify_result.result;

    base::ResetAndReturn(&callback_).Run(verify_result.error);
  }

  void OnJobCancelled() {
    job_ = nullptr;
    callback_.Reset();
  }

  const NetLogWithSource& net_log() const { return net_log_; }

 private:
  CertVerifierJob* job_;  // Not owned.
  CompletionOnceCallback callback_;
  CertVerifyResult* verify_result_;
  const NetLogWithSource net_log_;
};

// DoVerifyOnWorkerThread runs the verification synchronously on a worker
// thread.
std::unique_ptr<ResultHelper> DoVerifyOnWorkerThread(
    const scoped_refptr<CertVerifyProc>& verify_proc,
    const scoped_refptr<X509Certificate>& cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    int flags,
    const scoped_refptr<CRLSet>& crl_set,
    const CertificateList& additional_trust_anchors) {
  TRACE_EVENT0(kNetTracingCategory, "DoVerifyOnWorkerThread");
  auto verify_result = std::make_unique<ResultHelper>();
  MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives
      allow_base_sync_primitives;
  verify_result->error = verify_proc->Verify(
      cert.get(), hostname, ocsp_response, flags, crl_set.get(),
      additional_trust_anchors, &verify_result->result);
  return verify_result;
}

// CertVerifierJob lives only on the verifier's origin message loop.
class CertVerifierJob {
 public:
  CertVerifierJob(const CertVerifier::RequestParams& key,
                  NetLog* net_log,
                  MultiThreadedCertVerifier* cert_verifier)
      : key_(key),
        start_time_(base::TimeTicks::Now()),
        net_log_(NetLogWithSource::Make(net_log,
                                        NetLogSourceType::CERT_VERIFIER_JOB)),
        cert_verifier_(cert_verifier),
        is_first_job_(false),
        weak_ptr_factory_(this) {
    net_log_.BeginEvent(NetLogEventType::CERT_VERIFIER_JOB,
                        base::Bind(&NetLogX509CertificateCallback,
                                   base::Unretained(key.certificate().get())));
  }

  // Indicates whether this was the first job started by the CertVerifier. This
  // is only used for logging certain UMA stats.
  void set_is_first_job(bool is_first_job) { is_first_job_ = is_first_job; }

  const CertVerifier::RequestParams& key() const { return key_; }

  // Posts a task to TaskScheduler to do the verification. Once the verification
  // has completed, it will call OnJobCompleted() on the origin thread.
  void Start(const scoped_refptr<CertVerifyProc>& verify_proc,
             const CertVerifier::Config& config,
             uint32_t config_id) {
    int flags = GetFlagsForConfig(config);
    if (key_.flags() & CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES) {
      flags &= ~CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
      flags &= ~CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
    }
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&DoVerifyOnWorkerThread, verify_proc, key_.certificate(),
                       key_.hostname(), key_.ocsp_response(), flags,
                       config.crl_set, config.additional_trust_anchors),
        base::BindOnce(&CertVerifierJob::OnJobCompleted,
                       weak_ptr_factory_.GetWeakPtr(), config_id));
  }

  ~CertVerifierJob() {
    // If the job is in progress, cancel it.
    if (cert_verifier_) {
      cert_verifier_ = nullptr;

      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_JOB);

      // Notify each request of the cancellation.
      for (base::LinkNode<CertVerifierRequest>* it = requests_.head();
           it != requests_.end(); it = it->next()) {
        it->value()->OnJobCancelled();
      }
    }
  }

  // Creates and attaches a request to the Job.
  std::unique_ptr<CertVerifierRequest> CreateRequest(
      CompletionOnceCallback callback,
      CertVerifyResult* verify_result,
      const NetLogWithSource& net_log) {
    std::unique_ptr<CertVerifierRequest> request(new CertVerifierRequest(
        this, std::move(callback), verify_result, net_log));

    request->net_log().AddEvent(
        NetLogEventType::CERT_VERIFIER_REQUEST_BOUND_TO_JOB,
        net_log_.source().ToEventParametersCallback());

    requests_.Append(request.get());
    return request;
  }

 private:
  using RequestList = base::LinkedList<CertVerifierRequest>;

  // Called on completion of the Job to log UMA metrics and NetLog events.
  void LogMetrics(const ResultHelper& verify_result) {
    net_log_.EndEvent(
        NetLogEventType::CERT_VERIFIER_JOB,
        base::Bind(&CertVerifyResultCallback, verify_result.result));
    base::TimeDelta latency = base::TimeTicks::Now() - start_time_;
    if (cert_verifier_->should_record_histograms_) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_Job_Latency", latency,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10), 100);
      if (is_first_job_) {
        UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_First_Job_Latency",
                                   latency,
                                   base::TimeDelta::FromMilliseconds(1),
                                   base::TimeDelta::FromMinutes(10), 100);
      }
    }
  }

  void OnJobCompleted(uint32_t config_id,
                      std::unique_ptr<ResultHelper> verify_result) {
    TRACE_EVENT0(kNetTracingCategory, "CertVerifierJob::OnJobCompleted");
    std::unique_ptr<CertVerifierJob> keep_alive =
        cert_verifier_->RemoveJob(this);

    LogMetrics(*verify_result);
    if (cert_verifier_->verify_complete_callback_ &&
        config_id == cert_verifier_->config_id_) {
      cert_verifier_->verify_complete_callback_.Run(
          key_, net_log_, verify_result->error, verify_result->result,
          base::TimeTicks::Now() - start_time_, is_first_job_);
    }
    cert_verifier_ = nullptr;

    // TODO(eroman): If the cert_verifier_ is deleted from within one of the
    // callbacks, any remaining requests for that job should be cancelled. Right
    // now they will be called.
    while (!requests_.empty()) {
      base::LinkNode<CertVerifierRequest>* request = requests_.head();
      request->RemoveFromList();
      request->value()->Post(*verify_result);
    }
  }

  const CertVerifier::RequestParams key_;
  // The tick count of when the job started. This is used to measure how long
  // the job actually took to complete.
  const base::TimeTicks start_time_;

  RequestList requests_;  // Non-owned.

  const NetLogWithSource net_log_;
  MultiThreadedCertVerifier* cert_verifier_;  // Non-owned.

  bool is_first_job_;
  base::WeakPtrFactory<CertVerifierJob> weak_ptr_factory_;
};

MultiThreadedCertVerifier::MultiThreadedCertVerifier(
    scoped_refptr<CertVerifyProc> verify_proc)
    : requests_(0), inflight_joins_(0), verify_proc_(verify_proc) {}

MultiThreadedCertVerifier::~MultiThreadedCertVerifier() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
std::unique_ptr<MultiThreadedCertVerifier>
MultiThreadedCertVerifier::CreateForDualVerificationTrial(
    scoped_refptr<CertVerifyProc> verify_proc,
    VerifyCompleteCallback verify_complete_callback,
    bool should_record_histograms) {
  return base::WrapUnique(new net::MultiThreadedCertVerifier(
      std::move(verify_proc), std::move(verify_complete_callback),
      should_record_histograms));
}

int MultiThreadedCertVerifier::Verify(const RequestParams& params,
                                      CertVerifyResult* verify_result,
                                      CompletionOnceCallback callback,
                                      std::unique_ptr<Request>* out_req,
                                      const NetLogWithSource& net_log) {
  out_req->reset();

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (callback.is_null() || !verify_result || params.hostname().empty())
    return ERR_INVALID_ARGUMENT;

  requests_++;

  // See if an identical request is currently in flight.
  CertVerifierJob* job = FindJob(params);
  if (job) {
    // An identical request is in flight already. We'll just attach our
    // callback.
    inflight_joins_++;
  } else {
    // Need to make a new job.
    std::unique_ptr<CertVerifierJob> new_job =
        std::make_unique<CertVerifierJob>(params, net_log.net_log(), this);

    new_job->Start(verify_proc_, config_, config_id_);

    job = new_job.get();
    joinable_[job] = std::move(new_job);

    if (requests_ == 1)
      job->set_is_first_job(true);
  }

  std::unique_ptr<CertVerifierRequest> request =
      job->CreateRequest(std::move(callback), verify_result, net_log);
  *out_req = std::move(request);
  return ERR_IO_PENDING;
}

void MultiThreadedCertVerifier::SetConfig(const CertVerifier::Config& config) {
  ++config_id_;
  config_ = config;

  // In C++17, this would be a .merge() call to combine |joinable_| into
  // |inflight_|.
  inflight_.insert(std::make_move_iterator(joinable_.begin()),
                   std::make_move_iterator(joinable_.end()));
  joinable_.clear();
}

bool MultiThreadedCertVerifier::JobComparator::operator()(
    const CertVerifierJob* job1,
    const CertVerifierJob* job2) const {
  return job1->key() < job2->key();
}

MultiThreadedCertVerifier::MultiThreadedCertVerifier(
    scoped_refptr<CertVerifyProc> verify_proc,
    VerifyCompleteCallback verify_complete_callback,
    bool should_record_histograms)
    : config_id_(0),
      requests_(0),
      inflight_joins_(0),
      verify_proc_(verify_proc),
      verify_complete_callback_(std::move(verify_complete_callback)),
      should_record_histograms_(should_record_histograms) {}

std::unique_ptr<CertVerifierJob> MultiThreadedCertVerifier::RemoveJob(
    CertVerifierJob* job) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // See if it's a job from the current generation.
  auto joinable_it = joinable_.find(job);
  if (joinable_it != joinable_.end()) {
    std::unique_ptr<CertVerifierJob> job_ptr = std::move(joinable_it->second);
    joinable_.erase(joinable_it);
    return job_ptr;
  }

  // Otherwise, find it and remove it from previous generations.
  auto it = inflight_.find(job);
  DCHECK(it != inflight_.end());
  std::unique_ptr<CertVerifierJob> job_ptr = std::move(it->second);
  inflight_.erase(it);
  return job_ptr;
}

struct MultiThreadedCertVerifier::JobToRequestParamsComparator {
  bool operator()(const std::pair<CertVerifierJob* const,
                                  std::unique_ptr<CertVerifierJob>>& item,
                  const CertVerifier::RequestParams& value) const {
    return item.first->key() < value;
  }
};

CertVerifierJob* MultiThreadedCertVerifier::FindJob(const RequestParams& key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // The JobSet is kept in sorted order so items can be found using binary
  // search.
  auto it = std::lower_bound(joinable_.begin(), joinable_.end(), key,
                             JobToRequestParamsComparator());
  if (it != joinable_.end() && !(key < it->first->key()))
    return it->first;
  return nullptr;
}

}  // namespace net
