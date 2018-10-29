// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_thread_type.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

#include <memory>

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::Return;
using ::testing::Truly;

// This test uses actual threads since mutator logic requires it. This means we
// have dependency on Blink platform to create threads.

namespace blink {
namespace {

std::unique_ptr<Thread> CreateThread(const char* name) {
  return Platform::Current()->CreateThread(
      ThreadCreationParams(WebThreadType::kTestThread)
          .SetThreadNameForTest(name));
}

class MockAnimationWorkletMutator
    : public GarbageCollectedFinalized<MockAnimationWorkletMutator>,
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

  MOCK_CONST_METHOD0(GetScopeId, int());
  MOCK_METHOD1(MutateRef,
               AnimationWorkletOutput*(const AnimationWorkletInput&));

  scoped_refptr<base::SingleThreadTaskRunner> expected_runner_;
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
    auto mutator =
        std::make_unique<AnimationWorkletMutatorDispatcherImpl>(false);
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
      {11, 1}, "test1", 5000, nullptr, 1};

  AnimationWorkletInput::AddAndUpdateState state2{
      {22, 2}, "test2", 5000, nullptr, 1};

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
      new ::testing::StrictMock<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetScopeId()).Times(1).WillOnce(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(Truly(OnlyIncludesAnimation1)))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);

  mutator_->Mutate(CreateTestMutatorInput());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       RegisteredAnimatorShouldNotBeMutatedWhenNoInput) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      new ::testing::StrictMock<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetScopeId()).Times(1).WillOnce(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);

  AnimationWorkletInput::AddAndUpdateState state2{
      {22, 2}, "test2", 5000, nullptr, 1};

  auto input = std::make_unique<AnimationWorkletDispatcherInput>();
  input->Add(std::move(state2));

  mutator_->Mutate(std::move(input));
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsNotInvokedWithNoRegisteredAnimators) {
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  std::unique_ptr<AnimationWorkletDispatcherInput> input =
      std::make_unique<AnimationWorkletDispatcherInput>();
  mutator_->Mutate(std::move(input));
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsNotInvokedWithNullOutput) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      new ::testing::StrictMock<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());
  EXPECT_CALL(*first_mutator, GetScopeId()).Times(1).WillOnce(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(1).WillOnce(Return(nullptr));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  mutator_->Mutate(CreateTestMutatorInput());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsInvokedCorrectlyWithSingleRegisteredAnimator) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      new ::testing::StrictMock<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());
  EXPECT_CALL(*first_mutator, GetScopeId()).Times(1).WillOnce(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);
  mutator_->Mutate(CreateTestMutatorInput());

  // The above call blocks on mutator threads running their tasks so we can
  // safely verify here.
  Mock::VerifyAndClearExpectations(client_.get());

  // Ensure mutator is not invoked after unregistration.
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  mutator_->UnregisterAnimationWorkletMutator(first_mutator);

  mutator_->Mutate(CreateTestMutatorInput());
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnSameThread) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      new ::testing::StrictMock<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  MockAnimationWorkletMutator* second_mutator =
      new ::testing::StrictMock<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(second_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetScopeId()).Times(1).WillOnce(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetScopeId()).Times(1).WillOnce(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));

  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  mutator_->Mutate(CreateTestMutatorInput());
}

TEST_F(
    AnimationWorkletMutatorDispatcherImplTest,
    MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnDifferentThreads) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      new ::testing::StrictMock<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  std::unique_ptr<Thread> second_thread = CreateThread("SecondAnimationThread");
  MockAnimationWorkletMutator* second_mutator =
      new ::testing::StrictMock<MockAnimationWorkletMutator>(
          second_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(second_mutator,
                                            second_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetScopeId()).Times(1).WillOnce(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetScopeId()).Times(1).WillOnce(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  mutator_->Mutate(CreateTestMutatorInput());

  // The above call blocks on mutator threads running their tasks so we can
  // safely verify here.
  Mock::VerifyAndClearExpectations(client_.get());

  // Ensure mutator is not invoked after unregistration.
  mutator_->UnregisterAnimationWorkletMutator(first_mutator);

  EXPECT_CALL(*first_mutator, GetScopeId()).Times(0);
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*second_mutator, GetScopeId()).Times(1).WillOnce(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);
  mutator_->Mutate(CreateTestMutatorInput());
  Mock::VerifyAndClearExpectations(client_.get());
}

}  // namespace

}  // namespace blink
