// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_CONFIG_FACTORY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_CONFIG_FACTORY_IMPL_H_

#include "net/third_party/quiche/src/quic/core/crypto/proof_source.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_config_factory.h"
#include "third_party/webrtc/rtc_base/rtc_certificate.h"

namespace blink {

// An abstraction for building the crypto configurations to be used by the
// P2PQuicTransport.
class MODULES_EXPORT P2PQuicCryptoConfigFactoryImpl final
    : public P2PQuicCryptoConfigFactory {
 public:
  P2PQuicCryptoConfigFactoryImpl(
      quic::QuicRandom* const random_generator);

  ~P2PQuicCryptoConfigFactoryImpl() override {}

  // P2PQuicCryptoConfigFactory overrides.
  //
  // Creates a client config that accepts any remote certificate.
  std::unique_ptr<quic::QuicCryptoClientConfig> CreateClientCryptoConfig()
      override;
  // Creates a server config with a DummyProofSource.
  std::unique_ptr<quic::QuicCryptoServerConfig> CreateServerCryptoConfig()
      override;

 private:
  // TODO(crbug.com/874296): Add certificate(s) to create a proof source
  // that comes from the self signed certificate given to the RTCQuicTransport.
  quic::QuicRandom* const random_generator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_CRYPTO_CONFIG_FACTORY_IMPL_H_
