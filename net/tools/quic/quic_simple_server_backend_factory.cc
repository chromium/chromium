// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server_backend_factory.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_command_line_flags.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"

namespace net {

std::unique_ptr<quic::QuicSimpleServerBackend>
QuicSimpleServerBackendFactory::CreateBackend() {
  quic::QuicToyServer::MemoryCacheBackendFactory backend_factory;
  return backend_factory.CreateBackend();
}

}  // namespace net
