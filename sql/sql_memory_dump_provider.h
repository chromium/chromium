// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_SQL_MEMORY_DUMP_PROVIDER_H_
#define SQL_SQL_MEMORY_DUMP_PROVIDER_H_

#include "base/component_export.h"
#include "base/memory/singleton.h"
#include "base/trace_event/memory_dump_provider.h"

namespace sql {

// Adds process-wide memory usage statistics about sqlite to chrome://tracing.
// sql::Database::OnMemoryDump adds per-connection memory statistics.
class COMPONENT_EXPORT(SQL) SqlMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  static SqlMemoryDumpProvider* GetInstance();

  SqlMemoryDumpProvider(const SqlMemoryDumpProvider&) = delete;
  SqlMemoryDumpProvider& operator=(const SqlMemoryDumpProvider&) = delete;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend struct base::DefaultSingletonTraits<SqlMemoryDumpProvider>;

  SqlMemoryDumpProvider();
  ~SqlMemoryDumpProvider() override;
};

}  // namespace sql

#endif  // SQL_SQL_MEMORY_DUMP_PROVIDER_H_
