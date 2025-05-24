// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ssl_private_key_proxy.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"

namespace network {

SSLPrivateKeyProxy::SSLPrivateKeyProxy(
    const std::string& provider_name,
    const std::vector<uint16_t>& algorithm_preferences,
    mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key)
    : provider_name_(provider_name),
      algorithm_preferences_(algorithm_preferences),
      ssl_private_key_(std::move(ssl_private_key)) {
  ssl_private_key_.set_disconnect_handler(base::BindOnce(
      &SSLPrivateKeyProxy::HandleSSLPrivateKeyError, base::Unretained(this)));
}

SSLPrivateKeyProxy::~SSLPrivateKeyProxy() = default;

std::string SSLPrivateKeyProxy::GetProviderName() {
  return provider_name_;
}

std::vector<uint16_t> SSLPrivateKeyProxy::GetAlgorithmPreferences() {
  return algorithm_preferences_;
}

void SSLPrivateKeyProxy::Sign(uint16_t algorithm,
                              base::span<const uint8_t> input,
                              net::SSLPrivateKey::SignCallback callback) {
  std::vector<uint8_t> input_vector(input.begin(), input.end());
  if (!ssl_private_key_ || !ssl_private_key_.is_connected()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY,
                                  input_vector));
    return;
  }

  ssl_private_key_->Sign(
      algorithm, input_vector,
      base::BindOnce(&SSLPrivateKeyProxy::Callback, this, std::move(callback)));
}

void SSLPrivateKeyProxy::HandleSSLPrivateKeyError() {
  ssl_private_key_.reset();
}

void SSLPrivateKeyProxy::Callback(net::SSLPrivateKey::SignCallback callback,
                                  int32_t net_error,
                                  const std::vector<uint8_t>& input) {
  DCHECK_LE(net_error, 0);
  DCHECK_NE(net_error, net::ERR_IO_PENDING);
  std::move(callback).Run(static_cast<net::Error>(net_error), input);
}

}  // namespace network
