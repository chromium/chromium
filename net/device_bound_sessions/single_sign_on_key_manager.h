// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_
#define NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/transparent_hash.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/generate_key_param.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
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
  // `key_service` must outlive `this`.
  explicit SingleSignOnKeyManager(
      unexportable_keys::UnexportableKeyService& key_service,
      SessionService::CookieAccessCallback has_cookie_access_cb);
  // Deletes the pre-provisioned keys that are still stored, i.e. were never
  // consumed, from the key service.
  ~SingleSignOnKeyManager();

  SingleSignOnKeyManager(const SingleSignOnKeyManager&) = delete;
  SingleSignOnKeyManager& operator=(const SingleSignOnKeyManager&) = delete;
  SingleSignOnKeyManager(SingleSignOnKeyManager&&) = delete;
  SingleSignOnKeyManager& operator=(SingleSignOnKeyManager&&) = delete;

  // Returns whether a Secure-Session-GenerateKey request may start. The IdP at
  // `provider_origin` needs access to its cookies on `rp_origin`.
  bool CanGenerateKey(const url::Origin& provider_origin,
                      const url::Origin& rp_origin) const;

  // Generates an RP signing key, certifies it with the IdP's AIK `idp_aik` and
  // stores it as a pre-provisioned key. Call only after `CanGenerateKey()`
  // returned true. `callback` receives the serialized binding statement or an
  // error, and never runs before `GenerateKey()` returns. If `this` is
  // destroyed before the request finishes, `callback` is dropped. If
  // `callback` is cancelled, the request stops early and deletes its key.
  // Cancellation is detected via `callback.IsCancelled()`, so bind `callback`
  // to a `WeakPtr` and don't wrap it in another callback.
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
  // - the key already exists (same `rp_origin` and `provider_key`).
  bool AddPreProvisionedKey(const url::Origin& rp_origin,
                            std::string_view provider_key,
                            const url::Origin& provider_origin,
                            unexportable_keys::UnexportableSigningKeyId key_id);

  // Finds the pre-provisioned key named `provider_key` that was generated for
  // `initiator`. The cookie access check uses the IdP that generated the key.
  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
  FindPreProvisionedKey(std::string_view provider_key,
                        base::optional_ref<const url::Origin> initiator) const;

  // Removes the pre-provisioned key with `key_id`. Called when an SSO
  // registration completes, before the new session is added. Returns false if
  // the key is no longer stored, e.g. because the RP's site data was cleared
  // or another registration consumed it while this one was in flight. Unlike
  // `ClearPreProvisionedKeys()` and the destructor, this does not delete the
  // key from the key service, because the new session takes ownership of it.
  [[nodiscard]] bool ConsumePreProvisionedKey(
      unexportable_keys::UnexportableSigningKeyId key_id);

  // Clears pre-provisioned keys whose RP origin matches `matcher`, whichever
  // IdP generated them: keys are the RP's data. If `matcher` is null, clears
  // all keys. Cleared keys are also deleted from the key service.
  void ClearPreProvisionedKeys(
      const SessionService::OriginAndSiteMatcher& matcher);

 private:
  class GenerateKeyRequest;
  struct GeneratedKey;

  struct KeyMetadata {
    url::Origin provider_origin;
    unexportable_keys::UnexportableSigningKeyId key_id;
  };
  // An RP origin and the JWK thumbprint of a provider key.
  using RpAndProviderKey = std::pair<url::Origin, std::string>;
  // Non-owning `RpAndProviderKey` for lookups and `try_emplace()`, so the
  // origin and thumbprint are only copied when a key is added. Holds a
  // reference, so only build it from an origin that outlives the expression.
  using RpAndProviderKeyView = std::pair<const url::Origin&, std::string_view>;

  // Destroys `request` and passes `result` to its callback. The callback may
  // destroy `this`, so callers must return right after.
  void CompleteRequest(GenerateKeyRequest* request,
                       SessionErrorOr<std::string> result);

  // Called once `request` has finished. Stores the generated key unless the
  // request's callback was cancelled, `request`'s site data was cleared or
  // `AddPreProvisionedKey()` rejects it. Destroys `request` and passes the
  // binding statement or an error to the request's callback.
  void OnGenerateKeyDone(GenerateKeyRequest* request,
                         SessionErrorOr<GeneratedKey> result);

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  SessionService::CookieAccessCallback has_cookie_access_cb_;
  // Pre-provisioned keys by RP origin and the provider key's JWK thumbprint.
  absl::flat_hash_map<RpAndProviderKey,
                      KeyMetadata,
                      base::TransparentHashAs<RpAndProviderKeyView>,
                      base::TransparentEqualAs<RpAndProviderKeyView>>
      pre_provisioned_keys_;
  // In-flight `GenerateKey()` requests. Destroying `this` cancels them.
  absl::flat_hash_set<std::unique_ptr<GenerateKeyRequest>>
      generate_key_requests_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SINGLE_SIGN_ON_KEY_MANAGER_H_
