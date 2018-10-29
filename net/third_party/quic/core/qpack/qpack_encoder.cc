// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_encoder.h"

#include "base/logging.h"
#include "net/third_party/http2/hpack/huffman/hpack_huffman_encoder.h"
#include "net/third_party/http2/hpack/varint/hpack_varint_encoder.h"
#include "net/third_party/quic/core/qpack/qpack_constants.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_string_utils.h"

namespace quic {
namespace {

// An implementation of ProgressiveEncoder interface that encodes a single
// header block.
class QpackProgressiveEncoder : public spdy::HpackEncoder::ProgressiveEncoder {
 public:
  QpackProgressiveEncoder() = delete;
  QpackProgressiveEncoder(QpackHeaderTable* header_table,
                          const spdy::SpdyHeaderBlock* header_list);
  QpackProgressiveEncoder(const QpackProgressiveEncoder&) = delete;
  QpackProgressiveEncoder& operator=(const QpackProgressiveEncoder&) = delete;
  ~QpackProgressiveEncoder() = default;

  // Returns true iff more remains to encode.
  bool HasNext() const override;

  // Encodes up to |max_encoded_bytes| octets, appending to |output|.
  void Next(size_t max_encoded_bytes, QuicString* output) override;

 private:
  enum class State {
    // Every instruction starts encoding an integer on the first octet:
    // either an index or the length of the name string literal.
    kStart,
    kVarintResume,
    kVarintDone,
    // This might be followed by the name as a string literal.
    kNameString,
    // This might be followed by the length of the value.
    kValueStart,
    kValueLength,
    // This might be followed by the value as a string literal.
    kValueString
  };

  // One method for each state.  Some encode up to |max_encoded_bytes| octets,
  // appending to |output|.  Some only change internal state.
  size_t DoStart(size_t max_encoded_bytes, QuicString* output);
  size_t DoVarintResume(size_t max_encoded_bytes, QuicString* output);
  void DoVarintDone();
  size_t DoNameString(size_t max_encoded_bytes, QuicString* output);
  size_t DoValueStart(size_t max_encoded_bytes, QuicString* output);
  size_t DoValueLength(size_t max_encoded_bytes, QuicString* output);
  size_t DoValueString(size_t max_encoded_bytes, QuicString* output);

  const QpackHeaderTable* const header_table_;
  const spdy::SpdyHeaderBlock* const header_list_;
  spdy::SpdyHeaderBlock::const_iterator header_list_iterator_;
  State state_;
  http2::HpackVarintEncoder varint_encoder_;

  // The following variables are used to carry information between states
  // within a single header field.  That is, a value assigned while encoding one
  // header field shall never be used for encoding subsequent header fields.

  // True if the header field name is encoded as a string literal.
  bool literal_name_;

  // True if the header field value is encoded as a string literal.
  bool literal_value_;

  // While encoding a string literal, |string_to_write_| points to the substring
  // that remains to be written.  This may be a substring of a string owned by
  // |header_list_|, or a substring of |huffman_encoded_string_|.
  QuicStringPiece string_to_write_;

  // Storage for the Huffman encoded string literal to be written if Huffman
  // encoding is used.
  QuicString huffman_encoded_string_;
};

QpackProgressiveEncoder::QpackProgressiveEncoder(
    QpackHeaderTable* header_table,
    const spdy::SpdyHeaderBlock* header_list)
    : header_table_(header_table),
      header_list_(header_list),
      header_list_iterator_(header_list_->begin()),
      state_(State::kStart),
      literal_name_(false),
      literal_value_(false) {}

bool QpackProgressiveEncoder::HasNext() const {
  return header_list_iterator_ != header_list_->end();
}

void QpackProgressiveEncoder::Next(size_t max_encoded_bytes,
                                   QuicString* output) {
  DCHECK(HasNext());
  DCHECK_NE(0u, max_encoded_bytes);

  while (max_encoded_bytes > 0 && HasNext()) {
    size_t encoded_bytes = 0;

    switch (state_) {
      case State::kStart:
        encoded_bytes = DoStart(max_encoded_bytes, output);
        break;
      case State::kVarintResume:
        encoded_bytes = DoVarintResume(max_encoded_bytes, output);
        break;
      case State::kVarintDone:
        DoVarintDone();
        break;
      case State::kNameString:
        encoded_bytes = DoNameString(max_encoded_bytes, output);
        break;
      case State::kValueStart:
        encoded_bytes = DoValueStart(max_encoded_bytes, output);
        break;
      case State::kValueLength:
        encoded_bytes = DoValueLength(max_encoded_bytes, output);
        break;
      case State::kValueString:
        encoded_bytes = DoValueString(max_encoded_bytes, output);
        break;
    }

    DCHECK_LE(encoded_bytes, max_encoded_bytes);
    max_encoded_bytes -= encoded_bytes;
  }
}

size_t QpackProgressiveEncoder::DoStart(size_t max_encoded_bytes,
                                        QuicString* output) {
  QuicStringPiece name = header_list_iterator_->first;
  QuicStringPiece value = header_list_iterator_->second;
  size_t index;
  auto match_type = header_table_->FindHeaderField(name, value, &index);

  switch (match_type) {
    case QpackHeaderTable::MatchType::kNameAndValue:
      literal_name_ = false;
      literal_value_ = false;

      output->push_back(varint_encoder_.StartEncoding(
          kIndexedHeaderFieldOpcode | kIndexedHeaderFieldStaticBit,
          kIndexedHeaderFieldPrefixLength, index));

      break;
    case QpackHeaderTable::MatchType::kName:
      literal_name_ = false;
      literal_value_ = true;

      output->push_back(varint_encoder_.StartEncoding(
          kLiteralHeaderFieldNameReferenceOpcode |
              kLiteralHeaderFieldNameReferenceStaticBit,
          kLiteralHeaderFieldNameReferencePrefixLength, index));

      break;
    case QpackHeaderTable::MatchType::kNoMatch:
      literal_name_ = true;
      literal_value_ = true;

      http2::HuffmanEncode(name, &huffman_encoded_string_);

      // Use Huffman encoding if it cuts down on size.
      if (huffman_encoded_string_.size() < name.size()) {
        string_to_write_ = huffman_encoded_string_;
        output->push_back(varint_encoder_.StartEncoding(
            kLiteralHeaderFieldOpcode | kLiteralNameHuffmanMask,
            kLiteralHeaderFieldPrefixLength, string_to_write_.size()));
      } else {
        string_to_write_ = name;
        output->push_back(varint_encoder_.StartEncoding(
            kLiteralHeaderFieldOpcode, kLiteralHeaderFieldPrefixLength,
            string_to_write_.size()));
      }
      break;
  }

  state_ = varint_encoder_.IsEncodingInProgress() ? State::kVarintResume
                                                  : State::kVarintDone;
  return 1;
}

size_t QpackProgressiveEncoder::DoVarintResume(size_t max_encoded_bytes,
                                               QuicString* output) {
  DCHECK(varint_encoder_.IsEncodingInProgress());

  const size_t encoded_bytes =
      varint_encoder_.ResumeEncoding(max_encoded_bytes, output);
  if (varint_encoder_.IsEncodingInProgress()) {
    DCHECK_EQ(encoded_bytes, max_encoded_bytes);
    return encoded_bytes;
  }

  DCHECK_LE(encoded_bytes, max_encoded_bytes);

  state_ = State::kVarintDone;
  return encoded_bytes;
}

void QpackProgressiveEncoder::DoVarintDone() {
  if (literal_name_) {
    state_ = State::kNameString;
    return;
  }

  if (literal_value_) {
    state_ = State::kValueStart;
    return;
  }

  ++header_list_iterator_;
  state_ = State::kStart;
}

size_t QpackProgressiveEncoder::DoNameString(size_t max_encoded_bytes,
                                             QuicString* output) {
  DCHECK(literal_name_);

  if (max_encoded_bytes < string_to_write_.size()) {
    const size_t encoded_bytes = max_encoded_bytes;
    QuicStrAppend(output, string_to_write_.substr(0, encoded_bytes));
    string_to_write_ = string_to_write_.substr(encoded_bytes);
    return encoded_bytes;
  }

  const size_t encoded_bytes = string_to_write_.size();
  QuicStrAppend(output, string_to_write_);
  state_ = State::kValueStart;
  return encoded_bytes;
}

size_t QpackProgressiveEncoder::DoValueStart(size_t max_encoded_bytes,
                                             QuicString* output) {
  DCHECK(literal_value_);

  string_to_write_ = header_list_iterator_->second;
  http2::HuffmanEncode(string_to_write_, &huffman_encoded_string_);

  // Use Huffman encoding if it cuts down on size.
  if (huffman_encoded_string_.size() < string_to_write_.size()) {
    string_to_write_ = huffman_encoded_string_;
    output->push_back(varint_encoder_.StartEncoding(kLiteralValueHuffmanMask,
                                                    kLiteralValuePrefixLength,
                                                    string_to_write_.size()));
  } else {
    output->push_back(varint_encoder_.StartEncoding(
        kLiteralValueWithoutHuffmanEncoding, kLiteralValuePrefixLength,
        string_to_write_.size()));
  }

  state_ = varint_encoder_.IsEncodingInProgress() ? State::kValueLength
                                                  : State::kValueString;
  return 1;
}

size_t QpackProgressiveEncoder::DoValueLength(size_t max_encoded_bytes,
                                              QuicString* output) {
  DCHECK(literal_value_);
  DCHECK(varint_encoder_.IsEncodingInProgress());

  const size_t encoded_bytes =
      varint_encoder_.ResumeEncoding(max_encoded_bytes, output);
  if (varint_encoder_.IsEncodingInProgress()) {
    DCHECK_EQ(encoded_bytes, max_encoded_bytes);
  } else {
    DCHECK_LE(encoded_bytes, max_encoded_bytes);
    state_ = State::kValueString;
  }

  return encoded_bytes;
}

size_t QpackProgressiveEncoder::DoValueString(size_t max_encoded_bytes,
                                              QuicString* output) {
  DCHECK(literal_value_);

  if (max_encoded_bytes < string_to_write_.size()) {
    const size_t encoded_bytes = max_encoded_bytes;
    QuicStrAppend(output, string_to_write_.substr(0, encoded_bytes));
    string_to_write_ = string_to_write_.substr(encoded_bytes);
    return encoded_bytes;
  }

  const size_t encoded_bytes = string_to_write_.size();
  QuicStrAppend(output, string_to_write_);
  state_ = State::kStart;
  ++header_list_iterator_;
  return encoded_bytes;
}

}  // namespace

std::unique_ptr<spdy::HpackEncoder::ProgressiveEncoder>
QpackEncoder::EncodeHeaderList(const spdy::SpdyHeaderBlock* header_list) {
  return std::make_unique<QpackProgressiveEncoder>(&header_table_, header_list);
}

}  // namespace quic
