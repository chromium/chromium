// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_BTREE_H_
#define SQL_RECOVER_MODULE_BTREE_H_

#include <cstdint>

#include "base/logging.h"
#include "base/sequence_checker.h"

namespace sql {
namespace recover {

class DatabasePageReader;

// Streaming decoder for inner pages in SQLite table B-trees.
//
// The decoder outputs the page IDs of the inner page's children pages.
//
// An instance can only be used to decode a single page. Instances are not
// thread-safe.
class InnerPageDecoder {
 public:
  // Creates a decoder for a DatabasePageReader's last read page.
  //
  // |db_reader| must have been used to read an inner page of a table B-tree.
  // |db_reader| must outlive this instance.
  explicit InnerPageDecoder(DatabasePageReader* db_reader) noexcept;
  ~InnerPageDecoder() noexcept = default;

  InnerPageDecoder(const InnerPageDecoder&) = delete;
  InnerPageDecoder& operator=(const InnerPageDecoder&) = delete;

  // The ID of the database page decoded by this instance.
  int page_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return page_id_;
  }

  // Returns true iff TryAdvance() may be called.
  bool CanAdvance() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // The <= below is not a typo. Inner nodes store the right-most child
    // pointer in their headers, so their child count is (cell_count + 1).
    return next_read_index_ <= cell_count_;
  }

  // Advances the reader and returns the last read value.
  //
  // May return an invalid page ID if database was corrupted or the read failed.
  // The caller must use DatabasePageReader::IsValidPageId() to verify the
  // returned page ID. The caller should continue attempting to read as long as
  // CanAdvance() returns true.
  int TryAdvance();

  // True if the given reader may point to an inner page in a table B-tree.
  //
  // The last ReadPage() call on |db_reader| must have succeeded.
  static bool IsOnValidPage(DatabasePageReader* db_reader);

 private:
  // Returns the number of cells in the B-tree page.
  //
  // Checks for database corruption. The caller can assume that the cell pointer
  // array with the returned size will not extend past the page buffer.
  static int ComputeCellCount(DatabasePageReader* db_reader);

  // The number of the B-tree page this reader is reading.
  const int page_id_;
  // Used to read the tree page.
  //
  // Raw pointer usage is acceptable because this instance's owner is expected
  // to ensure that the DatabasePageReader outlives this.
  DatabasePageReader* const db_reader_;
  // Caches the ComputeCellCount() value for this reader's page.
  const int cell_count_ = ComputeCellCount(db_reader_);

  // The reader's cursor state.
  //
  // Each B-tree page has a header and many cells. In an inner B-tree page, each
  // cell points to a child page, and the header points to the last child page.
  // So, an inner page with N cells has N+1 children, and |next_read_index_|
  // takes values between 0 and |cell_count_| + 1.
  int next_read_index_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Streaming decoder for leaf pages in SQLite table B-trees.
//
// Conceptually, the decoder outputs (rowid, record size, record offset) tuples
// for all the values stored in the leaf page. The tuple members can be accessed
// via last_record_{rowid, size, offset}() methods.
//
// An instance can only be used to decode a single page. Instances are not
// thread-safe.
class LeafPageDecoder {
 public:
  // Creates a decoder for a DatabasePageReader's last read page.
  //
  // |db_reader| must have been used to read an inner page of a table B-tree.
  // |db_reader| must outlive this instance.
  explicit LeafPageDecoder(DatabasePageReader* db_reader) noexcept;
  ~LeafPageDecoder() noexcept = default;

  LeafPageDecoder(const LeafPageDecoder&) = delete;
  LeafPageDecoder& operator=(const LeafPageDecoder&) = delete;

  // The rowid of the most recent record read by TryAdvance().
  //
  // Must only be called after a successful call to TryAdvance().
  int64_t last_record_rowid() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(last_record_size_ != 0)
        << "TryAdvance() not called / did not succeed";
    return last_record_rowid_;
  }

  // The size of the most recent record read by TryAdvance().
  //
  // Must only be called after a successful call to TryAdvance().
  int64_t last_record_size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(last_record_size_ != 0)
        << "TryAdvance() not called / did not succeed";
    return last_record_size_;
  }

  // The page offset of the most recent record read by TryAdvance().
  //
  // Must only be called after a successful call to TryAdvance().
  int64_t last_record_offset() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(last_record_size_ != 0)
        << "TryAdvance() not called / did not succeed";
    return last_record_offset_;
  }

  // Returns true iff TryAdvance() may be called.
  bool CanAdvance() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return next_read_index_ < cell_count_;
  }

  // Advances the reader and returns the last read value.
  //
  // Returns false if the read fails. The caller should continue attempting to
  // read as long as CanAdvance() returns true.
  bool TryAdvance();

  // True if the given reader may point to an inner page in a table B-tree.
  //
  // The last ReadPage() call on |db_reader| must have succeeded.
  static bool IsOnValidPage(DatabasePageReader* db_reader);

 private:
  // Returns the number of cells in the B-tree page.
  //
  // Checks for database corruption. The caller can assume that the cell pointer
  // array with the returned size will not extend past the page buffer.
  static int ComputeCellCount(DatabasePageReader* db_reader);

  // The number of the B-tree page this reader is reading.
  const int64_t page_id_;
  // Used to read the tree page.
  //
  // Raw pointer usage is acceptable because this instance's owner is expected
  // to ensure that the DatabasePageReader outlives this.
  DatabasePageReader* const db_reader_;
  // Caches the ComputeCellCount() value for this reader's page.
  const int cell_count_ = ComputeCellCount(db_reader_);

  // The reader's cursor state.
  //
  // Each B-tree cell contains a value. So, this member takes values in
  // [0, cell_count_).
  int next_read_index_ = 0;

  int64_t last_record_size_ = 0;
  int64_t last_record_rowid_ = 0;
  int last_record_offset_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_BTREE_H_
