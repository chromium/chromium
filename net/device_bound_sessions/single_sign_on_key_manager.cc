// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/single_sign_on_key_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/unexportable_key.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/cookie_access_check_params.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/device_bound_sessions/session_error.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
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

// Deletes `key_ids` from `key_service` in the background, if non-empty.
void DeleteSigningKeys(
    unexportable_keys::UnexportableKeyService& key_service,
    base::span<const unexportable_keys::UnexportableSigningKeyId> key_ids) {
  if (key_ids.empty()) {
    return;
  }
  key_service.DeleteKeysSlowlyAsync(
      key_ids, unexportable_keys::BackgroundTaskPriority::kBestEffort,
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
// owns the request and decides whether to store the key. The request owns the
// caller's callback and stops early once that callback is cancelled.
class SingleSignOnKeyManager::GenerateKeyRequest {
 public:
  // Receives the generated key or an error. Always runs in a posted task, and
  // only while the request is alive. May destroy the request.
  using DoneCallback = base::OnceCallback<void(SessionErrorOr<GeneratedKey>)>;

  explicit GenerateKeyRequest(
      unexportable_keys::UnexportableKeyService& key_service,
      GenerateKeyParam param,
      url::Origin provider_origin,
      SessionService::KeyBindingCallback callback)
      : key_service_(key_service),
        param_(std::move(param)),
        provider_origin_(std::move(provider_origin)),
        callback_(std::move(callback)) {}

  GenerateKeyRequest(const GenerateKeyRequest&) = delete;
  GenerateKeyRequest& operator=(const GenerateKeyRequest&) = delete;

  // Deletes the RP key unless it was handed to the `DoneCallback`. This also
  // covers the manager being destroyed while certify or the posted result is
  // pending. A key that arrives after `Finish()` or after `this` is destroyed
  // is deleted in `OnGenerateSigningKeyDone()` or
  // `OnGenerateSigningKeyDoneOrDelete()`.
  ~GenerateKeyRequest() {
    if (rp_key_id_) {
      DeleteSigningKeys(*key_service_, {*rp_key_id_});
    }
  }

  const url::Origin& provider_origin() const { return provider_origin_; }
  const url::Origin& rp_origin() const { return param_.target_origin; }

  // Records that the RP's site data was cleared while the request was running,
  // so its key must not be stored, and fails the request now (asynchronously
  // via `Finish()`). Keeps `site_data_cleared_` set in case a success result is
  // already posted.
  void OnSiteDataCleared() {
    site_data_cleared_ = true;
    Finish(base::unexpected(SessionError::kSigningKeyGenerationError));
  }

  // Returns true once the key must not be stored, because the caller cancelled
  // its callback or the RP's site data was cleared.
  bool ShouldStop() const {
    return callback_.IsCancelled() || site_data_cleared_;
  }

  // Hands the caller's callback to the manager. Afterwards `ShouldStop()` must
  // not be called.
  SessionService::KeyBindingCallback TakeCallback() {
    return std::move(callback_);
  }

  AttestationKeyCallback Start(DoneCallback on_done) {
    on_done_ = std::move(on_done);
    // A static function rather than a weak method, so it still runs, and
    // deletes the key, if the request is gone by the time the key exists.
    key_service_->GenerateSigningKeySlowlyAsync(
        param_.supported_algos,
        unexportable_keys::BackgroundTaskPriority::kUserBlocking,
        base::BindOnce(&GenerateKeyRequest::OnGenerateSigningKeyDoneOrDelete,
                       weak_factory_.GetWeakPtr(),
                       base::UnsafeDangling(&key_service_.get())));
    // Fail the request if the caller destroys the returned callback without
    // running it. `Finish()` posts, so destruction never re-enters.
    base::ScopedClosureRunner on_dropped(base::BindOnce(
        &GenerateKeyRequest::OnAttestationKey, weak_factory_.GetWeakPtr(),
        base::unexpected(SessionError::kSigningKeyGenerationError)));
    return base::BindOnce(
        [](base::WeakPtr<GenerateKeyRequest> request,
           base::ScopedClosureRunner on_dropped,
           SessionErrorOr<unexportable_keys::UnexportableAttestationKeyId>
               result) {
          on_dropped.ReplaceClosure(base::NullCallback());
          if (request) {
            request->OnAttestationKey(std::move(result));
          }
        },
        weak_factory_.GetWeakPtr(), std::move(on_dropped));
  }

 private:
  // Passes `result` on to `request`, or deletes the generated key if `request`
  // was destroyed while keygen was pending.
  static void OnGenerateSigningKeyDoneOrDelete(
      base::WeakPtr<GenerateKeyRequest> request,
      MayBeDangling<unexportable_keys::UnexportableKeyService> key_service,
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> result) {
    if (request) {
      request->OnGenerateSigningKeyDone(std::move(result));
      return;
    }
    // The key service also runs callbacks with an error after it was
    // destroyed. Only a success result means that `key_service` is alive.
    if (result.has_value()) {
      DeleteSigningKeys(*key_service, {*result});
    }
  }

  void OnGenerateSigningKeyDone(
      unexportable_keys::ServiceErrorOr<
          unexportable_keys::UnexportableSigningKeyId> result) {
    if (!on_done_) {
      if (result) {
        DeleteSigningKeys(*key_service_, {*result});
      }
      return;
    }
    ASSIGN_OR_RETURN(rp_key_id_, std::move(result), [&](auto) {
      Finish(base::unexpected(SessionError::kSigningKeyGenerationError));
    });
    // Skip the slow certify call if its result would be thrown away anyway.
    if (ShouldStop()) {
      Finish(base::unexpected(SessionError::kSigningKeyGenerationError));
      return;
    }

    // Computing the thumbprint only needs the RP key, so a failure here also
    // skips the slow certify call.
    ASSIGN_OR_RETURN(
        thumbprint_, GetJwkThumbprint(*key_service_, *rp_key_id_), [&] {
          Finish(base::unexpected(SessionError::kSigningKeyGenerationError));
        });

    MaybeStartCertify();
  }

  void OnAttestationKey(
      SessionErrorOr<unexportable_keys::UnexportableAttestationKeyId> result) {
    if (!on_done_) {
      return;
    }
    ASSIGN_OR_RETURN(idp_aik_, std::move(result),
                     [&](SessionError::ErrorType error) {
                       Finish(base::unexpected(error));
                     });
    MaybeStartCertify();
  }

  void MaybeStartCertify() {
    if (!rp_key_id_ || !idp_aik_) {
      return;
    }
    // Skip the slow certify call if its result would be thrown away anyway.
    if (ShouldStop()) {
      Finish(base::unexpected(SessionError::kSigningKeyGenerationError));
      return;
    }
    key_service_->CertifySlowlyAsync(
        *idp_aik_, *rp_key_id_, base::as_byte_span(param_.challenge),
        unexportable_keys::BackgroundTaskPriority::kUserBlocking,
        base::BindOnce(&GenerateKeyRequest::OnCertifySlowlyDone,
                       weak_factory_.GetWeakPtr()));
  }

  void OnCertifySlowlyDone(
      unexportable_keys::ServiceErrorOr<crypto::AttestationStatement> result) {
    Finish(
        base::OptionalToExpected(base::OptionalFromExpected(std::move(result))
                                     .and_then(SerializeBindingStatement),
                                 SessionError::kAttestationCertificationError));
  }

  // Runs `on_done_` from a posted task rather than right away. The key service
  // or the AIK callback may reply inline (e.g. `CertifySlowlyAsync()` fails
  // synchronously for an unknown AIK), and posting guarantees that the
  // caller's callback never runs before `GenerateKey()` returns or
  // synchronously from the AIK callback.
  void Finish(SessionErrorOr<std::string> binding_statement) {
    if (!on_done_) {
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&GenerateKeyRequest::RunDoneCallback,
                       weak_factory_.GetWeakPtr(), std::move(on_done_),
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
  GenerateKeyParam param_;
  url::Origin provider_origin_;
  // The caller's callback. Destroying the request drops it.
  SessionService::KeyBindingCallback callback_;
  // Non-null until `Finish()` is called.
  DoneCallback on_done_;
  std::optional<unexportable_keys::UnexportableAttestationKeyId> idp_aik_;
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

// Deleting unclaimed keys here is best-effort. It covers the network context
// being torn down while the key service is still alive. If the key service
// shuts down first, or browser shutdown skips the task
// (`UnexportableKeyTaskManager` uses `SKIP_ON_SHUTDOWN`), the keys are left to
// key garbage collection.
SingleSignOnKeyManager::~SingleSignOnKeyManager() {
  DeleteSigningKeys(*key_service_, base::ToVector(pre_provisioned_keys_,
                                                  [](const auto& entry) {
                                                    return entry.second.key_id;
                                                  }));
}

bool SingleSignOnKeyManager::CanGenerateKey(
    const url::Origin& provider_origin,
    const url::Origin& rp_origin) const {
  return CanAccessPreProvisionedKey(has_cookie_access_cb_, provider_origin,
                                    rp_origin);
}

SingleSignOnKeyManager::AttestationKeyCallback
SingleSignOnKeyManager::GenerateKey(
    GenerateKeyParam param,
    url::Origin provider_origin,
    SessionService::KeyBindingCallback callback) {
  CHECK(!callback.is_null());
  GenerateKeyRequest* request =
      generate_key_requests_
          .insert(std::make_unique<GenerateKeyRequest>(
              *key_service_, std::move(param), std::move(provider_origin),
              std::move(callback)))
          .first->get();
  // `request` posts its result, so the caller's callback never runs before
  // `GenerateKey()` returns. `base::Unretained` is safe because `this` owns
  // `request`, which only calls `OnGenerateKeyDone()` while it is alive.
  return request->Start(
      base::BindOnce(&SingleSignOnKeyManager::OnGenerateKeyDone,
                     base::Unretained(this), request));
}

bool SingleSignOnKeyManager::AddPreProvisionedKey(
    const url::Origin& rp_origin,
    std::string_view provider_key,
    const url::Origin& provider_origin,
    unexportable_keys::UnexportableSigningKeyId key_id) {
  if (!CanAccessPreProvisionedKey(has_cookie_access_cb_, provider_origin,
                                  rp_origin)) {
    return false;
  }

  // `FindPreProvisionedKey()` looks keys up by RP origin and digest only, so
  // these must be unique across IdPs.
  auto [_, inserted] = pre_provisioned_keys_.try_emplace(
      RpAndProviderKeyView(rp_origin, provider_key), provider_origin, key_id);
  if (!inserted) {
    return false;
  }

  base::UmaHistogramCounts1000("Net.DeviceBoundSessions.PreProvisionedKeyCount",
                               pre_provisioned_keys_.size());
  return true;
}

SessionErrorOr<unexportable_keys::UnexportableSigningKeyId>
SingleSignOnKeyManager::FindPreProvisionedKey(
    std::string_view provider_key,
    const url::Origin& rp_origin) const {
  // Keys are looked up by (RP origin, digest). The IdP that generated the key
  // is only used for the cookie access check below, which goes beyond the
  // explainer.
  const KeyMetadata* metadata = base::FindOrNull(
      pre_provisioned_keys_, RpAndProviderKeyView(rp_origin, provider_key));
  if (!metadata) {
    return base::unexpected(SessionError::kPreProvisionedKeyNotFound);
  }

  if (!CanAccessPreProvisionedKey(has_cookie_access_cb_,
                                  metadata->provider_origin, rp_origin)) {
    return base::unexpected(SessionError::kPreProvisionedKeyAccessNotGranted);
  }

  return metadata->key_id;
}

bool SingleSignOnKeyManager::ConsumePreProvisionedKey(
    unexportable_keys::UnexportableSigningKeyId key_id) {
  return absl::erase_if(pre_provisioned_keys_, [&](const auto& entry) {
           return entry.second.key_id == key_id;
         }) > 0;
}

void SingleSignOnKeyManager::ClearPreProvisionedKeys(
    const SessionService::OriginAndSiteMatcher& matcher) {
  // We only delete a pre-provisioned key if the origin and site matches the
  // Relying Party's origin and site because the key is considered RP's data,
  // not IdP's.
  auto matches_rp = [&](const url::Origin& rp_origin) {
    return !matcher || matcher.Run(rp_origin, net::SchemefulSite(rp_origin));
  };

  std::vector<unexportable_keys::UnexportableSigningKeyId> key_ids;
  absl::erase_if(pre_provisioned_keys_, [&](const auto& entry) {
    const url::Origin& rp_origin = entry.first.first;
    if (matches_rp(rp_origin)) {
      key_ids.push_back(entry.second.key_id);
      return true;
    }
    return false;
  });
  DeleteSigningKeys(*key_service_, key_ids);

  // In-flight requests would otherwise store their key after the clear. They
  // fail and delete their key instead. Their callbacks still run
  // asynchronously, so nothing re-enters the caller.
  for (const auto& request : generate_key_requests_) {
    if (matches_rp(request->rp_origin())) {
      request->OnSiteDataCleared();
    }
  }
}

void SingleSignOnKeyManager::CompleteRequest(
    GenerateKeyRequest* request,
    SessionErrorOr<std::string> result) {
  SessionService::KeyBindingCallback callback = request->TakeCallback();
  // Destroys `request`.
  CHECK_EQ(generate_key_requests_.erase(request), 1u);
  // Runs last: `callback` may re-enter `this` (e.g. start another request) or
  // destroy it. Running a cancelled callback does nothing.
  std::move(callback).Run(std::move(result));
}

void SingleSignOnKeyManager::OnGenerateKeyDone(
    GenerateKeyRequest* request,
    SessionErrorOr<GeneratedKey> result) {
  SessionErrorOr<std::string> binding_statement = std::move(result).and_then(
      [&](GeneratedKey key) -> SessionErrorOr<std::string> {
        if (request->ShouldStop() ||
            !AddPreProvisionedKey(request->rp_origin(), key.thumbprint,
                                  request->provider_origin(), key.id)) {
          DeleteSigningKeys(*key_service_, {key.id});
          // TODO(crbug.com/575710508): Report a cleared site and a rejected
          // store with their own errors, e.g. `kPreProvisionedKeyCleared` and
          // `kPreProvisionedKeyStoreRejected`.
          return base::unexpected(SessionError::kSigningKeyGenerationError);
        }
        return std::move(key.binding_statement);
      });

  CompleteRequest(request, std::move(binding_statement));
}

}  // namespace net::device_bound_sessions
