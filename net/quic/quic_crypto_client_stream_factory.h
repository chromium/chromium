// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_FACTORY_H_
#define NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_FACTORY_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"

namespace quic {
class ProofVerifyContext;
}  // namespace quic
namespace net {
class QuicChromiumClientSession;
}  // namespace net
namespace quic {
class QuicCryptoClientConfig;

class QuicCryptoClientStream;
}  // namespace quic
namespace net {

// An interface used to instantiate quic::QuicCryptoClientStream objects. Used
// to facilitate testing code with mock implementations.
class NET_EXPORT QuicCryptoClientStreamFactory {
 public:
  virtual ~QuicCryptoClientStreamFactory() = default;

  virtual std::unique_ptr<quic::QuicCryptoClientStream>
  CreateQuicCryptoClientStream(
      const quic::QuicServerId& server_id,
      QuicChromiumClientSession* session,
      std::unique_ptr<quic::ProofVerifyContext> proof_verify_context,
      quic::QuicCryptoClientConfig* crypto_config) = 0;

  static QuicCryptoClientStreamFactory* GetDefaultFactory();
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_FACTORY_H_
