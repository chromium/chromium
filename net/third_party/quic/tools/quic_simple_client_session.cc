// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_simple_client_session.h"

#include "net/third_party/quic/platform/api/quic_ptr_util.h"

namespace quic {

std::unique_ptr<QuicSpdyClientStream>
QuicSimpleClientSession::CreateClientStream() {
  return QuicMakeUnique<QuicSimpleClientStream>(
      GetNextOutgoingStreamId(), this, BIDIRECTIONAL, drop_response_body_);
}

}  // namespace quic
