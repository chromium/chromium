// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_encoder_stream_receiver.h"

#include "net/third_party/http2/decoder/decode_buffer.h"
#include "net/third_party/http2/decoder/decode_status.h"
#include "net/third_party/quic/core/qpack/qpack_constants.h"

namespace quic {

QpackEncoderStreamReceiver::QpackEncoderStreamReceiver(Delegate* delegate)
    : delegate_(delegate),
      state_(State::kStart),
      is_huffman_(false),
      is_static_(false),
      literal_name_(false),
      name_index_(0),
      name_length_(0),
      value_length_(0),
      error_detected_(false) {
  DCHECK(delegate_);
}

void QpackEncoderStreamReceiver::Decode(QuicStringPiece data) {
  while (!error_detected_) {
    size_t bytes_consumed = 0;

    switch (state_) {
      case State::kStart:
        bytes_consumed = DoStart(data);
        break;
      case State::kNameIndexOrLengthResume:
        bytes_consumed = DoNameIndexOrLengthResume(data);
        break;
      case State::kNameIndexOrLengthDone:
        DoNameIndexOrLengthDone();
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
      case State::kInsertDone:
        DoInsertDone();
        break;
      case State::kIndexResume:
        bytes_consumed = DoIndexResume(data);
        break;
      case State::kIndexDone:
        DoIndexDone();
        break;
      case State::kMaxSizeResume:
        bytes_consumed = DoMaxSizeResume(data);
        break;
      case State::kMaxSizeDone:
        DoMaxSizeDone();
        break;
    }

    DCHECK_LE(bytes_consumed, data.size());

    data = QuicStringPiece(data.data() + bytes_consumed,
                           data.size() - bytes_consumed);

    // Stop processing if no more data but next state would require it.
    if (data.empty() && (state_ != State::kNameIndexOrLengthDone) &&
        (state_ != State::kDecodeName) && (state_ != State::kValueLengthDone) &&
        (state_ != State::kDecodeValue) && (state_ != State::kInsertDone) &&
        (state_ != State::kIndexDone) && (state_ != State::kMaxSizeDone)) {
      return;
    }
  }
}

size_t QpackEncoderStreamReceiver::DoStart(QuicStringPiece data) {
  DCHECK(!data.empty());

  size_t prefix_length;
  State state_varint_in_progress;
  State state_varint_done;
  if ((data[0] & kInsertWithNameReferenceOpcodeMask) ==
      kInsertWithNameReferenceOpcode) {
    is_static_ = (data[0] & kInsertWithNameReferenceStaticBit) ==
                 kInsertWithNameReferenceStaticBit;

    prefix_length = kInsertWithNameReferenceNameIndexPrefixLength;
    literal_name_ = false;
    state_varint_in_progress = State::kNameIndexOrLengthResume;
    state_varint_done = State::kNameIndexOrLengthDone;
  } else if ((data[0] & kInsertWithoutNameReferenceOpcodeMask) ==
             kInsertWithoutNameReferenceOpcode) {
    is_huffman_ = (data[0] & kInsertWithoutNameReferenceNameHuffmanBit) ==
                  kInsertWithoutNameReferenceNameHuffmanBit;
    prefix_length = kInsertWithoutNameReferenceNameLengthPrefixLength;
    literal_name_ = true;
    state_varint_in_progress = State::kNameIndexOrLengthResume;
    state_varint_done = State::kNameIndexOrLengthDone;
  } else if ((data[0] & kDuplicateOpcodeMask) == kDuplicateOpcode) {
    prefix_length = kDuplicateIndexPrefixLength;
    state_varint_in_progress = State::kIndexResume;
    state_varint_done = State::kIndexDone;
  } else {
    DCHECK_EQ(kDynamicTableSizeUpdateOpcode,
              data[0] & kDynamicTableSizeUpdateOpcodeMask);

    prefix_length = kDynamicTableSizeUpdateMaxSizePrefixLength;
    state_varint_in_progress = State::kMaxSizeResume;
    state_varint_done = State::kMaxSizeDone;
  }

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], prefix_length, &buffer);

  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = state_varint_done;
      break;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK(buffer.Empty());
      state_ = state_varint_in_progress;
      break;
    case http2::DecodeStatus::kDecodeError:
      OnError("Encoded integer too large.");
      break;
  }
  return 1 + buffer.Offset();
}

size_t QpackEncoderStreamReceiver::DoNameIndexOrLengthResume(
    QuicStringPiece data) {
  DCHECK(!data.empty());

  http2::DecodeBuffer buffer(data);
  http2::DecodeStatus status = varint_decoder_.Resume(&buffer);

  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kNameIndexOrLengthDone;
      break;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK(buffer.Empty());
      break;
    case http2::DecodeStatus::kDecodeError:
      OnError("Encoded integer too large.");
      break;
  }
  return buffer.Offset();
}

void QpackEncoderStreamReceiver::DoNameIndexOrLengthDone() {
  DCHECK(name_.empty());

  if (literal_name_) {
    name_length_ = varint_decoder_.value();
    name_.reserve(name_length_);
    // Do not handle empty names differently.  (They are probably forbidden by
    // higher layers, but it is not enforced in this class.)  If there is no
    // more data to read, then processing stalls, but the instruction is not
    // complete without the value, so OnInsertWithoutNameReference() could not
    // be called yet anyway.
    state_ = State::kReadName;
    return;
  }

  name_index_ = varint_decoder_.value();
  state_ = State::kValueLengthStart;
}

size_t QpackEncoderStreamReceiver::DoReadName(QuicStringPiece data) {
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

void QpackEncoderStreamReceiver::DoDecodeName() {
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

size_t QpackEncoderStreamReceiver::DoValueLengthStart(QuicStringPiece data) {
  DCHECK(!data.empty());

  is_huffman_ =
      (data[0] & kLiteralValueHuffmanMask) == kLiteralValueHuffmanMask;

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], kLiteralValuePrefixLength, &buffer);

  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kValueLengthDone;
      break;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK(buffer.Empty());
      state_ = State::kValueLengthResume;
      break;
    case http2::DecodeStatus::kDecodeError:
      OnError("ValueLen too large.");
      break;
  }
  return 1 + buffer.Offset();
}

size_t QpackEncoderStreamReceiver::DoValueLengthResume(QuicStringPiece data) {
  DCHECK(!data.empty());

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], kLiteralValuePrefixLength, &buffer);

  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kValueLengthDone;
      break;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK(buffer.Empty());
      break;
    case http2::DecodeStatus::kDecodeError:
      OnError("ValueLen too large.");
      break;
  }
  return 1 + buffer.Offset();
}

void QpackEncoderStreamReceiver::DoValueLengthDone() {
  DCHECK(value_.empty());

  value_length_ = varint_decoder_.value();

  // If value is empty, skip DoReadValue() and DoDecodeValue() and jump directly
  // to DoInsertDone().  This is so that OnInsertName*() is called even if there
  // is no more data.
  if (value_length_ == 0) {
    state_ = State::kInsertDone;
    return;
  }

  value_.reserve(value_length_);
  state_ = State::kReadValue;
}

size_t QpackEncoderStreamReceiver::DoReadValue(QuicStringPiece data) {
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

void QpackEncoderStreamReceiver::DoDecodeValue() {
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

  state_ = State::kInsertDone;
}

void QpackEncoderStreamReceiver::DoInsertDone() {
  if (literal_name_) {
    delegate_->OnInsertWithoutNameReference(name_, value_);
    name_.clear();
    value_.clear();
  } else {
    delegate_->OnInsertWithNameReference(is_static_, name_index_, value_);
    value_.clear();
  }

  state_ = State::kStart;
}

size_t QpackEncoderStreamReceiver::DoIndexResume(QuicStringPiece data) {
  DCHECK(!data.empty());

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], kLiteralValuePrefixLength, &buffer);

  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kIndexDone;
      break;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK(buffer.Empty());
      break;
    case http2::DecodeStatus::kDecodeError:
      OnError("Index too large.");
      break;
  }
  return 1 + buffer.Offset();
}

void QpackEncoderStreamReceiver::DoIndexDone() {
  delegate_->OnDuplicate(varint_decoder_.value());

  state_ = State::kStart;
}

size_t QpackEncoderStreamReceiver::DoMaxSizeResume(QuicStringPiece data) {
  DCHECK(!data.empty());

  http2::DecodeBuffer buffer(data.data() + 1, data.size() - 1);
  http2::DecodeStatus status =
      varint_decoder_.Start(data[0], kLiteralValuePrefixLength, &buffer);

  switch (status) {
    case http2::DecodeStatus::kDecodeDone:
      state_ = State::kMaxSizeDone;
      break;
    case http2::DecodeStatus::kDecodeInProgress:
      DCHECK(buffer.Empty());
      break;
    case http2::DecodeStatus::kDecodeError:
      OnError("Maximum table size too large.");
      break;
  }
  return 1 + buffer.Offset();
}

void QpackEncoderStreamReceiver::DoMaxSizeDone() {
  delegate_->OnDynamicTableSizeUpdate(varint_decoder_.value());

  state_ = State::kStart;
}

void QpackEncoderStreamReceiver::OnError(QuicStringPiece error_message) {
  DCHECK(!error_detected_);

  error_detected_ = true;
  delegate_->OnErrorDetected(error_message);
}

}  // namespace quic
