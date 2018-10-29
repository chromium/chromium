// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_STREAM_RECEIVER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_STREAM_RECEIVER_H_

#include <cstddef>
#include <cstdint>

#include "net/third_party/http2/hpack/huffman/hpack_huffman_decoder.h"
#include "net/third_party/http2/hpack/varint/hpack_varint_decoder.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

// This class decodes data received on the encoder stream.
class QUIC_EXPORT_PRIVATE QpackEncoderStreamReceiver {
 public:
  // An interface for handling instructions decoded from the encoder stream, see
  // https://quicwg.org/base-drafts/draft-ietf-quic-qpack.html#rfc.section.5.2
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // 5.2.1. Insert With Name Reference
    virtual void OnInsertWithNameReference(bool is_static,
                                           uint64_t name_index,
                                           QuicStringPiece value) = 0;
    // 5.2.2. Insert Without Name Reference
    virtual void OnInsertWithoutNameReference(QuicStringPiece name,
                                              QuicStringPiece value) = 0;
    // 5.2.3. Duplicate
    virtual void OnDuplicate(uint64_t index) = 0;
    // 5.2.4. Dynamic Table Size Update
    virtual void OnDynamicTableSizeUpdate(uint64_t max_size) = 0;
    // Decoding error
    virtual void OnErrorDetected(QuicStringPiece error_message) = 0;
  };

  explicit QpackEncoderStreamReceiver(Delegate* delegate);
  QpackEncoderStreamReceiver() = delete;
  QpackEncoderStreamReceiver(const QpackEncoderStreamReceiver&) = delete;
  QpackEncoderStreamReceiver& operator=(const QpackEncoderStreamReceiver&) =
      delete;

  // Decode data and call appropriate Delegate method after each decoded
  // instruction.  Once an error occurs, Delegate::OnErrorDetected() is called,
  // and all further data is ignored.
  void Decode(QuicStringPiece data);

 private:
  enum class State {
    // Identify the instruction and start decoding an integer.
    kStart,
    // Decode name index or name length.
    kNameIndexOrLengthResume,
    kNameIndexOrLengthDone,
    // Read name string literal, which is optionally Huffman encoded.
    kReadName,
    // Optionally decode Huffman encoded name.
    kDecodeName,
    // Read value string length.
    kValueLengthStart,
    kValueLengthResume,
    kValueLengthDone,
    // Read value string literal, which is optionally Huffman encoded.
    kReadValue,
    // Optionally decode Huffman encoded value.
    kDecodeValue,
    // Done with insertion instruction.
    kInsertDone,
    // Read index to duplicate.
    kIndexResume,
    kIndexDone,
    // Read maximum table size.
    kMaxSizeResume,
    kMaxSizeDone,
  };

  // One method for each state.  Some take input data and return the number of
  // octets processed.  Some only change internal state.
  size_t DoStart(QuicStringPiece data);
  size_t DoNameIndexOrLengthResume(QuicStringPiece data);
  void DoNameIndexOrLengthDone();
  size_t DoReadName(QuicStringPiece data);
  void DoDecodeName();
  size_t DoValueLengthStart(QuicStringPiece data);
  size_t DoValueLengthResume(QuicStringPiece data);
  void DoValueLengthDone();
  size_t DoReadValue(QuicStringPiece data);
  void DoDecodeValue();
  void DoInsertDone();
  size_t DoIndexResume(QuicStringPiece data);
  void DoIndexDone();
  size_t DoMaxSizeResume(QuicStringPiece data);
  void DoMaxSizeDone();

  void OnError(QuicStringPiece error_message);

  Delegate* const delegate_;
  http2::HpackVarintDecoder varint_decoder_;
  http2::HpackHuffmanDecoder huffman_decoder_;
  State state_;

  // True if the currently parsed string (name or value) is Huffman encoded.
  bool is_huffman_;

  // True if the name index refers to the static table.
  bool is_static_;

  // True if the header field value is encoded as a string literal.
  bool literal_name_;

  // Decoded name index.
  uint64_t name_index_;

  // Decoded length for header name.
  size_t name_length_;

  // Decoded header name.
  QuicString name_;

  // Decoded length for header value.
  size_t value_length_;

  // Decoded header value.
  QuicString value_;

  // True if a decoding error has been detected.
  bool error_detected_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_STREAM_RECEIVER_H_
