// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/database_memory_dump_provider.h"

#include <inttypes.h>

#include <cstdint>
#include <string>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

DatabaseMemoryDumpProvider::DatabaseMemoryDumpProvider(sqlite3* db,
                                                       const std::string& name)
    : db_(db), connection_name_(name) {}

DatabaseMemoryDumpProvider::~DatabaseMemoryDumpProvider() = default;

void DatabaseMemoryDumpProvider::ResetDatabase() {
  base::AutoLock lock(lock_);
  db_ = nullptr;
}

bool DatabaseMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kLight) {
    return true;
  }

  MemoryUsageResult memory_usage = GetDbMemoryUsage();
  if (!memory_usage.is_valid)
    return false;

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(FormatDumpName());
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_usage.cache_size + memory_usage.schema_size +
                      memory_usage.statement_size);
  dump->AddScalar("cache_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_usage.cache_size);
  dump->AddScalar("schema_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_usage.schema_size);
  dump->AddScalar("statement_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_usage.statement_size);
  return true;
}

bool DatabaseMemoryDumpProvider::ReportMemoryUsage(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& dump_name) {
  MemoryUsageResult memory_usage = GetDbMemoryUsage();
  if (!memory_usage.is_valid)
    return false;

  auto* mad = pmd->CreateAllocatorDump(dump_name);
  mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                 base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                 memory_usage.cache_size + memory_usage.schema_size +
                     memory_usage.statement_size);
  pmd->AddSuballocation(mad->guid(), FormatDumpName());

  return true;
}

DatabaseMemoryDumpProvider::MemoryUsageResult
DatabaseMemoryDumpProvider::GetDbMemoryUsage() {
  MemoryUsageResult result;

  // Lock is acquired here so that db_ is not reset in ResetDatabase when
  // collecting stats.
  base::AutoLock lock(lock_);
  if (!db_) {
    DCHECK_EQ(result.is_valid, false);
    return result;
  }

  // The following calls all set the high watermark to zero.
  // See /https://www.sqlite.org/c3ref/c_dbstatus_options.html
  int high_watermark = 0;

  auto sqlite_result_code = ToSqliteResultCode(sqlite3_db_status(
      db_, SQLITE_DBSTATUS_CACHE_USED, &result.cache_size, &high_watermark,
      /*resetFlg=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_status(SQLITE_DBSTATUS_CACHE_USED) failed";

#if DCHECK_IS_ON()
  int shared_cache_size = 0;
  sqlite_result_code = ToSqliteResultCode(
      sqlite3_db_status(db_, SQLITE_DBSTATUS_CACHE_USED_SHARED,
                        &shared_cache_size, &high_watermark, /*resetFlg=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_status(SQLITE_DBSTATUS_CACHE_USED_SHARED) failed";
  DCHECK_EQ(shared_cache_size, result.cache_size)
      << "Memory counting assumes that each database uses a private page cache";
#endif  // DCHECK_IS_ON()

  sqlite_result_code = ToSqliteResultCode(sqlite3_db_status(
      db_, SQLITE_DBSTATUS_SCHEMA_USED, &result.schema_size, &high_watermark,
      /*resetFlg=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_status(SQLITE_DBSTATUS_SCHEMA_USED) failed";

  sqlite_result_code = ToSqliteResultCode(sqlite3_db_status(
      db_, SQLITE_DBSTATUS_STMT_USED, &result.statement_size, &high_watermark,
      /*resetFlg=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_db_status(SQLITE_DBSTATUS_STMT_USED) failed";

  result.is_valid = true;
  return result;
}

std::string DatabaseMemoryDumpProvider::FormatDumpName() const {
  return base::StringPrintf(
      "sqlite/%s_connection/0x%" PRIXPTR,
      connection_name_.empty() ? "Unknown" : connection_name_.c_str(),
      reinterpret_cast<uintptr_t>(this));
}

}  // namespace sql
