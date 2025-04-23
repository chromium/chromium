// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_AUTH_AUTOFILL_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_AUTH_AUTOFILL_SOURCE_H_

#include "base/functional/callback.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/page.h"
#include "url/gurl.h"

using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;

namespace content {

// A data source for autofill, used to (a) augment it with suggestions coming
// from federated accounts and (b) handle when the suggestion gets selected.
class FederatedAuthAutofillSource {
 public:
  // For 3P logins usually the identity provider needs to issue a federated
  // token such as ID token, access token etc. to complete the flow. This
  // callback is triggered when the browser receives such a token.
  using OnFederatedTokenReceivedCallback = base::OnceClosure;

  FederatedAuthAutofillSource() = default;
  virtual ~FederatedAuthAutofillSource() = default;

  // Generates autofill suggestions from identity credential requests.
  virtual const std::optional<std::vector<IdentityRequestAccountPtr>>
  GetAutofillSuggestions() const = 0;
  // This is called when a suggestion from an identity credential conditional
  // request was accepted. e.g. a user has selected the suggestion from the
  // autofill dropdown UI. The callback will be called when a federated token is
  // received. Once it's called, the autofill dropdown will be hidden.
  virtual void NotifyAutofillSuggestionAccepted(
      const GURL& idp,
      const std::string& account_id,
      OnFederatedTokenReceivedCallback callback) = 0;

  // Returns the a data source for autofill if there is a pending conditional
  // FedCM requests. Returns null otherwise.
  CONTENT_EXPORT static FederatedAuthAutofillSource* FromPage(
      content::Page& page);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_AUTH_AUTOFILL_SOURCE_H_
