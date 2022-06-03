// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_context.h"

#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/quic_constants.h"

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
  config.SetConnectionOptionsToSend(params.connection_options);
  config.SetClientConnectionOptions(params.client_connection_options);
  config.set_max_undecryptable_packets(kMaxUndecryptablePackets);
  config.SetInitialSessionFlowControlWindowToSend(
      kQuicSessionMaxRecvWindowSize);
  config.SetInitialStreamFlowControlWindowToSend(kQuicStreamMaxRecvWindowSize);
  config.SetBytesForConnectionIdToSend(0);
  return config;
}

}  // namespace net
