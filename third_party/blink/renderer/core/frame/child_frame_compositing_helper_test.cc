// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/child_frame_compositing_helper.h"

#include <memory>

#include "base/test/task_environment.h"
#include "cc/layers/layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/child_frame_compositor.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

namespace {

class MockChildFrameCompositor final
    : public GarbageCollected<MockChildFrameCompositor>,
      public ChildFrameCompositor {
 public:
  MockChildFrameCompositor()
      : compositing_helper_(
            std::make_unique<ChildFrameCompositingHelper>(this)) {
    constexpr int width = 32;
    constexpr int height = 32;
    sad_page_bitmap_.allocN32Pixels(width, height);
  }
  MockChildFrameCompositor(const MockChildFrameCompositor&) = delete;
  MockChildFrameCompositor& operator=(const MockChildFrameCompositor&) = delete;

  void Trace(Visitor* visitor) const override {
    ChildFrameCompositor::Trace(visitor);
  }

  const scoped_refptr<cc::Layer>& GetCcLayer() override { return layer_; }

  void SetCcLayer(scoped_refptr<cc::Layer> layer,
                  bool is_surface_layer) override {
    layer_ = std::move(layer);
  }

  SkBitmap* GetSadPageBitmap() override { return &sad_page_bitmap_; }

  ChildFrameCompositingHelper& compositing_helper() {
    return *compositing_helper_;
  }

 private:
  scoped_refptr<cc::Layer> layer_;
  SkBitmap sad_page_bitmap_;
  std::unique_ptr<ChildFrameCompositingHelper> compositing_helper_;
};

viz::SurfaceId MakeSurfaceId(const viz::FrameSinkId& frame_sink_id,
                             uint32_t parent_sequence_number,
                             uint32_t child_sequence_number = 1u) {
  return viz::SurfaceId(
      frame_sink_id,
      viz::LocalSurfaceId(parent_sequence_number, child_sequence_number,
                          base::UnguessableToken::CreateForTesting(0, 1u)));
}

}  // namespace

class ChildFrameCompositingHelperTest : public testing::Test {
 public:
  ChildFrameCompositingHelperTest()
      : compositor_(MakeGarbageCollected<MockChildFrameCompositor>()) {}
  ChildFrameCompositingHelperTest(const ChildFrameCompositingHelperTest&) =
      delete;
  ChildFrameCompositingHelperTest& operator=(
      const ChildFrameCompositingHelperTest&) = delete;

  ~ChildFrameCompositingHelperTest() override {}

  ChildFrameCompositingHelper& compositing_helper() {
    return compositor_->compositing_helper();
  }
  const cc::SurfaceLayer& GetSurfaceLayer() {
    return *static_cast<cc::SurfaceLayer*>(compositor_->GetCcLayer().get());
  }

 private:
  Persistent<MockChildFrameCompositor> compositor_;
};

// This test verifies that the fallback surfaceId is cleared when the child
// frame is reported as being gone and a sad page is displayed.
TEST_F(ChildFrameCompositingHelperTest, ChildFrameGoneClearsFallback) {
  // The primary and fallback surface IDs should start out as invalid.
  EXPECT_FALSE(compositing_helper().surface_id().is_valid());

  const viz::SurfaceId surface_id = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);
  compositing_helper().SetSurfaceId(
      surface_id, ChildFrameCompositingHelper::AllowPaintHolding::kNo);
  EXPECT_EQ(surface_id, compositing_helper().surface_id());

  // Reporting that the child frame is gone should clear the surface id.
  compositing_helper().ChildFrameGone(1.f);
  EXPECT_FALSE(compositing_helper().surface_id().is_valid());
}

TEST_F(ChildFrameCompositingHelperTest, PaintHoldingTimeout) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  EXPECT_FALSE(compositing_helper().surface_id().is_valid());

  const viz::SurfaceId surface_id = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);
  compositing_helper().SetSurfaceId(
      surface_id, ChildFrameCompositingHelper::AllowPaintHolding::kNo);
  EXPECT_EQ(surface_id, GetSurfaceLayer().surface_id());
  EXPECT_FALSE(GetSurfaceLayer().oldest_acceptable_fallback());

  const viz::SurfaceId new_surface_id =
      MakeSurfaceId(viz::FrameSinkId(1, 1), 2);
  compositing_helper().SetSurfaceId(
      new_surface_id, ChildFrameCompositingHelper::AllowPaintHolding::kYes);
  EXPECT_EQ(new_surface_id, GetSurfaceLayer().surface_id());
  ASSERT_TRUE(GetSurfaceLayer().oldest_acceptable_fallback());
  EXPECT_EQ(surface_id, GetSurfaceLayer().oldest_acceptable_fallback().value());

  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(new_surface_id, GetSurfaceLayer().surface_id());
  EXPECT_FALSE(GetSurfaceLayer().oldest_acceptable_fallback());
}

}  // namespace blink
