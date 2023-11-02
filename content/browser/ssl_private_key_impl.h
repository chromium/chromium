// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_PRIVATE_KEY_IMPL_H_
#define CONTENT_BROWSER_SSL_PRIVATE_KEY_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace content {

class SSLPrivateKeyImpl : public network::mojom::SSLPrivateKey {
 public:
  explicit SSLPrivateKeyImpl(scoped_refptr<net::SSLPrivateKey> ssl_private_key);

  SSLPrivateKeyImpl(const SSLPrivateKeyImpl&) = delete;
  SSLPrivateKeyImpl& operator=(const SSLPrivateKeyImpl&) = delete;

  ~SSLPrivateKeyImpl() override;

  // network::mojom::SSLPrivateKey:
  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            network::mojom::SSLPrivateKey::SignCallback callback) override;

 private:
  void Callback(network::mojom::SSLPrivateKey::SignCallback callback,
                net::Error net_error,
                const std::vector<uint8_t>& signature);

  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_PRIVATE_KEY_IMPL_H_
