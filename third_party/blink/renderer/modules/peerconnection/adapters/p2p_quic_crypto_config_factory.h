// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_CONFIG_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_CONFIG_FACTORY_H_

#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_server_config.h"

namespace blink {

// Builds the crypto configurations to be used by the P2PQuicTransport.
class P2PQuicCryptoConfigFactory {
 public:
  virtual ~P2PQuicCryptoConfigFactory() = default;

  // Creates the client crypto configuration to be used by a
  // quic::QuicCryptoClientStream. This includes a ProofVerifier object that
  // verifies the server's certificate and is used in the QUIC handshake.
  virtual std::unique_ptr<quic::QuicCryptoClientConfig>
  CreateClientCryptoConfig() = 0;

  // Creates the server crypto configuration to be used by a
  // quic::QuicCryptoServerStream. This includes a ProofSource object that gives
  // the server's certificate.
  virtual std::unique_ptr<quic::QuicCryptoServerConfig>
  CreateServerCryptoConfig() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_CONFIG_FACTORY_H_
