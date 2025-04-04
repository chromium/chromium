// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SSL_PRIVATE_KEY_PROXY_H_
#define SERVICES_NETWORK_SSL_PRIVATE_KEY_PROXY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace network {

// Implements the net::SSLPrivateKey interface by forwarding signing requests
// via a Mojo pipe.
//
// This class holds a mojo::Remote<mojom::SSLPrivateKey> connection to an actual
// SSL private key implementation. When Sign() is called, it serializes the
// request, sends it over Mojo, and uses the response to invoke the
// net::SSLPrivateKey::SignCallback. It also handles disconnection of the Mojo
// pipe, treating it as an error.
class SSLPrivateKeyProxy : public net::SSLPrivateKey {
 public:
  SSLPrivateKeyProxy(const std::string& provider_name,
                     const std::vector<uint16_t>& algorithm_preferences,
                     mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key);

  SSLPrivateKeyProxy(const SSLPrivateKeyProxy&) = delete;
  SSLPrivateKeyProxy& operator=(const SSLPrivateKeyProxy&) = delete;

  // net::SSLPrivateKey:
  std::string GetProviderName() override;
  std::vector<uint16_t> GetAlgorithmPreferences() override;
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            net::SSLPrivateKey::SignCallback callback) override;

 private:
  ~SSLPrivateKeyProxy() override;

  void HandleSSLPrivateKeyError();

  void Callback(net::SSLPrivateKey::SignCallback callback,
                int32_t net_error,
                const std::vector<uint8_t>& input);

  const std::string provider_name_;
  const std::vector<uint16_t> algorithm_preferences_;
  mojo::Remote<mojom::SSLPrivateKey> ssl_private_key_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SSL_PRIVATE_KEY_PROXY_H_
