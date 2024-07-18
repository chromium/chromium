// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
using testing::ByRef;
using testing::Contains;
using testing::Eq;

namespace mojo {
namespace core {
namespace {

using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::SizeIs;

class FakeMessagePipeDispatcher : public Dispatcher {
 public:
  FakeMessagePipeDispatcher() = default;

  FakeMessagePipeDispatcher(const FakeMessagePipeDispatcher&) = delete;
  FakeMessagePipeDispatcher& operator=(const FakeMessagePipeDispatcher&) =
      delete;

  Type GetType() const override { return Type::MESSAGE_PIPE; }

  MojoResult Close() override { return MOJO_RESULT_OK; }

 private:
  ~FakeMessagePipeDispatcher() override = default;
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

TEST(HandleTableTest, GetInvalidDispatcher) {
  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());
  EXPECT_THAT(handle_table.GetDispatcher(MojoHandle(2)), IsNull());
}

TEST(HandleTableTest, GetDispatcher) {
  const scoped_refptr<Dispatcher> dispatcher(new FakeMessagePipeDispatcher);

  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());

  const MojoHandle handle = handle_table.AddDispatcher(dispatcher);
  ASSERT_THAT(handle, Ne(MOJO_HANDLE_INVALID));
  EXPECT_THAT(handle_table.GetDispatcher(handle), Eq(dispatcher));
}

TEST(HandleTableTest, AddAndGetDispatchers) {
  const scoped_refptr<Dispatcher> one(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> two(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> three(new FakeMessagePipeDispatcher);

  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());

  const MojoHandle one_handle = handle_table.AddDispatcher(one);
  ASSERT_THAT(one_handle, Ne(MOJO_HANDLE_INVALID));
  EXPECT_THAT(handle_table.GetDispatcher(one_handle), Eq(one));

  const MojoHandle two_handle = handle_table.AddDispatcher(two);
  ASSERT_THAT(two_handle, Ne(MOJO_HANDLE_INVALID));
  EXPECT_THAT(handle_table.GetDispatcher(two_handle), Eq(two));

  const MojoHandle three_handle = handle_table.AddDispatcher(three);
  ASSERT_THAT(three_handle, Ne(MOJO_HANDLE_INVALID));
  EXPECT_THAT(handle_table.GetDispatcher(three_handle), Eq(three));

  EXPECT_THAT(handle_table.GetDispatcher(one_handle), Eq(one));
  EXPECT_THAT(handle_table.GetDispatcher(two_handle), Eq(two));
  EXPECT_THAT(handle_table.GetDispatcher(three_handle), Eq(three));
}

TEST(HandleTableTest, GetAndRemoveInvalidDispatcher) {
  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());

  scoped_refptr<Dispatcher> dispatcher;
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(MojoHandle(2), &dispatcher),
              Eq(MOJO_RESULT_INVALID_ARGUMENT));
  EXPECT_THAT(dispatcher, IsNull());
}

TEST(HandleTableTest, GetAndRemoveDispatchers) {
  const scoped_refptr<Dispatcher> one(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> two(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> three(new FakeMessagePipeDispatcher);

  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());

  const MojoHandle one_handle = handle_table.AddDispatcher(one);
  const MojoHandle two_handle = handle_table.AddDispatcher(two);
  const MojoHandle three_handle = handle_table.AddDispatcher(three);
  EXPECT_THAT(handle_table.GetDispatcher(one_handle), Eq(one));
  EXPECT_THAT(handle_table.GetDispatcher(two_handle), Eq(two));
  EXPECT_THAT(handle_table.GetDispatcher(three_handle), Eq(three));

  scoped_refptr<Dispatcher> dispatcher;
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(three_handle, &dispatcher),
              Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatcher, NotNull());
  EXPECT_THAT(handle_table.GetDispatcher(three_handle), IsNull());

  dispatcher.reset();
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(two_handle, &dispatcher),
              Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatcher, NotNull());
  EXPECT_THAT(handle_table.GetDispatcher(two_handle), IsNull());

  dispatcher.reset();
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(one_handle, &dispatcher),
              Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatcher, NotNull());
  EXPECT_THAT(handle_table.GetDispatcher(one_handle), IsNull());
}

TEST(HandleTableTest, GetAndRemoveDispatcherInTransit) {
  const scoped_refptr<Dispatcher> one(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> two(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> three(new FakeMessagePipeDispatcher);

  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());

  const MojoHandle one_handle = handle_table.AddDispatcher(one);
  const MojoHandle two_handle = handle_table.AddDispatcher(two);
  const MojoHandle three_handle = handle_table.AddDispatcher(three);
  const MojoHandle handles[] = {one_handle, two_handle, three_handle};

  std::vector<Dispatcher::DispatcherInTransit> dispatchers_in_transit;
  // Leave out the last handle.
  EXPECT_THAT(
      handle_table.BeginTransit(handles,
                                /*num_handles=*/2, &dispatchers_in_transit),
      Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatchers_in_transit, SizeIs(2));

  scoped_refptr<Dispatcher> dispatcher;
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(three_handle, &dispatcher),
              Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatcher, NotNull());
  EXPECT_THAT(handle_table.GetDispatcher(three_handle), IsNull());

  dispatcher.reset();
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(two_handle, &dispatcher),
              Eq(MOJO_RESULT_BUSY));
  EXPECT_THAT(dispatcher, IsNull());
  EXPECT_THAT(handle_table.GetDispatcher(two_handle), NotNull());

  dispatcher.reset();
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(one_handle, &dispatcher),
              Eq(MOJO_RESULT_BUSY));
  EXPECT_THAT(dispatcher, IsNull());
  EXPECT_THAT(handle_table.GetDispatcher(one_handle), NotNull());
}

TEST(HandleTableTest, InvalidExtraBeginTransit) {
  const scoped_refptr<Dispatcher> one(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> two(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> three(new FakeMessagePipeDispatcher);

  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());

  const MojoHandle one_handle = handle_table.AddDispatcher(one);
  const MojoHandle two_handle = handle_table.AddDispatcher(two);
  const MojoHandle three_handle = handle_table.AddDispatcher(three);
  const MojoHandle handles[] = {one_handle, two_handle, three_handle};

  std::vector<Dispatcher::DispatcherInTransit> dispatchers_in_transit;
  // Leave out the first handle.
  EXPECT_THAT(
      handle_table.BeginTransit(handles + 1,
                                /*num_handles=*/2, &dispatchers_in_transit),
      Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatchers_in_transit, SizeIs(2));

  dispatchers_in_transit.clear();
  EXPECT_THAT(
      handle_table.BeginTransit(handles,
                                /*num_handles=*/3, &dispatchers_in_transit),
      Eq(MOJO_RESULT_BUSY));
  EXPECT_THAT(dispatchers_in_transit, SizeIs(1));
}

TEST(HandleTableTest, CompleteTransitAndClose) {
  const scoped_refptr<Dispatcher> one(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> two(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> three(new FakeMessagePipeDispatcher);

  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());

  const MojoHandle one_handle = handle_table.AddDispatcher(one);
  const MojoHandle two_handle = handle_table.AddDispatcher(two);
  const MojoHandle three_handle = handle_table.AddDispatcher(three);
  const MojoHandle handles[] = {one_handle, two_handle, three_handle};

  std::vector<Dispatcher::DispatcherInTransit> dispatchers_in_transit;
  EXPECT_THAT(
      handle_table.BeginTransit(handles,
                                /*num_handles=*/3, &dispatchers_in_transit),
      Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatchers_in_transit, SizeIs(3));

  handle_table.CompleteTransitAndClose(dispatchers_in_transit);
  EXPECT_THAT(handle_table.GetDispatcher(three_handle), IsNull());
  EXPECT_THAT(handle_table.GetDispatcher(two_handle), IsNull());
  EXPECT_THAT(handle_table.GetDispatcher(one_handle), IsNull());
}

TEST(HandleTableTest, CancelTransit) {
  const scoped_refptr<Dispatcher> one(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> two(new FakeMessagePipeDispatcher);
  const scoped_refptr<Dispatcher> three(new FakeMessagePipeDispatcher);

  HandleTable handle_table;
  const base::AutoLock auto_lock(handle_table.GetLock());

  const MojoHandle one_handle = handle_table.AddDispatcher(one);
  const MojoHandle two_handle = handle_table.AddDispatcher(two);
  const MojoHandle three_handle = handle_table.AddDispatcher(three);
  const MojoHandle handles[] = {one_handle, two_handle, three_handle};

  std::vector<Dispatcher::DispatcherInTransit> dispatchers_in_transit;
  EXPECT_THAT(
      handle_table.BeginTransit(handles,
                                /*num_handles=*/3, &dispatchers_in_transit),
      Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatchers_in_transit, SizeIs(3));

  handle_table.CancelTransit(dispatchers_in_transit);

  scoped_refptr<Dispatcher> dispatcher;
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(three_handle, &dispatcher),
              Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatcher, NotNull());
  EXPECT_THAT(handle_table.GetDispatcher(three_handle), IsNull());

  dispatcher.reset();
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(two_handle, &dispatcher),
              Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatcher, NotNull());
  EXPECT_THAT(handle_table.GetDispatcher(two_handle), IsNull());

  dispatcher.reset();
  EXPECT_THAT(handle_table.GetAndRemoveDispatcher(one_handle, &dispatcher),
              Eq(MOJO_RESULT_OK));
  EXPECT_THAT(dispatcher, NotNull());
  EXPECT_THAT(handle_table.GetDispatcher(one_handle), IsNull());
}

TEST(HandleTableTest, OnMemoryDump) {
  HandleTable ht;

  {
    base::AutoLock auto_lock(ht.GetLock());
    scoped_refptr<Dispatcher> dispatcher(new FakeMessagePipeDispatcher);
    ht.AddDispatcher(dispatcher);
  }

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd(args);
  ht.OnMemoryDump(args, &pmd);

  CheckNameAndValue(&pmd, "mojo/message_pipe", 1);
  CheckNameAndValue(&pmd, "mojo/data_pipe_consumer", 0);
}

}  // namespace core
}  // namespace mojo
