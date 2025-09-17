// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/email_verification_request.h"
#include "content/public/browser/storage_partition.h"

namespace content::webid {

EmailVerifierImpl::EmailVerifierImpl(
    content::RenderFrameHost& render_frame_host)
    : request_builder_(base::BindRepeating(
          [](content::RenderFrameHost* rfh) {
            return std::make_unique<EmailVerificationRequest>(*rfh);
          },
          &render_frame_host)) {}

EmailVerifierImpl::EmailVerifierImpl(RequestBuilder builder)
    : request_builder_(std::move(builder)) {}

EmailVerifierImpl::~EmailVerifierImpl() = default;

void EmailVerifierImpl::Verify(
    const std::string& email,
    const std::string& nonce,
    const url::Origin& rp_origin,
    EmailVerifier::OnEmailVerifiedCallback callback) {
  auto request = request_builder_.Run();
  auto* request_ptr = request.get();

  request_ptr->Send(email, nonce, rp_origin,
                    base::BindOnce(&EmailVerifierImpl::OnRequestComplete,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(request), std::move(callback)));
}

void EmailVerifierImpl::OnRequestComplete(
    std::unique_ptr<EmailVerificationRequest> request,
    EmailVerifier::OnEmailVerifiedCallback callback,
    std::optional<std::string> result) {
  std::move(callback).Run(std::move(result));
}

// static
std::unique_ptr<EmailVerifier> EmailVerifier::Create(
    content::RenderFrameHost& render_frame_host) {
  return std::make_unique<EmailVerifierImpl>(render_frame_host);
}

}  // namespace content::webid
