// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_encoder_stream_sender.h"

#include "net/third_party/http2/hpack/huffman/hpack_huffman_encoder.h"
#include "net/third_party/quic/core/qpack/qpack_constants.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

QpackEncoderStreamSender::QpackEncoderStreamSender(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

void QpackEncoderStreamSender::SendInsertWithNameReference(
    bool is_static,
    uint64_t name_index,
    QuicStringPiece value) {
  QuicString encoded_name_index_and_value_length;

  // Encode instruction code and name index.
  uint8_t high_bits = kInsertWithNameReferenceOpcode;
  if (is_static) {
    high_bits |= kInsertWithNameReferenceStaticBit;
  }
  encoded_name_index_and_value_length.push_back(varint_encoder_.StartEncoding(
      high_bits, kInsertWithNameReferenceNameIndexPrefixLength, name_index));
  if (varint_encoder_.IsEncodingInProgress()) {
    varint_encoder_.ResumeEncoding(kMaxExtensionBytesForVarintEncoding,
                                   &encoded_name_index_and_value_length);
  }
  DCHECK(!varint_encoder_.IsEncodingInProgress());

  // Huffman encode value string.
  QuicString huffman_encoded_value;
  http2::HuffmanEncode(value, &huffman_encoded_value);

  // Only use Huffman compression if it makes the string shorter.
  QuicStringPiece value_to_send;
  if (huffman_encoded_value.size() < value.size()) {
    value_to_send = huffman_encoded_value;
    high_bits = kLiteralValueHuffmanMask;
  } else {
    value_to_send = value;
    high_bits = kLiteralValueWithoutHuffmanEncoding;
  }

  // Encode value length.
  encoded_name_index_and_value_length.push_back(varint_encoder_.StartEncoding(
      high_bits, kLiteralValuePrefixLength, value_to_send.size()));
  if (varint_encoder_.IsEncodingInProgress()) {
    varint_encoder_.ResumeEncoding(kMaxExtensionBytesForVarintEncoding,
                                   &encoded_name_index_and_value_length);
  }
  DCHECK(!varint_encoder_.IsEncodingInProgress());

  // Write everything.
  DCHECK(!encoded_name_index_and_value_length.empty());
  delegate_->Write(encoded_name_index_and_value_length);
  if (!value_to_send.empty()) {
    delegate_->Write(value_to_send);
  }
}

void QpackEncoderStreamSender::SendInsertWithoutNameReference(
    QuicStringPiece name,
    QuicStringPiece value) {
  QuicString huffman_encoded_name;
  http2::HuffmanEncode(name, &huffman_encoded_name);

  // Only use Huffman compression if it makes the string shorter.
  QuicStringPiece name_to_send;
  uint8_t high_bits = kInsertWithoutNameReferenceOpcode;
  if (huffman_encoded_name.size() < name.size()) {
    name_to_send = huffman_encoded_name;
    high_bits |= kInsertWithoutNameReferenceNameHuffmanBit;
  } else {
    name_to_send = name;
  }

  // Encode instruction code and name length.
  QuicString encoded_name_length;
  encoded_name_length.push_back(varint_encoder_.StartEncoding(
      high_bits, kInsertWithoutNameReferenceNameLengthPrefixLength,
      name_to_send.size()));
  if (varint_encoder_.IsEncodingInProgress()) {
    varint_encoder_.ResumeEncoding(kMaxExtensionBytesForVarintEncoding,
                                   &encoded_name_length);
  }
  DCHECK(!varint_encoder_.IsEncodingInProgress());

  // Write instruction code, name length, and name.
  DCHECK(!encoded_name_length.empty());
  delegate_->Write(encoded_name_length);
  if (!name_to_send.empty()) {
    delegate_->Write(name_to_send);
  }

  // Huffman encode value string.
  QuicString huffman_encoded_value;
  http2::HuffmanEncode(value, &huffman_encoded_value);

  // Only use Huffman compression if it makes the string shorter.
  QuicStringPiece value_to_send;
  if (huffman_encoded_value.size() < value.size()) {
    value_to_send = huffman_encoded_value;
    high_bits = kLiteralValueHuffmanMask;
  } else {
    value_to_send = value;
    high_bits = kLiteralValueWithoutHuffmanEncoding;
  }

  // Encode value length.
  QuicString encoded_value_length;
  encoded_value_length.push_back(varint_encoder_.StartEncoding(
      high_bits, kLiteralValuePrefixLength, value_to_send.size()));
  if (varint_encoder_.IsEncodingInProgress()) {
    varint_encoder_.ResumeEncoding(kMaxExtensionBytesForVarintEncoding,
                                   &encoded_value_length);
  }
  DCHECK(!varint_encoder_.IsEncodingInProgress());

  // Write value length and value.
  DCHECK(!encoded_value_length.empty());
  delegate_->Write(encoded_value_length);
  if (!value_to_send.empty()) {
    delegate_->Write(value_to_send);
  }
}

void QpackEncoderStreamSender::SendDuplicate(uint64_t index) {
  QuicString data;
  data.push_back(varint_encoder_.StartEncoding(
      kDuplicateOpcode, kDuplicateIndexPrefixLength, index));
  if (varint_encoder_.IsEncodingInProgress()) {
    varint_encoder_.ResumeEncoding(kMaxExtensionBytesForVarintEncoding, &data);
  }
  DCHECK(!varint_encoder_.IsEncodingInProgress());

  DCHECK(!data.empty());
  delegate_->Write(data);
}

void QpackEncoderStreamSender::SendDynamicTableSizeUpdate(uint64_t max_size) {
  QuicString data;
  data.push_back(varint_encoder_.StartEncoding(
      kDynamicTableSizeUpdateOpcode, kDynamicTableSizeUpdateMaxSizePrefixLength,
      max_size));
  if (varint_encoder_.IsEncodingInProgress()) {
    varint_encoder_.ResumeEncoding(kMaxExtensionBytesForVarintEncoding, &data);
  }
  DCHECK(!varint_encoder_.IsEncodingInProgress());

  DCHECK(!data.empty());
  delegate_->Write(data);
}

}  // namespace quic
