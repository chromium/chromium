// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

#include <memory>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrictMock;
using ::testing::Truly;

// This test uses actual threads since mutator logic requires it. This means we
// have dependency on Blink platform to create threads.

namespace blink {
namespace {

std::unique_ptr<Thread> CreateThread(const char* name) {
  return Platform::Current()->CreateThread(
      ThreadCreationParams(ThreadType::kTestThread).SetThreadNameForTest(name));
}

class MockAnimationWorkletMutator
    : public GarbageCollected<MockAnimationWorkletMutator>,
      public AnimationWorkletMutator {
  USING_GARBAGE_COLLECTED_MIXIN(MockAnimationWorkletMutator);

 public:
  MockAnimationWorkletMutator(
      scoped_refptr<base::SingleThreadTaskRunner> expected_runner)
      : expected_runner_(expected_runner) {}

  ~MockAnimationWorkletMutator() override {}

  std::unique_ptr<AnimationWorkletOutput> Mutate(
      std::unique_ptr<AnimationWorkletInput> input) override {
    return std::unique_ptr<AnimationWorkletOutput>(MutateRef(*input));
  }

  // Blocks the worklet thread by posting a task that will complete only when
  // signaled. This blocking ensures that tests of async mutations do not
  // encounter race conditions when validating queuing strategies.
  void BlockWorkletThread() {
    PostCrossThreadTask(
        *expected_runner_, FROM_HERE,
        CrossThreadBindOnce(
            [](base::WaitableEvent* start_processing_event) {
              start_processing_event->Wait();
            },
            WTF::CrossThreadUnretained(&start_processing_event_)));
  }

  void UnblockWorkletThread() { start_processing_event_.Signal(); }

  MOCK_CONST_METHOD0(GetWorkletId, int());
  MOCK_METHOD1(MutateRef,
               AnimationWorkletOutput*(const AnimationWorkletInput&));

  scoped_refptr<base::SingleThreadTaskRunner> expected_runner_;
  base::WaitableEvent start_processing_event_;
};

class MockCompositorMutatorClient : public CompositorMutatorClient {
 public:
  MockCompositorMutatorClient(
      std::unique_ptr<AnimationWorkletMutatorDispatcherImpl> mutator)
      : CompositorMutatorClient(std::move(mutator)) {}
  ~MockCompositorMutatorClient() override {}
  // gmock cannot mock methods with move-only args so we forward it to ourself.
  void SetMutationUpdate(
      std::unique_ptr<cc::MutatorOutputState> output_state) override {
    SetMutationUpdateRef(output_state.get());
  }

  MOCK_METHOD1(SetMutationUpdateRef,
               void(cc::MutatorOutputState* output_state));
};

class AnimationWorkletMutatorDispatcherImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto mutator = std::make_unique<AnimationWorkletMutatorDispatcherImpl>(
        /*main_thread_task_runner=*/true);
    mutator_ = mutator.get();
    client_ =
        std::make_unique<::testing::StrictMock<MockCompositorMutatorClient>>(
            std::move(mutator));
  }

  void TearDown() override { mutator_ = nullptr; }

  std::unique_ptr<::testing::StrictMock<MockCompositorMutatorClient>> client_;
  AnimationWorkletMutatorDispatcherImpl* mutator_;
};

std::unique_ptr<AnimationWorkletDispatcherInput> CreateTestMutatorInput() {
  AnimationWorkletInput::AddAndUpdateState state1{
      {11, 1}, "test1", 5000, nullptr, nullptr};

  AnimationWorkletInput::AddAndUpdateState state2{
      {22, 2}, "test2", 5000, nullptr, nullptr};

  auto input = std::make_unique<AnimationWorkletDispatcherInput>();
  input->Add(std::move(state1));
  input->Add(std::move(state2));

  return input;
}

bool OnlyIncludesAnimation1(const AnimationWorkletInput& in) {
  return in.added_and_updated_animations.size() == 1 &&
         in.added_and_updated_animations[0].worklet_animation_id.animation_id ==
             1;
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       RegisteredAnimatorShouldOnlyReceiveInputForItself) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(Truly(OnlyIncludesAnimation1)))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);
  mutator_->MutateSynchronously(CreateTestMutatorInput());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       RegisteredAnimatorShouldNotBeMutatedWhenNoInput) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);

  AnimationWorkletInput::AddAndUpdateState state{
      {22, 2}, "test2", 5000, nullptr, nullptr};

  auto input = std::make_unique<AnimationWorkletDispatcherInput>();
  input->Add(std::move(state));

  mutator_->MutateSynchronously(std::move(input));
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsNotInvokedWithNoRegisteredAnimators) {
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  std::unique_ptr<AnimationWorkletDispatcherInput> input =
      std::make_unique<AnimationWorkletDispatcherInput>();
  mutator_->MutateSynchronously(std::move(input));
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsNotInvokedWithNullOutput) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(1).WillOnce(Return(nullptr));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  mutator_->MutateSynchronously(CreateTestMutatorInput());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsInvokedCorrectlyWithSingleRegisteredAnimator) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);
  mutator_->MutateSynchronously(CreateTestMutatorInput());

  // The above call blocks on mutator threads running their tasks so we can
  // safely verify here.
  Mock::VerifyAndClearExpectations(client_.get());

  // Ensure mutator is not invoked after unregistration.
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  mutator_->UnregisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator));

  mutator_->MutateSynchronously(CreateTestMutatorInput());
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnSameThread) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  MockAnimationWorkletMutator* second_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(second_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  mutator_->MutateSynchronously(CreateTestMutatorInput());
}

TEST_F(
    AnimationWorkletMutatorDispatcherImplTest,
    MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnDifferentThreads) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  std::unique_ptr<Thread> second_thread = CreateThread("SecondAnimationThread");
  MockAnimationWorkletMutator* second_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          second_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(second_mutator),
      second_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  mutator_->MutateSynchronously(CreateTestMutatorInput());

  // The above call blocks on mutator threads running their tasks so we can
  // safely verify here.
  Mock::VerifyAndClearExpectations(client_.get());

  // Ensure first_mutator is not invoked after unregistration.
  mutator_->UnregisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator));

  EXPECT_CALL(*first_mutator, GetWorkletId()).Times(0);
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);
  mutator_->MutateSynchronously(CreateTestMutatorInput());

  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       DispatcherShouldNotHangWhenMutatorGoesAway) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId()).WillRepeatedly(Return(11));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);

  // Shutdown the thread so its task runner no longer executes tasks.
  first_thread.reset();

  mutator_->MutateSynchronously(CreateTestMutatorInput());

  Mock::VerifyAndClearExpectations(client_.get());
}

// -----------------------------------------------------------------------
// Asynchronous version of tests.

using MutatorDispatcherRef =
    scoped_refptr<AnimationWorkletMutatorDispatcherImpl>;

class AnimationWorkletMutatorDispatcherImplAsyncTest
    : public AnimationWorkletMutatorDispatcherImplTest {
 public:
  AnimationWorkletMutatorDispatcher::AsyncMutationCompleteCallback
  CreateIntermediateResultCallback(MutateStatus expected_result) {
    return CrossThreadBindOnce(
        &AnimationWorkletMutatorDispatcherImplAsyncTest ::
            VerifyExpectedMutationResult,
        CrossThreadUnretained(this), expected_result);
  }

  AnimationWorkletMutatorDispatcher::AsyncMutationCompleteCallback
  CreateNotReachedCallback() {
    return CrossThreadBindOnce([](MutateStatus unused) {
      NOTREACHED() << "Mutate complete callback should not have been triggered";
    });
  }

  AnimationWorkletMutatorDispatcher::AsyncMutationCompleteCallback
  CreateTestCompleteCallback(
      MutateStatus expected_result = MutateStatus::kCompletedWithUpdate) {
    return CrossThreadBindOnce(
        &AnimationWorkletMutatorDispatcherImplAsyncTest ::
            VerifyCompletedMutationResultAndFinish,
        CrossThreadUnretained(this), expected_result);
  }

  // Executes run loop until quit closure is called.
  void WaitForTestCompletion() { run_loop_.Run(); }

  void VerifyExpectedMutationResult(MutateStatus expectation,
                                    MutateStatus result) {
    EXPECT_EQ(expectation, result);
    IntermediateResultCallbackRef();
  }

  void VerifyCompletedMutationResultAndFinish(MutateStatus expectation,
                                              MutateStatus result) {
    EXPECT_EQ(expectation, result);
    run_loop_.Quit();
  }

  // Verifying that intermediate result callbacks are invoked the correct number
  // of times.
  MOCK_METHOD0(IntermediateResultCallbackRef, void());

  static const MutateQueuingStrategy kNormalPriority =
      MutateQueuingStrategy::kQueueAndReplaceNormalPriority;

  static const MutateQueuingStrategy kHighPriority =
      MutateQueuingStrategy::kQueueHighPriority;

 private:
  base::RunLoop run_loop_;
};

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       RegisteredAnimatorShouldOnlyReceiveInputForItself) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);

  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       RegisteredAnimatorShouldNotBeMutatedWhenNoInput) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  AnimationWorkletInput::AddAndUpdateState state{
      {22, 2}, "test2", 5000, nullptr, nullptr};

  auto input = std::make_unique<AnimationWorkletDispatcherInput>();
  input->Add(std::move(state));

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));

  EXPECT_FALSE(mutator_->MutateAsynchronously(std::move(input), kNormalPriority,
                                              CreateNotReachedCallback()));
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateIsNotInvokedWithNoRegisteredAnimators) {
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  std::unique_ptr<AnimationWorkletDispatcherInput> input =
      std::make_unique<AnimationWorkletDispatcherInput>();
  EXPECT_FALSE(mutator_->MutateAsynchronously(std::move(input), kNormalPriority,
                                              CreateNotReachedCallback()));
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateIsNotInvokedWithNullOutput) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(1).WillOnce(Return(nullptr));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);

  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority,
      CreateTestCompleteCallback(MutateStatus::kCompletedNoUpdate)));

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateIsInvokedCorrectlyWithSingleRegisteredAnimator) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);

  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));

  WaitForTestCompletion();

  // Above call blocks until complete signal is received.
  Mock::VerifyAndClearExpectations(client_.get());

  // Ensure mutator is not invoked after unregistration.
  mutator_->UnregisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator));
  EXPECT_FALSE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateNotReachedCallback()));

  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnSameThread) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  MockAnimationWorkletMutator* second_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(second_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);

  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));

  WaitForTestCompletion();
}

TEST_F(
    AnimationWorkletMutatorDispatcherImplAsyncTest,
    MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnDifferentThreads) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  std::unique_ptr<Thread> second_thread = CreateThread("SecondAnimationThread");
  MockAnimationWorkletMutator* second_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          second_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(second_mutator),
      second_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);

  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateDroppedWhenBusy) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);

  // Block Responses until all requests have been queued.
  first_mutator->BlockWorkletThread();
  // Response for first mutator call is blocked until after the second
  // call is sent.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));
  // Second request dropped since busy processing first.
  EXPECT_FALSE(mutator_->MutateAsynchronously(CreateTestMutatorInput(),
                                              MutateQueuingStrategy::kDrop,
                                              CreateNotReachedCallback()));
  // Unblock first request.
  first_mutator->UnblockWorkletThread();

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateQueuedWhenBusy) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");

  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(2))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(2)
      .WillOnce(Return(new AnimationWorkletOutput()))
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  EXPECT_CALL(*this, IntermediateResultCallbackRef()).Times(1);

  // Block Responses until all requests have been queued.
  first_mutator->BlockWorkletThread();
  // Response for first mutator call is blocked until after the second
  // call is sent.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority,
      CreateIntermediateResultCallback(MutateStatus::kCompletedWithUpdate)));
  // First request still processing, queue request.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));
  // Unblock first request.
  first_mutator->UnblockWorkletThread();

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateQueueWithReplacementWhenBusy) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");

  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(2))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(2)
      .WillOnce(Return(new AnimationWorkletOutput()))
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  EXPECT_CALL(*this, IntermediateResultCallbackRef()).Times(2);

  // Block Responses until all requests have been queued.
  first_mutator->BlockWorkletThread();
  // Response for first mutator call is blocked until after the second
  // call is sent.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority,
      CreateIntermediateResultCallback(MutateStatus::kCompletedWithUpdate)));
  // First request still processing, queue a second request, which will get
  // canceled by a third request.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority,
      CreateIntermediateResultCallback(MutateStatus::kCanceled)));
  // First request still processing, clobber second request in queue.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));
  // Unblock first request.
  first_mutator->UnblockWorkletThread();

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateMultipleQueuesWhenBusy) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");

  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(
      WrapCrossThreadPersistent(first_mutator), first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(3))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(3)
      .WillOnce(Return(new AnimationWorkletOutput()))
      .WillOnce(Return(new AnimationWorkletOutput()))
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(3);
  EXPECT_CALL(*this, IntermediateResultCallbackRef()).Times(2);

  // Block Responses until all requests have been queued.
  first_mutator->BlockWorkletThread();
  // Response for first mutator call is blocked until after the second
  // call is sent.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority,
      CreateIntermediateResultCallback(MutateStatus::kCompletedWithUpdate)));
  // First request still processing, queue a second request.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));
  // First request still processing. This request uses a separate queue from the
  // second request. It should not replace the second request but should be
  // dispatched ahead of the second request.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kHighPriority,
      CreateIntermediateResultCallback(MutateStatus::kCompletedWithUpdate)));
  // Unblock first request.
  first_mutator->UnblockWorkletThread();

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest, HistogramTester) {
  const char* histogram_name =
      "Animation.AnimationWorklet.Dispatcher.AsynchronousMutateDuration";
  base::HistogramTester histogram_tester;

  std::unique_ptr<base::TickClock> mock_clock =
      std::make_unique<base::SimpleTestTickClock>();
  base::SimpleTestTickClock* mock_clock_ptr =
      static_cast<base::SimpleTestTickClock*>(mock_clock.get());
  mutator_->SetClockForTesting(std::move(mock_clock));

  std::unique_ptr<Thread> thread = CreateThread("MyThread");
  MockAnimationWorkletMutator* mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(WrapCrossThreadPersistent(mutator),
                                            thread->GetTaskRunner());

  EXPECT_CALL(*mutator, GetWorkletId())
      .Times(AtLeast(2))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*mutator, MutateRef(_))
      .Times(2)
      .WillOnce(Return(new AnimationWorkletOutput()))
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);

  // Block Responses until all requests have been queued.
  mutator->BlockWorkletThread();

  base::TimeDelta time_delta = base::TimeDelta::FromMilliseconds(10);

  // Expected Elapsed time is the sum of all clock advancements until unblocked,
  // which totals to 30 ms.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kHighPriority,
      CreateIntermediateResultCallback(MutateStatus::kCompletedWithUpdate)));
  mock_clock_ptr->Advance(time_delta);

  // This request will get stomped by the next request, but the start time is
  // preserved.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority,
      CreateIntermediateResultCallback(MutateStatus::kCanceled)));
  mock_clock_ptr->Advance(time_delta);

  // Replaces previous request. Since 10 ms has elapsed prior to replacing the
  // previous request, the expected elapsed time is 20 ms.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), kNormalPriority, CreateTestCompleteCallback()));
  mock_clock_ptr->Advance(time_delta);

  mutator->UnblockWorkletThread();
  WaitForTestCompletion();

  histogram_tester.ExpectTotalCount(histogram_name, 2);
  // Times are in microseconds.
  histogram_tester.ExpectBucketCount(histogram_name, 20000, 1);
  histogram_tester.ExpectBucketCount(histogram_name, 30000, 1);
}

}  // namespace

}  // namespace blink
