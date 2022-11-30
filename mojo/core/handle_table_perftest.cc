// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/handle_table.h"

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/timer/lap_timer.h"
#include "mojo/core/dispatcher.h"
#include "mojo/public/c/system/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace mojo::core {
namespace {

using ::base::LapTimer;
using ::perf_test::PerfResultReporter;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Ne;

class FakeMessagePipeDispatcherForTesting : public Dispatcher {
 public:
  FakeMessagePipeDispatcherForTesting() = default;

  FakeMessagePipeDispatcherForTesting(
      const FakeMessagePipeDispatcherForTesting&) = delete;
  FakeMessagePipeDispatcherForTesting& operator=(
      const FakeMessagePipeDispatcherForTesting&) = delete;

  Type GetType() const override { return Type::MESSAGE_PIPE; }
  MojoResult Close() override { return MOJO_RESULT_OK; }

 private:
  ~FakeMessagePipeDispatcherForTesting() override = default;
};

// Returns the handles of the dispatchers added.
std::vector<MojoHandle> AddDispatchersForTesting(
    const int num_dispatchers_to_add,
    HandleTable* handle_table) {
  std::vector<MojoHandle> handles;
  handles.reserve(num_dispatchers_to_add);
  scoped_refptr<Dispatcher> dispatcher(new FakeMessagePipeDispatcherForTesting);
  const base::AutoLock auto_lock(handle_table->GetLock());
  for (int i = 0; i < num_dispatchers_to_add; ++i) {
    const MojoHandle handle = handle_table->AddDispatcher(dispatcher);
    EXPECT_THAT(handle, Ne(MOJO_HANDLE_INVALID));
    handles.push_back(handle);
  }
  return handles;
}

constexpr char kMetricThroughput[] = "Throughput";

PerfResultReporter MakeReporter(const std::string& story_name) {
  PerfResultReporter reporter("HandleTable", story_name);
  reporter.RegisterImportantMetric(kMetricThroughput, "runs/s");
  return reporter;
}

}  // namespace

TEST(HandleTablePerfTest, GetDispatcherDifferentHandles) {
  // The number below is based on https://crbug.com/1295449#c2.
  constexpr int kNumDispatchers = 10000;
  HandleTable handle_table;
  const std::vector<MojoHandle> handles =
      AddDispatchersForTesting(kNumDispatchers, &handle_table);
  ASSERT_THAT(handles.size(), Gt(0ul));
  const int handles_last_index = handles.size() - 1;

  int current_index = 0;
  LapTimer timer;
  // Query for dispatchers in a round-robin manner until the time limit expires.
  while (!timer.HasTimeLimitExpired()) {
    handle_table.GetDispatcher(handles[current_index]);
    current_index = current_index == handles_last_index ? 0 : current_index + 1;
    timer.NextLap();
  }

  PerfResultReporter reporter = MakeReporter("GetDispatcherDifferentHandles");
  reporter.AddResult(kMetricThroughput, timer.LapsPerSecond());
}

TEST(HandleTablePerfTest, GetDispatcherSameHandle) {
  // The number below is based on https://crbug.com/1295449#c2.
  constexpr int kNumDispatchers = 10000;
  HandleTable handle_table;
  const std::vector<MojoHandle> handles =
      AddDispatchersForTesting(kNumDispatchers, &handle_table);
  ASSERT_THAT(handles.size(), Gt(0ul));

  LapTimer timer;
  while (!timer.HasTimeLimitExpired()) {
    handle_table.GetDispatcher(handles[0]);
    timer.NextLap();
  }

  PerfResultReporter reporter = MakeReporter("GetDispatcherSameHandle");
  reporter.AddResult(kMetricThroughput, timer.LapsPerSecond());
}

TEST(HandleTablePerfTest, GetDispatcherMixedHandles) {
  // The number below is based on https://crbug.com/1295449#c2.
  constexpr int kNumDispatchers = 10000;
  HandleTable handle_table;
  const std::vector<MojoHandle> handles =
      AddDispatchersForTesting(kNumDispatchers, &handle_table);
  ASSERT_THAT(handles.size(), Gt(0ul));
  const int handles_last_index = handles.size() - 1;

  int current_index = 0;
  LapTimer timer;
  while (!timer.HasTimeLimitExpired()) {
    // Sample each index 3 times, thus sampling the same index as the previous
    // one roughly 66% of the time. Based on https://crbug.com/1295449.
    handle_table.GetDispatcher(handles[current_index / 4]);
    current_index = current_index == handles_last_index ? 0 : current_index + 1;
    timer.NextLap();
  }

  PerfResultReporter reporter = MakeReporter("GetDispatcherMixedHandles");
  reporter.AddResult(kMetricThroughput, timer.LapsPerSecond());
}

TEST(HandleTablePerfTest, AddAndRemoveDispatcher) {
  // The number below is based on https://crbug.com/1295449#c2.
  constexpr int kNumDispatchers = 10000;
  HandleTable handle_table;
  const std::vector<MojoHandle> handles =
      AddDispatchersForTesting(kNumDispatchers, &handle_table);
  ASSERT_THAT(handles.size(), Gt(0ul));

  LapTimer timer;
  while (!timer.HasTimeLimitExpired()) {
    const base::AutoLock auto_lock(handle_table.GetLock());
    scoped_refptr<Dispatcher> dispatcher(
        new FakeMessagePipeDispatcherForTesting);
    const MojoHandle handle = handle_table.AddDispatcher(std::move(dispatcher));
    EXPECT_THAT(handle, Ne(MOJO_HANDLE_INVALID));
    const MojoResult result =
        handle_table.GetAndRemoveDispatcher(handle, &dispatcher);
    EXPECT_THAT(result, Eq(MOJO_RESULT_OK));
    timer.NextLap();
  }

  PerfResultReporter reporter = MakeReporter("AddAndRemoveDispatcher");
  reporter.AddResult(kMetricThroughput, timer.LapsPerSecond());
}

}  // namespace mojo::core
