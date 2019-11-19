// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_stream_factory_impl.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_server_stream.h"

namespace blink {

std::unique_ptr<quic::QuicCryptoClientStream>
P2PQuicCryptoStreamFactoryImpl::CreateClientCryptoStream(
    quic::QuicSession* session,
    quic::QuicCryptoClientConfig* crypto_config,
    quic::QuicCryptoClientStream::ProofHandler* proof_handler) {
  // The QuicServerId is not important since we are communicating with a peer
  // endpoint and the connection is on top of ICE.
  quic::QuicServerId server_id(
      /*host=*/"",
      /*port=*/0,
      /*privacy_mode_enabled=*/false);
  return std::make_unique<quic::QuicCryptoClientStream>(
      server_id, session,
      crypto_config->proof_verifier()->CreateDefaultContext(), crypto_config,
      proof_handler);
}

std::unique_ptr<quic::QuicCryptoServerStream>
P2PQuicCryptoStreamFactoryImpl::CreateServerCryptoStream(
    const quic::QuicCryptoServerConfig* crypto_config,
    quic::QuicCompressedCertsCache* compressed_certs_cache,
    quic::QuicSession* session,
    quic::QuicCryptoServerStream::Helper* helper) {
  return std::make_unique<quic::QuicCryptoServerStream>(
      crypto_config, compressed_certs_cache, session, helper);
}

}  // namespace blink
