// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_DELEGATE_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Delegate to control FedCM's popupd. An example of a use case is if a user is
// signed-in according to the FedCM IDP Sign-in Status API but we find that the
// user has no accounts, a failure dialog is shown. The failure dialog contains
// a button which opens a popup and allows the user to complete the sign-in
// flow on the popup. This delegate controls that popup.
class IdentityRegistryDelegate {
 public:
  IdentityRegistryDelegate() = default;
  virtual ~IdentityRegistryDelegate() = default;

  // Closes the FedCM modal dialog, if any.
  virtual void OnClose() = 0;

  // The modal dialog has provided a token to resolve the original request.
  // `idp_config_url` is passed by value so that it remains valid even if the
  // implementation destructs the IdentityRegistry (e.g. by closing the
  // associated WebContents).
  // If account_id is nullopt, uses the account that was selected in the
  // account chooser.
  virtual bool OnResolve(GURL idp_config_url,
                         const std::optional<std::string>& account_id,
                         const base::Value& token) = 0;

  enum class Method { kClose, kResolve };

  // Notifies the delegate for an origin mismatch so they can output debugging
  // messages.
  virtual void OnOriginMismatch(Method method,
                                const url::Origin& expected,
                                const url::Origin& actual) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDENTITY_REGISTRY_DELEGATE_H_
