// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/mock_crypto_client_stream_factory.h"

#include "base/lazy_instance.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_crypto_client_stream.h"

using std::string;

namespace net {

MockCryptoClientStreamFactory::~MockCryptoClientStreamFactory() = default;

MockCryptoClientStreamFactory::MockCryptoClientStreamFactory()
    : config_(std::make_unique<quic::QuicConfig>()) {}

void MockCryptoClientStreamFactory::SetConfig(const quic::QuicConfig& config) {
  config_ = std::make_unique<quic::QuicConfig>(config);
}

void MockCryptoClientStreamFactory::SetConfigForServerId(
    const quic::QuicServerId& server_id,
    const quic::QuicConfig& config) {
  config_for_server_[server_id] = std::make_unique<quic::QuicConfig>(config);
}

std::unique_ptr<quic::QuicCryptoClientStream>
MockCryptoClientStreamFactory::CreateQuicCryptoClientStream(
    const quic::QuicServerId& server_id,
    QuicChromiumClientSession* session,
    std::unique_ptr<quic::ProofVerifyContext> /*proof_verify_context*/,
    quic::QuicCryptoClientConfig* crypto_config) {
  const ProofVerifyDetailsChromium* proof_verify_details = nullptr;
  if (!proof_verify_details_queue_.empty()) {
    proof_verify_details = proof_verify_details_queue_.front();
    proof_verify_details_queue_.pop();
  }

  // Find a config in `config_for_server_`, falling back to `config_` if none
  // exists.
  auto it = config_for_server_.find(server_id);
  quic::QuicConfig* config =
      it == config_for_server_.end() ? config_.get() : it->second.get();

  std::unique_ptr<MockCryptoClientStream> stream =
      std::make_unique<MockCryptoClientStream>(
          server_id, session, nullptr, *config, crypto_config, handshake_mode_,
          proof_verify_details, use_mock_crypter_);
  streams_.push_back(stream->GetWeakPtr());
  return stream;
}

MockCryptoClientStream* MockCryptoClientStreamFactory::last_stream() const {
  CHECK(!streams_.empty());
  return streams_.back().get();
}

}  // namespace net
