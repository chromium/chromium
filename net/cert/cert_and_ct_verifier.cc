// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_and_ct_verifier.h"

#include "base/callback.h"
#include "net/cert/ct_verifier.h"

namespace net {

CertAndCTVerifier::CertAndCTVerifier(
    std::unique_ptr<CertVerifier> cert_verifier,
    std::unique_ptr<CTVerifier> ct_verifier)
    : cert_verifier_(std::move(cert_verifier)),
      ct_verifier_(std::move(ct_verifier)) {}

CertAndCTVerifier::~CertAndCTVerifier() = default;

int CertAndCTVerifier::Verify(const RequestParams& params,
                              CertVerifyResult* verify_result,
                              CompletionOnceCallback callback,
                              std::unique_ptr<Request>* out_req,
                              const NetLogWithSource& net_log) {
  // It's safe to use |base::Unretained| here, because if this object is
  // deleted, |cert_verifier_| will be deleted and this callback will not
  // be invoked.
  // It's not necessary to wrap |out_req|, because if |out_req| is deleted,
  // this callback will be Reset(), which will also Reset |callback|, because it
  // is moved and bound to |ct_callback|.
  CompletionOnceCallback ct_callback = base::BindOnce(
      &CertAndCTVerifier::OnCertVerifyComplete, base::Unretained(this), params,
      std::move(callback), verify_result, net_log);

  int result = cert_verifier_->Verify(params, verify_result,
                                      std::move(ct_callback), out_req, net_log);

  // If the certificate verification completed synchronously and successfully,
  // then directly perform CT verification (which is always synchronous as it
  // has all the data it needs for SCT verification and does not do any external
  // communication).
  if (result != ERR_IO_PENDING &&
      (result == OK || IsCertificateError(result))) {
    DCHECK(verify_result->verified_cert);
    ct_verifier_->Verify(params.hostname(), verify_result->verified_cert.get(),
                         params.ocsp_response(), params.sct_list(),
                         &verify_result->scts, net_log);
  }

  return result;
}

void CertAndCTVerifier::SetConfig(const Config& config) {
  cert_verifier_->SetConfig(config);
}

void CertAndCTVerifier::OnCertVerifyComplete(const RequestParams& params,
                                             CompletionOnceCallback callback,
                                             CertVerifyResult* verify_result,
                                             const NetLogWithSource& net_log,
                                             int result) {
  // Only perform CT verification if the certificate verification completed
  // successfully.
  if (result == OK || IsCertificateError(result)) {
    DCHECK(verify_result->verified_cert);
    ct_verifier_->Verify(params.hostname(), verify_result->verified_cert.get(),
                         params.ocsp_response(), params.sct_list(),
                         &verify_result->scts, net_log);
  }

  // Now chain to the user's callback, which may delete |this|.
  std::move(callback).Run(result);
}

}  // namespace net
