// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_
#define NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

// Client-side storage of Identity Provider pre-provisioned keys for DBSC-SSO,
// handed to Relying Party registrations.
class NET_EXPORT SingleSignOnKeyManager {
 public:
  // Maximum number of pre-provisioned keys per Identity Provider site.
  static constexpr size_t kMaxPreProvisionedKeysPerIdentityProvider = 10;

  explicit SingleSignOnKeyManager(
      SessionService::CookieAccessCallback has_cookie_access_cb);
  ~SingleSignOnKeyManager();

  SingleSignOnKeyManager(const SingleSignOnKeyManager&) = delete;
  SingleSignOnKeyManager& operator=(const SingleSignOnKeyManager&) = delete;
  SingleSignOnKeyManager(SingleSignOnKeyManager&&) = delete;
  SingleSignOnKeyManager& operator=(SingleSignOnKeyManager&&) = delete;

  // Adds a pre-provisioned key to the in-memory store.
  //
  // Returns true if the key was successfully added, false otherwise.
  //
  // A key insertion can fail if:
  // - the Identity Provider site does not have access to its cookies from a
  //   third-party context;
  // - the key already exists (same `provider_url`, `rp_origin`, and
  //   `provider_key`);
  // - there are already `kMaxPreProvisionedKeysPerIdentityProvider` keys
  //   stored for the Identity Provider site.
  bool AddPreProvisionedKey(const url::Origin& rp_origin,
                            std::string_view provider_key,
                            const GURL& provider_url,
                            unexportable_keys::UnexportableSigningKeyId key_id);

  // Finds a pre-provisioned key that matches `provider_params` and `initiator`.
  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
  FindPreProvisionedKey(const ProviderRegistrationParams& provider_params,
                        base::optional_ref<const url::Origin> initiator) const;

  // Removes the pre-provisioned key used by `session`, if any. Called when a
  // registration completes, before the new session is added.
  void ConsumeKeyForSession(const Session& session);

  // Clears pre-provisioned keys matching `matcher`. If `matcher` is null,
  // clears all keys.
  void ClearPreProvisionedKeys(
      const SessionService::OriginAndSiteMatcher& matcher);

 private:
  struct PreProvisionedKeyEntry {
    GURL provider_url;
    net::SchemefulSite provider_site;
    url::Origin rp_origin;
    std::string provider_key;
    unexportable_keys::UnexportableSigningKeyId key_id;
  };

  // Helper function to check if a pre-provisioned key created by
  // `provider_url` can be added to the in-memory store.
  bool CanAddPreProvisionedKey(const GURL& provider_url,
                               const url::Origin& rp_origin) const;

  SessionService::CookieAccessCallback has_cookie_access_cb_;
  std::vector<PreProvisionedKeyEntry> pre_provisioned_keys_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_
