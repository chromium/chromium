// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_memory_dump_provider.h"

#include "base/trace_event/process_memory_dump.h"
#include "sql/test/sql_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using SQLMemoryDumpProviderTest = sql::SQLTestBase;
}

TEST_F(SQLMemoryDumpProviderTest, OnMemoryDump) {
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  base::trace_event::ProcessMemoryDump pmd(args);
  ASSERT_TRUE(
      sql::SqlMemoryDumpProvider::GetInstance()->OnMemoryDump(args, &pmd));
  ASSERT_TRUE(pmd.GetAllocatorDump("sqlite"));
}
