// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_PAGER_H_
#define SQL_RECOVER_MODULE_PAGER_H_

#include <cstdint>
#include <memory>

#include "base/logging.h"
#include "base/sequence_checker.h"

struct sqlite3_file;

namespace sql {
namespace recover {

class VirtualTable;

// Page reader for SQLite database files.
//
// Contains logic for retrying reads on I/O errors. Caches the last read page,
// to facilitate layering in higher-level code.
//
// Instances should be members of high-level constructs such as tables or
// cursors. Instances are not thread-safe.
class DatabasePageReader {
 public:
  // Guaranteed to be an invalid page number.
  static constexpr int kInvalidPageId = 0;

  // Minimum database page size supported by SQLite.
  static constexpr int kMinPageSize = 512;
  // Maximum database page size supported by SQLite.
  static constexpr int kMaxPageSize = 65536;

  // The size of the header at the beginning of a SQLite database file.
  static constexpr int kDatabaseHeaderSize = 100;

  // Minimum usable size of a SQLite database page.
  //
  // This differs from |kMinPageSize| because the first page in a SQLite
  // database starts with the database header. That page's header starts right
  // after the database header.
  static constexpr int kMinUsablePageSize = kMinPageSize - kDatabaseHeaderSize;

  // Maximum number of pages in a SQLite database.
  //
  // This is the maximum value of SQLITE_MAX_PAGE_COUNT plus 1, because page IDs
  // start at 1. The numerical value, which is the same as
  // std::numeric_limits<int32_t>::max() - 1, is quoted from
  // https://www.sqlite.org/limits.html.
  static constexpr int kMaxPageId = 2147483646 + 1;

  // Creates a reader that uses the SQLite VFS backing |table|.
  //
  // |table| must outlive this instance.
  explicit DatabasePageReader(VirtualTable* table);
  ~DatabasePageReader();

  DatabasePageReader(const DatabasePageReader&) = delete;
  DatabasePageReader& operator=(const DatabasePageReader&) = delete;

  // The page data read by the last ReadPage() call.
  //
  // The page data is undefined if the last ReadPage() call failed, or if
  // ReadPage() was never called.
  const uint8_t* page_data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_NE(page_id_, kInvalidPageId)
        << "Successful ReadPage() required before accessing pager state";
    return page_data_.get();
  }

  // The number of bytes in the page read by the last ReadPage() call.
  //
  // The result is guaranteed to be in [kMinUsablePageSize, kMaxPageSize].
  //
  // In general, pages have the same size. However, the first page in each
  // database is smaller, because it starts after the database header.
  //
  // The result is undefined if the last ReadPage() call failed, or if
  // ReadPage() was never called.
  int page_size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_NE(page_id_, kInvalidPageId)
        << "Successful ReadPage() required before accessing pager state";
    DCHECK_GE(page_size_, kMinUsablePageSize);
    DCHECK_LE(page_size_, kMaxPageSize);
    return page_size_;
  }

  // Returns the |page_id| argument for the last successful ReadPage() call.
  //
  // The result is undefined if the last ReadPage() call failed, or if
  // ReadPage() was never called.
  int page_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_NE(page_id_, kInvalidPageId)
        << "Successful ReadPage() required before accessing pager state";
    return page_id_;
  }

  // Reads a database page. Returns a SQLite status code.
  //
  // SQLite uses 1-based indexing for its page numbers.
  //
  // This method is idempotent, because it caches its result.
  int ReadPage(int page_id);

  // True if the given database page size is supported by SQLite.
  static constexpr bool IsValidPageSize(int page_size) noexcept {
    // SQLite page sizes must be powers of two.
    return page_size >= kMinPageSize && page_size <= kMaxPageSize &&
           (page_size & (page_size - 1)) == 0;
  }

  // True if the given number is a valid SQLite database page ID.
  //
  // Valid page IDs are positive 32-bit integers.
  static constexpr bool IsValidPageId(int64_t page_id) noexcept {
    return page_id > kInvalidPageId && page_id <= kMaxPageId;
  }

  // Low-level read wrapper. Returns a SQLite error code.
  //
  // |read_size| and |read_offset| are expressed in bytes.
  static int RawRead(sqlite3_file* sqlite_file,
                     int read_size,
                     int64_t read_offset,
                     uint8_t* buffer);

 private:
  // Points to the last page successfully read by ReadPage().
  // Set to kInvalidPageId if the last read was unsuccessful.
  int page_id_ = kInvalidPageId;
  // Stores the bytes of the last page successfully read by ReadPage().
  // The content is undefined if the last call to ReadPage() did not succeed.
  const std::unique_ptr<uint8_t[]> page_data_;
  // Raw pointer usage is acceptable because this instance's owner is expected
  // to ensure that the VirtualTable outlives this.
  VirtualTable* const table_;
  int page_size_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_PAGER_H_
