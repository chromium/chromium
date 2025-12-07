// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/email_verification_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"

namespace content::webid {

namespace {
const char kEmailVerifierKey[] = "kEmailVerifierKey";
}

EmailVerifierImpl::EmailVerifierImpl(RenderFrameHostImpl* render_frame_host)
    : request_builder_(base::BindRepeating(
          [](RenderFrameHostImpl* rfh) {
            return std::make_unique<EmailVerificationRequest>(*rfh);
          },
          render_frame_host)) {}

EmailVerifierImpl::EmailVerifierImpl(RequestBuilder builder)
    : request_builder_(std::move(builder)) {}

EmailVerifierImpl::~EmailVerifierImpl() = default;

void EmailVerifierImpl::Verify(
    const std::string& email,
    const std::string& nonce,
    EmailVerifier::OnEmailVerifiedCallback callback) {
  auto request = request_builder_.Run();
  auto* request_ptr = request.get();

  request_ptr->Send(email, nonce,
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
EmailVerifier* EmailVerifier::GetOrCreateForFrame(
    RenderFrameHost* render_frame_host) {
  auto* rfh = static_cast<RenderFrameHostImpl*>(render_frame_host);
  if (!rfh->GetUserData(kEmailVerifierKey)) {
    rfh->SetUserData(kEmailVerifierKey,
                     std::make_unique<EmailVerifierImpl>(rfh));
  }
  return static_cast<EmailVerifier*>(rfh->GetUserData(kEmailVerifierKey));
}

}  // namespace content::webid
