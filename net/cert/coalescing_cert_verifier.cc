// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/coalescing_cert_verifier.h"

#include <algorithm>

#include "base/bind.h"
#include "base/containers/linked_list.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/pem.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"

namespace net {

// DESIGN OVERVIEW:
//
// The CoalescingCertVerifier implements an algorithm to group multiple calls
// to Verify() into a single Job. This avoids overloading the underlying
// CertVerifier, particularly those that are expensive to talk to (e.g.
// talking to the system verifier or across processes), batching multiple
// requests to CoaleacingCertVerifier::Verify() into a single underlying call.
//
// However, this makes lifetime management a bit more complex.
//   - The Job object represents all of the state for a single verification to
//     the CoalescingCertVerifier's underlying CertVerifier.
//       * It keeps the CertVerifyResult alive, which is required as long as
//         there is a pending verification.
//       * It keeps the CertVerify::Request to the underlying verifier alive,
//         as long as there is a pending Request attached to the Job.
//       * It keeps track of every CoalescingCertVerifier::Request that is
//         interested in receiving notification. However, it does NOT own
//         these objects, and thus needs to coordinate with the Request (via
//         AddRequest/AbortRequest) to make sure it never has a stale
//         pointer.
//         NB: It would have also been possible for the Job to only
//         hold WeakPtr<Request>s, rather than Request*, but that seemed less
//         clear as to the lifetime invariants, even if it was more clear
//         about how the pointers are used.
//  - The Job object is always owned by the CoalescingCertVerifier. If the
//    CoalescingCertVerifier is deleted, all in-flight requests to the
//    underlying verifier should be cancelled. When the Job goes away, all the
//    Requests will be orphaned.
//  - The Request object is always owned by the CALLER. It is a handle to
//    allow a caller to cancel a request, per the CertVerifier interface. If
//    the Request goes away, no caller callbacks should be invoked if the Job
//    it was (previously) attached to completes.
//  - Per the CertVerifier interface, when the CoalescingCertVerifier is
//    deleted, then regardless of there being any live Requests, none of those
//    caller callbacks should be invoked.
//
// Finally, to add to the complexity, it's possible that, during the handling
// of a result from the underlying CertVerifier, a Job may begin dispatching
// to its Requests. The Request may delete the CoalescingCertVerifier. If that
// happens, then the Job being processed is also deleted, and none of the
// other Requests should be notified.

namespace {

base::Value CertVerifierParams(const CertVerifier::RequestParams& params) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("certificates",
              NetLogX509CertificateList(params.certificate().get()));
  if (!params.ocsp_response().empty()) {
    dict.SetStringPath("ocsp_response", PEMEncode(params.ocsp_response(),
                                                  "NETLOG OCSP RESPONSE"));
  }
  if (!params.sct_list().empty()) {
    dict.SetStringPath("sct_list",
                       PEMEncode(params.sct_list(), "NETLOG SCT LIST"));
  }
  dict.SetPath("host", NetLogStringValue(params.hostname()));
  dict.SetIntPath("verifier_flags", params.flags());

  return dict;
}

}  // namespace

// Job contains all the state for a single verification using the underlying
// verifier.
class CoalescingCertVerifier::Job {
 public:
  Job(CoalescingCertVerifier* parent,
      const CertVerifier::RequestParams& params,
      NetLog* net_log,
      bool is_first_job);
  ~Job();

  const CertVerifier::RequestParams& params() const { return params_; }
  const CertVerifyResult& verify_result() const { return verify_result_; }

  // Attaches |request|, causing it to be notified once this Job completes.
  void AddRequest(CoalescingCertVerifier::Request* request);

  // Stops |request| from being notified. If there are no Requests remaining,
  // the Job will be cancelled.
  // NOTE: It's only necessary to call this if the Job has not yet completed.
  // If the Request has been notified of completion, this should not be called.
  void AbortRequest(CoalescingCertVerifier::Request* request);

  // Starts a verification using |underlying_verifier|. If this completes
  // synchronously, returns the result code, with the associated result being
  // available via |verify_result()|. Otherwise, it will complete
  // asynchronously, notifying any Requests associated via |AttachRequest|.
  int Start(CertVerifier* underlying_verifier);

 private:
  void OnVerifyComplete(int result);

  void LogMetrics();

  CoalescingCertVerifier* parent_verifier_;
  const CertVerifier::RequestParams params_;
  const NetLogWithSource net_log_;
  bool is_first_job_ = false;
  CertVerifyResult verify_result_;

  base::TimeTicks start_time_;
  std::unique_ptr<CertVerifier::Request> pending_request_;

  base::LinkedList<CoalescingCertVerifier::Request> attached_requests_;
  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

// Tracks the state associated with a single CoalescingCertVerifier::Verify
// request.
//
// There are two ways for requests to be cancelled:
//   - The caller of Verify() can delete the Request object, indicating
//     they are no longer interested in this particular request.
//   - The caller can delete the CoalescingCertVerifier, which should cause
//     all in-process Jobs to be aborted and deleted. Any Requests attached to
//     Jobs should be orphaned, and do nothing when the Request is (eventually)
//     deleted.
class CoalescingCertVerifier::Request
    : public base::LinkNode<CoalescingCertVerifier::Request>,
      public CertVerifier::Request {
 public:
  // Create a request that will be attached to |job|, and will notify
  // |callback| and fill |verify_result| if the Job completes successfully.
  // If the Request is deleted, or the Job is deleted, |callback| will not
  // be notified.
  Request(CoalescingCertVerifier::Job* job,
          CertVerifyResult* verify_result,
          CompletionOnceCallback callback,
          const NetLogWithSource& net_log);

  ~Request() override;

  const NetLogWithSource& net_log() const { return net_log_; }

  // Called by Job to complete the requests (either successfully or as a sign
  // that the underlying Job is going away).
  void Complete(int result);

  // Called when |job_| is being deleted, to ensure that the Request does not
  // attempt to access the Job further. No callbacks will be invoked,
  // consistent with the CoalescingCertVerifier's contract.
  void OnJobAbort();

 private:
  CoalescingCertVerifier::Job* job_;

  CertVerifyResult* verify_result_;
  CompletionOnceCallback callback_;
  const NetLogWithSource net_log_;
};

CoalescingCertVerifier::Job::Job(CoalescingCertVerifier* parent,
                                 const CertVerifier::RequestParams& params,
                                 NetLog* net_log,
                                 bool is_first_job)
    : parent_verifier_(parent),
      params_(params),
      net_log_(
          NetLogWithSource::Make(net_log, NetLogSourceType::CERT_VERIFIER_JOB)),
      is_first_job_(is_first_job) {}

CoalescingCertVerifier::Job::~Job() {
  // If there was at least one outstanding Request still pending, then this
  // Job was aborted, rather than being completed normally and cleaned up.
  if (!attached_requests_.empty() && pending_request_) {
    net_log_.AddEvent(NetLogEventType::CANCELLED);
    net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_JOB);
  }

  while (!attached_requests_.empty()) {
    auto* link_node = attached_requests_.head();
    link_node->RemoveFromList();
    link_node->value()->OnJobAbort();
  }
}

void CoalescingCertVerifier::Job::AddRequest(
    CoalescingCertVerifier::Request* request) {
  // There must be a pending asynchronous verification in process.
  DCHECK(pending_request_);

  request->net_log().AddEventReferencingSource(
      NetLogEventType::CERT_VERIFIER_REQUEST_BOUND_TO_JOB, net_log_.source());
  attached_requests_.Append(request);
}

void CoalescingCertVerifier::Job::AbortRequest(
    CoalescingCertVerifier::Request* request) {
  // Check to make sure |request| hasn't already been removed.
  DCHECK(request->previous() || request->next());

  request->RemoveFromList();

  // If there are no more pending requests, abort. This isn't strictly
  // necessary; the request could be allowed to run to completion (and
  // potentially to allow later Requests to join in), but in keeping with the
  // idea of providing more stable guarantees about resources, clean up early.
  if (attached_requests_.empty()) {
    // If this was the last Request, then the Job had not yet completed; this
    // matches the logic in the dtor, which handles when it's the Job that is
    // deleted first, rather than the last Request.
    net_log_.AddEvent(NetLogEventType::CANCELLED);
    net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_JOB);

    // DANGER: This will cause |this_| to be deleted!
    parent_verifier_->RemoveJob(this);
    return;
  }
}

int CoalescingCertVerifier::Job::Start(CertVerifier* underlying_verifier) {
  // Requests are only attached for asynchronous completion, so they must
  // always be attached after Start() has been called.
  DCHECK(attached_requests_.empty());
  // There should not be a pending request already started (e.g. Start called
  // multiple times).
  DCHECK(!pending_request_);

  net_log_.BeginEvent(NetLogEventType::CERT_VERIFIER_JOB,
                      [&] { return CertVerifierParams(params_); });

  verify_result_.Reset();

  start_time_ = base::TimeTicks::Now();
  int result = underlying_verifier->Verify(
      params_, &verify_result_,
      // Safe, because |verify_request_| is self-owned and guarantees the
      // callback won't be called if |this| is deleted.
      base::BindOnce(&CoalescingCertVerifier::Job::OnVerifyComplete,
                     base::Unretained(this)),
      &pending_request_, net_log_);
  if (result != ERR_IO_PENDING) {
    LogMetrics();
    net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_JOB,
                      [&] { return verify_result_.NetLogParams(result); });
  }

  return result;
}

void CoalescingCertVerifier::Job::OnVerifyComplete(int result) {
  LogMetrics();

  pending_request_.reset();  // Reset to signal clean completion.
  net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_JOB,
                    [&] { return verify_result_.NetLogParams(result); });

  // It's possible that during the process of invoking a callback for a
  // Request, |this| may get deleted (along with the associated parent). If
  // that happens, it's important to ensure that processing of the Job is
  // stopped - i.e. no other callbacks are invoked for other Requests, nor is
  // |this| accessed.
  //
  // To help detect and protect against this, a WeakPtr to |this| is taken. If
  // |this| is deleted, the destructor will have invalidated the WeakPtr.
  //
  // Note that if a Job had already been deleted, this method would not have
  // been invoked in the first place, as the Job (via |pending_request_|) owns
  // the underlying CertVerifier::Request that this method was bound to as a
  // callback. This is why it's OK to grab the WeakPtr from |this| initially.
  base::WeakPtr<Job> weak_this = weak_ptr_factory_.GetWeakPtr();
  while (!attached_requests_.empty()) {
    // Note: It's also possible for additional Requests to be attached to the
    // current Job while processing a Request.
    auto* link_node = attached_requests_.head();
    link_node->RemoveFromList();

    // Note: |this| MAY be deleted here.
    //   - If the CoalescingCertVerifier is deleted, it will delete the
    //     Jobs (including |this|)
    //   - If this is the second-to-last Request, and the completion of this
    //     event causes the other Request to be deleted, detaching that Request
    //     from this Job will lead to this Job being deleted (via
    //     Job::AbortRequest())
    link_node->value()->Complete(result);

    // Check if |this| has been deleted (which implicitly includes
    // |parent_verifier_|), and abort if so, since no further cleanup is
    // needed.
    if (!weak_this)
      return;
  }

  // DANGER: |this| will be invalidated (deleted) after this point.
  return parent_verifier_->RemoveJob(this);
}

void CoalescingCertVerifier::Job::LogMetrics() {
  base::TimeDelta latency = base::TimeTicks::Now() - start_time_;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_Job_Latency", latency,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10), 100);
  if (is_first_job_) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_First_Job_Latency", latency,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMinutes(10), 100);
  }
}

CoalescingCertVerifier::Request::Request(CoalescingCertVerifier::Job* job,
                                         CertVerifyResult* verify_result,
                                         CompletionOnceCallback callback,
                                         const NetLogWithSource& net_log)
    : job_(job),
      verify_result_(verify_result),
      callback_(std::move(callback)),
      net_log_(net_log) {
  net_log_.BeginEvent(NetLogEventType::CERT_VERIFIER_REQUEST);
}

CoalescingCertVerifier::Request::~Request() {
  if (job_) {
    net_log_.AddEvent(NetLogEventType::CANCELLED);
    net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_REQUEST);

    // If the Request is deleted before the Job, then detach from the Job.
    // Note: This may cause |job_| to be deleted.
    job_->AbortRequest(this);
    job_ = nullptr;
  }
}

void CoalescingCertVerifier::Request::Complete(int result) {
  DCHECK(job_);  // There must be a pending/non-aborted job to complete.

  *verify_result_ = job_->verify_result();

  // On successful completion, the Job removes the Request from its set;
  // similarly, break the association here so that when the Request is
  // deleted, it does not try to abort the (now-completed) Job.
  job_ = nullptr;

  net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_REQUEST);

  // Run |callback_|, which may delete |this|.
  std::move(callback_).Run(result);
}

void CoalescingCertVerifier::Request::OnJobAbort() {
  DCHECK(job_);  // There must be a pending job to abort.

  // If the Job is deleted before the Request, just clean up. The Request will
  // eventually be deleted by the caller.
  net_log_.AddEvent(NetLogEventType::CANCELLED);
  net_log_.EndEvent(NetLogEventType::CERT_VERIFIER_REQUEST);

  job_ = nullptr;
  // Note: May delete |this|, if the caller made |callback_| own the Request.
  callback_.Reset();
}

CoalescingCertVerifier::CoalescingCertVerifier(
    std::unique_ptr<CertVerifier> verifier)
    : verifier_(std::move(verifier)),
      config_id_(0),
      requests_(0),
      inflight_joins_(0) {}

CoalescingCertVerifier::~CoalescingCertVerifier() = default;

int CoalescingCertVerifier::Verify(
    const RequestParams& params,
    CertVerifyResult* verify_result,
    CompletionOnceCallback callback,
    std::unique_ptr<CertVerifier::Request>* out_req,
    const NetLogWithSource& net_log) {
  DCHECK(verify_result);
  DCHECK(!callback.is_null());

  out_req->reset();
  ++requests_;

  Job* job = FindJob(params);
  if (job) {
    // An identical request is in-flight and joinable, so just attach the
    // callback.
    ++inflight_joins_;
  } else {
    // No existing Jobs can be used. Create and start a new one.
    std::unique_ptr<Job> new_job =
        std::make_unique<Job>(this, params, net_log.net_log(), requests_ == 1);
    int result = new_job->Start(verifier_.get());
    if (result != ERR_IO_PENDING) {
      *verify_result = new_job->verify_result();
      return result;
    }

    job = new_job.get();
    joinable_jobs_[params] = std::move(new_job);
  }

  std::unique_ptr<CoalescingCertVerifier::Request> request =
      std::make_unique<CoalescingCertVerifier::Request>(
          job, verify_result, std::move(callback), net_log);
  job->AddRequest(request.get());
  *out_req = std::move(request);
  return ERR_IO_PENDING;
}

void CoalescingCertVerifier::SetConfig(const CertVerifier::Config& config) {
  ++config_id_;
  verifier_->SetConfig(config);

  for (auto& job : joinable_jobs_) {
    inflight_jobs_.emplace_back(std::move(job.second));
  }
  joinable_jobs_.clear();
}

CoalescingCertVerifier::Job* CoalescingCertVerifier::FindJob(
    const RequestParams& params) {
  auto it = joinable_jobs_.find(params);
  if (it != joinable_jobs_.end())
    return it->second.get();
  return nullptr;
}

void CoalescingCertVerifier::RemoveJob(Job* job) {
  // See if this was a job from the current configuration generation.
  // Note: It's also necessary to compare that the underlying pointer is the
  // same, and not merely a Job with the same parameters.
  auto joinable_it = joinable_jobs_.find(job->params());
  if (joinable_it != joinable_jobs_.end() && joinable_it->second.get() == job) {
    joinable_jobs_.erase(joinable_it);
    return;
  }

  // Otherwise, it MUST have been a job from a previous generation.
  auto inflight_it = std::find_if(inflight_jobs_.begin(), inflight_jobs_.end(),
                                  base::MatchesUniquePtr(job));
  DCHECK(inflight_it != inflight_jobs_.end());
  inflight_jobs_.erase(inflight_it);
  return;
}

}  // namespace net
