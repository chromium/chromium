// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_H_

#include <cstdint>
#include <memory>

#include "net/third_party/quic/core/qpack/qpack_header_table.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/spdy/core/hpack/hpack_encoder.h"
#include "net/third_party/spdy/core/spdy_header_block.h"

namespace quic {

// QPACK encoder class.  Exactly one instance should exist per QUIC connection.
// This class vends a new ProgressiveEncoder instance for each new header list
// to be encoded.
// TODO(bnc): This class will manage the encoding context, send data on the
// encoder stream, and receive data on the decoder stream.
class QUIC_EXPORT_PRIVATE QpackEncoder {
 public:
  // This factory method should be called to start encoding a header list.
  // |*header_list| must remain valid and must not change
  // during the lifetime of the returned ProgressiveEncoder instance.
  std::unique_ptr<spdy::HpackEncoder::ProgressiveEncoder> EncodeHeaderList(
      const spdy::SpdyHeaderBlock* header_list);

 private:
  QpackHeaderTable header_table_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_H_
