// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_RECORD_H_
#define SQL_RECOVER_MODULE_RECORD_H_

#include <cstdint>
#include <vector>

#include "base/sequence_checker.h"

struct sqlite3_context;

namespace sql {
namespace recover {

// The effective type of a column's value in a SQLite row.
enum class ValueType {
  kNull,
  kInteger,
  kFloat,
  kText,
  kBlob,
};

class LeafPayloadReader;

// Reads records from SQLite B-trees.
//
// Instances are designed to be reused for reading multiple records. Instances
// are not thread-safe.
//
// A record is a list of column values. SQLite uses "manifest typing", meaning
// that values don't necessarily match the column types declared in the
// table/index schema.
//
// Reading a record is started by calling Initialize(). Afterwards,
// GetValueType() can be used to validate the types of the record's values, and
// ReadValue() can be used to read the values into a SQLite user-defined
// function context.
class RecordReader {
 public:
  struct ValueHeader {
    explicit ValueHeader(int64_t offset,
                         int64_t size,
                         ValueType type,
                         int8_t inline_value,
                         bool has_inline_value)
        : offset(offset),
          size(size),
          type(type),
          inline_value(inline_value),
          has_inline_value(has_inline_value) {}

    // The position of the first byte used to encode the value, in the record.
    int64_t offset;
    // The number of bytes used to encode the value.
    int64_t size;
    // The SQLite type for the value.
    ValueType type;
    // The value encoded directly in the type, if |has_inline_value| is true.
    int8_t inline_value;
    // True if |inline_value| is defined.
    bool has_inline_value;
  };

  // Creates an uninitialized record reader from a SQLite table B-tree.
  //
  // |payload_reader_| must outlive this instance, and should always point to
  // leaf pages in the same tree. |column_count| must match the number of
  // columns in the table's schema.
  //
  // The underlying table should not be modified while the record is
  // initialized.
  explicit RecordReader(LeafPayloadReader* payload_reader_, int column_count);
  ~RecordReader();

  RecordReader(const RecordReader&) = delete;
  RecordReader& operator=(const RecordReader&) = delete;

  // Sets up the reader for a new payload.
  //
  // The LeafPayloadReader passed to the constructor must be focused on the
  // page containing the payload.
  //
  // This method must complete successfully before any other method on this
  // class can be called.
  bool Initialize();

  // True if the last call to Initialize succeeded.
  bool IsInitialized() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return value_headers_.size() == static_cast<size_t>(column_count_);
  }

  // The type of a value in the record. |column_index| is 0-based.
  ValueType GetValueType(int column_index) const;

  // Reads a value in the record into a SQLite user-defined function context.
  //
  // |column_index| is 0-based.
  //
  // The value is reported by calling a sqlite3_result_*() function on
  // |receiver|. SQLite's result-reporting API is documented at
  // https://www.sqlite.org/c3ref/result_blob.html
  //
  // Returns false if the reading value fails. This can happen if a value is
  // stored across overflow pages, and reading one of the overflow pages results
  // in an I/O error.
  bool ReadValue(int column_index, sqlite3_context* receiver) const;

  // Resets the reader.
  //
  // This method is idempotent. After it is called, IsInitialized() will return
  // false.
  void Reset();

 private:
  // Reads the record's header into |header_buffer_|.
  //
  // Returns the size of the record's header, or 0 (zero) in case of failure.
  // No valid record header has 0 bytes, because the record header includes at
  // least one varint.
  //
  // On success, |header_buffer_|'s size will be set correctly.
  int64_t InitializeHeaderBuffer();

  // Stores decoded type IDs from the record's header.
  std::vector<ValueHeader> value_headers_;

  // Stores the header of the record being read.
  //
  // The header is only used during Initialize(). This buffer is reused across
  // multiple Initialize() calls to reduce heap churn.
  std::vector<uint8_t> header_buffer_;

  // Brings the record's bytes from the SQLite database pages.
  //
  // Raw pointer usage is acceptable because this instance's owner is expected
  // to ensure that the LeafPayloadReader outlives this.
  LeafPayloadReader* const payload_reader_;

  // The number of columns in the table schema. No payload should have more than
  // this number of columns.
  const int column_count_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_RECORD_H_
