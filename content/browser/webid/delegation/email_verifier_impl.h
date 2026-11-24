// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/webid/delegation/email_verification_request.h"
#include "content/common/content_export.h"
#include "content/public/browser/webid/email_verifier.h"
#include "third_party/blink/public/mojom/webid/email_verification_request.mojom-shared.h"

// An implementation of an email verifier that follows the
// Email Verification Protocol as described here:
// https://github.com/dickhardt/email-verification-protocol
//
// EmailVerifierImpl is associated with a valid and alive
// RenderFrameHost which has to outlive it.
namespace content {

class RenderFrameHostImpl;

namespace webid {

class CONTENT_EXPORT EmailVerifierImpl : public EmailVerifier {
 public:
  using RequestBuilder =
      base::RepeatingCallback<std::unique_ptr<EmailVerificationRequest>()>;

  explicit EmailVerifierImpl(RenderFrameHostImpl* rfh);
  explicit EmailVerifierImpl(RequestBuilder builder);
  // Cancels any in-flight requests; callbacks are not invoked after `this` is
  // destroyed.
  ~EmailVerifierImpl() override;

  EmailVerifierImpl(const EmailVerifierImpl&) = delete;
  EmailVerifierImpl& operator=(const EmailVerifierImpl&) = delete;

  // EmailVerifier:
  void CheckIfVerifiable(const std::string& email,
                         base::OnceClosure on_dns_resolved_callback,
                         IsVerifiableCallback callback) override;
  void Verify(const Result& result,
              const std::string& nonce,
              OnEmailVerifiedCallback callback) override;

 private:
  void OnVerifyComplete(EmailVerificationRequest* request,
                        OnEmailVerifiedCallback callback,
                        std::optional<std::string> token,
                        blink::mojom::EmailVerificationRequestResult status,
                        base::TimeDelta duration);
  void OnDnsResolved(base::OnceClosure on_dns_resolved_callback);
  void OnCheckIfVerifiableComplete(
      EmailVerificationRequest* request,
      IsVerifiableCallback callback,
      std::optional<Result> result,
      blink::mojom::EmailVerificationRequestResult status,
      base::TimeDelta duration);

  RequestBuilder request_builder_;

  // In-flight `CheckIfVerifiable()` and `Verify()` requests. Owned here so
  // destroying `this` cancels any pending requests.
  base::flat_set<std::unique_ptr<EmailVerificationRequest>,
                 base::UniquePtrComparator>
      pending_requests_;

  base::WeakPtrFactory<EmailVerifierImpl> weak_ptr_factory_{this};
};

}  // namespace webid

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_
