// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_IMPL_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/version_info/channel.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_client.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_keys_fetcher.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_service.h"

class GaiaId;

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

 private:
  // Callback invoked when the client finishes creating the request.
  void OnRequestCreated(const GaiaId& gaia_id,
                        sync_pb::GetDeviceAuthorizationKeyRequest request);

  // Callback invoked when the network fetch completes.
  void OnFetchCompleted(
      const GaiaId& gaia_id,
      base::expected<sync_pb::GetDeviceAuthorizationKeyResponse,
                     DeviceAuthorizationKeysFetcher::Error> response);

  // Used to obtain the primary account and authenticate requests.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // Factory used to create loaders for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Provides embedder-specific functionality (e.g. displaying UI).
  std::unique_ptr<DeviceAuthorizationClient> client_;

  // Executes network requests to retrieve the keys from the server.
  std::unique_ptr<DeviceAuthorizationKeysFetcher> fetcher_;

  // True if a network fetch is currently in flight.
  bool is_fetching_ = false;

  // Callback to invoke when the in-flight fetch completes.
  FetchDeviceAuthKeysCallback pending_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DeviceAuthorizationServiceImpl> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SERVICE_IMPL_H_
