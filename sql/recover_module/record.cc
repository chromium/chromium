// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/record.h"

#include <cstddef>
#include <limits>
#include <type_traits>

#include "sql/recover_module/integers.h"
#include "sql/recover_module/payload.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace recover {

RecordReader::RecordReader(LeafPayloadReader* payload_reader, int column_count)
    : payload_reader_(payload_reader), column_count_(column_count) {
  DCHECK(payload_reader != nullptr);
  DCHECK_GT(column_count, 0);
  value_headers_.reserve(column_count);
}

RecordReader::~RecordReader() = default;

namespace {

// Value type indicating a null.
constexpr int kNullType = 0;
// Value type indicating a 1-byte signed integer.
constexpr int kInt1Type = 1;
// Value type indicating a 2-byte signed big-endian integer.
constexpr int kInt2Type = 2;
// Value type indicating a 3-byte signed big-endian integer.
constexpr int kInt3Type = 3;
// Value type indicating a 4-byte signed big-endian integer.
constexpr int kInt4Type = 4;
// Value type indicating a 6-byte signed big-endian integer.
constexpr int kInt6Type = 5;
// Value type indicating an 8-byte signed big-endian integer.
constexpr int kInt8Type = 6;
// Value type indicating a big-endian IEEE 754 64-bit floating point number.
constexpr int kDoubleType = 7;
// Value type indicating the integer 0 (zero).
constexpr int kIntZeroType = 8;
// Value type indicating the integer 1 (one).
constexpr int kIntOneType = 9;
// Value types greater than or equal to this indicate blobs or text.
constexpr int kMinBlobOrStringType = 12;

// The return value of ParseHeaderType below.
struct ParsedHeaderType {
  // True for the special value used to communicate a parsing error.
  bool IsInvalid() const {
    return type == ValueType::kNull && has_inline_value;
  }

  const ValueType type;
  const int64_t size;
  const int8_t inline_value;
  const bool has_inline_value;
};

// Decodes a type identifier in a SQLite record header.
//
// The type identifier includes the type and the size.
//
// Returns {kNull, 1} when parsing fails. Null values never require any extra
// bytes, so this special return value will never occur during normal
// processing.
ParsedHeaderType ParseHeaderType(int64_t encoded_type) {
  static constexpr int8_t kNoInlineValue = 0;

  if (encoded_type == kNullType)
    return {ValueType::kNull, 0, kNoInlineValue, false};
  if (encoded_type == kInt1Type)
    return {ValueType::kInteger, 1, kNoInlineValue, false};
  if (encoded_type == kInt2Type)
    return {ValueType::kInteger, 2, kNoInlineValue, false};
  if (encoded_type == kInt3Type)
    return {ValueType::kInteger, 3, kNoInlineValue, false};
  if (encoded_type == kInt4Type)
    return {ValueType::kInteger, 4, kNoInlineValue, false};
  if (encoded_type == kInt6Type)
    return {ValueType::kInteger, 6, kNoInlineValue, false};
  if (encoded_type == kInt8Type)
    return {ValueType::kInteger, 8, kNoInlineValue, false};
  if (encoded_type == kDoubleType)
    return {ValueType::kFloat, 8, kNoInlineValue, false};
  if (encoded_type == kIntZeroType)
    return {ValueType::kInteger, 0, 0, true};
  if (encoded_type == kIntOneType)
    return {ValueType::kInteger, 0, 1, true};

  if (encoded_type < kMinBlobOrStringType) {
    // Types between |kIntOneType| and |kMinBlobOrStringType| are reserved for
    // SQLite internal usage, and should not appear in persistent databases.
    // This shows database corruption.
    return {ValueType::kNull, 0, kNoInlineValue, true};
  }

  // Blobs and texts take alternating numbers starting at 12.
  encoded_type -= kMinBlobOrStringType;
  const ValueType value_type =
      (encoded_type & 1) == 0 ? ValueType::kBlob : ValueType::kText;
  const int64_t value_size = encoded_type >> 1;
  return {value_type, value_size, kNoInlineValue, false};
}

}  // namespace

bool RecordReader::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The size of |value_headers_| is used in DCHECKs to track whether
  // Initialize() succeeded.
  value_headers_.clear();

  int64_t next_value_offset = InitializeHeaderBuffer();
  if (next_value_offset == 0)
    return false;

  const uint8_t* header_pointer = header_buffer_.data();
  const uint8_t* header_end = header_buffer_.data() + header_buffer_.size();

  for (int i = 0; i < column_count_; ++i) {
    int64_t encoded_type;
    if (header_pointer == header_end) {
      // SQLite versions built with SQLITE_ENABLE_NULL_TRIM don't store trailing
      // null type IDs in the header.
      encoded_type = kNullType;
    } else {
      std::tie(encoded_type, header_pointer) =
          ParseVarint(header_pointer, header_end);
    }

    ParsedHeaderType parsed_type = ParseHeaderType(encoded_type);
    if (parsed_type.IsInvalid()) {
      // Parsing failed. The record is corrupted.
      return false;
    }
    value_headers_.emplace_back(next_value_offset, parsed_type.size,
                                parsed_type.type, parsed_type.inline_value,
                                parsed_type.has_inline_value);

    next_value_offset += parsed_type.size;
  }

  DCHECK_EQ(value_headers_.size(), static_cast<size_t>(column_count_));
  return true;
}

ValueType RecordReader::GetValueType(int column_index) const {
  DCHECK(IsInitialized());
  DCHECK_GE(column_index, 0);
  DCHECK_LT(static_cast<size_t>(column_index), value_headers_.size());

  return value_headers_[column_index].type;
}

namespace {

// Deallocates buffers passed to sqlite3_result_{blob,text}64().
void ValueBytesDeleter(void* buffer) {
  DCHECK(buffer != nullptr);
  uint8_t* value_bytes = reinterpret_cast<uint8_t*>(buffer);
  delete[] value_bytes;
}

}  // namespace

bool RecordReader::ReadValue(int column_index,
                             sqlite3_context* receiver) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());
  DCHECK_GE(column_index, 0);
  DCHECK_LT(static_cast<size_t>(column_index), value_headers_.size());
  DCHECK(receiver != nullptr);

  const ValueHeader& header = value_headers_[column_index];

  const int64_t offset = header.offset;
  const int64_t size = header.size;

  if (header.type == ValueType::kNull) {
    DCHECK_EQ(size, 0);
    DCHECK(!header.has_inline_value);

    sqlite3_result_null(receiver);
    return true;
  }

  if (header.type == ValueType::kInteger) {
    if (header.has_inline_value) {
      sqlite3_result_int(receiver, header.inline_value);
      return true;
    }

    uint8_t value_bytes[8];
    DCHECK_GT(size, 0);
    DCHECK_LE(size, static_cast<int64_t>(sizeof(value_bytes)));
    // SQLite integers are big-endian, so the least significant bytes are at the
    // end of the integer's buffer.
    uint8_t* const first_read_byte = value_bytes + 8 - size;
    if (!payload_reader_->ReadPayload(offset, size, first_read_byte))
      return false;

    // Sign-extend the number.
    const uint8_t sign_byte = (*first_read_byte & 0x80) ? 0xff : 0;
    for (uint8_t* sign_extended_byte = &value_bytes[0];
         sign_extended_byte < first_read_byte; ++sign_extended_byte) {
      *sign_extended_byte = sign_byte;
    }

    const int64_t value = LoadBigEndianInt64(value_bytes);
    sqlite3_result_int64(receiver, value);
    return true;
  }

  if (header.type == ValueType::kFloat) {
    DCHECK_EQ(header.size, static_cast<int64_t>(sizeof(double)));
    DCHECK(!header.has_inline_value);

    union {
      double fp;
      int64_t integer;
      uint8_t bytes[8];
    } value;
    static_assert(sizeof(double) == 8,
                  "double is not the correct type to represent SQLite floats");
    if (!payload_reader_->ReadPayload(header.offset, sizeof(double),
                                      reinterpret_cast<uint8_t*>(&value))) {
      return false;
    }
    // SQLite's doubles are big-endian.
    value.integer = LoadBigEndianInt64(value.bytes);
    sqlite3_result_double(receiver, value.fp);
    return true;
  }

  if (header.type == ValueType::kBlob || header.type == ValueType::kText) {
    DCHECK_GE(header.size, 0);
    DCHECK(!header.has_inline_value);

    uint8_t* const value_bytes = new uint8_t[size];
    if (!payload_reader_->ReadPayload(offset, size, value_bytes)) {
      delete[] value_bytes;
      return false;
    }
    if (header.type == ValueType::kBlob) {
      sqlite3_result_blob64(receiver, value_bytes, static_cast<uint64_t>(size),
                            &ValueBytesDeleter);
    } else {
      DCHECK_EQ(header.type, ValueType::kText);

      const unsigned char encoding = SQLITE_UTF8;
      sqlite3_result_text64(receiver, reinterpret_cast<char*>(value_bytes),
                            static_cast<uint64_t>(size), &ValueBytesDeleter,
                            encoding);
    }
    return true;
  }

  NOTREACHED() << "Invalid value type";
  return false;
}

void RecordReader::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  value_headers_.clear();
}

int64_t RecordReader::InitializeHeaderBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const uint8_t* const inline_payload_start =
      payload_reader_->ReadInlinePayload();
  if (inline_payload_start == nullptr) {
    // Read failure.
    return 0;
  }

  const int64_t inline_payload_size = payload_reader_->inline_payload_size();
  const uint8_t* const inline_payload_end =
      inline_payload_start + inline_payload_size;
  int64_t header_size;
  const uint8_t* payload_header_start;
  std::tie(header_size, payload_header_start) =
      ParseVarint(inline_payload_start, inline_payload_end);

  if (header_size < 0 || header_size > payload_reader_->payload_size()) {
    // The header is bigger than the entire record. This record is corrupted.
    return 0;
  }

  int header_size_size = payload_header_start - inline_payload_start;
  static_assert(std::numeric_limits<int>::max() > kMaxVarintSize,
                "The |header_size_size| computation above may overflow");

  // The header size varint is included in the header size computation.
  const int64_t header_data_size = header_size - header_size_size;
  header_buffer_.resize(header_data_size);
  if (!payload_reader_->ReadPayload(header_size_size, header_data_size,
                                    header_buffer_.data())) {
    // Read failure.
    return 0;
  }

  return header_size;
}

}  // namespace recover
}  // namespace sql