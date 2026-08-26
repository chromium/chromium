// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/ssl_private_key_wrapper.h"

#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"

namespace remoting {

namespace {
// Limit max input size for signing to prevent memory exhaustion or DoS attacks.
constexpr size_t kMaxSignatureInputSize = 64 * 1024;  // 64 KB
}  // namespace

SSLPrivateKeyWrapper::SSLPrivateKeyWrapper(
    scoped_refptr<net::SSLPrivateKey> ssl_private_key)
    : ssl_private_key_(std::move(ssl_private_key)) {
  DCHECK(ssl_private_key_);
}

SSLPrivateKeyWrapper::~SSLPrivateKeyWrapper() = default;

void SSLPrivateKeyWrapper::Sign(
    uint16_t algorithm,
    const std::vector<uint8_t>& input,
    network::mojom::SSLPrivateKey::SignCallback callback) {
  if (input.size() > kMaxSignatureInputSize) {
    std::move(callback).Run(static_cast<int32_t>(net::ERR_FAILED), {});
    return;
  }
  base::span<const uint8_t> input_span(input);
  ssl_private_key_->Sign(
      algorithm, input_span,
      base::BindOnce(&SSLPrivateKeyWrapper::Callback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SSLPrivateKeyWrapper::Callback(
    network::mojom::SSLPrivateKey::SignCallback callback,
    net::Error net_error,
    const std::vector<uint8_t>& signature) {
  std::move(callback).Run(static_cast<int32_t>(net_error), signature);
}

}  // namespace remoting
