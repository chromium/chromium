// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_STREAM_SENDER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_STREAM_SENDER_H_

#include <cstdint>

#include "net/third_party/http2/hpack/varint/hpack_varint_encoder.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

// This class serializes instructions for transmission on the encoder stream.
class QUIC_EXPORT_PRIVATE QpackEncoderStreamSender {
 public:
  // An interface for handling encoded data.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Encoded |data| is ready to be written on the encoder stream.
    // |data| is guaranteed to be not empty.
    virtual void Write(QuicStringPiece data) = 0;
  };

  explicit QpackEncoderStreamSender(Delegate* delegate);
  QpackEncoderStreamSender() = delete;
  QpackEncoderStreamSender(const QpackEncoderStreamSender&) = delete;
  QpackEncoderStreamSender& operator=(const QpackEncoderStreamSender&) = delete;

  // Methods for sending instructions, see
  // https://quicwg.org/base-drafts/draft-ietf-quic-qpack.html#rfc.section.5.2

  // 5.2.1. Insert With Name Reference
  void SendInsertWithNameReference(bool is_static,
                                   uint64_t name_index,
                                   QuicStringPiece value);
  // 5.2.2. Insert Without Name Reference
  void SendInsertWithoutNameReference(QuicStringPiece name,
                                      QuicStringPiece value);
  // 5.2.3. Duplicate
  void SendDuplicate(uint64_t index);
  // 5.2.4. Dynamic Table Size Update
  void SendDynamicTableSizeUpdate(uint64_t max_size);

 private:
  Delegate* const delegate_;
  http2::HpackVarintEncoder varint_encoder_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_STREAM_SENDER_H_
