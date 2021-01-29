// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_
#define NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"

namespace quic {
class QuicCryptoClientStream;
}  // namespace quic
namespace net {

class MockCryptoClientStreamFactory : public QuicCryptoClientStreamFactory {
 public:
  MockCryptoClientStreamFactory();
  ~MockCryptoClientStreamFactory() override;

  quic::QuicCryptoClientStream* CreateQuicCryptoClientStream(
      const quic::QuicServerId& server_id,
      QuicChromiumClientSession* session,
      std::unique_ptr<quic::ProofVerifyContext> proof_verify_context,
      quic::QuicCryptoClientConfig* crypto_config) override;

  void set_handshake_mode(
      MockCryptoClientStream::HandshakeMode handshake_mode) {
    handshake_mode_ = handshake_mode;
  }

  void set_use_mock_crypter(bool use_mock_crypter) {
    use_mock_crypter_ = use_mock_crypter;
  }

  // The caller keeps ownership of |proof_verify_details|.
  void AddProofVerifyDetails(
      const ProofVerifyDetailsChromium* proof_verify_details) {
    proof_verify_details_queue_.push(proof_verify_details);
  }

  MockCryptoClientStream* last_stream() const { return last_stream_; }

  // Sets initial config for new sessions.
  void SetConfig(const quic::QuicConfig& config);

 private:
  MockCryptoClientStream::HandshakeMode handshake_mode_;
  MockCryptoClientStream* last_stream_;
  base::queue<const ProofVerifyDetailsChromium*> proof_verify_details_queue_;
  std::unique_ptr<quic::QuicConfig> config_;
  bool use_mock_crypter_;

  DISALLOW_COPY_AND_ASSIGN(MockCryptoClientStreamFactory);
};

}  // namespace net

#endif  // NET_QUIC_MOCK_CRYPTO_CLIENT_STREAM_FACTORY_H_
