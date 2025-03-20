// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FEDERATED_AUTH_AUTOFILL_SOURCE_H_
#define CONTENT_PUBLIC_BROWSER_FEDERATED_AUTH_AUTOFILL_SOURCE_H_

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
  FederatedAuthAutofillSource() = default;
  virtual ~FederatedAuthAutofillSource() = default;

  virtual const std::optional<std::vector<IdentityRequestAccountPtr>>
  GetAutofillSuggestions() const = 0;
  virtual void NotifyAutofillSuggestionAccepted(
      const GURL& idp,
      const std::string& account_id) = 0;

  // Returns the a data source for autofill if there is a pending conditional
  // FedCM requests. Returns null otherwise.
  CONTENT_EXPORT static FederatedAuthAutofillSource* FromPage(
      content::Page& page);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FEDERATED_AUTH_AUTOFILL_SOURCE_H_
