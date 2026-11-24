// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_impl.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/email_verification_request.h"
#include "content/public/browser/render_frame_host.h"

namespace content::webid {

namespace {
const char kEmailVerifierKey[] = "kEmailVerifierKey";
}  // namespace

EmailVerifierImpl::EmailVerifierImpl(RenderFrameHostImpl* render_frame_host)
    : request_builder_(base::BindRepeating(
          [](RenderFrameHostImpl* rfh) {
            return std::make_unique<EmailVerificationRequest>(*rfh);
          },
          render_frame_host)) {}

EmailVerifierImpl::EmailVerifierImpl(RequestBuilder builder)
    : request_builder_(std::move(builder)) {}

EmailVerifierImpl::~EmailVerifierImpl() = default;

void EmailVerifierImpl::Verify(const Result& result,
                               const std::string& nonce,
                               OnEmailVerifiedCallback callback) {
  std::unique_ptr<EmailVerificationRequest> request = request_builder_.Run();
  EmailVerificationRequest* request_ptr = request.get();
  pending_requests_.insert(std::move(request));

  request_ptr->Verify(result, nonce,
                      base::BindOnce(&EmailVerifierImpl::OnVerifyComplete,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     request_ptr, std::move(callback)));
}

void EmailVerifierImpl::CheckIfVerifiable(
    const std::string& email,
    base::OnceClosure on_dns_resolved_callback,
    IsVerifiableCallback callback) {
  std::unique_ptr<EmailVerificationRequest> request = request_builder_.Run();
  EmailVerificationRequest* request_ptr = request.get();
  pending_requests_.insert(std::move(request));

  base::OnceClosure wrapped_dns_resolved_callback =
      on_dns_resolved_callback
          ? base::BindOnce(&EmailVerifierImpl::OnDnsResolved,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(on_dns_resolved_callback))
          : base::OnceClosure();
  request_ptr->CheckIfVerifiable(
      email, std::move(wrapped_dns_resolved_callback),
      base::BindOnce(&EmailVerifierImpl::OnCheckIfVerifiableComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_ptr,
                     std::move(callback)));
}

void EmailVerifierImpl::OnVerifyComplete(
    EmailVerificationRequest* request,
    OnEmailVerifiedCallback callback,
    std::optional<std::string> token,
    blink::mojom::EmailVerificationRequestResult status,
    base::TimeDelta duration) {
  // Erase before running `callback` to release the completed request right
  // away and preventatively guard against `callback` synchronously destroying
  // `this` (only exercised in unit tests today).
  const size_t erased = pending_requests_.erase(request);
  CHECK_EQ(erased, 1u);
  std::move(callback).Run(std::move(token), status, duration);
}

void EmailVerifierImpl::OnDnsResolved(
    base::OnceClosure on_dns_resolved_callback) {
  std::move(on_dns_resolved_callback).Run();
}

void EmailVerifierImpl::OnCheckIfVerifiableComplete(
    EmailVerificationRequest* request,
    IsVerifiableCallback callback,
    std::optional<Result> result,
    blink::mojom::EmailVerificationRequestResult status,
    base::TimeDelta duration) {
  // Erase before running `callback` to release the completed request right
  // away and preventatively guard against `callback` synchronously destroying
  // `this` (only exercised in unit tests today).
  const size_t erased = pending_requests_.erase(request);
  CHECK_EQ(erased, 1u);
  std::move(callback).Run(std::move(result), status, duration);
}

// static
EmailVerifier* EmailVerifier::GetOrCreateForFrame(
    RenderFrameHost* render_frame_host) {
  auto* rfh = static_cast<RenderFrameHostImpl*>(render_frame_host);
  if (!rfh->GetUserData(kEmailVerifierKey)) {
    rfh->SetUserData(kEmailVerifierKey,
                     std::make_unique<EmailVerifierImpl>(rfh));
  }
  return static_cast<EmailVerifier*>(rfh->GetUserData(kEmailVerifierKey));
}

// static
void EmailVerifier::SetForFrameForTest(  // IN-TEST
    RenderFrameHost* render_frame_host,
    std::unique_ptr<EmailVerifier> verifier) {
  auto* rfh = static_cast<RenderFrameHostImpl*>(render_frame_host);
  rfh->SetUserData(kEmailVerifierKey, std::move(verifier));
}

}  // namespace content::webid
