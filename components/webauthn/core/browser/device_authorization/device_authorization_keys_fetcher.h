// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_KEYS_FETCHER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_KEYS_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
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

namespace webauthn {

inline constexpr char kDeviceAuthorizationKeyEndpointUrl[] =
    "https://chromesyncpasswords-pa.googleapis.com/v1/users/me/"
    "deviceAuthorizationKey";

// Handles requests to fetch device authorization keys for password manager
// passkeys.
class DeviceAuthorizationKeysFetcher {
 public:
  using FetchKeysCallback = base::OnceCallback<void(
      std::unique_ptr<endpoint_fetcher::EndpointResponse>)>;

  explicit DeviceAuthorizationKeysFetcher(version_info::Channel channel);
  DeviceAuthorizationKeysFetcher(const DeviceAuthorizationKeysFetcher&) =
      delete;
  DeviceAuthorizationKeysFetcher& operator=(
      const DeviceAuthorizationKeysFetcher&) = delete;
  ~DeviceAuthorizationKeysFetcher();

  // Initiates the request to fetch device authorization keys.
  // TODO(crbug.com/405036154): Handle passing request params.
  void FetchDeviceAuthorizationKeys(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      FetchKeysCallback callback);

 private:
  // TODO(crbug.com/405036154): Handle parsing response.
  void OnFetchCompleted(
      FetchKeysCallback callback,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  const version_info::Channel channel_;
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_KEYS_FETCHER_H_
