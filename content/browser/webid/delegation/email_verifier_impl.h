// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_

#include <map>
#include <memory>
#include <set>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/webid/delegation/email_verification_request.h"
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

class CONTENT_EXPORT EmailVerifierImpl : public EmailVerifier {
 public:
  using RequestBuilder =
      base::RepeatingCallback<std::unique_ptr<EmailVerificationRequest>()>;

  explicit EmailVerifierImpl(RenderFrameHostImpl* rfh);
  explicit EmailVerifierImpl(RequestBuilder builder);
  ~EmailVerifierImpl() override;

  EmailVerifierImpl(const EmailVerifierImpl&) = delete;
  EmailVerifierImpl& operator=(const EmailVerifierImpl&) = delete;

  // Checks if the given `email` is verifiable.
  void CheckIfVerifiable(const std::string& email,
                         IsVerifiableCallback callback) override;

  // Starts the verification process.
  void Verify(const EmailVerifier::Result& result,
              const std::string& nonce,
              EmailVerifier::OnEmailVerifiedCallback callback) override;
 private:
  class PerformanceMetricsObserver : public EmailVerificationRequest::Observer {
   public:
    PerformanceMetricsObserver();
    ~PerformanceMetricsObserver() override;

    void OnIsVerifiableStart(EmailVerificationRequest* request) override;
    void OnIsVerifiableComplete(
        EmailVerificationRequest* request,
        blink::mojom::EmailVerificationRequestResult status) override;
    void OnVerifyStart(EmailVerificationRequest* request) override;
    void OnVerifyComplete(
        EmailVerificationRequest* request,
        blink::mojom::EmailVerificationRequestResult status) override;
    void OnRequestDestroyed(EmailVerificationRequest* request) override;

   private:
    // Maps request pointers to their corresponding start times. We use maps
    // (instead of scalar members) because multiple requests can execute
    // concurrently, and using the request pointer as a key guarantees timing
    // isolation. Request lifetimes are safely tracked, and entries are erased
    // either on completion, or inside `OnRequestDestroyed()` if a request is
    // cancelled or destroyed prematurely.
    base::flat_map<EmailVerificationRequest*, base::TimeTicks>
        is_verifiable_start_times_;
    base::flat_map<EmailVerificationRequest*, base::TimeTicks>
        verify_start_times_;
  };

  void OnRequestComplete(std::unique_ptr<EmailVerificationRequest> request,
                         EmailVerifier::OnEmailVerifiedCallback callback,
                         std::optional<std::string> result);

  RequestBuilder request_builder_;

  PerformanceMetricsObserver performance_metrics_observer_;

  base::WeakPtrFactory<EmailVerifierImpl> weak_ptr_factory_{this};
};

}  // namespace webid

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EMAIL_VERIFIER_IMPL_H_
