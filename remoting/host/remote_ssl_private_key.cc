// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_ssl_private_key.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/net_errors.h"

namespace remoting {

RemoteSSLPrivateKey::RemoteSSLPrivateKey(
    std::string provider_name,
    std::vector<uint16_t> algorithm_preferences,
    mojo::PendingRemote<network::mojom::SSLPrivateKey> remote_key)
    : provider_name_(std::move(provider_name)),
      algorithm_preferences_(std::move(algorithm_preferences)),
      remote_key_(std::move(remote_key)) {
  DCHECK(remote_key_.is_bound());
  remote_key_.set_disconnect_handler(base::BindOnce(
      &RemoteSSLPrivateKey::OnDisconnected, weak_factory_.GetWeakPtr()));
}

RemoteSSLPrivateKey::~RemoteSSLPrivateKey() = default;

std::string RemoteSSLPrivateKey::GetProviderName() {
  return provider_name_;
}

std::vector<uint16_t> RemoteSSLPrivateKey::GetAlgorithmPreferences() {
  return algorithm_preferences_;
}

void RemoteSSLPrivateKey::Sign(uint16_t algorithm,
                               base::span<const uint8_t> input,
                               SignCallback callback) {
  if (input.size() > kMaxSignatureInputSize) {
    LOG(ERROR) << "Signature input size exceeds maximum allowed size: "
               << input.size();
    std::move(callback).Run(net::ERR_FAILED, {});
    return;
  }

  if (!remote_key_.is_bound() || !remote_key_.is_connected()) {
    LOG(ERROR) << "SSLPrivateKey remote is disconnected.";
    std::move(callback).Run(net::ERR_FAILED, {});
    return;
  }

  std::vector<uint8_t> input_vec(input.begin(), input.end());
  remote_key_->Sign(
      algorithm, input_vec,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&RemoteSSLPrivateKey::OnSignComplete,
                         std::move(callback)),
          static_cast<int32_t>(net::ERR_FAILED), std::vector<uint8_t>()));
}

void RemoteSSLPrivateKey::OnDisconnected() {
  LOG(WARNING) << "SSLPrivateKey Mojo connection disconnected.";
}

// static
void RemoteSSLPrivateKey::OnSignComplete(
    SignCallback callback,
    int32_t net_error,
    const std::vector<uint8_t>& signature) {
  std::move(callback).Run(static_cast<net::Error>(net_error), signature);
}

}  // namespace remoting
