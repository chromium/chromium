// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_quic_crypto_client_config_handle.h"

namespace net {

TestQuicCryptoClientConfigHandle::TestQuicCryptoClientConfigHandle(
    quic::QuicCryptoClientConfig* crypto_config)
    : crypto_config_(crypto_config) {}

TestQuicCryptoClientConfigHandle::~TestQuicCryptoClientConfigHandle() = default;

quic::QuicCryptoClientConfig* TestQuicCryptoClientConfigHandle::GetConfig()
    const {
  return crypto_config_;
}

}  // namespace net
