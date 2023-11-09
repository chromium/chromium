// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cert_verifier_with_trust_anchors.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/coalescing_cert_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"

namespace network {

namespace {

void MaybeSignalAnchorUse(int error,
                          const base::RepeatingClosure& anchor_used_callback,
                          const net::CertVerifyResult& verify_result) {
  if (error != net::OK || !verify_result.is_issued_by_additional_trust_anchor ||
      anchor_used_callback.is_null()) {
    return;
  }
  anchor_used_callback.Run();
}

void CompleteAndSignalAnchorUse(
    const base::RepeatingClosure& anchor_used_callback,
    net::CompletionOnceCallback completion_callback,
    const net::CertVerifyResult* verify_result,
    int error) {
  MaybeSignalAnchorUse(error, anchor_used_callback, *verify_result);
  std::move(completion_callback).Run(error);
}

net::CertVerifier::Config ExtendTrustAnchorsAndTempCerts(
    const net::CertVerifier::Config& config,
    const net::CertificateList& trust_anchors,
    const net::CertificateList& untrusted_authorities) {
  net::CertVerifier::Config new_config = config;
  new_config.additional_trust_anchors.insert(
      new_config.additional_trust_anchors.begin(), trust_anchors.begin(),
      trust_anchors.end());
  new_config.additional_untrusted_authorities.insert(
      new_config.additional_untrusted_authorities.begin(),
      untrusted_authorities.begin(), untrusted_authorities.end());
  return new_config;
}

}  // namespace

CertVerifierWithTrustAnchors::CertVerifierWithTrustAnchors(
    const base::RepeatingClosure& anchor_used_callback)
    : anchor_used_callback_(anchor_used_callback) {
  DETACH_FROM_THREAD(thread_checker_);
}

CertVerifierWithTrustAnchors::~CertVerifierWithTrustAnchors() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void CertVerifierWithTrustAnchors::InitializeOnIOThread(
    std::unique_ptr<net::CertVerifier> delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = std::move(delegate);
  delegate_->SetConfig(ExtendTrustAnchorsAndTempCerts(
      orig_config_, trust_anchors_, untrusted_authorities_));
}

void CertVerifierWithTrustAnchors::SetAdditionalCerts(
    const net::CertificateList& trust_anchors,
    const net::CertificateList& untrusted_authorities) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (std::tie(trust_anchors, untrusted_authorities) ==
      std::tie(trust_anchors_, untrusted_authorities_))
    return;
  trust_anchors_ = trust_anchors;
  untrusted_authorities_ = untrusted_authorities;
  if (!delegate_)
    return;
  delegate_->SetConfig(ExtendTrustAnchorsAndTempCerts(
      orig_config_, trust_anchors_, untrusted_authorities_));
}

int CertVerifierWithTrustAnchors::Verify(
    const RequestParams& params,
    net::CertVerifyResult* verify_result,
    net::CompletionOnceCallback completion_callback,
    std::unique_ptr<Request>* out_req,
    const net::NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  DCHECK(completion_callback);
  net::CompletionOnceCallback wrapped_callback =
      base::BindOnce(&CompleteAndSignalAnchorUse, anchor_used_callback_,
                     std::move(completion_callback), verify_result);

  int error = delegate_->Verify(params, verify_result,
                                std::move(wrapped_callback), out_req, net_log);
  MaybeSignalAnchorUse(error, anchor_used_callback_, *verify_result);
  return error;
}

void CertVerifierWithTrustAnchors::SetConfig(const Config& config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  orig_config_ = config;
  delegate_->SetConfig(ExtendTrustAnchorsAndTempCerts(
      orig_config_, trust_anchors_, untrusted_authorities_));
}

void CertVerifierWithTrustAnchors::AddObserver(Observer* observer) {
  delegate_->AddObserver(observer);
}

void CertVerifierWithTrustAnchors::RemoveObserver(Observer* observer) {
  delegate_->RemoveObserver(observer);
}

}  // namespace network
