// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_
#define NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/generate_key_param.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

namespace unexportable_keys {
class UnexportableKeyService;
}  // namespace unexportable_keys

namespace net::device_bound_sessions {

// Client-side storage of Identity Provider pre-provisioned keys for DBSC-SSO,
// handed to Relying Party registrations, and generation of Relying Party keys
// on Secure-Session-GenerateKey certified with the IdP's attestation key (AIK).
class NET_EXPORT SingleSignOnKeyManager {
 public:
  // Maximum number of pre-provisioned keys per Identity Provider site.
  static constexpr size_t kMaxPreProvisionedKeysPerIdentityProvider = 10;

  // `key_service` must outlive `this`.
  explicit SingleSignOnKeyManager(
      unexportable_keys::UnexportableKeyService& key_service,
      SessionService::CookieAccessCallback has_cookie_access_cb);
  ~SingleSignOnKeyManager();

  SingleSignOnKeyManager(const SingleSignOnKeyManager&) = delete;
  SingleSignOnKeyManager& operator=(const SingleSignOnKeyManager&) = delete;
  SingleSignOnKeyManager(SingleSignOnKeyManager&&) = delete;
  SingleSignOnKeyManager& operator=(SingleSignOnKeyManager&&) = delete;

  // Returns whether a Secure-Session-GenerateKey request may start. The IdP at
  // `provider_origin` needs access to its cookies on `rp_origin`. Its stored
  // keys plus in-flight requests must stay below
  // `kMaxPreProvisionedKeysPerIdentityProvider`.
  bool CanGenerateKey(const url::Origin& provider_origin,
                      const url::Origin& rp_origin) const;

  // Generates an RP signing key, certifies it with the IdP's AIK `idp_aik` and
  // stores it as a pre-provisioned key. Call only after `CanGenerateKey()`
  // returned true. `callback` receives the serialized binding statement or an
  // error, and never runs before `GenerateKey()` returns. If `this` is
  // destroyed before the request finishes, `callback` is dropped.
  void GenerateKey(unexportable_keys::UnexportableAttestationKeyId idp_aik,
                   GenerateKeyParam param,
                   url::Origin provider_origin,
                   SessionService::KeyBindingCallback callback);

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
  class GenerateKeyRequest;
  struct GeneratedKey;

  // Whether `CountKeys()` includes in-flight `GenerateKey()` requests.
  enum class InFlightRequests { kCount, kIgnore };

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

  // Returns the number of keys stored for `provider_site`, plus its in-flight
  // `GenerateKey()` requests if `in_flight_requests` is `kCount`.
  size_t CountKeys(const net::SchemefulSite& provider_site,
                   InFlightRequests in_flight_requests) const;

  // Called once `request` has finished. Stores the generated key unless
  // `request`'s site data was cleared or `AddPreProvisionedKey()` rejects it.
  // Destroys `request` and passes the binding statement or an error to
  // `callback`.
  void OnGenerateKeyDone(GenerateKeyRequest* request,
                         SessionService::KeyBindingCallback callback,
                         SessionErrorOr<GeneratedKey> result);

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  SessionService::CookieAccessCallback has_cookie_access_cb_;
  std::vector<PreProvisionedKeyEntry> pre_provisioned_keys_;
  // In-flight `GenerateKey()` requests. They count towards the per-IdP limit
  // in `CanGenerateKey()`, and destroying `this` cancels them.
  absl::flat_hash_set<std::unique_ptr<GenerateKeyRequest>>
      generate_key_requests_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_
