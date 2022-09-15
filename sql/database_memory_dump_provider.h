// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_DATABASE_MEMORY_DUMP_PROVIDER_H_
#define SQL_DATABASE_MEMORY_DUMP_PROVIDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/trace_event/memory_dump_provider.h"

struct sqlite3;

namespace base::trace_event {
struct MemoryDumpArgs;
class ProcessMemoryDump;
}  // namespace base::trace_event

namespace sql {

class DatabaseMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  DatabaseMemoryDumpProvider(sqlite3* db, const std::string& name);

  DatabaseMemoryDumpProvider(const DatabaseMemoryDumpProvider&) = delete;
  DatabaseMemoryDumpProvider& operator=(const DatabaseMemoryDumpProvider&) =
      delete;

  ~DatabaseMemoryDumpProvider() override;

  void ResetDatabase();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

  // Reports memory usage into provided memory dump with the given |dump_name|.
  // Called by sql::Database when its owner asks it to report memory usage.
  bool ReportMemoryUsage(base::trace_event::ProcessMemoryDump* pmd,
                         const std::string& dump_name);

 private:
  struct MemoryUsageResult {
    bool is_valid = false;
    int cache_size = 0;
    int schema_size = 0;
    int statement_size = 0;
  };
  MemoryUsageResult GetDbMemoryUsage();

  std::string FormatDumpName() const;

  base::Lock lock_;
  raw_ptr<sqlite3> db_ GUARDED_BY_CONTEXT(lock_);  // not owned.
  const std::string connection_name_;
};

}  // namespace sql

#endif  // SQL_DATABASE_MEMORY_DUMP_PROVIDER_H_
