// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_placeholder.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

using testing::_;
using testing::Mock;
using testing::Test;

namespace blink {

namespace {
constexpr uint32_t kClientId = 2;
constexpr uint32_t kSinkId = 1;

constexpr size_t kWidth = 10;
constexpr size_t kHeight = 10;

class MockCanvasResourceDispatcher : public CanvasResourceDispatcher {
 public:
  explicit MockCanvasResourceDispatcher(unsigned placeholder_id)
      : CanvasResourceDispatcher(
            /*client=*/nullptr,
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            kClientId,
            kSinkId,
            placeholder_id,
            /*canvas_size=*/{kWidth, kHeight}) {}

  MOCK_METHOD2(ReclaimResource,
               void(viz::ResourceId, scoped_refptr<CanvasResource>&&));
};

unsigned GenPlaceholderId() {
  DEFINE_STATIC_LOCAL(unsigned, s_id, (0));
  return ++s_id;
}

}  // unnamed namespace

class OffscreenCanvasPlaceholderTest : public Test {
 public:
  MockCanvasResourceDispatcher* dispatcher() { return dispatcher_.get(); }
  OffscreenCanvasPlaceholder* placeholder() { return &placeholder_; }
  CanvasResource* DispatchOneFrame();
  viz::ResourceId PeekNextResourceId() {
    return dispatcher_->id_generator_.PeekNextValueForTesting();
  }
  void DrawSomething();

 protected:
  void SetUp() override;
  void TearDown() override;

 private:
  test::TaskEnvironment task_environment_;
  OffscreenCanvasPlaceholder placeholder_;
  std::unique_ptr<MockCanvasResourceDispatcher> dispatcher_;
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider_;
};

void OffscreenCanvasPlaceholderTest::SetUp() {
  Test::SetUp();
  test_web_shared_image_interface_provider_ =
      TestWebGraphicsSharedImageInterfaceProvider::Create();

  unsigned placeholder_id = GenPlaceholderId();
  placeholder_.RegisterPlaceholderCanvas(placeholder_id);
  dispatcher_ = std::make_unique<MockCanvasResourceDispatcher>(placeholder_id);
  dispatcher_->SetPlaceholderCanvasDispatcher(placeholder_id);
  resource_provider_ = CanvasResourceProvider::CreateSharedBitmapProvider(
      SkImageInfo::MakeN32Premul(kWidth, kHeight),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      dispatcher_->GetWeakPtr(),
      test_web_shared_image_interface_provider_.get());
}

void OffscreenCanvasPlaceholderTest::TearDown() {
  resource_provider_.reset();
  dispatcher_.reset();
  placeholder_.UnregisterPlaceholderCanvas();
  Test::TearDown();
}

void OffscreenCanvasPlaceholderTest::DrawSomething() {
  // 'needs_will_draw=true' is required to ensure the CanvasResourceProvider
  // does not retain a reference on the previous frame.
  resource_provider_->Canvas(/*needs_will_draw=*/true).clear(SkColors::kWhite);
}

CanvasResource* OffscreenCanvasPlaceholderTest::DispatchOneFrame() {
  scoped_refptr<CanvasResource> resource =
      resource_provider_->ProduceCanvasResource(FlushReason::kTesting);
  CanvasResource* resource_raw_ptr = resource.get();
  dispatcher_->DispatchFrame(
      std::move(resource), base::TimeTicks(), SkIRect::MakeEmpty(),
      false /* needs_vertical_flip */, false /* is-opaque */);
  // We avoid holding a ref here to avoid interfering with
  // OffscreenCanvasPlaceholder's ref count logic.  This pointer should only
  // be used for validations.
  return resource_raw_ptr;
}

namespace {

TEST_F(OffscreenCanvasPlaceholderTest, OldFrameSentBack) {
  // This test verifies that OffscreenCanvasPlaceholder send back
  // the previous frame when it receives a new one.
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  EXPECT_CALL(*(dispatcher()), ReclaimResource(_, _)).Times(0);
  DrawSomething();
  viz::ResourceId frame1_id = PeekNextResourceId();
  CanvasResource* frame1_raw_ptr = DispatchOneFrame();
  EXPECT_TRUE(frame1_raw_ptr->HasOneRef());
  Mock::VerifyAndClearExpectations(dispatcher());

  EXPECT_CALL(*(dispatcher()), ReclaimResource(_, _)).Times(0);
  // Run task that propagates the frame to the placeholder canvas.
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame().get(), nullptr);
  platform->RunUntilIdle();
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame().get(), frame1_raw_ptr);
  Mock::VerifyAndClearExpectations(dispatcher());

  EXPECT_CALL(*(dispatcher()), ReclaimResource(_, _)).Times(0);
  DrawSomething();
  CanvasResource* frame2_raw_ptr = DispatchOneFrame();
  Mock::VerifyAndClearExpectations(dispatcher());

  EXPECT_CALL(*(dispatcher()), ReclaimResource(frame1_id, _)).Times(1);
  // Propagate second frame to the placeholder, causing frame 1 to be
  // reclaimed.
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame().get(), frame1_raw_ptr);
  platform->RunUntilIdle();
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame().get(), frame2_raw_ptr);
  Mock::VerifyAndClearExpectations(dispatcher());
}

TEST_F(OffscreenCanvasPlaceholderTest, OldFrameNotReclaimedUntilUnref) {
  // This test verifies that OffscreenCanvasPlaceholder send back
  // the previous frame when it receives a new one.
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  EXPECT_CALL(*(dispatcher()), ReclaimResource(_, _)).Times(0);
  DrawSomething();
  viz::ResourceId frame1_id = PeekNextResourceId();
  CanvasResource* frame1_raw_ptr = DispatchOneFrame();
  EXPECT_TRUE(frame1_raw_ptr->HasOneRef());
  Mock::VerifyAndClearExpectations(dispatcher());

  EXPECT_CALL(*(dispatcher()), ReclaimResource(_, _)).Times(0);
  // Run task that propagates the frame to the placeholder canvas.
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame().get(), nullptr);
  platform->RunUntilIdle();
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame().get(), frame1_raw_ptr);
  scoped_refptr<CanvasResource> extra_ref =
      placeholder()->OffscreenCanvasFrame();
  Mock::VerifyAndClearExpectations(dispatcher());

  EXPECT_CALL(*(dispatcher()), ReclaimResource(_, _)).Times(0);
  DrawSomething();
  CanvasResource* frame2_raw_ptr = DispatchOneFrame();
  Mock::VerifyAndClearExpectations(dispatcher());

  EXPECT_CALL(*(dispatcher()), ReclaimResource(_, _)).Times(0);
  // Propagate second frame to the placeholder.  First frame will not be
  // reclaimed due to extra_ref.
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame().get(), frame1_raw_ptr);
  platform->RunUntilIdle();
  EXPECT_EQ(placeholder()->OffscreenCanvasFrame().get(), frame2_raw_ptr);
  Mock::VerifyAndClearExpectations(dispatcher());

  EXPECT_CALL(*(dispatcher()), ReclaimResource(_, _)).Times(0);
  extra_ref = nullptr;  // Deref cause resource to be reclaimed asynchronously.
  Mock::VerifyAndClearExpectations(dispatcher());

  EXPECT_CALL(*(dispatcher()), ReclaimResource(frame1_id, _)).Times(1);
  // Run pending task to complete the reclaim.
  platform->RunUntilIdle();
  Mock::VerifyAndClearExpectations(dispatcher());
}

}  // namespace

}  // namespace blink
