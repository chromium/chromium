// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_decoder.h"

#include "base/logging.h"
#include "net/third_party/http2/decoder/decode_buffer.h"
#include "net/third_party/http2/decoder/decode_status.h"
#include "net/third_party/quic/core/qpack/qpack_constants.h"

namespace quic {

QpackDecoder::ProgressiveDecoder::ProgressiveDecoder(
    QpackHeaderTable* header_table,
    QpackDecoder::HeadersHandlerInterface* handler)
    : header_table_(header_table),
      handler_(handler),
      state_(State::kStart),
      decoding_(true),
      error_detected_(false),
      literal_name_(false),
      literal_value_(false),
      name_length_(0),
      value_length_(0),
      is_huffman_(false) {}

void QpackDecoder::ProgressiveDecoder::Decode(QuicStringPiece data) {
  DCHECK(decoding_);

  if (data.empty() || error_detected_) {
    return;
  }

  while (true) {
    size_t bytes_consumed = 0;

    switch (state_) {
      case State::kStart:
        bytes_consumed = DoStart(data);
        break;
      case State::kVarintResume:
        bytes_consumed = DoVarintResume(data);
        break;
      case State::kVarintDone:
        DoVarintDone();
        break;
      case State::kReadName:
        bytes_consumed = DoReadName(data);
        break;
      case State::kDecodeName:
        DoDecodeName();
        break;
      case State::kValueLengthStart:
        bytes_consumed = DoValueLengthStart(data);
        break;
      case State::kValueLengthResume:
        bytes_consumed = DoValueLengthResume(data);
        break;
      case State::kValueLengthDone:
        DoValueLengthDone();
        break;
      case State::kReadValue:
        bytes_consumed = DoReadValue(data);
        break;
      case State::kDecodeValue:
        DoDecodeValue();
        break;
      case State::kDone:
        DoDone();
        break;
    }

    if (error_detected_) {
      return;
    }

    DCHECK_LE(bytes_consumed, data.size());

    data = QuicStringPiece(data.data() + bytes_consumed,
                           data.size() - bytes_consumed);

    // Stop processing if no more data but next state would require it.
    if (data.empty() && (state_ != State::kVarintDone) &&
        (state_ != State::kDecodeName) && (state_ != State::kValueLengthDone) &&
        (state_ != State::kDecodeValue) && (state_ != State::kDone)) {
      return;
    }
  }
}

void QpackDecoder::ProgressiveDecoder::EndHeaderBlock() {
  DCHECK(decoding_);
  decoding_ = false;

  if (error_detected_) {
    return;
  }

  if (state_ == State::kStart) {
    handler_->OnDecodingCompleted();
  } else {
    OnError("Incomplete header block.");
  }
}

size_t QpackDecoder::ProgressiveDecoder::DoStart(QuicStringPiece data) {
  DCHECK(!data.empty());

  size_t prefix_length = 0;
  if ((data[0] & kIndexedHeaderFieldOpcodeMask) == kIndexedHeaderFieldOpcode) {
    if ((data[0] & kIndexedHeaderFieldStaticBit) !=
        kIndexedHeaderFieldStaticBit) {
      // TODO(bnc): Implement.
      OnError("Indexed Header Field with dynamic entry not implemented.");
      return 0;
    }
    prefix_length = kIndexedHeaderFieldPrefixLength;
    literal_name_ = false;
    literal_value_ = false;
  } else if ((data[0] & kIndexedHeaderFieldPostBaseOpcodeMask) ==
             kIndexedHeaderFieldPostBaseOpcode) {
    // TODO(bnc): Implement.
    OnError("Indexed Header Field With Post-Base Index not implemented.");
    return 0;
  } else if ((data[0] & kLiteralHeaderFieldNameReferenceOpcodeMask) ==
             kLiteralHeaderFieldNameReferenceOpcode) {
    if ((data[0] & kLiteralHeaderFieldNameReferenceStaticBit) !=
        kLiteralHeaderFieldNameReferenceStaticBit) {
      // TODO(bnc): Implement.
      OnError(
          "Literal Header Field With Name Reference with dynamic entry not "
          "implemented.");
      return 0;
    }
    prefix_length = kLiteralHeaderFieldNameReferencePrefixLength;
    literal_name_ = false;
    literal_value_ = true;
  } else if ((data[0] & kLiteralHeaderFieldPostBaseOpcodeMask) ==
             kLiteralHeaderFieldPostBaseOpcode) {
    // TODO(bnc): Implement.
    OnError(
        "Literal Header Field With Post-Base Name Reference not implemented.");
    return 0;
  } else {
    DCHECK_EQ(kLiteralHeaderFieldOpcode,
              data[0] & kLiteralHeaderFieldOpcodeMask);

    is_huffman_ =
        (data[0] & kLiteralNameHuffmanMask) == kLiteralNameHuffmanMask;
    prefix_length = kLiteralHeaderFieldPrefixLength;
    literal_name_ = true;
    literal_value_ = true;
  }

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], prefix_length, &buffer);

  size_t bytes_consumed = 1 + buffer.Offset();
  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kVarintDone;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeInProgress:
      state_ = State::kVarintResume;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeError:
      OnError("Encoded integer too large.");
      return bytes_consumed;
  }
}

size_t QpackDecoder::ProgressiveDecoder::DoVarintResume(QuicStringPiece data) {
  DCHECK(!data.empty());

  http2::DecodeBuffer buffer(data);
  http2::DecodeStatus status = varint_decoder_.Resume(&buffer);

  size_t bytes_consumed = buffer.Offset();
  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kVarintDone;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK_EQ(bytes_consumed, data.size());
      DCHECK(buffer.Empty());
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeError:
      OnError("Encoded integer too large.");
      return bytes_consumed;
  }
}

void QpackDecoder::ProgressiveDecoder::DoVarintDone() {
  if (literal_name_) {
    // TODO(bnc): Impose a sensible limit on length to avoid memory exhaustion
    // attacks.
    name_length_ = varint_decoder_.value();
    name_.clear();
    name_.reserve(name_length_);
    // Do not handle empty names differently.  (They are probably forbidden by
    // higher layers, but it is not enforced in this class.)  If there is no
    // more data to read, then processing stalls, but the instruction is not
    // complete without the value, so OnHeaderDecoded() could not be called yet
    // anyway.
    state_ = State::kReadName;
    return;
  }

  auto entry = header_table_->LookupEntry(varint_decoder_.value());
  if (!entry) {
    OnError("Invalid static table index.");
    return;
  }

  if (literal_value_) {
    name_.assign(entry->name().data(), entry->name().size());
    state_ = State::kValueLengthStart;
    return;
  }

  // Call OnHeaderDecoded() here instead of changing to State::kDone
  // to prevent copying two strings.
  handler_->OnHeaderDecoded(entry->name(), entry->value());
  state_ = State::kStart;
}

size_t QpackDecoder::ProgressiveDecoder::DoReadName(QuicStringPiece data) {
  DCHECK(!data.empty());
  // |name_length_| might be zero.
  DCHECK_LE(name_.size(), name_length_);

  size_t bytes_consumed = std::min(name_length_ - name_.size(), data.size());
  name_.append(data.data(), bytes_consumed);

  DCHECK_LE(name_.size(), name_length_);
  if (name_.size() == name_length_) {
    state_ = State::kDecodeName;
  }

  return bytes_consumed;
}

void QpackDecoder::ProgressiveDecoder::DoDecodeName() {
  DCHECK_EQ(name_.size(), name_length_);

  if (is_huffman_) {
    huffman_decoder_.Reset();
    // HpackHuffmanDecoder::Decode() cannot perform in-place decoding.
    QuicString decoded_name;
    huffman_decoder_.Decode(name_, &decoded_name);
    if (!huffman_decoder_.InputProperlyTerminated()) {
      OnError("Error in Huffman-encoded name.");
      return;
    }
    name_ = decoded_name;
  }

  state_ = State::kValueLengthStart;
}

size_t QpackDecoder::ProgressiveDecoder::DoValueLengthStart(
    QuicStringPiece data) {
  DCHECK(!data.empty());
  DCHECK(literal_value_);

  is_huffman_ =
      (data[0] & kLiteralValueHuffmanMask) == kLiteralValueHuffmanMask;

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], kLiteralValuePrefixLength, &buffer);

  size_t bytes_consumed = 1 + buffer.Offset();
  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kValueLengthDone;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeInProgress:
      state_ = State::kValueLengthResume;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeError:
      OnError("ValueLen too large.");
      return bytes_consumed;
  }
}

size_t QpackDecoder::ProgressiveDecoder::DoValueLengthResume(
    QuicStringPiece data) {
  DCHECK(!data.empty());

  http2::DecodeBuffer buffer(data);
  http2::DecodeStatus status = varint_decoder_.Resume(&buffer);

  size_t bytes_consumed = buffer.Offset();
  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kValueLengthDone;
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK_EQ(bytes_consumed, data.size());
      DCHECK(buffer.Empty());
      return bytes_consumed;
    case http2::DecodeStatus::kDecodeError:
      OnError("ValueLen too large.");
      return bytes_consumed;
  }
}

void QpackDecoder::ProgressiveDecoder::DoValueLengthDone() {
  value_.clear();
  // TODO(bnc): Impose a sensible limit on length to avoid memory exhaustion
  // attacks.
  value_length_ = varint_decoder_.value();

  // If value is empty, skip DoReadValue() and DoDecodeValue() and jump directly
  // to DoDone().  This is so that OnHeaderDecoded() is called even if there is
  // no more data.
  if (value_length_ == 0) {
    state_ = State::kDone;
    return;
  }

  value_.reserve(value_length_);
  state_ = State::kReadValue;
}

size_t QpackDecoder::ProgressiveDecoder::DoReadValue(QuicStringPiece data) {
  DCHECK(!data.empty());
  DCHECK_LT(0u, value_length_);
  DCHECK_LT(value_.size(), value_length_);

  size_t bytes_consumed = std::min(value_length_ - value_.size(), data.size());
  value_.append(data.data(), bytes_consumed);

  DCHECK_LE(value_.size(), value_length_);
  if (value_.size() == value_length_) {
    state_ = State::kDecodeValue;
  }
  return bytes_consumed;
}

void QpackDecoder::ProgressiveDecoder::DoDecodeValue() {
  DCHECK_EQ(value_.size(), value_length_);

  if (is_huffman_) {
    huffman_decoder_.Reset();
    // HpackHuffmanDecoder::Decode() cannot perform in-place decoding.
    QuicString decoded_value;
    huffman_decoder_.Decode(value_, &decoded_value);
    if (!huffman_decoder_.InputProperlyTerminated()) {
      OnError("Error in Huffman-encoded value.");
      return;
    }
    value_ = decoded_value;
  }

  state_ = State::kDone;
}

void QpackDecoder::ProgressiveDecoder::DoDone() {
  handler_->OnHeaderDecoded(name_, value_);
  state_ = State::kStart;
}

void QpackDecoder::ProgressiveDecoder::OnError(QuicStringPiece error_message) {
  DCHECK(!error_detected_);

  error_detected_ = true;
  handler_->OnDecodingErrorDetected(error_message);
}

std::unique_ptr<QpackDecoder::ProgressiveDecoder>
QpackDecoder::DecodeHeaderBlock(
    QpackDecoder::HeadersHandlerInterface* handler) {
  return std::make_unique<ProgressiveDecoder>(&header_table_, handler);
}

}  // namespace quic
