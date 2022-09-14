// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_threaded_cert_verifier.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "crypto/crypto_buildflags.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/cert/x509_util_nss.h"
#endif

namespace net {

// Allows DoVerifyOnWorkerThread to wait on a base::WaitableEvent.
// DoVerifyOnWorkerThread may wait on network operations done on a separate
// sequence. For instance when using the NSS-based implementation of certificate
// verification, the library requires a blocking callback for fetching OCSP and
// AIA responses.
class MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives
    : public base::ScopedAllowBaseSyncPrimitives {};

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
    const scoped_refptr<CRLSet>& crl_set,
    const CertificateList& additional_trust_anchors,
    const NetLogWithSource& net_log) {
  TRACE_EVENT0(NetTracingCategory(), "DoVerifyOnWorkerThread");
  auto verify_result = std::make_unique<ResultHelper>();
  verify_result->net_log = net_log;
  MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives
      allow_base_sync_primitives;
  verify_result->error = verify_proc->Verify(
      cert.get(), hostname, ocsp_response, sct_list, flags, crl_set.get(),
      additional_trust_anchors, &verify_result->result, net_log);
  // The CertVerifyResult is created and populated on the worker thread and
  // then returned to the network thread. Detach now before returning the
  // result, since any further access will be on the network thread.
  verify_result->result.DetachFromSequence();
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
    flags &= ~CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
    flags &= ~CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  }
  DCHECK(config.crl_set);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DoVerifyOnWorkerThread, verify_proc, params.certificate(),
                     params.hostname(), params.ocsp_response(),
                     params.sct_list(), flags, config.crl_set,
                     config.additional_trust_anchors, net_log),
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
    scoped_refptr<CertVerifyProc> verify_proc)
    : MultiThreadedCertVerifier(std::move(verify_proc), nullptr) {}

MultiThreadedCertVerifier::MultiThreadedCertVerifier(
    scoped_refptr<CertVerifyProc> verify_proc,
    scoped_refptr<CertVerifyProcFactory> verify_proc_factory)
    : verify_proc_(std::move(verify_proc)),
      verify_proc_factory_(std::move(verify_proc_factory)) {
  // Guarantee there is always a CRLSet (this can be overridden via SetConfig).
  config_.crl_set = CRLSet::BuiltinCRLSet();
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

void MultiThreadedCertVerifier::UpdateChromeRootStoreData(
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    const ChromeRootStoreData* root_store_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(hchao): investigate to see if we can make this a DCHECK.
  if (verify_proc_factory_) {
    verify_proc_ = verify_proc_factory_->CreateCertVerifyProc(
        std::move(cert_net_fetcher), root_store_data);
  }
}

void MultiThreadedCertVerifier::SetConfig(const CertVerifier::Config& config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG_IF(DFATAL, verify_proc_ &&
                     !verify_proc_->SupportsAdditionalTrustAnchors() &&
                     !config.additional_trust_anchors.empty())
      << "Attempted to set a CertVerifier::Config with additional trust "
         "anchors, but |verify_proc_| does not support additional trust "
         "anchors.";

// TODO(https://crbug.com/978854): Pass these into the actual CertVerifyProc
// rather than relying on global side-effects.
#if !BUILDFLAG(USE_NSS_CERTS)
  // Not yet implemented.
  DCHECK(config.additional_untrusted_authorities.empty());
#else
  // Construct a temporary list and then swap that into the member variable, to
  // be polite to any verifications that might be in progress in a background
  // thread. This ensures that, at least for certs that are present in both the
  // old and new config, there will not be a time when the refcount drops to
  // zero. For the case where a cert was in the old config and is not in the
  // new config, it might be removed while a verification is still going on
  // that might be able to use it. Oh well. Ideally the list should be passed
  // into CertVerifyProc as noted by the TODO(https://crbug.com/978854), since
  // the workers could then keep a reference to the appropriate certs as long
  // as they need.
  net::ScopedCERTCertificateList temp_certs;
  for (const auto& cert : config.additional_untrusted_authorities) {
    ScopedCERTCertificate nss_cert =
        x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
    if (nss_cert)
      temp_certs.push_back(std::move(nss_cert));
  }
  temp_certs_ = std::move(temp_certs);
#endif

  config_ = config;
  if (!config_.crl_set)
    config_.crl_set = CRLSet::BuiltinCRLSet();
}

}  // namespace net
