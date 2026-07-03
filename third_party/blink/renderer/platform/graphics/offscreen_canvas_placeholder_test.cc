// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_placeholder.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;
using testing::Mock;
using testing::Test;

namespace blink {

class MockPlaceholderClient : public OffscreenCanvasPlaceholder::Client {
 public:
  explicit MockPlaceholderClient(DOMNodeId placeholder_id)
      : OffscreenCanvasPlaceholder::Client(
            placeholder_id,
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            base::BindRepeating(&MockPlaceholderClient::OnAnimationStateUpdated,
                                base::Unretained(this))) {}

  void OnMainThreadReceivedImage() override {
    MainThreadReceivedImage();
    OffscreenCanvasPlaceholder::Client::OnMainThreadReceivedImage();
  }

  void PostImageToPlaceholder(
      scoped_refptr<ExportedCanvasResource>&& resource) override {
    OffscreenCanvasPlaceholder::Client::PostImageToPlaceholder(
        std::move(resource));
    OnPostImageToPlaceholder();
  }

  MOCK_METHOD0(OnPostImageToPlaceholder, void());
  MOCK_METHOD0(MainThreadReceivedImage, void());
  MOCK_METHOD0(OnAnimationStateUpdated, void());
};

namespace {
DOMNodeId GenPlaceholderId() {
  static DOMNodeId s_id = 0;
  return ++s_id;
}

}  // unnamed namespace

class OffscreenCanvasPlaceholderTest : public Test {
 public:
  OffscreenCanvasPlaceholder* placeholder() { return &placeholder_; }
  MockPlaceholderClient* client() { return placeholder_client_.get(); }

  CanvasResource* DispatchOneFrame();
  scoped_refptr<CanvasResource> DrawSomething();
  void CreateClient();

 protected:
  void SetUp() override;
  void TearDown() override;

  unsigned GetNumPendingPlaceholderResources() {
    return placeholder_client_->num_pending_placeholder_resources_;
  }

  ExportedCanvasResource* GetLatestUnpostedImage() {
    return placeholder_client_->latest_unposted_resource_.get();
  }

 private:
  test::TaskEnvironment task_environment_;
  OffscreenCanvasPlaceholder placeholder_;
  std::unique_ptr<MockPlaceholderClient> placeholder_client_;
  std::unique_ptr<CanvasNon2DResourceProvider> resource_provider_;
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider_;
  DOMNodeId placeholder_id_ = 0;
};

void OffscreenCanvasPlaceholderTest::SetUp() {
  Test::SetUp();
  test_web_shared_image_interface_provider_ =
      TestWebGraphicsSharedImageInterfaceProvider::Create();

  placeholder_id_ = GenPlaceholderId();
  placeholder_.RegisterPlaceholderCanvas(placeholder_id_);
}

void OffscreenCanvasPlaceholderTest::TearDown() {
  resource_provider_.reset();
  placeholder_.UnregisterPlaceholderCanvas();
  Test::TearDown();
}

void OffscreenCanvasPlaceholderTest::CreateClient() {
  placeholder_client_ =
      std::make_unique<MockPlaceholderClient>(placeholder_id_);
  resource_provider_ = CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
      gfx::Size(10, 10), GetN32FormatForCanvas(), kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(),
      test_web_shared_image_interface_provider_.get());
}

scoped_refptr<CanvasResource> OffscreenCanvasPlaceholderTest::DrawSomething() {
  return resource_provider_->DoExternalOverdrawAndProduceResource(
      [](cc::PaintCanvas& canvas) { canvas.clear(SkColors::kWhite); });
}

CanvasResource* OffscreenCanvasPlaceholderTest::DispatchOneFrame() {
  scoped_refptr<CanvasResource> resource = DrawSomething();
  CanvasResource* resource_raw_ptr = resource.get();
  placeholder_client_->DispatchFrame(
      base::MakeRefCounted<ExportedCanvasResource>(std::move(resource)));
  // We avoid holding a ref here to avoid interfering with
  // OffscreenCanvasPlaceholder's ref count logic.  This pointer should only
  // be used for validations.
  return resource_raw_ptr;
}

namespace {

TEST_F(OffscreenCanvasPlaceholderTest, OldFrameCleared) {
  // This test verifies that OffscreenCanvasPlaceholder clears
  // the previous frame when it receives a new one.
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  CreateClient();

  DrawSomething();
  CanvasResource* frame1_raw_ptr = DispatchOneFrame();

  EXPECT_CALL(*(client()), MainThreadReceivedImage()).Times(1);
  // Run task that propagates the frame to the placeholder canvas.
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame(), nullptr);
  platform->RunUntilIdle();
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame()->GetResourceForTesting(),
            frame1_raw_ptr);
  Mock::VerifyAndClearExpectations(client());

  EXPECT_CALL(*(client()), MainThreadReceivedImage()).Times(0);
  DrawSomething();
  CanvasResource* frame2_raw_ptr = DispatchOneFrame();
  Mock::VerifyAndClearExpectations(client());

  EXPECT_CALL(*(client()), MainThreadReceivedImage()).Times(1);
  // Propagate second frame to the placeholder, causing frame 1 to be
  // cleared.
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame()->GetResourceForTesting(),
            frame1_raw_ptr);
  platform->RunUntilIdle();
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame()->GetResourceForTesting(),
            frame2_raw_ptr);
  Mock::VerifyAndClearExpectations(client());
}

TEST_F(OffscreenCanvasPlaceholderTest, OldFrameClearedWithExtraRef) {
  // This test verifies that OffscreenCanvasPlaceholder clears
  // the previous frame when it receives a new one regardless of whether there
  // is another ref on that previous frame.
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  CreateClient();

  DrawSomething();
  CanvasResource* frame1_raw_ptr = DispatchOneFrame();

  EXPECT_CALL(*(client()), MainThreadReceivedImage()).Times(1);
  // Run task that propagates the frame to the placeholder canvas.
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame(), nullptr);
  platform->RunUntilIdle();
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame()->GetResourceForTesting(),
            frame1_raw_ptr);
  scoped_refptr<CanvasResource> extra_ref =
      placeholder()->OffscreenCanvasFrame()->GetResourceForTesting();
  Mock::VerifyAndClearExpectations(client());

  EXPECT_CALL(*(client()), MainThreadReceivedImage()).Times(0);
  DrawSomething();
  CanvasResource* frame2_raw_ptr = DispatchOneFrame();
  Mock::VerifyAndClearExpectations(client());

  EXPECT_CALL(*(client()), MainThreadReceivedImage()).Times(1);
  // Propagate second frame to the placeholder. First frame will be cleared.
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame()->GetResourceForTesting(),
            frame1_raw_ptr);
  platform->RunUntilIdle();
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame()->GetResourceForTesting(),
            frame2_raw_ptr);
  Mock::VerifyAndClearExpectations(client());

  EXPECT_CALL(*(client()), MainThreadReceivedImage()).Times(0);
  extra_ref = nullptr;
  Mock::VerifyAndClearExpectations(client());

  EXPECT_CALL(*(client()), MainThreadReceivedImage()).Times(0);
  platform->RunUntilIdle();
  Mock::VerifyAndClearExpectations(client());
}

TEST_F(OffscreenCanvasPlaceholderTest, DeferredAnimationStateIsApplied) {
  // Test that changes to the animation state are deferred until the resource is
  // provided to the placeholder.
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  // Animation state changes are only deferred before the placeholder has a
  // dispatcher, so be sure that we don't have one now.
  ASSERT_FALSE(client());
  const auto initial_state = placeholder()->GetAnimationStateForTesting();
  constexpr auto deferred_state =
      OffscreenCanvasPlaceholder::AnimationState::kSuspended;
  // It doesn't really matter what the initial animation state is, but we want
  // to be sure that we're actually going to change it.
  ASSERT_NE(initial_state, deferred_state);
  // Change the state, which should be deferred.
  placeholder()->SetSuspendOffscreenCanvasAnimation(deferred_state);
  EXPECT_EQ(initial_state, placeholder()->GetAnimationStateForTesting());

  // Now that the state change is deferred, we can create a dispatcher to
  // undefer it.
  CreateClient();
  EXPECT_EQ(initial_state, client()->GetAnimationState());
  EXPECT_CALL(*client(), OnAnimationStateUpdated());

  platform->RunUntilIdle();
  // Both the placeholder and the dispatcher should now agree on the new
  // animation state.
  EXPECT_EQ(deferred_state, placeholder()->GetAnimationStateForTesting());
  EXPECT_EQ(deferred_state, client()->GetAnimationState());
}

TEST_F(OffscreenCanvasPlaceholderTest,
       AnimationStateIsNotDeferredWithDispatcher) {
  // Test that, once we have a dispatcher, animation state changes are applied
  // right away.
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  CreateClient();
  const auto initial_state = placeholder()->GetAnimationStateForTesting();
  constexpr auto deferred_state =
      OffscreenCanvasPlaceholder::AnimationState::kSuspended;
  ASSERT_NE(initial_state, deferred_state);
  placeholder()->SetSuspendOffscreenCanvasAnimation(deferred_state);
  EXPECT_CALL(*client(), OnAnimationStateUpdated());
  platform->RunUntilIdle();
  EXPECT_EQ(deferred_state, placeholder()->GetAnimationStateForTesting());
  EXPECT_EQ(deferred_state, client()->GetAnimationState());
}

TEST_F(OffscreenCanvasPlaceholderTest, PlaceholderRunsNormally) {
  CreateClient();
  // Post first frame
  EXPECT_CALL(*(client()), OnPostImageToPlaceholder());
  DispatchOneFrame();
  EXPECT_EQ(1u, GetNumPendingPlaceholderResources());
  Mock::VerifyAndClearExpectations(client());

  // Post second frame
  EXPECT_CALL(*(client()), OnPostImageToPlaceholder());
  DispatchOneFrame();
  EXPECT_EQ(2u, GetNumPendingPlaceholderResources());
  Mock::VerifyAndClearExpectations(client());

  // Post third frame
  EXPECT_CALL(*(client()), OnPostImageToPlaceholder());
  DispatchOneFrame();
  EXPECT_EQ(3u, GetNumPendingPlaceholderResources());
  EXPECT_EQ(nullptr, GetLatestUnpostedImage());
  Mock::VerifyAndClearExpectations(client());

  // Receive first frame
  client()->OnMainThreadReceivedImage();
  EXPECT_EQ(2u, GetNumPendingPlaceholderResources());

  // Receive second frame
  client()->OnMainThreadReceivedImage();
  EXPECT_EQ(1u, GetNumPendingPlaceholderResources());

  // Receive third frame
  client()->OnMainThreadReceivedImage();
  EXPECT_EQ(0u, GetNumPendingPlaceholderResources());
}

TEST_F(OffscreenCanvasPlaceholderTest, PlaceholderBeingBlocked) {
  CreateClient();
  /* When main thread is blocked, attempting to post one more than the max
   * number of pending frames will result in the latest attempt being saved as
   * an unposted resource. */
  EXPECT_CALL(*(client()), OnPostImageToPlaceholder())
      .Times(MockPlaceholderClient::kMaxPendingPlaceholderResources);

  // Attempt to post kMaxPendingPlaceholderResources+1 times
  for (unsigned i = 0;
       i < MockPlaceholderClient::kMaxPendingPlaceholderResources + 1; i++) {
    DispatchOneFrame();
  }
  EXPECT_EQ(MockPlaceholderClient::kMaxPendingPlaceholderResources,
            GetNumPendingPlaceholderResources());
  EXPECT_TRUE(GetLatestUnpostedImage());

  // Attempt to post again. The latest unposted image will be replaced.
  DispatchOneFrame();
  EXPECT_EQ(MockPlaceholderClient::kMaxPendingPlaceholderResources,
            GetNumPendingPlaceholderResources());
  EXPECT_TRUE(GetLatestUnpostedImage());

  Mock::VerifyAndClearExpectations(client());

  /* The main thread becoming unblocked will trigger CanvasResourceDispatcher
   * to post the last saved image. */
  EXPECT_CALL(*(client()), OnPostImageToPlaceholder());
  client()->OnMainThreadReceivedImage();

  // The main thread received 1 frame and the dispatcher thread posted 1 frame,
  // so the number of pending placeholder resources should have remained the
  // same.
  EXPECT_EQ(MockPlaceholderClient::kMaxPendingPlaceholderResources,
            GetNumPendingPlaceholderResources());
  // Not generating new resource Id
  EXPECT_FALSE(GetLatestUnpostedImage());
  Mock::VerifyAndClearExpectations(client());

  EXPECT_CALL(*(client()), OnPostImageToPlaceholder()).Times(0);
  client()->OnMainThreadReceivedImage();
  EXPECT_EQ(MockPlaceholderClient::kMaxPendingPlaceholderResources - 1,
            GetNumPendingPlaceholderResources());
  Mock::VerifyAndClearExpectations(client());
}
}  // namespace

}  // namespace blink
