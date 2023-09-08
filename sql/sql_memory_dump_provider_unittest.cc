// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_memory_dump_provider.h"

#include "base/files/scoped_temp_dir.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

namespace {

class SQLMemoryDumpProviderTest : public testing::Test {
 public:
  ~SQLMemoryDumpProviderTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(db_.Open(
        temp_dir_.GetPath().AppendASCII("memory_dump_provider_test.sqlite")));

    ASSERT_TRUE(db_.Execute("CREATE TABLE foo (a, b)"));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  Database db_;
};

TEST_F(SQLMemoryDumpProviderTest, OnMemoryDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd(args);
  ASSERT_TRUE(SqlMemoryDumpProvider::GetInstance()->OnMemoryDump(args, &pmd));
  ASSERT_TRUE(pmd.GetAllocatorDump("sqlite"));
}

}  // namespace

}  // namespace sql
