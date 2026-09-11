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
    std::move(pending_callback_).Run(std::nullopt);
  }
  fetcher_.reset();
  client_.reset();
  identity_manager_ = nullptr;
  url_loader_factory_.reset();
}

void DeviceAuthorizationServiceImpl::GetOrFetchKeys(
    FetchDeviceAuthKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  // TODO(crbug.com/405036154): Consider caching multiple requests for same
  // gaia_id.
  if (is_fetching_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const GaiaId gaia_id =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  std::optional<DeviceAuthorizationKeys> cached_keys =
      client_->GetCachedKeys(gaia_id);
  // TODO(crbug.com/405036154): Implement cache version logic invalidation.
  if (cached_keys.has_value() && cached_keys->keys_size() > 0) {
    std::move(callback).Run(std::move(*cached_keys));
    return;
  }

  is_fetching_ = true;
  pending_callback_ = std::move(callback);

  client_->CreateDeviceAuthorizationRequest(
      base::BindOnce(&DeviceAuthorizationServiceImpl::OnRequestCreated,
                     weak_ptr_factory_.GetWeakPtr(), gaia_id));
}

void DeviceAuthorizationServiceImpl::OnRequestCreated(
    const GaiaId& gaia_id,
    sync_pb::GetDeviceAuthorizationKeyRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetcher_->FetchDeviceAuthorizationKeys(
      request, url_loader_factory_, identity_manager_,
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

  // TODO(crbug.com/405036154): Handle returned ReAuth params.
  if (!response.has_value() || !response->has_device_authorization_keys()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // TODO(crbug.com/405036154): Check if `gaia_id` is still the identity
  // manager's current primary account gaia_id before storing keys.
  // TODO(crbug.com/405036154): Handle key validation (e.g. expected count).
  DeviceAuthorizationKeys keys = response->device_authorization_keys();
  if (!client_->StoreKeys(gaia_id, keys)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(std::move(keys));
}

}  // namespace webauthn
