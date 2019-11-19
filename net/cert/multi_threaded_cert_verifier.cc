// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_threaded_cert_verifier.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"

namespace net {

// Allows DoVerifyOnWorkerThread to wait on a base::WaitableEvent.
// DoVerifyOnWorkerThread may wait on network operations done on a separate
// sequence. For instance when using the NSS-based implementation of certificate
// verification, the library requires a blocking callback for fetching OCSP and
// AIA responses.
class MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives
    : public base::ScopedAllowBaseSyncPrimitives {};

namespace {

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

// DoVerifyOnWorkerThread runs the verification synchronously on a worker
// thread.
std::unique_ptr<ResultHelper> DoVerifyOnWorkerThread(
    const scoped_refptr<CertVerifyProc>& verify_proc,
    const scoped_refptr<X509Certificate>& cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    const scoped_refptr<CRLSet>& crl_set,
    const CertificateList& additional_trust_anchors) {
  TRACE_EVENT0(NetTracingCategory(), "DoVerifyOnWorkerThread");
  auto verify_result = std::make_unique<ResultHelper>();
  MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives
      allow_base_sync_primitives;
  verify_result->error = verify_proc->Verify(
      cert.get(), hostname, ocsp_response, sct_list, flags, crl_set.get(),
      additional_trust_anchors, &verify_result->result);
  // The CertVerifyResult is created and populated on the worker thread and
  // then returned to the network thread. Detach now before returning the
  // result, since any further access will be on the network thread.
  verify_result->result.DetachFromSequence();
  return verify_result;
}

// Helper to allow callers to cancel pending CertVerifier::Verify requests.
// Note that because the CertVerifyProc is blocking, it's not actually
// possible to cancel the in-progress request; instead, this simply guarantees
// that the provided callback will not be invoked if the Request is deleted.
class InternalRequest : public CertVerifier::Request {
 public:
  InternalRequest(CompletionOnceCallback callback,
                  CertVerifyResult* caller_result);
  ~InternalRequest() override;

  void Start(const scoped_refptr<CertVerifyProc>& verify_proc,
             const CertVerifier::Config& config,
             const CertVerifier::RequestParams& params);

 private:
  void OnJobComplete(std::unique_ptr<ResultHelper> verify_result);

  CompletionOnceCallback callback_;
  CertVerifyResult* caller_result_;

  base::WeakPtrFactory<InternalRequest> weak_factory_{this};
};

InternalRequest::InternalRequest(CompletionOnceCallback callback,
                                 CertVerifyResult* caller_result)
    : callback_(std::move(callback)), caller_result_(caller_result) {}

InternalRequest::~InternalRequest() = default;

void InternalRequest::Start(const scoped_refptr<CertVerifyProc>& verify_proc,
                            const CertVerifier::Config& config,
                            const CertVerifier::RequestParams& params) {
  int flags = GetFlagsForConfig(config);
  if (params.flags() & CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES) {
    flags &= ~CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
    flags &= ~CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  }
  DCHECK(config.crl_set);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DoVerifyOnWorkerThread, verify_proc, params.certificate(),
                     params.hostname(), params.ocsp_response(),
                     params.sct_list(), flags, config.crl_set,
                     config.additional_trust_anchors),
      base::BindOnce(&InternalRequest::OnJobComplete,
                     weak_factory_.GetWeakPtr()));
}

void InternalRequest::OnJobComplete(
    std::unique_ptr<ResultHelper> verify_result) {
  *caller_result_ = verify_result->result;
  // Note: May delete |this|.
  std::move(callback_).Run(verify_result->error);
}

}  // namespace

MultiThreadedCertVerifier::MultiThreadedCertVerifier(
    scoped_refptr<CertVerifyProc> verify_proc)
    : verify_proc_(std::move(verify_proc)) {
  // Guarantee there is always a CRLSet (this can be overridden via SetConfig).
  config_.crl_set = CRLSet::BuiltinCRLSet();
}

MultiThreadedCertVerifier::~MultiThreadedCertVerifier() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  request->Start(verify_proc_, config_, params);
  *out_req = std::move(request);
  return ERR_IO_PENDING;
}

void MultiThreadedCertVerifier::SetConfig(const CertVerifier::Config& config) {
  config_ = config;
  if (!config_.crl_set)
    config_.crl_set = CRLSet::BuiltinCRLSet();
}

}  // namespace net
