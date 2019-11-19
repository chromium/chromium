// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_CLIENT_CONFIG_HANDLE_H_
#define NET_QUIC_QUIC_CRYPTO_CLIENT_CONFIG_HANDLE_H_

#include "base/macros.h"
#include "net/base/net_export.h"

namespace quic {
class QuicCryptoClientConfig;
}  // namespace quic

namespace net {

// Class that allows consumers to access a quic::QuicCryptoClientConfig, while
// ensuring that the QuciStreamFactory that owns it keeps it alive. Once a
// QuicCryptoClientConfigHandle is destroyed, the underlying
// QuicCryptoClientConfig object may be destroyed as well. All
// QuicCryptoClientConfigHandle must be destroyed before the end of the
// QuciStreamFactory's destructor.
//
// This ownership model is used instead of refcounting for stronger safety
// guarantees, and because the underlying QuicCryptoClientConfig depends on
// other network objects that may be deleted after the QuicStreamFactory.
class NET_EXPORT_PRIVATE QuicCryptoClientConfigHandle {
 public:
  virtual ~QuicCryptoClientConfigHandle();
  virtual quic::QuicCryptoClientConfig* GetConfig() const = 0;

 protected:
  QuicCryptoClientConfigHandle();

 private:
  DISALLOW_ASSIGN(QuicCryptoClientConfigHandle);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_CLIENT_CONFIG_HANDLE_H_
