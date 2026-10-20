// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/single_sign_on_key_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/unexportable_key.h"
#include "net/device_bound_sessions/cookie_access_check_params.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/device_bound_sessions/session_error.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

bool CanAccessPreProvisionedKey(
    const SessionService::CookieAccessCallback& cookie_access_cb,
    const url::Origin& provider_origin,
    const url::Origin& rp_origin) {
  return cookie_access_cb &&
         cookie_access_cb.Run({.provider_origin{provider_origin},
                               .relying_party_origin{rp_origin}});
}

// Deletes `key_id` in the background, e.g. after a generated key turned out
// not to be needed.
void DeleteSigningKey(unexportable_keys::UnexportableKeyService& key_service,
                      unexportable_keys::UnexportableSigningKeyId key_id) {
  key_service.DeleteKeysSlowlyAsync(
      {key_id}, unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::DoNothing());
}

}  // namespace

// An RP key that was generated and certified, but not stored yet.
struct SingleSignOnKeyManager::GeneratedKey {
  unexportable_keys::UnexportableSigningKeyId id;
  std::string thumbprint;
  std::string binding_statement;
};

// Generates an RP signing key and certifies it with the IdP's AIK. The manager
// owns the request and decides whether to store the key.
class SingleSignOnKeyManager::GenerateKeyRequest {
 public:
  // Receives the generated key or an error. Always runs in a posted task, and
  // only while the request is alive. May destroy the request.
  using DoneCallback = base::OnceCallback<void(SessionErrorOr<GeneratedKey>)>;

  explicit GenerateKeyRequest(
      unexportable_keys::UnexportableKeyService& key_service,
      unexportable_keys::UnexportableAttestationKeyId idp_aik,
      GenerateKeyParam param,
      url::Origin provider_origin)
      : key_service_(key_service),
        idp_aik_(idp_aik),
        param_(std::move(param)),
        provider_origin_(std::move(provider_origin)),
        provider_site_(provider_origin_) {}

  GenerateKeyRequest(const GenerateKeyRequest&) = delete;
  GenerateKeyRequest& operator=(const GenerateKeyRequest&) = delete;

  // Deletes the RP key unless it was handed to the `DoneCallback`. This also
  // covers the manager being destroyed while certify or the posted result is
  // pending. A key that is only generated after that is deleted in
  // `OnGenerateSigningKeyDoneOrDelete()`.
  ~GenerateKeyRequest() {
    if (rp_key_id_) {
      DeleteSigningKey(*key_service_, *rp_key_id_);
    }
  }

  const url::Origin& provider_origin() const { return provider_origin_; }
  const net::SchemefulSite& provider_site() const { return provider_site_; }
  const url::Origin& rp_origin() const { return param_.target_origin; }
  bool site_data_cleared() const { return site_data_cleared_; }

  // Records that the RP's site data was cleared while the request was running,
  // so its key must not be stored.
  void MarkSiteDataCleared() { site_data_cleared_ = true; }

  void Start(DoneCallback on_done) {
    // A static function rather than a weak method, so it still runs, and
    // deletes the key, if the request is gone by the time the key exists.
    key_service_->GenerateSigningKeySlowlyAsync(
        param_.supported_algos,
        unexportable_keys::BackgroundTaskPriority::kUserBlocking,
        base::BindOnce(&GenerateKeyRequest::OnGenerateSigningKeyDoneOrDelete,
                       weak_factory_.GetWeakPtr(),
                       base::UnsafeDangling(&key_service_.get()),
                       std::move(on_done)));
  }

 private:
  // Passes `result` on to `request`, or deletes the generated key if `request`
  // was destroyed while keygen was pending.
  static void OnGenerateSigningKeyDoneOrDelete(
      base::WeakPtr<GenerateKeyRequest> request,
      MayBeDangling<unexportable_keys::UnexportableKeyService> key_service,
      DoneCallback on_done,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> result) {
    if (request) {
      request->OnGenerateSigningKeyDone(std::move(on_done), std::move(result));
      return;
    }
    // The key service also runs callbacks with an error after it was
    // destroyed. Only a success result means that `key_service` is alive.
    if (result.has_value()) {
      DeleteSigningKey(*key_service, *result);
    }
  }

  void OnGenerateSigningKeyDone(
      DoneCallback on_done,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> result) {
    ASSIGN_OR_RETURN(rp_key_id_, std::move(result), [&](auto) {
      Finish(std::move(on_done),
             base::unexpected(SessionError::kSigningKeyGenerationError));
    });

    // Skip the slow certify call if its result would be thrown away anyway.
    if (site_data_cleared_) {
      Finish(std::move(on_done),
             base::unexpected(SessionError::kSigningKeyGenerationError));
      return;
    }

    // Computing the thumbprint only needs the RP key, so a failure here also
    // skips the slow certify call.
    ASSIGN_OR_RETURN(
        thumbprint_, GetJwkThumbprint(*key_service_, *rp_key_id_), [&] {
          Finish(std::move(on_done),
                 base::unexpected(SessionError::kSigningKeyGenerationError));
        });

    key_service_->CertifySlowlyAsync(
        idp_aik_, *rp_key_id_, base::as_byte_span(param_.challenge),
        unexportable_keys::BackgroundTaskPriority::kUserBlocking,
        base::BindOnce(&GenerateKeyRequest::OnCertifySlowlyDone,
                       weak_factory_.GetWeakPtr(), std::move(on_done)));
  }

  void OnCertifySlowlyDone(
      DoneCallback on_done,
      unexportable_keys::ServiceErrorOr<crypto::AttestationStatement> result) {
    Finish(
        std::move(on_done),
        base::OptionalToExpected(base::OptionalFromExpected(std::move(result))
                                     .and_then(SerializeBindingStatement),
                                 SessionError::kAttestationCertificationError));
  }

  // Runs `on_done` from a posted task rather than right away. The key service
  // may reply inline (e.g. `CertifySlowlyAsync()` fails synchronously for an
  // unknown AIK), and posting guarantees that the caller's callback never runs
  // before `GenerateKey()` returns.
  void Finish(DoneCallback on_done,
              SessionErrorOr<std::string> binding_statement) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&GenerateKeyRequest::RunDoneCallback,
                       weak_factory_.GetWeakPtr(), std::move(on_done),
                       std::move(binding_statement)));
  }

  void RunDoneCallback(DoneCallback on_done,
                       SessionErrorOr<std::string> binding_statement) {
    // Hand the key over only now, so the destructor deletes it if `this` was
    // destroyed while this task was pending.
    SessionErrorOr<GeneratedKey> key =
        std::move(binding_statement).transform([&](std::string statement) {
          return GeneratedKey{.id = *std::exchange(rp_key_id_, std::nullopt),
                              .thumbprint = std::move(thumbprint_),
                              .binding_statement = std::move(statement)};
        });
    // May destroy `this`.
    std::move(on_done).Run(std::move(key));
  }

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  const unexportable_keys::UnexportableAttestationKeyId idp_aik_;
  GenerateKeyParam param_;
  url::Origin provider_origin_;
  net::SchemefulSite provider_site_;
  std::optional<unexportable_keys::UnexportableSigningKeyId> rp_key_id_;
  // JWK thumbprint of `rp_key_id_`, set right after keygen.
  std::string thumbprint_;
  bool site_data_cleared_ = false;
  base::WeakPtrFactory<GenerateKeyRequest> weak_factory_{this};
};

SingleSignOnKeyManager::SingleSignOnKeyManager(
    unexportable_keys::UnexportableKeyService& key_service,
    SessionService::CookieAccessCallback has_cookie_access_cb)
    : key_service_(key_service),
      has_cookie_access_cb_(std::move(has_cookie_access_cb)) {}

// TODO(crbug.com/573951667): Delete the stored keys from `key_service_`.
SingleSignOnKeyManager::~SingleSignOnKeyManager() = default;

bool SingleSignOnKeyManager::CanGenerateKey(
    const url::Origin& provider_origin,
    const url::Origin& rp_origin) const {
  return CanAccessPreProvisionedKey(has_cookie_access_cb_, provider_origin,
                                    rp_origin) &&
         CountKeys(net::SchemefulSite(provider_origin),
                   InFlightRequests::kCount) <
             kMaxPreProvisionedKeysPerIdentityProvider;
}

void SingleSignOnKeyManager::GenerateKey(
    unexportable_keys::UnexportableAttestationKeyId idp_aik,
    GenerateKeyParam param,
    url::Origin provider_origin,
    SessionService::KeyBindingCallback callback) {
  CHECK(!callback.is_null());
  // Checks the count part of `CanGenerateKey()`. The cookie part depends on
  // `has_cookie_access_cb_`, so it is only checked again when storing the key.
  CHECK_LT(
      CountKeys(net::SchemefulSite(provider_origin), InFlightRequests::kCount),
      kMaxPreProvisionedKeysPerIdentityProvider);
  GenerateKeyRequest* request =
      generate_key_requests_
          .insert(std::make_unique<GenerateKeyRequest>(
              *key_service_, idp_aik, std::move(param),
              std::move(provider_origin)))
          .first->get();
  // `request` posts its result, so `callback` never runs before
  // `GenerateKey()` returns. `base::Unretained` is safe because `this` owns
  // `request`, which only runs the callback while it is alive.
  request->Start(base::BindOnce(&SingleSignOnKeyManager::OnGenerateKeyDone,
                                base::Unretained(this), request,
                                std::move(callback)));
}

bool SingleSignOnKeyManager::AddPreProvisionedKey(
    const url::Origin& rp_origin,
    std::string_view provider_key,
    const url::Origin& provider_origin,
    unexportable_keys::UnexportableSigningKeyId key_id) {
  // An opaque origin is its own `SchemefulSite`, so it would escape the per-IdP
  // key limit.
  CHECK(!provider_origin.opaque());
  if (!CanAddPreProvisionedKey(provider_origin, rp_origin)) {
    return false;
  }

  // `FindPreProvisionedKey()` looks keys up by RP origin and digest only, so
  // these must be unique across IdPs.
  if (std::ranges::any_of(pre_provisioned_keys_, [&](const auto& pk) {
        return pk.rp_origin == rp_origin && pk.provider_key == provider_key;
      })) {
    return false;
  }

  pre_provisioned_keys_.push_back(
      {.provider_origin{provider_origin},
       .provider_site{SchemefulSite(provider_origin)},
       .rp_origin{rp_origin},
       .provider_key{std::string(provider_key)},
       .key_id{key_id}});
  return true;
}

SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
SingleSignOnKeyManager::FindPreProvisionedKey(
    std::string_view provider_key,
    base::optional_ref<const url::Origin> initiator) const {
  if (!initiator) {
    return base::unexpected(
        SessionError::kInvalidPreProvisionedKeyInitiatorMissing);
  }

  // Keys are looked up by (initiator, digest), as in WICG/dbsc-sso#18. The IdP
  // that generated the key is only used for the cookie access check below,
  // which goes beyond the explainer.
  auto key_it =
      std::ranges::find_if(pre_provisioned_keys_, [&](const auto& pk) {
        return pk.rp_origin == *initiator && pk.provider_key == provider_key;
      });
  if (key_it == pre_provisioned_keys_.end()) {
    return base::unexpected(SessionError::kPreProvisionedKeyNotFound);
  }

  if (!CanAccessPreProvisionedKey(has_cookie_access_cb_,
                                  key_it->provider_origin, *initiator)) {
    return base::unexpected(SessionError::kPreProvisionedKeyAccessNotGranted);
  }

  return key_it->key_id;
}

void SingleSignOnKeyManager::ConsumeKeyForSession(const Session& session) {
  if (session.unexportable_key_id().has_value()) {
    std::erase_if(pre_provisioned_keys_, [&](const PreProvisionedKeyEntry& pk) {
      return pk.key_id == session.unexportable_key_id();
    });
  }
}

void SingleSignOnKeyManager::ClearPreProvisionedKeys(
    const SessionService::OriginAndSiteMatcher& matcher) {
  // We only delete a pre-provisioned key if the origin and site matches the
  // Relying Party's origin and site because the key is considered RP's data,
  // not IdP's.
  auto matches_rp = [&](const url::Origin& rp_origin) {
    return !matcher || matcher.Run(rp_origin, net::SchemefulSite(rp_origin));
  };

  // TODO(crbug.com/573951667): Also delete the erased keys from `key_service_`.
  std::erase_if(pre_provisioned_keys_, [&](const PreProvisionedKeyEntry& key) {
    return matches_rp(key.rp_origin);
  });

  // In-flight requests would otherwise store their key after the clear. They
  // fail and delete their key instead. Their callbacks still run
  // asynchronously, so nothing re-enters the caller.
  for (const auto& request : generate_key_requests_) {
    if (matches_rp(request->rp_origin())) {
      request->MarkSiteDataCleared();
    }
  }
}

bool SingleSignOnKeyManager::CanAddPreProvisionedKey(
    const url::Origin& provider_origin,
    const url::Origin& rp_origin) const {
  // Called while the finishing request is still in `generate_key_requests_`,
  // so counting in-flight requests would count it against its own key. The
  // slots of in-flight requests were already reserved by `CanGenerateKey()`.
  return CanAccessPreProvisionedKey(has_cookie_access_cb_, provider_origin,
                                    rp_origin) &&
         CountKeys(SchemefulSite(provider_origin), InFlightRequests::kIgnore) <
             kMaxPreProvisionedKeysPerIdentityProvider;
}

size_t SingleSignOnKeyManager::CountKeys(
    const net::SchemefulSite& provider_site,
    InFlightRequests in_flight_requests) const {
  size_t count = static_cast<size_t>(
      std::ranges::count(pre_provisioned_keys_, provider_site,
                         &PreProvisionedKeyEntry::provider_site));
  if (in_flight_requests == InFlightRequests::kCount) {
    count += static_cast<size_t>(
        std::ranges::count(generate_key_requests_, provider_site,
                           &GenerateKeyRequest::provider_site));
  }
  return count;
}

void SingleSignOnKeyManager::OnGenerateKeyDone(
    GenerateKeyRequest* request,
    SessionService::KeyBindingCallback callback,
    SessionErrorOr<GeneratedKey> result) {
  SessionErrorOr<std::string> binding_statement = std::move(result).and_then(
      [&](GeneratedKey key) -> SessionErrorOr<std::string> {
        if (request->site_data_cleared() ||
            !AddPreProvisionedKey(request->rp_origin(), key.thumbprint,
                                  request->provider_origin(), key.id)) {
          DeleteSigningKey(*key_service_, key.id);
          // TODO(crbug.com/575710508): Report a cleared site and a rejected
          // store with their own errors, e.g. `kPreProvisionedKeyCleared` and
          // `kPreProvisionedKeyStoreRejected`.
          return base::unexpected(SessionError::kSigningKeyGenerationError);
        }
        return std::move(key.binding_statement);
      });

  // Destroys `request`.
  CHECK_EQ(generate_key_requests_.erase(request), 1u);
  // Runs last: `callback` may re-enter `this` (e.g. start another request) or
  // destroy it.
  std::move(callback).Run(std::move(binding_statement));
}

}  // namespace net::device_bound_sessions
