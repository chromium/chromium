// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_

#include <map>
#include <memory>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/webid/email_verifier.h"

// An implementation of an email verifier that follows the
// Email Verification Protocol as described here:
// https://github.com/dickhardt/email-verification-protocol
//
// EmailVerifierImpl is associated with a valid and alive
// RenderFrameHost which has to outlive it.
namespace content {

class RenderFrameHostImpl;

namespace webid {

class EmailVerificationRequest;

class CONTENT_EXPORT EmailVerifierImpl : public EmailVerifier {
 public:
  using RequestBuilder =
      base::RepeatingCallback<std::unique_ptr<EmailVerificationRequest>()>;

  explicit EmailVerifierImpl(RenderFrameHostImpl* rfh);
  explicit EmailVerifierImpl(RequestBuilder builder);
  ~EmailVerifierImpl() override;

  EmailVerifierImpl(const EmailVerifierImpl&) = delete;
  EmailVerifierImpl& operator=(const EmailVerifierImpl&) = delete;

  // Starts the verification process for the given `email`.
  void Verify(const std::string& email,
              const std::string& nonce,
              EmailVerifier::OnEmailVerifiedCallback callback) override;

 private:
  void OnRequestComplete(std::unique_ptr<EmailVerificationRequest> request,
                         EmailVerifier::OnEmailVerifiedCallback callback,
                         std::optional<std::string> result);

  RequestBuilder request_builder_;

  base::WeakPtrFactory<EmailVerifierImpl> weak_ptr_factory_{this};
};

}  // namespace webid

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_
