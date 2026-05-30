// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBID_EMAIL_VERIFIER_H_
#define CONTENT_PUBLIC_BROWSER_WEBID_EMAIL_VERIFIER_H_

#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/schemeful_site.h"

namespace content::webid {

// An implementation of an email verifier that follows the
// Email Verification Protocol (EVP for short) as described here:
// https://github.com/dickhardt/email-verification-protocol
//
// EmailVerifier is associated with a valid and alive
// RenderFrameHost which has to outlive it.
class EmailVerifier : public base::SupportsUserData::Data {
 public:
  // The result of checking for EVP support on the issuer
  // for a specific email address.
  struct Result {
    // The email address that is being verified.
    std::string email;

    // The site of the issuer that verified the email.
    net::SchemefulSite issuer_site;

    // The endpoint to request tokens from.
    GURL issuance_endpoint;

    // The JWKS URI to fetch public keys.
    GURL jwks_uri;

    // The signing algorithms supported by the issuer.
    std::vector<std::string> signing_alg_values_supported;

    bool operator==(const Result& other) const = default;
  };

  using OnEmailVerifiedCallback =
      base::OnceCallback<void(std::optional<std::string> token)>;

  ~EmailVerifier() override = default;

  // Phase 1: Pre-Prompt Validation
  // Performs a silent background check to determine if an email address can be
  // verified automatically. This triggers network discovery (DNS lookups and
  // .well-known fetches and the accounts list fetch) to confirm that:
  //   1) The email domain supports EVP.
  //   2) The user is actively logged into that issuer.
  //
  // Embedders MUST call this before showing a permission prompt to ensure
  // the browser only requests permission for flows it is confident it can
  // fulfill.
  using IsVerifiableCallback =
      base::OnceCallback<void(std::optional<Result> result)>;
  virtual void CheckIfVerifiable(const std::string& email,
                                 IsVerifiableCallback callback) = 0;

  // Phase 2: Post-Prompt Execution
  // Generates and fetches the signed cryptographic verification token.
  // This MUST only be called after the user has explicitly confirmed the
  // permission prompt.
  //
  // To avoid duplicate network latency, this requires the `Result` object
  // returned by `CheckIfVerifiable()`, allowing this step to reuse the
  // discovered metadata endpoints instead of re-fetching them.
  virtual void Verify(const Result& result,
                      const std::string& nonce,
                      OnEmailVerifiedCallback callback) = 0;

  // Returns the EmailVerifier associated with the given RenderFrameHost, or
  // creates one if none exists yet.
  // The RenderFrameHost must outlive it.
  CONTENT_EXPORT static EmailVerifier* GetOrCreateForFrame(
      content::RenderFrameHost* render_frame_host);

  // Enforces `GetOrCreateForFrame(render_frame_host) == verifier.get()`.
  CONTENT_EXPORT static void SetForFrameForTest(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<EmailVerifier> verifier);
};

}  // namespace content::webid

#endif  // CONTENT_PUBLIC_BROWSER_WEBID_EMAIL_VERIFIER_H_
