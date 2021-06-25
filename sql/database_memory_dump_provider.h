// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_DATABASE_MEMORY_DUMP_PROVIDER_H_
#define SQL_DATABASE_MEMORY_DUMP_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"

struct sqlite3;

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}  // namespace base

namespace sql {

class DatabaseMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  DatabaseMemoryDumpProvider(sqlite3* db, const std::string& name);
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
  bool GetDbMemoryUsage(int* cache_size, int* schema_size, int* statement_size);

  std::string FormatDumpName() const;

  sqlite3* db_;  // not owned.
  base::Lock lock_;
  std::string connection_name_;

  DISALLOW_COPY_AND_ASSIGN(DatabaseMemoryDumpProvider);
};

}  // namespace sql

#endif  // SQL_DATABASE_MEMORY_DUMP_PROVIDER_H_
