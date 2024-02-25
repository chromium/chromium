// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_worklet_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace blink {
namespace {
// We need a thread (or multiple threads) for the (mock) worklets to run on.
std::unique_ptr<NonMainThread> CreateTestThread(const char* name) {
  return NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread).SetThreadNameForTest(name));
}

class PaintWorkletPaintDispatcherAsyncTest : public ::testing::Test {
 public:
  PlatformPaintWorkletLayerPainter::DoneCallback CreateTestCompleteCallback() {
    return base::BindOnce(
        &PaintWorkletPaintDispatcherAsyncTest::VerifyResultAndFinish,
        base::Unretained(this));
  }

  // Allows a test to block on |VerifyResultAndFinish| being called. If a
  // PaintWorkletPaintDispatcherAsyncTest test times out, it likely means the
  // callback created by |CreateTestCompleteCallback| was never posted by the
  // worklet thread.
  void WaitForTestCompletion() { run_loop_.Run(); }

 private:
  void VerifyResultAndFinish(cc::PaintWorkletJobMap results) {
    run_loop_.Quit();
  }

  test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
};

class MockPaintWorkletPainter
    : public GarbageCollected<MockPaintWorkletPainter>,
      public PaintWorkletPainter {
 public:
  MockPaintWorkletPainter(int worklet_id) {
    ON_CALL(*this, GetWorkletId).WillByDefault(Return(worklet_id));
  }
  ~MockPaintWorkletPainter() override = default;

  MOCK_CONST_METHOD0(GetWorkletId, int());
  MOCK_METHOD2(Paint,
               PaintRecord(const cc::PaintWorkletInput*,
                           const cc::PaintWorkletJob::AnimatedPropertyValues&));
};

class MockPaintWorkletInput : public cc::PaintWorkletInput {
 public:
  explicit MockPaintWorkletInput(int worklet_id) {
    ON_CALL(*this, WorkletId).WillByDefault(Return(worklet_id));
  }
  ~MockPaintWorkletInput() override = default;

  MOCK_CONST_METHOD0(GetSize, gfx::SizeF());
  MOCK_CONST_METHOD0(WorkletId, int());
  MOCK_CONST_METHOD0(GetPropertyKeys,
                     const std::vector<PaintWorkletInput::PropertyKey>&());
  MOCK_CONST_METHOD0(IsCSSPaintWorkletInput, bool());
};

cc::PaintWorkletInput* AddPaintWorkletInputToMap(cc::PaintWorkletJobMap& map,
                                                 int worklet_id) {
  if (!map.contains(worklet_id))
    map[worklet_id] = base::MakeRefCounted<cc::PaintWorkletJobVector>();
  auto input = base::MakeRefCounted<MockPaintWorkletInput>(worklet_id);
  MockPaintWorkletInput* input_ptr = input.get();
  cc::PaintWorkletJob::AnimatedPropertyValues animated_property_values;
  map[worklet_id]->data.emplace_back(/*layer_id=*/1, std::move(input),
                                     animated_property_values);
  return input_ptr;
}

class PaintWorkletPaintDispatcherMainThread
    : public PaintWorkletPaintDispatcher {
 protected:
  scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner()
      override {
    // There is no compositor thread in testing, so return the current thread.
    return scheduler::GetSingleThreadTaskRunnerForTesting();
  }
};

}  // namespace

TEST_F(PaintWorkletPaintDispatcherAsyncTest, DispatchedWorkletIsPainted) {
  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcherMainThread>();

  const int worklet_id = 4;
  MockPaintWorkletPainter* mock_painter =
      MakeGarbageCollected<NiceMock<MockPaintWorkletPainter>>(worklet_id);
  std::unique_ptr<NonMainThread> worklet_thread =
      CreateTestThread("WorkletThread");
  dispatcher->RegisterPaintWorkletPainter(mock_painter,
                                          worklet_thread->GetTaskRunner());

  cc::PaintWorkletJobMap job_map;
  Vector<cc::PaintWorkletInput*> inputs = {
      AddPaintWorkletInputToMap(job_map, worklet_id),
      AddPaintWorkletInputToMap(job_map, worklet_id),
      AddPaintWorkletInputToMap(job_map, worklet_id),
  };

  // The input jobs match the registered painter, so we should see a series of
  // calls to Paint() with the appropriate PaintWorkletInputs.
  for (cc::PaintWorkletInput* input : inputs)
    EXPECT_CALL(*mock_painter, Paint(input, _)).Times(1);
  dispatcher->DispatchWorklets(job_map, CreateTestCompleteCallback());

  WaitForTestCompletion();
}

TEST_F(PaintWorkletPaintDispatcherAsyncTest, DispatchCompletesWithNoPainters) {
  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcherMainThread>();

  cc::PaintWorkletJobMap job_map;
  AddPaintWorkletInputToMap(job_map, /*worklet_id=*/2);
  AddPaintWorkletInputToMap(job_map, /*worklet_id=*/2);
  AddPaintWorkletInputToMap(job_map, /*worklet_id=*/5);

  // There are no painters to dispatch to, matching or otherwise, but the
  // callback should still be called so this test passes if it doesn't hang on
  // WaitForTestCompletion.
  dispatcher->DispatchWorklets(job_map, CreateTestCompleteCallback());

  WaitForTestCompletion();
}

TEST_F(PaintWorkletPaintDispatcherAsyncTest, DispatchHandlesEmptyInput) {
  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcherMainThread>();

  const int worklet_id = 4;
  auto* mock_painter =
      MakeGarbageCollected<NiceMock<MockPaintWorkletPainter>>(worklet_id);
  std::unique_ptr<NonMainThread> worklet_thread =
      CreateTestThread("WorkletThread");
  dispatcher->RegisterPaintWorkletPainter(mock_painter,
                                          worklet_thread->GetTaskRunner());

  cc::PaintWorkletJobMap job_map;

  // The input job map is empty, so we should see no calls to Paint but the
  // callback should still be called.
  EXPECT_CALL(*mock_painter, Paint(_, _)).Times(0);
  dispatcher->DispatchWorklets(job_map, CreateTestCompleteCallback());

  WaitForTestCompletion();
}

TEST_F(PaintWorkletPaintDispatcherAsyncTest, DispatchSelectsCorrectPainter) {
  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcherMainThread>();

  const int first_worklet_id = 2;
  auto* first_mock_painter =
      MakeGarbageCollected<NiceMock<MockPaintWorkletPainter>>(first_worklet_id);
  std::unique_ptr<NonMainThread> first_thread =
      CreateTestThread("WorkletThread1");
  dispatcher->RegisterPaintWorkletPainter(first_mock_painter,
                                          first_thread->GetTaskRunner());

  const int second_worklet_id = 3;
  auto* second_mock_painter =
      MakeGarbageCollected<NiceMock<MockPaintWorkletPainter>>(
          second_worklet_id);
  std::unique_ptr<NonMainThread> second_thread =
      CreateTestThread("WorkletThread2");
  dispatcher->RegisterPaintWorkletPainter(second_mock_painter,
                                          second_thread->GetTaskRunner());

  cc::PaintWorkletJobMap job_map;
  Vector<cc::PaintWorkletInput*> inputs{
      AddPaintWorkletInputToMap(job_map, second_worklet_id),
      AddPaintWorkletInputToMap(job_map, second_worklet_id),
  };

  // Paint should only be called on the correct painter, with our input.
  EXPECT_CALL(*first_mock_painter, Paint(_, _)).Times(0);
  for (cc::PaintWorkletInput* input : inputs) {
    EXPECT_CALL(*second_mock_painter, Paint(input, _)).Times(1);
  }
  dispatcher->DispatchWorklets(job_map, CreateTestCompleteCallback());

  WaitForTestCompletion();
}

TEST_F(PaintWorkletPaintDispatcherAsyncTest, DispatchIgnoresNonMatchingInput) {
  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcherMainThread>();

  const int worklet_id = 2;
  auto* mock_painter =
      MakeGarbageCollected<NiceMock<MockPaintWorkletPainter>>(worklet_id);
  std::unique_ptr<NonMainThread> worklet_thread =
      CreateTestThread("WorkletThread");
  dispatcher->RegisterPaintWorkletPainter(mock_painter,
                                          worklet_thread->GetTaskRunner());

  cc::PaintWorkletJobMap job_map;
  const int non_registered_worklet_id = 3;
  cc::PaintWorkletInput* matching_input =
      AddPaintWorkletInputToMap(job_map, worklet_id);
  AddPaintWorkletInputToMap(job_map, non_registered_worklet_id);

  // Only one job matches, so our painter should only be called once, and the
  // callback should still be called.
  EXPECT_CALL(*mock_painter, Paint(matching_input, _)).Times(1);
  dispatcher->DispatchWorklets(job_map, CreateTestCompleteCallback());

  WaitForTestCompletion();
}

TEST_F(PaintWorkletPaintDispatcherAsyncTest,
       DispatchCorrectlyAssignsInputsToMultiplePainters) {
  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcherMainThread>();

  const int first_worklet_id = 5;
  auto* first_mock_painter =
      MakeGarbageCollected<NiceMock<MockPaintWorkletPainter>>(first_worklet_id);
  std::unique_ptr<NonMainThread> first_thread =
      CreateTestThread("WorkletThread1");
  dispatcher->RegisterPaintWorkletPainter(first_mock_painter,
                                          first_thread->GetTaskRunner());

  const int second_worklet_id = 1;
  auto* second_mock_painter =
      MakeGarbageCollected<NiceMock<MockPaintWorkletPainter>>(
          second_worklet_id);
  std::unique_ptr<NonMainThread> second_thread =
      CreateTestThread("WorkletThread2");
  dispatcher->RegisterPaintWorkletPainter(second_mock_painter,
                                          second_thread->GetTaskRunner());

  cc::PaintWorkletJobMap job_map;
  cc::PaintWorkletInput* first_input =
      AddPaintWorkletInputToMap(job_map, first_worklet_id);
  cc::PaintWorkletInput* second_input =
      AddPaintWorkletInputToMap(job_map, second_worklet_id);

  // Both painters should be called with the correct inputs.
  EXPECT_CALL(*first_mock_painter, Paint(first_input, _)).Times(1);
  EXPECT_CALL(*second_mock_painter, Paint(second_input, _)).Times(1);
  dispatcher->DispatchWorklets(job_map, CreateTestCompleteCallback());

  WaitForTestCompletion();
}

TEST_F(PaintWorkletPaintDispatcherAsyncTest,
       HasOngoingDispatchIsTrackedCorrectly) {
  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcherMainThread>();

  const int first_worklet_id = 2;
  auto* first_mock_painter =
      MakeGarbageCollected<NiceMock<MockPaintWorkletPainter>>(first_worklet_id);
  std::unique_ptr<NonMainThread> first_thread =
      CreateTestThread("WorkletThread1");
  dispatcher->RegisterPaintWorkletPainter(first_mock_painter,
                                          first_thread->GetTaskRunner());

  // Nothing going on; no dispatch.
  EXPECT_FALSE(dispatcher->HasOngoingDispatch());

  cc::PaintWorkletJobMap job_map;
  AddPaintWorkletInputToMap(job_map, first_worklet_id);

  dispatcher->DispatchWorklets(job_map, CreateTestCompleteCallback());
  EXPECT_TRUE(dispatcher->HasOngoingDispatch());

  WaitForTestCompletion();
  EXPECT_FALSE(dispatcher->HasOngoingDispatch());
}

}  // namespace blink
