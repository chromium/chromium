// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/pager.h"

#include <limits>

#include "sql/recover_module/table.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace recover {

constexpr int DatabasePageReader::kInvalidPageId;
constexpr int DatabasePageReader::kMinPageSize;
constexpr int DatabasePageReader::kMaxPageSize;
constexpr int DatabasePageReader::kDatabaseHeaderSize;
constexpr int DatabasePageReader::kMinUsablePageSize;
constexpr int DatabasePageReader::kMaxPageId;

static_assert(DatabasePageReader::kMaxPageId <= std::numeric_limits<int>::max(),
              "ints are not appropriate for representing page IDs");

DatabasePageReader::DatabasePageReader(VirtualTable* table)
    : page_data_(std::make_unique<uint8_t[]>(table->page_size())),
      table_(table) {
  DCHECK(table != nullptr);
  DCHECK(IsValidPageSize(table->page_size()));
}

DatabasePageReader::~DatabasePageReader() = default;

int DatabasePageReader::ReadPage(int page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(page_id, kInvalidPageId);
  DCHECK_LE(page_id, kMaxPageId);

  if (page_id_ == page_id)
    return SQLITE_OK;

  sqlite3_file* const sqlite_file = table_->SqliteFile();
  const int page_size = table_->page_size();
  const int page_offset = (page_id == 1) ? kDatabaseHeaderSize : 0;
  const int read_size = page_size - page_offset;
  static_assert(kMinPageSize >= kDatabaseHeaderSize,
                "The |read_size| computation above may overflow");

  page_size_ = read_size;
  DCHECK_GE(page_size_, kMinUsablePageSize);
  DCHECK_LE(page_size_, kMaxPageSize);

  const int64_t read_offset = (page_id - 1) * page_size + page_offset;
  static_assert((kMaxPageId - 1) * static_cast<int64_t>(kMaxPageSize) +
                        kDatabaseHeaderSize <=
                    std::numeric_limits<int64_t>::max(),
                "The |read_offset| computation above may overflow");

  int sqlite_status =
      RawRead(sqlite_file, read_size, read_offset, page_data_.get());

  // |page_id_| needs to be set to kInvalidPageId if the read failed.
  // Otherwise, future ReadPage() calls with the previous |page_id_| value
  // would return SQLITE_OK, but the page data buffer might be trashed.
  page_id_ = (sqlite_status == SQLITE_OK) ? page_id : kInvalidPageId;
  return sqlite_status;
}

// static
int DatabasePageReader::RawRead(sqlite3_file* sqlite_file,
                                int read_size,
                                int64_t read_offset,
                                uint8_t* result_buffer) {
  DCHECK(sqlite_file != nullptr);
  DCHECK_GE(read_size, 0);
  DCHECK_GE(read_offset, 0);
  DCHECK(result_buffer != nullptr);

  // Retry the I/O operations a few times if they fail. This is especially
  // useful when recovering from database corruption.
  static constexpr int kRetryCount = 10;

  int sqlite_status;
  bool got_lock = false;
  for (int i = kRetryCount; i > 0; --i) {
    sqlite_status =
        sqlite_file->pMethods->xLock(sqlite_file, SQLITE_LOCK_SHARED);
    if (sqlite_status == SQLITE_OK) {
      got_lock = true;
      break;
    }
  }

  // Try reading even if we don't have a shared lock on the database. If the
  // read fails, the database page is completely skipped, so any data we might
  // get from the read is better than nothing.
  for (int i = kRetryCount; i > 0; --i) {
    sqlite_status = sqlite_file->pMethods->xRead(sqlite_file, result_buffer,
                                                 read_size, read_offset);
    if (sqlite_status == SQLITE_OK)
      break;
    if (sqlite_status == SQLITE_IOERR_SHORT_READ) {
      // The read succeeded, but hit EOF. The extra bytes in the page buffer
      // are set to zero. This is acceptable for our purposes.
      sqlite_status = SQLITE_OK;
      break;
    }
  }

  if (got_lock) {
    // TODO(pwnall): This logic was ported from the old C-in-SQLite-style patch.
    //               Dropping the lock here is incorrect, because the file
    //               descriptor is shared with the SQLite pager, which may
    //               expect to be holding a lock.
    sqlite_file->pMethods->xUnlock(sqlite_file, SQLITE_LOCK_NONE);
  }
  return sqlite_status;
}

}  // namespace recover
}  // namespace sql