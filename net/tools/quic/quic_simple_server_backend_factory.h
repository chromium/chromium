// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_BACKEND_FACTORY_H_
#define NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_BACKEND_FACTORY_H_

#include "net/third_party/quiche/src/quiche/quic/tools/quic_toy_server.h"

namespace net {

// A factory for creating either QuicMemoryCacheBackend or QuicHttpProxyBackend
// instances.
class QuicSimpleServerBackendFactory
    : public quic::QuicToyServer::BackendFactory {
 public:
  std::unique_ptr<quic::QuicSimpleServerBackend> CreateBackend() override;
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_BACKEND_FACTORY_H_
