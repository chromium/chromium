// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/table.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "sql/recover_module/cursor.h"
#include "sql/recover_module/integers.h"
#include "sql/recover_module/pager.h"

namespace sql {
namespace recover {

// Returns the number of the page holding the root page of a table's B-tree.
//
// Returns a null optional if the operation fails in any way. The failure is
// most likely due to an incorrect table spec (missing attachment or table).
// Corrupted SQLite metadata can cause failures here.
base::Optional<int> GetTableRootPageId(sqlite3* sqlite_db,
                                       const TargetTableSpec& table) {
  if (table.table_name == "sqlite_master") {
    // The sqlite_master table is always rooted at the first page.
    // SQLite page IDs use 1-based indexing.
    return base::Optional<int64_t>(1);
  }

  std::string select_sql =
      base::StrCat({"SELECT rootpage FROM ", table.db_name,
                    ".sqlite_master WHERE type='table' AND tbl_name=?"});
  sqlite3_stmt* sqlite_statement;
  if (sqlite3_prepare_v3(sqlite_db, select_sql.c_str(), select_sql.size() + 1,
                         SQLITE_PREPARE_NO_VTAB, &sqlite_statement,
                         nullptr) != SQLITE_OK) {
    // The sqlite_master table is missing or its schema is corrupted.
    return base::nullopt;
  }

  if (sqlite3_bind_text(sqlite_statement, 1, table.table_name.c_str(),
                        static_cast<int>(table.table_name.size()),
                        SQLITE_STATIC) != SQLITE_OK) {
    // Binding the table name failed. This shouldn't happen.
    sqlite3_finalize(sqlite_statement);
    return base::nullopt;
  }

  if (sqlite3_step(sqlite_statement) != SQLITE_ROW) {
    // The database attachment point or table does not exist.
    sqlite3_finalize(sqlite_statement);
    return base::nullopt;
  }

  int64_t root_page = sqlite3_column_int64(sqlite_statement, 0);
  sqlite3_finalize(sqlite_statement);

  if (!DatabasePageReader::IsValidPageId(root_page)) {
    // Database corruption.
    return base::nullopt;
  }

  static_assert(
      DatabasePageReader::kMaxPageId <= std::numeric_limits<int>::max(),
      "Converting the page ID to int may overflow");
  return base::make_optional(static_cast<int>(root_page));
}

// Returns (SQLite status, a SQLite database's page size).
std::pair<int, int> GetDatabasePageSize(sqlite3_file* sqlite_file) {
  // The SQLite header is documented at:
  //   https://www.sqlite.org/fileformat.html#the_database_header
  //
  // Read the entire header.
  static constexpr int kHeaderOffset = 0;
  static constexpr int kHeaderSize = 100;
  uint8_t header_bytes[kHeaderSize];
  int sqlite_status = DatabasePageReader::RawRead(sqlite_file, kHeaderSize,
                                                  kHeaderOffset, header_bytes);
  if (sqlite_status != SQLITE_OK)
    return {sqlite_status, 0};

  // This computation uses the alternate interpretation that the page size
  // header field is a little-endian number encoding the page size divided by
  // 256.
  static constexpr int kPageSizeHeaderOffset = 16;
  const int page_size =
      LoadBigEndianUint16(header_bytes + kPageSizeHeaderOffset);

  if (!DatabasePageReader::IsValidPageSize(page_size)) {
    // Invalid page numbers are considered irrecoverable corruption.
    return {SQLITE_CORRUPT, 0};
  }

  // TODO(pwnall): This method needs a better name. It also checks the database
  //               header for unsupported edge cases.

  static constexpr int kReservedSizeHeaderOffset = 20;
  const uint8_t page_reserved_size = header_bytes[kReservedSizeHeaderOffset];
  if (page_reserved_size != 0) {
    // Chrome does not use any extension that requires reserved page space.
    return {SQLITE_CORRUPT, 0};
  }

  // The text encoding is stored at offset 56, as a 4-byte big-endian integer.
  // However, the range of values is 1-3, so reading the last byte is
  // sufficient.
  static_assert(SQLITE_UTF8 <= std::numeric_limits<uint8_t>::max(),
                "Text encoding field reading shortcut is invalid.");
  static constexpr int kTextEncodingHeaderOffset = 59;
  const uint8_t text_encoding = header_bytes[kTextEncodingHeaderOffset];
  if (text_encoding != SQLITE_UTF8) {
    // This extension only supports databases that use UTF-8 encoding.
    return {SQLITE_CORRUPT, 0};
  }

  return {SQLITE_OK, page_size};
}

// static
std::pair<int, std::unique_ptr<VirtualTable>> VirtualTable::Create(
    sqlite3* sqlite_db,
    TargetTableSpec backing_table_spec,
    std::vector<RecoveredColumnSpec> column_specs) {
  DCHECK(backing_table_spec.IsValid());

  base::Optional<int64_t> backing_table_root_page_id =
      GetTableRootPageId(sqlite_db, backing_table_spec);
  if (!backing_table_root_page_id.has_value()) {
    // Either the backing table specification is incorrect, or the database
    // metadata is corrupted beyond hope.
    //
    // This virtual table is intended to be used by Chrome features, whose code
    // is covered by tests. Therefore, the most likely cause is metadata
    // corruption.
    return {SQLITE_CORRUPT, nullptr};
  }

  sqlite3_file* sqlite_file;
  int sqlite_status =
      sqlite3_file_control(sqlite_db, backing_table_spec.db_name.c_str(),
                           SQLITE_FCNTL_FILE_POINTER, &sqlite_file);
  if (sqlite_status != SQLITE_OK) {
    // Failed to get the backing store's file. GetTableRootPage() succeeded, so
    // the attachment point name must be correct. So, this is definitely a
    // SQLite error, not a virtual table use error. Report the error code as-is,
    // so it can be captured in histograms.
    return {sqlite_status, nullptr};
  }

  int page_size;
  std::tie(sqlite_status, page_size) = GetDatabasePageSize(sqlite_file);
  if (sqlite_status != SQLITE_OK) {
    // By the same reasoning as above, report the error code as-is.
    return {sqlite_status, nullptr};
  }

  return {SQLITE_OK,
          std::make_unique<VirtualTable>(sqlite_db, sqlite_file,
                                         backing_table_root_page_id.value(),
                                         page_size, std::move(column_specs))};
}

VirtualTable::VirtualTable(sqlite3* sqlite_db,
                           sqlite3_file* sqlite_file,
                           int root_page_id,
                           int page_size,
                           std::vector<RecoveredColumnSpec> column_specs)
    : sqlite_file_(sqlite_file),
      root_page_id_(root_page_id),
      page_size_(page_size),
      column_specs_(std::move(column_specs)) {
  DCHECK(sqlite_db != nullptr);
  DCHECK(sqlite_file != nullptr);
  DCHECK_GT(root_page_id_, 0);
  DCHECK(DatabasePageReader::IsValidPageSize(page_size));
  DCHECK(!column_specs_.empty());
}

VirtualTable::~VirtualTable() {
#if DCHECK_IS_ON()
  DCHECK_EQ(0, open_cursor_count_.load(std::memory_order_relaxed))
      << "SQLite forgot to xClose() an xOpen()ed cursor";
#endif  // DCHECK_IS_ON()
}

std::string VirtualTable::ToCreateTableSql() const {
  std::vector<std::string> column_sqls;
  column_sqls.reserve(column_specs_.size());
  for (const RecoveredColumnSpec& column_spec : column_specs_)
    column_sqls.push_back(column_spec.ToCreateTableSql());

  static constexpr base::StringPiece kCreateTableSqlStart("CREATE TABLE t(");
  static constexpr base::StringPiece kCreateTableSqlEnd(")");
  static constexpr base::StringPiece kColumnSqlSeparator(",");
  return base::StrCat({kCreateTableSqlStart,
                       base::JoinString(column_sqls, kColumnSqlSeparator),
                       kCreateTableSqlEnd});
}

VirtualCursor* VirtualTable::CreateCursor() {
#if DCHECK_IS_ON()
  open_cursor_count_.fetch_add(1, std::memory_order_relaxed);
#endif  // DCHECK_IS_ON()

  VirtualCursor* result = new VirtualCursor(this);
  return result;
}

void VirtualTable::WillDeleteCursor(VirtualCursor* cursor) {
#if DCHECK_IS_ON()
  DCHECK_GT(open_cursor_count_.load(std::memory_order_relaxed), 0);
  open_cursor_count_.fetch_sub(1, std::memory_order_relaxed);
#endif  // DCHECK_IS_ON()
}

}  // namespace recover
}  // namespace sql
