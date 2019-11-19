// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_CURSOR_H_
#define SQL_RECOVER_MODULE_CURSOR_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/sequence_checker.h"
#include "sql/recover_module/btree.h"
#include "sql/recover_module/pager.h"
#include "sql/recover_module/parsing.h"
#include "sql/recover_module/payload.h"
#include "sql/recover_module/record.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace recover {

class VirtualTable;

// Represents a virtual table cursor created by SQLite in a recovery table.
//
// Instances are allocated on the heap using the C++ new operator, and passed to
// SQLite via pointers to the sqlite_vtab members. SQLite is responsible for
// managing the instances' lifetimes. SQLite will call xClose() for every
// successful xOpen().
//
// Instances are not thread-safe. This should be fine, as long as each SQLite
// statement that reads from a virtual table is only used on one sequence. This
// assumption is verified by a sequence checker.
//
// If it turns out that VirtualCursor needs to be thread-safe, the best solution
// is to add a base::Lock to VirtualCursor, and keep all underlying classes not
// thread-safe.
class VirtualCursor {
 public:
  // Creates a cursor that iterates over |table|.
  //
  // |table| must outlive this instance. SQLite is trusted to call xClose() for
  // this cursor before calling xDestroy() / xDisconnect() for the virtual table
  // related to the cursor.
  explicit VirtualCursor(VirtualTable* table);
  ~VirtualCursor();

  VirtualCursor(const VirtualCursor&) = delete;
  VirtualCursor& operator=(const VirtualCursor&) = delete;

  // Returns the embedded SQLite virtual table cursor.
  //
  // This getter is not const because SQLite wants a non-const pointer to the
  // structure.
  sqlite3_vtab_cursor* SqliteCursor() { return &sqlite_cursor_; }

  // The VirtualCursor instance that embeds a given SQLite virtual table cursor.
  //
  // |sqlite_cursor| must have been returned by VirtualTable::SqliteCursor().
  static inline VirtualCursor* FromSqliteCursor(
      sqlite3_vtab_cursor* sqlite_cursor) {
    static_assert(std::is_standard_layout<VirtualCursor>::value,
                  "needed for the reinterpret_cast below");
    static_assert(offsetof(VirtualCursor, sqlite_cursor_) == 0,
                  "sqlite_cursor_ must be the first member of the class");
    VirtualCursor* result = reinterpret_cast<VirtualCursor*>(sqlite_cursor);
    DCHECK_EQ(sqlite_cursor, &result->sqlite_cursor_);
    return result;
  }

  // Seeks the cursor to the first readable row. Returns a SQLite status code.
  int First();

  // Seeks the cursor to the next row. Returns a SQLite status code.
  int Next();

  // Returns true if the cursor points to a valid row, false otherwise.
  bool IsValid() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return record_reader_.IsInitialized();
  }

  // Reports a value in the record to SQLite. |column_index| is 0-based.
  //
  // Returns a SQLite error code. This method can fail can happen if a value is
  // stored across overflow pages, and reading one of the overflow pages results
  // in an I/O error.
  int ReadColumn(int column_index, sqlite3_context* result_context);

  // Returns the rowid of the current row. The cursor must point to a valid row.
  int64_t RowId();

 private:
  // Appends a decoder for the given page at the end of the current chain.
  //
  // No modification is performed in case of failures due to I/O errors or
  // database corruption.
  void AppendPageDecoder(int page_id);

  // True if the current record is acceptable given the recovery schema.
  bool IsAcceptableRecord();

  // SQLite handle for this cursor. The struct is populated and used by SQLite.
  sqlite3_vtab_cursor sqlite_cursor_;

  // The table this cursor was created for.
  //
  // Raw pointer usage is acceptable because SQLite will ensure that the
  // VirtualTable, which is passed around as a sqlite3_vtab*, will outlive this
  // cursor, which is passed around as a sqlite3_cursor*.
  VirtualTable* const table_;

  // Reads database pages for this cursor.
  DatabasePageReader db_reader_;

  // Reads record payloads for this cursor.
  LeafPayloadReader payload_reader_;

  // Reads record rows for this cursor.
  RecordReader record_reader_;

  // Decoders for the current chain of inner pages.
  //
  // The current chain of pages consists of the inner page decoders here and the
  // decoder in |leaf_decoder_|.
  std::vector<std::unique_ptr<InnerPageDecoder>> inner_decoders_;

  // Decodes the leaf page containing records.
  std::unique_ptr<LeafPageDecoder> leaf_decoder_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_CURSOR_H_
