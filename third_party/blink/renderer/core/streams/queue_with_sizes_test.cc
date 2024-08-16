// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/queue_with_sizes.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

using ::testing::Values;

TEST(QueueWithSizesTest, TotalSizeStartsAtZero) {
  test::TaskEnvironment task_environment;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  EXPECT_EQ(queue->TotalSize(), 0.0);
  EXPECT_TRUE(queue->IsEmpty());
}

TEST(QueueWithSizesTest, EnqueueIncreasesTotalSize) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 4.5,
                              ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_EQ(queue->TotalSize(), 4.5);
}

TEST(QueueWithSizesTest, EnqueueAddsSize) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 4.5,
                              ASSERT_NO_EXCEPTION);
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 2.0,
                              ASSERT_NO_EXCEPTION);
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 1.25,
                              ASSERT_NO_EXCEPTION);
  EXPECT_EQ(queue->TotalSize(), 7.75);
}

class QueueWithSizesBadSizeTest : public ::testing::TestWithParam<double> {};

TEST_P(QueueWithSizesBadSizeTest, BadSizeThrowsException) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  ExceptionState exception_state(isolate, v8::ExceptionContext::kOperation, "",
                                 "");
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), GetParam(),
                              exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_TRUE(queue->IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         QueueWithSizesBadSizeTest,
                         Values(-1,
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity()));

TEST(QueueWithSizesTest, DequeueReturnsSameObject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  auto chunk = v8::Object::New(isolate);
  queue->EnqueueValueWithSize(isolate, chunk, 1, ASSERT_NO_EXCEPTION);
  auto new_chunk = queue->DequeueValue(isolate);
  EXPECT_EQ(chunk, new_chunk);
}

TEST(QueueWithSizesTest, DequeueSubtractsSize) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 1,
                              ASSERT_NO_EXCEPTION);
  queue->DequeueValue(isolate);
  EXPECT_TRUE(queue->IsEmpty());
  EXPECT_EQ(queue->TotalSize(), 0.0);
}

TEST(QueueWithSizesTest, PeekReturnsSameObject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  auto chunk = v8::Object::New(isolate);
  queue->EnqueueValueWithSize(isolate, chunk, 1, ASSERT_NO_EXCEPTION);
  auto peeked_chunk = queue->PeekQueueValue(isolate);
  EXPECT_EQ(chunk, peeked_chunk);
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_EQ(queue->TotalSize(), 1.0);
}

TEST(QueueWithSizesTest, ResetQueueClearsSize) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 1,
                              ASSERT_NO_EXCEPTION);
  queue->ResetQueue();
  EXPECT_TRUE(queue->IsEmpty());
  EXPECT_EQ(queue->TotalSize(), 0.0);
}

TEST(QueueWithSizesTest, UsesDoubleArithmetic) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 1e-15,
                              ASSERT_NO_EXCEPTION);
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 1,
                              ASSERT_NO_EXCEPTION);
  // 1e-15 + 1 can be represented in a double.
  EXPECT_EQ(queue->TotalSize(), 1.000000000000001);
  queue->DequeueValue(isolate);
  EXPECT_EQ(queue->TotalSize(), 1.0);
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 1e-16,
                              ASSERT_NO_EXCEPTION);
  // 1 + 1e-16 can't be represented in a double; gets rounded down to 1.
  EXPECT_EQ(queue->TotalSize(), 1.0);
}

TEST(QueueWithSizesTest, TotalSizeIsNonNegative) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* queue = MakeGarbageCollected<QueueWithSizes>();
  auto* isolate = scope.GetIsolate();
  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 1,
                              ASSERT_NO_EXCEPTION);
  EXPECT_EQ(queue->TotalSize(), 1.0);

  queue->EnqueueValueWithSize(isolate, v8::Undefined(isolate), 1e-16,
                              ASSERT_NO_EXCEPTION);
  EXPECT_EQ(queue->TotalSize(), 1.0);

  queue->DequeueValue(isolate);
  EXPECT_EQ(queue->TotalSize(), 0.0);

  queue->DequeueValue(isolate);
  // Size would become -1e-16, but it is forced to be non-negative, hence 0.
  EXPECT_EQ(queue->TotalSize(), 0.0);
}

}  // namespace

}  // namespace blink
