// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_STREAM_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_STREAM_FACTORY_H_

#include "net/third_party/quiche/src/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_server_stream.h"

namespace blink {

// Builds the crypto stream to be used by the P2PQuicTransport.
class P2PQuicCryptoStreamFactory {
 public:
  virtual ~P2PQuicCryptoStreamFactory() = default;

  virtual std::unique_ptr<quic::QuicCryptoClientStream>
  CreateClientCryptoStream(
      quic::QuicSession* session,
      quic::QuicCryptoClientConfig* crypto_config,
      quic::QuicCryptoClientStream::ProofHandler* proof_handler) = 0;

  virtual std::unique_ptr<quic::QuicCryptoServerStream>
  CreateServerCryptoStream(
      const quic::QuicCryptoServerConfig* crypto_config,
      quic::QuicCompressedCertsCache* compressed_certs_cache,
      quic::QuicSession* session,
      quic::QuicCryptoServerStream::Helper* helper) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_STREAM_FACTORY_H_
