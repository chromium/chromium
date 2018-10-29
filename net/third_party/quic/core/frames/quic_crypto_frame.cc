// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_crypto_frame.h"

#include "net/third_party/quic/platform/api/quic_logging.h"

namespace quic {

QuicCryptoFrame::QuicCryptoFrame() : QuicCryptoFrame(0, nullptr, 0) {}

QuicCryptoFrame::QuicCryptoFrame(QuicStreamOffset offset, QuicStringPiece data)
    : QuicCryptoFrame(offset, data.data(), data.length()) {}

QuicCryptoFrame::QuicCryptoFrame(QuicStreamOffset offset,
                                 const char* data_buffer,
                                 QuicPacketLength data_length)
    : data_length(data_length), data_buffer(data_buffer), offset(offset) {}

QuicCryptoFrame::~QuicCryptoFrame() {}

std::ostream& operator<<(std::ostream& os,
                         const QuicCryptoFrame& stream_frame) {
  os << "{ offset: " << stream_frame.offset
     << ", length: " << stream_frame.data_length << " }\n";
  return os;
}

}  // namespace quic
