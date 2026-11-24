// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation_frame/worker_animation_frame_provider.h"

#include "components/viz/test/begin_frame_args_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider_params.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

class CountingFrameCallback : public FrameCallback {
 public:
  void Invoke(double) override { ++invocations_; }
  int invocations() const { return invocations_; }

 private:
  int invocations_ = 0;
};

}  // namespace

class WorkerAnimationFrameProviderTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

// BeginFrame on a paused context must not invoke queued rAF callbacks.
TEST_F(WorkerAnimationFrameProviderTest, DropsBeginFrameWhenPaused) {
  ScopedNullExecutionContext scoped_context;
  NullExecutionContext& context = scoped_context.GetExecutionContext();

  auto* provider = MakeGarbageCollected<WorkerAnimationFrameProvider>(
      &context, BeginFrameProviderParams{});

  auto* callback = MakeGarbageCollected<CountingFrameCallback>();
  provider->callback_collection_.RegisterFrameCallback(
      callback, FrameCallbackType::kWebExposed);

  context.SetLifecycleState(mojom::blink::FrameLifecycleState::kPaused);

  provider->BeginFrame(
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1));

  EXPECT_EQ(callback->invocations(), 0);
  EXPECT_TRUE(provider->callback_collection_.HasFrameCallback());
}

namespace {

// Builds a provider wired to a mock compositor frame sink so we can observe
// SetNeedsBeginFrame calls from `RequestBeginFrame`.
struct MockFrameSinkFixture {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  MockEmbeddedFrameSinkProvider provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider> receiver{&provider};
  std::unique_ptr<TestingPlatformSupport::ScopedOverrideMojoInterface>
      override_;

  MockFrameSinkFixture() {
    provider.set_num_expected_set_needs_begin_frame_on_sink_construction(1);
    override_ = provider.CreateScopedOverrideMojoInterface(&receiver);
  }
};

BeginFrameProviderParams ValidFrameSinkParams() {
  BeginFrameProviderParams params;
  params.parent_frame_sink_id = viz::FrameSinkId(1, 1);
  params.frame_sink_id = viz::FrameSinkId(2, 2);
  return params;
}

}  // namespace

// Registering a callback while paused must defer the compositor request until
// the context resumes, then issue it from the lifecycle handler.
TEST_F(WorkerAnimationFrameProviderTest, ResumeAfterPauseRequestsBeginFrame) {
  MockFrameSinkFixture fixture;

  ScopedNullExecutionContext scoped_context;
  NullExecutionContext& context = scoped_context.GetExecutionContext();
  context.SetLifecycleState(mojom::blink::FrameLifecycleState::kPaused);

  auto* provider = MakeGarbageCollected<WorkerAnimationFrameProvider>(
      &context, ValidFrameSinkParams());

  EXPECT_CALL(fixture.provider, CreateCompositorFrameSink_(testing::_))
      .Times(0);
  auto* callback = MakeGarbageCollected<CountingFrameCallback>();
  ASSERT_GE(provider->RegisterCallback(callback), 0);
  fixture.platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&fixture.provider);

  EXPECT_CALL(fixture.provider, CreateCompositorFrameSink_(testing::_))
      .Times(1);
  context.SetLifecycleState(mojom::blink::FrameLifecycleState::kRunning);
  fixture.platform->RunUntilIdle();
}

// With the kill switch off the paused guard is bypassed, so RegisterCallback
// still asks the compositor for a BeginFrame.
TEST_F(WorkerAnimationFrameProviderTest, FeatureOffIgnoresPausedState) {
  ScopedWorkerRafRunsInBeginFrameTaskForTest feature_off(false);
  MockFrameSinkFixture fixture;

  ScopedNullExecutionContext scoped_context;
  NullExecutionContext& context = scoped_context.GetExecutionContext();
  context.SetLifecycleState(mojom::blink::FrameLifecycleState::kPaused);

  auto* provider = MakeGarbageCollected<WorkerAnimationFrameProvider>(
      &context, ValidFrameSinkParams());

  EXPECT_CALL(fixture.provider, CreateCompositorFrameSink_(testing::_))
      .Times(1);
  auto* callback = MakeGarbageCollected<CountingFrameCallback>();
  ASSERT_GE(provider->RegisterCallback(callback), 0);
  fixture.platform->RunUntilIdle();
}

}  // namespace blink
