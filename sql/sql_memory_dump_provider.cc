// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_memory_dump_provider.h"

#include <cstdint>

#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// static
SqlMemoryDumpProvider* SqlMemoryDumpProvider::GetInstance() {
  return base::Singleton<
      SqlMemoryDumpProvider,
      base::LeakySingletonTraits<SqlMemoryDumpProvider>>::get();
}

// static
int64_t SqlMemoryDumpProvider::GetMemoryUse(int64_t* high_water) {
  sqlite3_int64 memory_used = 0;
  sqlite3_int64 memory_high_water = 0;
  auto result_code = ToSqliteResultCode(sqlite3_status64(
      SQLITE_STATUS_MEMORY_USED, &memory_used, &memory_high_water,
      /*resetFlag=*/high_water ? 1 : 0));
  DCHECK_EQ(result_code, SqliteResultCode::kOk)
      << "sqlite3_status64(STATUS_MEMORY_USED) failed";
  if (high_water) {
    *high_water = memory_high_water;
  }
  return memory_used;
}

SqlMemoryDumpProvider::SqlMemoryDumpProvider() = default;

SqlMemoryDumpProvider::~SqlMemoryDumpProvider() = default;

bool SqlMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  int64_t memory_high_water = 0;
  int64_t memory_used = GetMemoryUse(&memory_high_water);

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump("sqlite");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_used);
  dump->AddScalar("malloc_high_wmark_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_high_water);

  sqlite3_int64 dummy_high_water = -1;
  sqlite3_int64 malloc_count = -1;
  auto sqlite_result_code = ToSqliteResultCode(sqlite3_status64(
      SQLITE_STATUS_MALLOC_COUNT, &malloc_count, &dummy_high_water,
      /*resetFlag=*/0));
  DCHECK_EQ(sqlite_result_code, SqliteResultCode::kOk)
      << "sqlite3_status64(SQLITE_STATUS_MALLOC_COUNT) failed";
  dump->AddScalar("malloc_count",
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  malloc_count);

  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (system_allocator_name) {
    pmd->AddSuballocation(dump->guid(), system_allocator_name);
  }
  return true;
}

}  // namespace sql
