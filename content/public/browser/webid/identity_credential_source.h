// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBID_IDENTITY_CREDENTIAL_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_WEBID_IDENTITY_CREDENTIAL_SOURCE_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "url/gurl.h"

namespace content {
class Page;
}

namespace url {
class Origin;
}  // namespace url

namespace content::webid {

enum class FederatedLoginResult {
  kSuccess = 0,
  kContinuation,
  kAccountNotLoggedIn,
  kAccountIsSignUp,
  kAccountNotAvailable,
  kIdpReturnedError,
  kIdpNetworkError,
  kTokenRequestAborted,
  kFrameNotActive,
  kExpectedAccountNotPresent,
  kTimeout,
};

// A data source for embedder initiated login, used to fetch accounts from
// identity providers.
class CONTENT_EXPORT IdentityCredentialSource {
 public:
  using OnFederatedTokenReceivedCallback = base::OnceCallback<void(bool)>;

  virtual ~IdentityCredentialSource() = default;

  using GetIdentityCredentialSuggestionsCallback = base::OnceCallback<void(
      const std::optional<
          std::vector<scoped_refptr<content::IdentityRequestAccount>>>&)>;
  // Generates embedder login suggestions from identity credential requests.
  virtual void GetIdentityCredentialSuggestions(
      const std::vector<GURL>& embedder_requested_idps,
      GetIdentityCredentialSuggestionsCallback callback) = 0;

  // Selects the account with the given `account_id` from `idp_origin`.
  // Returns false if such an account is not found or there is no dialog.
  virtual bool SelectAccount(const url::Origin& idp_origin,
                             const std::string& account_id) = 0;

  // Returns the a data source for embedder initiated login.
  static IdentityCredentialSource* FromPage(content::Page& page);
};

}  // namespace content::webid

#endif  // CONTENT_PUBLIC_BROWSER_WEBID_IDENTITY_CREDENTIAL_SOURCE_H_
