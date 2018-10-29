// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/handle_table.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/traced_value.h"
#include "mojo/core/dispatcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::trace_event::MemoryAllocatorDump;
using testing::Contains;
using testing::Eq;
using testing::Contains;
using testing::ByRef;

namespace mojo {
namespace core {
namespace {

class FakeMessagePipeDispatcher : public Dispatcher {
 public:
  FakeMessagePipeDispatcher() {}

  Type GetType() const override { return Type::MESSAGE_PIPE; }

  MojoResult Close() override { return MOJO_RESULT_OK; }

 private:
  ~FakeMessagePipeDispatcher() override {}
  DISALLOW_COPY_AND_ASSIGN(FakeMessagePipeDispatcher);
};

void CheckNameAndValue(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& name,
                       uint64_t value) {
  base::trace_event::MemoryAllocatorDump* mad = pmd->GetAllocatorDump(name);
  ASSERT_TRUE(mad);

  MemoryAllocatorDump::Entry expected(
      "object_count", MemoryAllocatorDump::kUnitsObjects, value);
  EXPECT_THAT(mad->entries(), Contains(Eq(ByRef(expected))));
}

}  // namespace

TEST(HandleTableTest, OnMemoryDump) {
  HandleTable ht;

  {
    base::AutoLock auto_lock(ht.GetLock());
    scoped_refptr<Dispatcher> dispatcher(new FakeMessagePipeDispatcher);
    ht.AddDispatcher(dispatcher);
  }

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  base::trace_event::ProcessMemoryDump pmd(args);
  ht.OnMemoryDump(args, &pmd);

  CheckNameAndValue(&pmd, "mojo/message_pipe", 1);
  CheckNameAndValue(&pmd, "mojo/data_pipe_consumer", 0);
}

}  // namespace core
}  // namespace mojo
