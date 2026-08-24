// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_KEYS_FETCHER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_KEYS_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/version_info/channel.h"

namespace endpoint_fetcher {
class EndpointFetcher;
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace sync_pb {
class GetDeviceAuthorizationKeyRequest;
class GetDeviceAuthorizationKeyResponse;
}  // namespace sync_pb

namespace webauthn {

// Handles requests to fetch device authorization keys for password manager
// passkeys.
class DeviceAuthorizationKeysFetcher {
 public:
  // Represents the possible error outcomes when fetching the keys.
  enum class Error {
    kNetworkError,
    kHttpError,
    kProtoParseError,
    kAlreadyInProgress,
  };

  using FetchKeysCallback = base::OnceCallback<void(
      base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>)>;

  explicit DeviceAuthorizationKeysFetcher(version_info::Channel channel);
  DeviceAuthorizationKeysFetcher(const DeviceAuthorizationKeysFetcher&) =
      delete;
  DeviceAuthorizationKeysFetcher& operator=(
      const DeviceAuthorizationKeysFetcher&) = delete;
  ~DeviceAuthorizationKeysFetcher();

  // Initiates the request to fetch device authorization keys.
  void FetchDeviceAuthorizationKeys(
      const sync_pb::GetDeviceAuthorizationKeyRequest& request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      FetchKeysCallback callback);

 private:
  void OnFetchCompleted(
      FetchKeysCallback callback,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  const version_info::Channel channel_;
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_KEYS_FETCHER_H_
