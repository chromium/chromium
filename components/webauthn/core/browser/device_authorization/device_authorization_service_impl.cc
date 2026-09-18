// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/device_authorization/device_authorization_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_features.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace webauthn {

DeviceAuthorizationServiceImpl::DeviceAuthorizationServiceImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DeviceAuthorizationClient> client,
    version_info::Channel channel)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      client_(std::move(client)),
      fetcher_(std::make_unique<DeviceAuthorizationKeysFetcher>(channel)) {
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);
  CHECK(client_);
  CHECK(fetcher_);
}

DeviceAuthorizationServiceImpl::~DeviceAuthorizationServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DeviceAuthorizationServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_fetching_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (pending_callback_) {
    std::move(pending_callback_).Run(DeviceAuthFetchResult{});
  }
  fetcher_.reset();
  client_.reset();
  identity_manager_ = nullptr;
  url_loader_factory_.reset();
}

void DeviceAuthorizationServiceImpl::GetOrFetchKeys(
    FetchDeviceAuthKeysCallback callback) {
  FetchKeysImpl(/*reauth_proof_token=*/std::nullopt, std::move(callback));
}

void DeviceAuthorizationServiceImpl::FetchKeysWithReAuthToken(
    std::string reauth_proof_token,
    FetchDeviceAuthKeysCallback callback) {
  CHECK(!reauth_proof_token.empty());
  FetchKeysImpl(std::move(reauth_proof_token), std::move(callback));
}

void DeviceAuthorizationServiceImpl::FetchKeysImpl(
    std::optional<std::string> reauth_proof_token,
    FetchDeviceAuthKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  // TODO(crbug.com/405036154): Consider caching multiple requests for same
  // gaia_id.
  if (is_fetching_) {
    std::move(callback).Run(DeviceAuthFetchResult{});
    return;
  }

  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(callback).Run(DeviceAuthFetchResult{});
    return;
  }

  const GaiaId gaia_id =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  if (!reauth_proof_token.has_value()) {
    client_->GetCachedKeys(
        gaia_id,
        base::BindOnce(&DeviceAuthorizationServiceImpl::OnCachedKeysFetched,
                       weak_ptr_factory_.GetWeakPtr(), gaia_id,
                       std::move(callback)));
    return;
  }

  is_fetching_ = true;
  pending_callback_ = std::move(callback);

  sync_pb::GetDeviceAuthorizationKeyRequest request;
  request.set_reauth_proof_token(*std::move(reauth_proof_token));

  client_->PopulatePlatformData(
      gaia_id, std::move(request),
      base::BindOnce(&DeviceAuthorizationServiceImpl::OnPlatformDataPopulated,
                     weak_ptr_factory_.GetWeakPtr(), gaia_id));
}

void DeviceAuthorizationServiceImpl::OnCachedKeysFetched(
    const GaiaId& gaia_id,
    FetchDeviceAuthKeysCallback callback,
    std::optional<CachedDeviceAuthorizationKeys> cached_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cached_keys.has_value() &&
      cached_keys->cache_version() ==
          features::kDeviceAuthorizationKeyCacheVersion.Get() &&
      cached_keys->keys().keys_size() > 0) {
    std::move(callback).Run(DeviceAuthFetchResult{cached_keys->keys()});
    return;
  }

  if (is_fetching_) {
    std::move(callback).Run(DeviceAuthFetchResult{});
    return;
  }

  is_fetching_ = true;
  pending_callback_ = std::move(callback);

  client_->PopulatePlatformData(
      gaia_id, sync_pb::GetDeviceAuthorizationKeyRequest{},
      base::BindOnce(&DeviceAuthorizationServiceImpl::OnPlatformDataPopulated,
                     weak_ptr_factory_.GetWeakPtr(), gaia_id));
}

void DeviceAuthorizationServiceImpl::OnPlatformDataPopulated(
    const GaiaId& gaia_id,
    sync_pb::GetDeviceAuthorizationKeyRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetcher_->FetchDeviceAuthorizationKeys(
      std::move(request), url_loader_factory_, identity_manager_,
      base::BindOnce(&DeviceAuthorizationServiceImpl::OnFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr(), gaia_id));
}

// TODO(crbug.com/405036154): Ensure account switch mid-request is handled.
void DeviceAuthorizationServiceImpl::OnFetchCompleted(
    const GaiaId& gaia_id,
    base::expected<sync_pb::GetDeviceAuthorizationKeyResponse,
                   DeviceAuthorizationKeysFetcher::Error> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_fetching_ = false;
  FetchDeviceAuthKeysCallback callback = std::move(pending_callback_);
  if (!callback) {
    return;
  }

  if (!response.has_value()) {
    std::move(callback).Run(DeviceAuthFetchResult{});
    return;
  }

  if (response->has_device_authorization_keys()) {
    // TODO(crbug.com/405036154): Check if `gaia_id` is still the identity
    // manager's current primary account gaia_id before storing keys.
    // TODO(crbug.com/405036154): Handle key validation (e.g. expected count).
    DeviceAuthorizationKeys keys = response->device_authorization_keys();
    CachedDeviceAuthorizationKeys cached_keys;
    cached_keys.set_cache_version(
        features::kDeviceAuthorizationKeyCacheVersion.Get());
    *cached_keys.mutable_keys() = keys;
    client_->StoreKeys(
        gaia_id, cached_keys,
        base::BindOnce(&DeviceAuthorizationServiceImpl::OnKeysStored,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(keys)));
    return;
  }

  if (response->has_re_auth_params()) {
    std::move(callback).Run(DeviceAuthFetchResult{response->re_auth_params()});
    return;
  }

  std::move(callback).Run(DeviceAuthFetchResult{});
}

void DeviceAuthorizationServiceImpl::OnKeysStored(
    FetchDeviceAuthKeysCallback callback,
    DeviceAuthorizationKeys keys,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    std::move(callback).Run(DeviceAuthFetchResult{});
    return;
  }

  std::move(callback).Run(DeviceAuthFetchResult{std::move(keys)});
}

}  // namespace webauthn
