// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_SSL_PRIVATE_KEY_H_
#define REMOTING_HOST_REMOTE_SSL_PRIVATE_KEY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace remoting {

// Implements net::SSLPrivateKey by forwarding Sign requests over Mojo to a
// remote network::mojom::SSLPrivateKey instance.
class RemoteSSLPrivateKey : public net::SSLPrivateKey {
 public:
  static constexpr size_t kMaxSignatureInputSize = 64 * 1024;  // 64 KB

  RemoteSSLPrivateKey(
      std::string provider_name,
      std::vector<uint16_t> algorithm_preferences,
      mojo::PendingRemote<network::mojom::SSLPrivateKey> remote_key);

  RemoteSSLPrivateKey(const RemoteSSLPrivateKey&) = delete;
  RemoteSSLPrivateKey& operator=(const RemoteSSLPrivateKey&) = delete;

  // net::SSLPrivateKey implementation:
  std::string GetProviderName() override;
  std::vector<uint16_t> GetAlgorithmPreferences() override;
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override;

 private:
  ~RemoteSSLPrivateKey() override;

  static void OnSignComplete(SignCallback callback,
                             int32_t net_error,
                             const std::vector<uint8_t>& signature);

  void OnDisconnected();

  std::string provider_name_;
  std::vector<uint16_t> algorithm_preferences_;
  mojo::Remote<network::mojom::SSLPrivateKey> remote_key_;
  base::WeakPtrFactory<RemoteSSLPrivateKey> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_SSL_PRIVATE_KEY_H_
