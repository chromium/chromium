// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_DECODER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_DECODER_H_

#include <memory>

#include "net/third_party/http2/hpack/huffman/hpack_huffman_decoder.h"
#include "net/third_party/http2/hpack/varint/hpack_varint_decoder.h"
#include "net/third_party/quic/core/qpack/qpack_header_table.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

// QPACK decoder class.  Exactly one instance should exist per QUIC connection.
// This class vends a new ProgressiveDecoder instance for each new header list
// to be encoded.
// TODO(bnc): This class will manage the decoding context, send data on the
// decoder stream, and receive data on the encoder stream.
class QUIC_EXPORT_PRIVATE QpackDecoder {
 public:
  // Interface for receiving decoded header block from the decoder.
  class QUIC_EXPORT_PRIVATE HeadersHandlerInterface {
   public:
    virtual ~HeadersHandlerInterface() {}

    // Called when a new header name-value pair is decoded.  Multiple values for
    // a given name will be emitted as multiple calls to OnHeader.
    virtual void OnHeaderDecoded(QuicStringPiece name,
                                 QuicStringPiece value) = 0;

    // Called when the header block is completely decoded.
    // Indicates the total number of bytes in this block.
    // The decoder will not access the handler after this call.
    // Note that this method might not be called synchronously when the header
    // block is received on the wire, in case decoding is blocked on receiving
    // entries on the encoder stream.  TODO(bnc): Implement blocked decoding.
    virtual void OnDecodingCompleted() = 0;

    // Called when a decoding error has occurred.  No other methods will be
    // called afterwards.
    virtual void OnDecodingErrorDetected(QuicStringPiece error_message) = 0;
  };

  // Class to decode a single header block.
  class QUIC_EXPORT_PRIVATE ProgressiveDecoder {
   public:
    ProgressiveDecoder() = delete;
    ProgressiveDecoder(QpackHeaderTable* header_table,
                       HeadersHandlerInterface* handler);
    ProgressiveDecoder(const ProgressiveDecoder&) = delete;
    ProgressiveDecoder& operator=(const ProgressiveDecoder&) = delete;
    ~ProgressiveDecoder() = default;

    // Provide a data fragment to decode.
    void Decode(QuicStringPiece data);

    // Signal that the entire header block has been received and passed in
    // through Decode().  No methods must be called afterwards.
    void EndHeaderBlock();

   private:
    enum class State {
      // Every instruction starts encoding an integer on the first octet:
      // either an index or the length of the name string literal.
      kStart,
      kVarintResume,
      kVarintDone,
      // This might be followed by the name as a string literal,
      // optionally Huffman encoded.
      kReadName,
      kDecodeName,
      // This might be followed by the length of the value.
      kValueLengthStart,
      kValueLengthResume,
      kValueLengthDone,
      // This might be followed by the value as a string literal,
      // optionally Huffman encoded.
      kReadValue,
      kDecodeValue,
      kDone,
    };

    // One method for each state.  Some take input data and return the number of
    // octets processed.  Some only change internal state.
    size_t DoStart(QuicStringPiece data);
    size_t DoVarintResume(QuicStringPiece data);
    void DoVarintDone();
    size_t DoReadName(QuicStringPiece data);
    void DoDecodeName();
    size_t DoValueLengthStart(QuicStringPiece data);
    size_t DoValueLengthResume(QuicStringPiece data);
    void DoValueLengthDone();
    size_t DoReadValue(QuicStringPiece data);
    void DoDecodeValue();
    void DoDone();

    void OnError(QuicStringPiece error_message);

    const QpackHeaderTable* const header_table_;
    HeadersHandlerInterface* handler_;
    State state_;
    http2::HpackVarintDecoder varint_decoder_;
    http2::HpackHuffmanDecoder huffman_decoder_;

    // True until EndHeaderBlock() is called.
    bool decoding_;

    // True if a decoding error has been detected.
    bool error_detected_;

    // The following variables are used to carry information between states
    // within a single header field.  That is, a value assigned while decoding
    // one header field shall never be used for decoding subsequent header
    // fields.

    // True if the header field name is encoded as a string literal.
    bool literal_name_;

    // True if the header field value is encoded as a string literal.
    bool literal_value_;

    // Decoded length for header name.
    size_t name_length_;

    // Decoded length for header value.
    size_t value_length_;

    // Whether the currently parsed string (name or value) is
    // Huffman encoded.
    bool is_huffman_;

    // Decoded header name and value.
    QuicString name_;
    QuicString value_;
  };

  // Factory method to create a ProgressiveDecoder for decoding a header block.
  // |handler| must remain valid until the returned ProgressiveDecoder instance
  // is destroyed or the decoder calls |handler->OnHeaderBlockEnd()|.
  std::unique_ptr<ProgressiveDecoder> DecodeHeaderBlock(
      HeadersHandlerInterface* handler);

 private:
  QpackHeaderTable header_table_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_DECODER_H_
