// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl_private_key_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"

namespace content {

SSLPrivateKeyImpl::SSLPrivateKeyImpl(
    scoped_refptr<net::SSLPrivateKey> ssl_private_key)
    : ssl_private_key_(std::move(ssl_private_key)) {}

SSLPrivateKeyImpl::~SSLPrivateKeyImpl() = default;

void SSLPrivateKeyImpl::Sign(
    uint16_t algorithm,
    const std::vector<uint8_t>& input,
    network::mojom::SSLPrivateKey::SignCallback callback) {
  base::span<const uint8_t> input_span(input);
  ssl_private_key_->Sign(
      algorithm, input_span,
      base::BindOnce(&SSLPrivateKeyImpl::Callback, base::Unretained(this),
                     std::move(callback)));
}

void SSLPrivateKeyImpl::Callback(
    network::mojom::SSLPrivateKey::SignCallback callback,
    net::Error net_error,
    const std::vector<uint8_t>& signature) {
  std::move(callback).Run(static_cast<int32_t>(net_error), signature);
}

}  // namespace content
