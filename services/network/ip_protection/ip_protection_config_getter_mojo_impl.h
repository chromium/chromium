// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_GETTER_MOJO_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_GETTER_MOJO_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/ip_protection/ip_protection_config_getter.h"
#include "services/network/ip_protection/ip_protection_data_types.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionConfigGetterMojoImpl final
    : public IpProtectionConfigGetter {
 public:
  explicit IpProtectionConfigGetterMojoImpl(
      mojo::PendingRemote<network::mojom::IpProtectionConfigGetter>
          config_getter);
  ~IpProtectionConfigGetterMojoImpl() override;

  void TryGetAuthTokens(uint32_t batch_size,
                        IpProtectionProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;
  void GetProxyList(GetProxyListCallback callback) override;
  bool IsAvailable() override;

 private:
  void OnGotProxyList(
      GetProxyListCallback callback,
      const std::optional<std::vector<net::ProxyChain>>& proxy_list,
      const std::optional<GeoHint>& geo_hint);
  void OnGotAuthTokens(
      TryGetAuthTokensCallback callback,
      const std::optional<std::vector<BlindSignedAuthToken>>& tokens,
      std::optional<::base::Time> expiration_time);

  bool is_available_ = false;
  mojo::Remote<network::mojom::IpProtectionConfigGetter> config_getter_;
  base::WeakPtrFactory<IpProtectionConfigGetterMojoImpl> weak_ptr_factory_{
      this};
};
}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_GETTER_MOJO_IMPL_H_
