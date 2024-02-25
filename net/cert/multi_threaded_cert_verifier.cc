// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_threaded_cert_verifier.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"

namespace net {

// Allows DoVerifyOnWorkerThread to wait on a base::WaitableEvent.
// DoVerifyOnWorkerThread may wait on network operations done on a separate
// sequence. For instance when using the NSS-based implementation of certificate
// verification, the library requires a blocking callback for fetching OCSP and
// AIA responses.
class [[maybe_unused,
        nodiscard]] MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives
    : public base::ScopedAllowBaseSyncPrimitives{};

namespace {

// Used to pass the result of DoVerifyOnWorkerThread() to
// MultiThreadedCertVerifier::InternalRequest::OnJobComplete().
struct ResultHelper {
  int error;
  CertVerifyResult result;
  NetLogWithSource net_log;
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

// Runs the verification synchronously on a worker thread.
std::unique_ptr<ResultHelper> DoVerifyOnWorkerThread(
    const scoped_refptr<CertVerifyProc>& verify_proc,
    const scoped_refptr<X509Certificate>& cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    const NetLogWithSource& net_log) {
  TRACE_EVENT0(NetTracingCategory(), "DoVerifyOnWorkerThread");
  auto verify_result = std::make_unique<ResultHelper>();
  verify_result->net_log = net_log;
  MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives
      allow_base_sync_primitives;
  verify_result->error =
      verify_proc->Verify(cert.get(), hostname, ocsp_response, sct_list, flags,
                          &verify_result->result, net_log);
  return verify_result;
}

}  // namespace

// Helper to allow callers to cancel pending CertVerifier::Verify requests.
// Note that because the CertVerifyProc is blocking, it's not actually
// possible to cancel the in-progress request; instead, this simply guarantees
// that the provided callback will not be invoked if the Request is deleted.
class MultiThreadedCertVerifier::InternalRequest
    : public CertVerifier::Request,
      public base::LinkNode<InternalRequest> {
 public:
  InternalRequest(CompletionOnceCallback callback,
                  CertVerifyResult* caller_result);
  ~InternalRequest() override;

  void Start(const scoped_refptr<CertVerifyProc>& verify_proc,
             const CertVerifier::Config& config,
             const CertVerifier::RequestParams& params,
             const NetLogWithSource& caller_net_log);

  void ResetCallback() { callback_.Reset(); }

 private:
  // This is a static method with a |self| weak pointer instead of a regular
  // method, so that PostTask will still run it even if the weakptr is no
  // longer valid.
  static void OnJobComplete(base::WeakPtr<InternalRequest> self,
                            std::unique_ptr<ResultHelper> verify_result);

  CompletionOnceCallback callback_;
  raw_ptr<CertVerifyResult> caller_result_;

  base::WeakPtrFactory<InternalRequest> weak_factory_{this};
};

MultiThreadedCertVerifier::InternalRequest::InternalRequest(
    CompletionOnceCallback callback,
    CertVerifyResult* caller_result)
    : callback_(std::move(callback)), caller_result_(caller_result) {}

MultiThreadedCertVerifier::InternalRequest::~InternalRequest() {
  if (callback_) {
    // This InternalRequest was eagerly cancelled as the callback is still
    // valid, so |this| needs to be removed from MultiThreadedCertVerifier's
    // list.
    RemoveFromList();
  }
}

void MultiThreadedCertVerifier::InternalRequest::Start(
    const scoped_refptr<CertVerifyProc>& verify_proc,
    const CertVerifier::Config& config,
    const CertVerifier::RequestParams& params,
    const NetLogWithSource& caller_net_log) {
  const NetLogWithSource net_log(NetLogWithSource::Make(
      caller_net_log.net_log(), NetLogSourceType::CERT_VERIFIER_TASK));
  net_log.BeginEvent(NetLogEventType::CERT_VERIFIER_TASK);
  caller_net_log.AddEventReferencingSource(
      NetLogEventType::CERT_VERIFIER_TASK_BOUND, net_log.source());

  int flags = GetFlagsForConfig(config);
  if (params.flags() & CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES) {
    flags |= CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DoVerifyOnWorkerThread, verify_proc, params.certificate(),
                     params.hostname(), params.ocsp_response(),
                     params.sct_list(), flags, net_log),
      base::BindOnce(&MultiThreadedCertVerifier::InternalRequest::OnJobComplete,
                     weak_factory_.GetWeakPtr()));
}

// static
void MultiThreadedCertVerifier::InternalRequest::OnJobComplete(
    base::WeakPtr<InternalRequest> self,
    std::unique_ptr<ResultHelper> verify_result) {
  // Always log the EndEvent, even if the Request has been destroyed.
  verify_result->net_log.EndEvent(NetLogEventType::CERT_VERIFIER_TASK);

  // Check |self| weakptr and don't continue if the Request was destroyed.
  if (!self)
    return;

  DCHECK(verify_result);

  // If the MultiThreadedCertVerifier has been deleted, the callback will have
  // been reset to null.
  if (!self->callback_)
    return;

  // If ~MultiThreadedCertVerifier has not Reset() our callback, then this
  // InternalRequest will not have been removed from MultiThreadedCertVerifier's
  // list yet.
  self->RemoveFromList();

  *self->caller_result_ = verify_result->result;
  // Note: May delete |self|.
  std::move(self->callback_).Run(verify_result->error);
}

MultiThreadedCertVerifier::MultiThreadedCertVerifier(
    scoped_refptr<CertVerifyProc> verify_proc,
    scoped_refptr<CertVerifyProcFactory> verify_proc_factory)
    : verify_proc_(std::move(verify_proc)),
      verify_proc_factory_(std::move(verify_proc_factory)) {
  CHECK(verify_proc_);
  CHECK(verify_proc_factory_);
}

MultiThreadedCertVerifier::~MultiThreadedCertVerifier() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Reset the callbacks for each InternalRequest to fulfill the respective
  // net::CertVerifier contract.
  for (base::LinkNode<InternalRequest>* node = request_list_.head();
       node != request_list_.end();) {
    // Resetting the callback may delete the request, so save a pointer to the
    // next node first.
    base::LinkNode<InternalRequest>* next_node = node->next();
    node->value()->ResetCallback();
    node = next_node;
  }
}

int MultiThreadedCertVerifier::Verify(const RequestParams& params,
                                      CertVerifyResult* verify_result,
                                      CompletionOnceCallback callback,
                                      std::unique_ptr<Request>* out_req,
                                      const NetLogWithSource& net_log) {
  CHECK(params.certificate());
  out_req->reset();

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (callback.is_null() || !verify_result || params.hostname().empty())
    return ERR_INVALID_ARGUMENT;

  std::unique_ptr<InternalRequest> request =
      std::make_unique<InternalRequest>(std::move(callback), verify_result);
  request->Start(verify_proc_, config_, params, net_log);
  request_list_.Append(request.get());
  *out_req = std::move(request);
  return ERR_IO_PENDING;
}

void MultiThreadedCertVerifier::UpdateVerifyProcData(
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    const net::CertVerifyProc::ImplParams& impl_params,
    const net::CertVerifyProc::InstanceParams& instance_params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  verify_proc_ = verify_proc_factory_->CreateCertVerifyProc(
      std::move(cert_net_fetcher), impl_params, instance_params);
  CHECK(verify_proc_);
  NotifyCertVerifierChanged();
}

void MultiThreadedCertVerifier::SetConfig(const CertVerifier::Config& config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  config_ = config;
}

void MultiThreadedCertVerifier::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);
}

void MultiThreadedCertVerifier::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

void MultiThreadedCertVerifier::NotifyCertVerifierChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (Observer& observer : observers_) {
    observer.OnCertVerifierChanged();
  }
}

}  // namespace net
