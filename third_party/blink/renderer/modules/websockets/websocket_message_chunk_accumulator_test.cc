// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_message_chunk_accumulator.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class WebSocketMessageChunkAccumulatorTest : public testing::Test {
 public:
  using FakeTaskRunner = scheduler::FakeTaskRunner;

  static Vector<char> Flatten(const Vector<base::span<const char>>& chunks) {
    Vector<char> v;
    for (const auto& chunk : chunks) {
      v.AppendSpan(chunk);
    }
    return v;
  }

  static constexpr auto kSegmentSize =
      WebSocketMessageChunkAccumulator::kSegmentSize;
  static constexpr auto kFreeDelay =
      WebSocketMessageChunkAccumulator::kFreeDelay;
  test::TaskEnvironment task_environment_;
};

constexpr size_t WebSocketMessageChunkAccumulatorTest::kSegmentSize;
constexpr base::TimeDelta WebSocketMessageChunkAccumulatorTest::kFreeDelay;

TEST_F(WebSocketMessageChunkAccumulatorTest, Empty) {
  auto task_runner = base::MakeRefCounted<FakeTaskRunner>();
  WebSocketMessageChunkAccumulator* chunks =
      MakeGarbageCollected<WebSocketMessageChunkAccumulator>(task_runner);
  chunks->SetTaskRunnerForTesting(task_runner, task_runner->GetMockTickClock());

  EXPECT_EQ(chunks->GetSize(), 0u);
  EXPECT_TRUE(chunks->GetView().empty());
}

TEST_F(WebSocketMessageChunkAccumulatorTest, Append) {
  WebSocketMessageChunkAccumulator* chunks =
      MakeGarbageCollected<WebSocketMessageChunkAccumulator>(
          base::MakeRefCounted<FakeTaskRunner>());

  Vector<char> chunk(8, 'x');

  chunks->Append(base::make_span(chunk));

  EXPECT_EQ(chunks->GetSize(), chunk.size());
  EXPECT_EQ(8u, chunks->GetSize());
  ASSERT_EQ(chunks->GetView().size(), 1u);
  ASSERT_EQ(chunks->GetView()[0].size(), 8u);
  ASSERT_EQ(Flatten(chunks->GetView()), chunk);
}

TEST_F(WebSocketMessageChunkAccumulatorTest, AppendChunkWithInternalChunkSize) {
  auto task_runner = base::MakeRefCounted<FakeTaskRunner>();
  WebSocketMessageChunkAccumulator* chunks =
      MakeGarbageCollected<WebSocketMessageChunkAccumulator>(task_runner);
  chunks->SetTaskRunnerForTesting(task_runner, task_runner->GetMockTickClock());

  Vector<char> chunk(kSegmentSize, 'y');

  chunks->Append(base::make_span(chunk));

  EXPECT_EQ(chunks->GetSize(), chunk.size());
  ASSERT_EQ(chunks->GetView().size(), 1u);
  ASSERT_EQ(chunks->GetView()[0].size(), kSegmentSize);
  ASSERT_EQ(Flatten(chunks->GetView()), chunk);
}

TEST_F(WebSocketMessageChunkAccumulatorTest, AppendLargeChunk) {
  auto task_runner = base::MakeRefCounted<FakeTaskRunner>();
  WebSocketMessageChunkAccumulator* chunks =
      MakeGarbageCollected<WebSocketMessageChunkAccumulator>(task_runner);
  chunks->SetTaskRunnerForTesting(task_runner, task_runner->GetMockTickClock());

  Vector<char> chunk(kSegmentSize * 2 + 2, 'y');

  chunks->Append(base::make_span(chunk));

  EXPECT_EQ(chunks->GetSize(), chunk.size());
  ASSERT_EQ(chunks->GetView().size(), 3u);
  ASSERT_EQ(chunks->GetView()[0].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[1].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[2].size(), 2u);
  ASSERT_EQ(Flatten(chunks->GetView()), chunk);
}

TEST_F(WebSocketMessageChunkAccumulatorTest, AppendRepeatedly) {
  auto task_runner = base::MakeRefCounted<FakeTaskRunner>();
  WebSocketMessageChunkAccumulator* chunks =
      MakeGarbageCollected<WebSocketMessageChunkAccumulator>(task_runner);
  chunks->SetTaskRunnerForTesting(task_runner, task_runner->GetMockTickClock());

  Vector<char> chunk1(8, 'a');
  Vector<char> chunk2(4, 'b');
  Vector<char> chunk3;  // empty
  Vector<char> chunk4(kSegmentSize * 3 - 12, 'd');
  Vector<char> chunk5(6, 'e');
  Vector<char> chunk6(kSegmentSize - 5, 'f');

  // This will grow over time.
  Vector<char> expected;

  chunks->Append(base::make_span(chunk1));
  expected.AppendVector(chunk1);

  EXPECT_EQ(chunks->GetSize(), expected.size());
  ASSERT_EQ(chunks->GetView().size(), 1u);
  ASSERT_EQ(chunks->GetView()[0].size(), 8u);
  ASSERT_EQ(Flatten(chunks->GetView()), expected);

  chunks->Append(base::make_span(chunk2));
  expected.AppendVector(chunk2);

  EXPECT_EQ(chunks->GetSize(), expected.size());
  ASSERT_EQ(chunks->GetView().size(), 1u);
  ASSERT_EQ(chunks->GetView()[0].size(), 12u);
  ASSERT_EQ(Flatten(chunks->GetView()), expected);

  chunks->Append(base::make_span(chunk3));
  expected.AppendVector(chunk3);

  EXPECT_EQ(chunks->GetSize(), expected.size());
  ASSERT_EQ(chunks->GetView().size(), 1u);
  ASSERT_EQ(chunks->GetView()[0].size(), 12u);
  ASSERT_EQ(Flatten(chunks->GetView()), expected);

  chunks->Append(base::make_span(chunk4));
  expected.AppendVector(chunk4);

  EXPECT_EQ(chunks->GetSize(), expected.size());
  ASSERT_EQ(chunks->GetView().size(), 3u);
  ASSERT_EQ(chunks->GetView()[0].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[1].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[2].size(), kSegmentSize);
  ASSERT_EQ(Flatten(chunks->GetView()), expected);

  chunks->Append(base::make_span(chunk5));
  expected.AppendVector(chunk5);

  EXPECT_EQ(chunks->GetSize(), expected.size());
  ASSERT_EQ(chunks->GetView().size(), 4u);
  ASSERT_EQ(chunks->GetView()[0].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[1].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[2].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[3].size(), 6u);
  ASSERT_EQ(Flatten(chunks->GetView()), expected);

  chunks->Append(base::make_span(chunk6));
  expected.AppendVector(chunk6);

  EXPECT_EQ(chunks->GetSize(), expected.size());
  ASSERT_EQ(chunks->GetView().size(), 5u);
  ASSERT_EQ(chunks->GetView()[0].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[1].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[2].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[3].size(), kSegmentSize);
  ASSERT_EQ(chunks->GetView()[4].size(), 1u);
  ASSERT_EQ(Flatten(chunks->GetView()), expected);
}

TEST_F(WebSocketMessageChunkAccumulatorTest, ClearAndAppend) {
  auto task_runner = base::MakeRefCounted<FakeTaskRunner>();
  WebSocketMessageChunkAccumulator* chunks =
      MakeGarbageCollected<WebSocketMessageChunkAccumulator>(task_runner);
  chunks->SetTaskRunnerForTesting(task_runner, task_runner->GetMockTickClock());

  Vector<char> chunk1(8, 'x');
  Vector<char> chunk2(3, 'y');

  chunks->Clear();

  EXPECT_EQ(chunks->GetSize(), 0u);
  ASSERT_EQ(chunks->GetView().size(), 0u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 0u);

  chunks->Append(base::make_span(chunk1));

  EXPECT_EQ(chunks->GetSize(), 8u);
  ASSERT_EQ(chunks->GetView().size(), 1u);
  ASSERT_EQ(Flatten(chunks->GetView()), chunk1);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 0u);

  chunks->Clear();

  EXPECT_EQ(chunks->GetSize(), 0u);
  ASSERT_EQ(chunks->GetView().size(), 0u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 1u);

  chunks->Append(base::make_span(chunk2));

  EXPECT_EQ(chunks->GetSize(), 3u);
  ASSERT_EQ(chunks->GetView().size(), 1u);
  ASSERT_EQ(Flatten(chunks->GetView()), chunk2);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 0u);
}

TEST_F(WebSocketMessageChunkAccumulatorTest, ClearTimer) {
  auto task_runner = base::MakeRefCounted<FakeTaskRunner>();
  WebSocketMessageChunkAccumulator* chunks =
      MakeGarbageCollected<WebSocketMessageChunkAccumulator>(task_runner);
  chunks->SetTaskRunnerForTesting(task_runner, task_runner->GetMockTickClock());

  Vector<char> chunk1(kSegmentSize * 4, 'x');
  Vector<char> chunk2(kSegmentSize * 3, 'x');
  Vector<char> chunk3(kSegmentSize * 1, 'x');

  // We don't start the timer because GetPoolSizeForTesting() is 0.
  chunks->Clear();
  EXPECT_FALSE(chunks->IsTimerActiveForTesting());

  EXPECT_EQ(chunks->GetSize(), 0u);
  ASSERT_EQ(chunks->GetView().size(), 0u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 0u);

  chunks->Append(base::make_span(chunk1));

  ASSERT_EQ(chunks->GetView().size(), 4u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 0u);

  // We start the timer here.
  // |num_pooled_segments_to_be_removed_| is 4.
  chunks->Clear();
  EXPECT_TRUE(chunks->IsTimerActiveForTesting());

  ASSERT_EQ(chunks->GetView().size(), 0u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 4u);

  chunks->Append(base::make_span(chunk2));

  ASSERT_EQ(chunks->GetView().size(), 3u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 1u);

  // We don't start the timer because it's already active.
  // |num_pooled_segments_to_be_removed_| is set to 1.
  chunks->Clear();
  EXPECT_TRUE(chunks->IsTimerActiveForTesting());

  ASSERT_EQ(chunks->GetView().size(), 0u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 4u);

  // We remove 1 chunk from |pooled_segments_|.
  // We start the timer because |num_pooled_segments_to_be_removed_| > 0.
  task_runner->AdvanceTimeAndRun(kFreeDelay);
  EXPECT_TRUE(chunks->IsTimerActiveForTesting());

  ASSERT_EQ(chunks->GetView().size(), 0u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 3u);

  chunks->Append(base::make_span(chunk3));

  ASSERT_EQ(chunks->GetView().size(), 1u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 2u);

  // We remove 2 chunks from |pooled_segments_|.
  // |num_pooled_segments_to_be_removed_| is 3 but we only have 2 pooled
  // segments. We don't start the timer because we don't have pooled
  // segments any more.
  task_runner->AdvanceTimeAndRun(kFreeDelay);
  EXPECT_FALSE(chunks->IsTimerActiveForTesting());

  ASSERT_EQ(chunks->GetView().size(), 1u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 0u);

  // We start the timer here. num_pooled_segments_to_be_removed_ is set to 1.
  chunks->Clear();
  EXPECT_TRUE(chunks->IsTimerActiveForTesting());

  ASSERT_EQ(chunks->GetView().size(), 0u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 1u);

  // We remove 1 chunk from |pooled_segments_|.
  // We don't start the timer because we don't have pooled segments any more.
  task_runner->AdvanceTimeAndRun(kFreeDelay);
  EXPECT_FALSE(chunks->IsTimerActiveForTesting());

  ASSERT_EQ(chunks->GetView().size(), 0u);
  EXPECT_EQ(chunks->GetPoolSizeForTesting(), 0u);
}

}  // namespace

}  // namespace blink
