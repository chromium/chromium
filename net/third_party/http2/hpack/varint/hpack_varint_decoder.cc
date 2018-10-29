// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/http2/hpack/varint/hpack_varint_decoder.h"

#include "net/third_party/http2/platform/api/http2_string_utils.h"

namespace http2 {

DecodeStatus HpackVarintDecoder::Start(uint8_t prefix_value,
                                       uint8_t prefix_length,
                                       DecodeBuffer* db) {
  DCHECK_LE(3u, prefix_length);
  DCHECK_LE(prefix_length, 7u);

  // |prefix_mask| defines the sequence of low-order bits of the first byte
  // that encode the prefix of the value. It is also the marker in those bits
  // of the first byte indicating that at least one extension byte is needed.
  const uint8_t prefix_mask = (1 << prefix_length) - 1;

  // Ignore the bits that aren't a part of the prefix of the varint.
  value_ = prefix_value & prefix_mask;

  if (value_ < prefix_mask) {
    MarkDone();
    return DecodeStatus::kDecodeDone;
  }

  offset_ = 0;
  return Resume(db);
}

DecodeStatus HpackVarintDecoder::StartExtended(uint8_t prefix_length,
                                               DecodeBuffer* db) {
  DCHECK_LE(3u, prefix_length);
  DCHECK_LE(prefix_length, 7u);

  value_ = (1 << prefix_length) - 1;
  offset_ = 0;
  return Resume(db);
}

DecodeStatus HpackVarintDecoder::Resume(DecodeBuffer* db) {
  const uint32_t kMaxOffset = 28;
  CheckNotDone();
  do {
    if (db->Empty()) {
      return DecodeStatus::kDecodeInProgress;
    }
    uint8_t byte = db->DecodeUInt8();
    if (offset_ == kMaxOffset && byte != 0)
      break;
    value_ += (byte & 0x7f) << offset_;
    if ((byte & 0x80) == 0) {
      MarkDone();
      return DecodeStatus::kDecodeDone;
    }
    offset_ += 7;
  } while (offset_ <= kMaxOffset);
  DLOG(WARNING) << "Variable length int encoding is too large or too long. "
                << DebugString();
  MarkDone();
  return DecodeStatus::kDecodeError;
}

uint32_t HpackVarintDecoder::value() const {
  CheckDone();
  return value_;
}

void HpackVarintDecoder::set_value(uint32_t v) {
  MarkDone();
  value_ = v;
}

Http2String HpackVarintDecoder::DebugString() const {
  return Http2StrCat("HpackVarintDecoder(value=", value_, ", offset=", offset_,
                     ")");
}

DecodeStatus HpackVarintDecoder::StartForTest(uint8_t prefix_value,
                                              uint8_t prefix_length,
                                              DecodeBuffer* db) {
  return Start(prefix_value, prefix_length, db);
}

DecodeStatus HpackVarintDecoder::StartExtendedForTest(uint8_t prefix_length,
                                                      DecodeBuffer* db) {
  return StartExtended(prefix_length, db);
}

DecodeStatus HpackVarintDecoder::ResumeForTest(DecodeBuffer* db) {
  return Resume(db);
}

}  // namespace http2
