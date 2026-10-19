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
  // `provider_key` must be the JWK thumbprint of the key, computed by the
  // browser, never a value supplied by the Identity Provider. Keys are unique
  // per (`rp_origin`, `provider_key`) across Identity Providers, so an
  // IdP-chosen `provider_key` would let one IdP squat another IdP's key name
  // for the same Relying Party.
  //
  // Returns true if the key was successfully added, false otherwise.
  //
  // A key insertion can fail if:
  // - the Identity Provider site does not have access to its cookies from a
  //   third-party context;
  // - the key already exists (same `rp_origin` and `provider_key`);
  // - there are already `kMaxPreProvisionedKeysPerIdentityProvider` keys
  //   stored for the Identity Provider site.
  bool AddPreProvisionedKey(const url::Origin& rp_origin,
                            std::string_view provider_key,
                            const url::Origin& provider_origin,
                            unexportable_keys::UnexportableSigningKeyId key_id);

  // Finds the pre-provisioned key named `provider_key` that was generated for
  // `initiator`. The cookie access check uses the IdP that generated the key.
  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
  FindPreProvisionedKey(std::string_view provider_key,
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
    url::Origin provider_origin;
    net::SchemefulSite provider_site;
    url::Origin rp_origin;
    std::string provider_key;
    unexportable_keys::UnexportableSigningKeyId key_id;
  };

  // Helper function to check if a pre-provisioned key created by
  // `provider_origin` can be added to the in-memory store.
  bool CanAddPreProvisionedKey(const url::Origin& provider_origin,
                               const url::Origin& rp_origin) const;

  SessionService::CookieAccessCallback has_cookie_access_cb_;
  std::vector<PreProvisionedKeyEntry> pre_provisioned_keys_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_
