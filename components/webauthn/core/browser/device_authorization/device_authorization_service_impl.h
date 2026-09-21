// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_IMPL_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/version_info/channel.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_client.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_keys_fetcher.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace sync_pb {
class GetDeviceAuthorizationKeyRequest;
}  // namespace sync_pb

namespace webauthn {

// Coordinates the process of retrieving the device authorization keys,
// including communication with the server, validation of the retrieved keys
// and delegating platform-specific logic to DeviceAuthorizationClient.
class DeviceAuthorizationServiceImpl : public DeviceAuthorizationService {
 public:
  DeviceAuthorizationServiceImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DeviceAuthorizationClient> client,
      version_info::Channel channel);
  DeviceAuthorizationServiceImpl(const DeviceAuthorizationServiceImpl&) =
      delete;
  DeviceAuthorizationServiceImpl& operator=(
      const DeviceAuthorizationServiceImpl&) = delete;
  ~DeviceAuthorizationServiceImpl() override;

  // KeyedService:
  void Shutdown() override;

  // DeviceAuthorizationService:
  void GetOrFetchKeys(FetchDeviceAuthKeysCallback callback) override;
  void FetchKeysWithReAuthToken(std::string reauth_proof_token,
                                FetchDeviceAuthKeysCallback callback) override;

 private:
  void FetchKeysImpl(std::optional<std::string> reauth_proof_token,
                     FetchDeviceAuthKeysCallback callback);

  // Callback invoked when local cached keys have been retrieved.
  void OnCachedKeysFetched(
      const GaiaId& gaia_id,
      std::optional<CachedDeviceAuthorizationKeys> cached_keys);

  // Callback invoked when the client finishes populating platform data.
  void OnPlatformDataPopulated(
      const GaiaId& gaia_id,
      sync_pb::GetDeviceAuthorizationKeyRequest request);

  // Callback invoked when the network fetch completes.
  void OnFetchCompleted(
      const GaiaId& gaia_id,
      base::expected<sync_pb::GetDeviceAuthorizationKeyResponse,
                     DeviceAuthorizationKeysFetcher::Error> response);

  // Callback invoked when keys have been stored in the local cache.
  void OnKeysStored(const GaiaId& gaia_id,
                    DeviceAuthorizationKeys keys,
                    bool success);

  // Invokes all pending callbacks for `gaia_id` with `result` and clears them.
  void NotifyPendingCallbacks(const GaiaId& gaia_id,
                              const DeviceAuthFetchResult& result);

  // Used to obtain the primary account and authenticate requests.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // Factory used to create loaders for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Provides embedder-specific functionality (e.g. displaying UI).
  std::unique_ptr<DeviceAuthorizationClient> client_;

  // Executes network requests to retrieve the keys from the server.
  std::unique_ptr<DeviceAuthorizationKeysFetcher> fetcher_;

  // Pending callbacks keyed by GaiaId for coalesced requests.
  base::flat_map<GaiaId, std::vector<FetchDeviceAuthKeysCallback>>
      pending_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DeviceAuthorizationServiceImpl> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_IMPL_H_
