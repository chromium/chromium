// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_QUIC_CRYPTO_CLIENT_CONFIG_HANDLE_H_
#define NET_QUIC_TEST_QUIC_CRYPTO_CLIENT_CONFIG_HANDLE_H_

#include "base/memory/raw_ptr.h"
#include "net/quic/quic_crypto_client_config_handle.h"

namespace quic {
class QuicCryptoClientConfig;
}  // namespace quic

namespace net {

// Test implementation of QuicCryptoClientConfigHandle. Wraps a passed in
// QuicCryptoClientConfig and returns it as needed. Does nothing on destruction.
class TestQuicCryptoClientConfigHandle : public QuicCryptoClientConfigHandle {
 public:
  explicit TestQuicCryptoClientConfigHandle(
      quic::QuicCryptoClientConfig* crypto_config);

  TestQuicCryptoClientConfigHandle& operator=(
      const TestQuicCryptoClientConfigHandle&) = delete;

  ~TestQuicCryptoClientConfigHandle() override;

  quic::QuicCryptoClientConfig* GetConfig() const override;

 private:
  const raw_ptr<quic::QuicCryptoClientConfig> crypto_config_;
};

}  // namespace net

#endif  // NET_QUIC_TEST_QUIC_CRYPTO_CLIENT_CONFIG_HANDLE_H_
