// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_context.h"

#include "base/containers/contains.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/ssl/cert_compression.h"
#include "net/ssl/ssl_key_logger.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_constants.h"

namespace net {

namespace {

// The maximum receive window sizes for QUIC sessions and streams.
const int32_t kQuicSessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
const int32_t kQuicStreamMaxRecvWindowSize = 6 * 1024 * 1024;    // 6 MB

// Set the maximum number of undecryptable packets the connection will store.
const int32_t kMaxUndecryptablePackets = 100;

}  // namespace

QuicParams::QuicParams() = default;

QuicParams::QuicParams(const QuicParams& other) = default;

QuicParams::~QuicParams() = default;

QuicContext::QuicContext()
    : QuicContext(std::make_unique<QuicChromiumConnectionHelper>(
          quic::QuicChromiumClock::GetInstance(),
          quic::QuicRandom::GetInstance())) {}

QuicContext::QuicContext(
    std::unique_ptr<quic::QuicConnectionHelperInterface> helper)
    : helper_(std::move(helper)) {}

QuicContext::~QuicContext() = default;

quic::QuicConfig InitializeQuicConfig(const QuicParams& params) {
  DCHECK_GT(params.idle_connection_timeout, base::TimeDelta());
  quic::QuicConfig config;
  config.SetIdleNetworkTimeout(
      quic::QuicTime::Delta::FromMicroseconds(
          params.idle_connection_timeout.InMicroseconds()));
  config.set_max_time_before_crypto_handshake(
      quic::QuicTime::Delta::FromMicroseconds(
          params.max_time_before_crypto_handshake.InMicroseconds()));
  config.set_max_idle_time_before_crypto_handshake(
      quic::QuicTime::Delta::FromMicroseconds(
          params.max_idle_time_before_crypto_handshake.InMicroseconds()));
  quic::QuicTagVector copt_to_send = params.connection_options;
  config.SetConnectionOptionsToSend(copt_to_send);
  config.SetClientConnectionOptions(params.client_connection_options);
  config.set_max_undecryptable_packets(kMaxUndecryptablePackets);
  config.SetInitialSessionFlowControlWindowToSend(
      kQuicSessionMaxRecvWindowSize);
  config.SetInitialStreamFlowControlWindowToSend(kQuicStreamMaxRecvWindowSize);
  config.SetBytesForConnectionIdToSend(0);
  return config;
}

void ConfigureQuicCryptoClientConfig(
    quic::QuicCryptoClientConfig& crypto_config) {
  if (SSLKeyLoggerManager::IsActive()) {
    SSL_CTX_set_keylog_callback(crypto_config.ssl_ctx(),
                                SSLKeyLoggerManager::KeyLogCallback);
  }
  ConfigureCertificateCompression(crypto_config.ssl_ctx());
}

}  // namespace net
