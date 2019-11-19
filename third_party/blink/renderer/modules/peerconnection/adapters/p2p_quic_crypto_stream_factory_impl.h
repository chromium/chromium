// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_STREAM_FACTORY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_STREAM_FACTORY_IMPL_H_

#include "net/third_party/quiche/src/quic/core/quic_crypto_server_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_stream_factory.h"

namespace blink {

// The default factory for creating crypto streams to be used by the
// P2PQuicTransport.
class MODULES_EXPORT P2PQuicCryptoStreamFactoryImpl final
    : public P2PQuicCryptoStreamFactory {
 public:
  ~P2PQuicCryptoStreamFactoryImpl() override {}

  // P2PQuicCryptoStreamFactory overrides.
  std::unique_ptr<quic::QuicCryptoClientStream> CreateClientCryptoStream(
      quic::QuicSession* session,
      quic::QuicCryptoClientConfig* crypto_config,
      quic::QuicCryptoClientStream::ProofHandler* proof_handler) override;

  std::unique_ptr<quic::QuicCryptoServerStream> CreateServerCryptoStream(
      const quic::QuicCryptoServerConfig* crypto_config,
      quic::QuicCompressedCertsCache* compressed_certs_cache,
      quic::QuicSession* session,
      quic::QuicCryptoServerStream::Helper* helper) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_STREAM_FACTORY_IMPL_H_
