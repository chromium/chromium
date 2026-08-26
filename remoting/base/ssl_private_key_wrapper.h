// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SSL_PRIVATE_KEY_WRAPPER_H_
#define REMOTING_BASE_SSL_PRIVATE_KEY_WRAPPER_H_

#include <stdint.h>

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace remoting {

// Wraps a net::SSLPrivateKey and exposes it over the
// network::mojom::SSLPrivateKey Mojo interface.
class SSLPrivateKeyWrapper : public network::mojom::SSLPrivateKey {
 public:
  explicit SSLPrivateKeyWrapper(
      scoped_refptr<net::SSLPrivateKey> ssl_private_key);

  SSLPrivateKeyWrapper(const SSLPrivateKeyWrapper&) = delete;
  SSLPrivateKeyWrapper& operator=(const SSLPrivateKeyWrapper&) = delete;

  ~SSLPrivateKeyWrapper() override;

  // network::mojom::SSLPrivateKey implementation.
  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            network::mojom::SSLPrivateKey::SignCallback callback) override;

 private:
  void Callback(network::mojom::SSLPrivateKey::SignCallback callback,
                net::Error net_error,
                const std::vector<uint8_t>& signature);

  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;
  base::WeakPtrFactory<SSLPrivateKeyWrapper> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_SSL_PRIVATE_KEY_WRAPPER_H_
