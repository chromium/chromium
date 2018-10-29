// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"

#include <memory>

#include "components/viz/common/quads/texture_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"

using testing::_;
using testing::Mock;
using testing::ValuesIn;

namespace blink {

namespace {
constexpr uint32_t kClientId = 2;
constexpr uint32_t kSinkId = 1;

constexpr size_t kWidth = 10;
constexpr size_t kHeight = 10;

struct TestParams {
  bool context_alpha;
  bool vertical_flip;
};
}  // namespace

class MockCanvasResourceDispatcher : public CanvasResourceDispatcher {
 public:
  MockCanvasResourceDispatcher()
      : CanvasResourceDispatcher(nullptr /* client */,
                                 kClientId,
                                 kSinkId,
                                 0 /* placeholder_canvas_id* */,
                                 {kWidth, kHeight} /* canvas_size */) {}

  MOCK_METHOD2(PostImageToPlaceholder,
               void(scoped_refptr<CanvasResource>, unsigned resource_id));
};

class CanvasResourceDispatcherTest
    : public testing::Test,
      public ::testing::WithParamInterface<TestParams> {
 public:
  void DispatchOneFrame() {
    dispatcher_->DispatchFrame(resource_provider_->ProduceFrame(),
                               base::TimeTicks(), SkIRect::MakeEmpty(),
                               false /* needs_vertical_flip */,
                               false /* is-opaque */);
  }

  unsigned GetNumUnreclaimedFramesPosted() {
    return dispatcher_->num_unreclaimed_frames_posted_;
  }

  CanvasResource* GetLatestUnpostedImage() {
    return dispatcher_->latest_unposted_image_.get();
  }

  viz::ResourceId GetLatestUnpostedResourceId() {
    return dispatcher_->latest_unposted_resource_id_;
  }

  viz::ResourceId GetCurrentResourceId() {
    return dispatcher_->next_resource_id_;
  }

  const IntSize& GetSize() const { return dispatcher_->size_; }

 protected:
  CanvasResourceDispatcherTest() = default;

  void CreateCanvasResourceDispatcher() {
    dispatcher_ = std::make_unique<MockCanvasResourceDispatcher>();
    resource_provider_ = CanvasResourceProvider::Create(
        IntSize(kWidth, kHeight),
        CanvasResourceProvider::kSoftwareCompositedResourceUsage,
        nullptr,  // context_provider_wrapper
        0,        // msaa_sample_count
        CanvasColorParams(), CanvasResourceProvider::kDefaultPresentationMode,
        dispatcher_->GetWeakPtr());
  }

  MockCanvasResourceDispatcher* Dispatcher() { return dispatcher_.get(); }

 private:
  scoped_refptr<StaticBitmapImage> PrepareStaticBitmapImage();
  std::unique_ptr<MockCanvasResourceDispatcher> dispatcher_;
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
};

TEST_F(CanvasResourceDispatcherTest, PlaceholderRunsNormally) {
  CreateCanvasResourceDispatcher();
  /* We allow OffscreenCanvas to post up to 3 frames without hearing a response
   * from placeholder. */
  // Post first frame
  unsigned post_resource_id = 1u;
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, post_resource_id));
  DispatchOneFrame();
  EXPECT_EQ(1u, GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(1u, GetCurrentResourceId());
  Mock::VerifyAndClearExpectations(Dispatcher());

  // Post second frame
  post_resource_id++;
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, post_resource_id));
  DispatchOneFrame();
  EXPECT_EQ(2u, GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(2u, GetCurrentResourceId());
  Mock::VerifyAndClearExpectations(Dispatcher());

  // Post third frame
  post_resource_id++;
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, post_resource_id));
  DispatchOneFrame();
  EXPECT_EQ(3u, GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(3u, GetCurrentResourceId());
  EXPECT_EQ(nullptr, GetLatestUnpostedImage());
  Mock::VerifyAndClearExpectations(Dispatcher());

  /* We mock the behavior of placeholder on main thread here, by reclaiming
   * the resources in order. */
  // Reclaim first frame
  unsigned reclaim_resource_id = 1u;
  Dispatcher()->ReclaimResource(reclaim_resource_id);
  EXPECT_EQ(2u, GetNumUnreclaimedFramesPosted());

  // Reclaim second frame
  reclaim_resource_id++;
  Dispatcher()->ReclaimResource(reclaim_resource_id);
  EXPECT_EQ(1u, GetNumUnreclaimedFramesPosted());

  // Reclaim third frame
  reclaim_resource_id++;
  Dispatcher()->ReclaimResource(reclaim_resource_id);
  EXPECT_EQ(0u, GetNumUnreclaimedFramesPosted());
}

TEST_F(CanvasResourceDispatcherTest, PlaceholderBeingBlocked) {
  CreateCanvasResourceDispatcher();
  /* When main thread is blocked, attempting to post more than 3 frames will
   * result in only 3 PostImageToPlaceholder. The latest unposted image will
   * be saved. */
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, _)).Times(3);

  // Attempt to post 4 times
  DispatchOneFrame();
  DispatchOneFrame();
  DispatchOneFrame();
  DispatchOneFrame();
  unsigned post_resource_id = 4u;
  EXPECT_EQ(3u, GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(post_resource_id, GetCurrentResourceId());
  EXPECT_TRUE(GetLatestUnpostedImage());
  EXPECT_EQ(post_resource_id, GetLatestUnpostedResourceId());

  // Attempt to post the 5th time. The latest unposted image will be replaced.
  post_resource_id++;
  DispatchOneFrame();
  EXPECT_EQ(3u, GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(post_resource_id, GetCurrentResourceId());
  EXPECT_TRUE(GetLatestUnpostedImage());
  EXPECT_EQ(post_resource_id, GetLatestUnpostedResourceId());

  Mock::VerifyAndClearExpectations(Dispatcher());

  /* When main thread becomes unblocked, the first reclaim called by placeholder
   * will trigger CanvasResourceDispatcher to post the last saved image.
   * Resource reclaim happens in the same order as frame posting. */
  unsigned reclaim_resource_id = 1u;
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, post_resource_id));
  Dispatcher()->ReclaimResource(reclaim_resource_id);
  // Reclaim 1 frame and post 1 frame, so numPostImagesUnresponded remains as 3
  EXPECT_EQ(3u, GetNumUnreclaimedFramesPosted());
  // Not generating new resource Id
  EXPECT_EQ(post_resource_id, GetCurrentResourceId());
  EXPECT_FALSE(GetLatestUnpostedImage());
  EXPECT_EQ(0u, GetLatestUnpostedResourceId());
  Mock::VerifyAndClearExpectations(Dispatcher());

  reclaim_resource_id++;
  Dispatcher()->ReclaimResource(reclaim_resource_id);
  EXPECT_EQ(2u, GetNumUnreclaimedFramesPosted());
}

TEST_P(CanvasResourceDispatcherTest, DispatchFrame) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  ::testing::InSequence s;

  // To intercept SubmitCompositorFrame/SubmitCompositorFrameSync messages sent
  // by theCanvasResourceDispatcher, we have to override the Mojo
  // EmbeddedFrameSinkProvider interface impl and its CompositorFrameSinkClient.
  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Binding<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_binding(&mock_embedded_frame_sink_provider);
  auto override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_binding);

  CreateCanvasResourceDispatcher();

  // CanvasResourceDispatcher ctor will cause a CreateCompositorFrameSink() to
  // be issued.
  EXPECT_CALL(mock_embedded_frame_sink_provider,
              CreateCompositorFrameSink_(viz::FrameSinkId(kClientId, kSinkId)));
  platform->RunUntilIdle();

  auto canvas_resource = CanvasResourceSharedBitmap::Create(
      GetSize(), CanvasColorParams(), nullptr /* provider */,
      kLow_SkFilterQuality);
  EXPECT_TRUE(!!canvas_resource);
  EXPECT_EQ(canvas_resource->Size(), GetSize());

  const bool context_alpha = GetParam().context_alpha;
  const bool vertical_flip = GetParam().vertical_flip;

  constexpr size_t kDamageWidth = 8;
  constexpr size_t kDamageHeight = 6;
  ASSERT_LE(kDamageWidth, kWidth);
  ASSERT_LE(kDamageHeight, kHeight);

  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, _));
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SubmitCompositorFrame_(_))
      .WillOnce(::testing::WithArg<0>(
          ::testing::Invoke([context_alpha](const viz::CompositorFrame* frame) {
            const viz::RenderPass* render_pass =
                frame->render_pass_list[0].get();

            EXPECT_EQ(render_pass->transform_to_root_target, gfx::Transform());
            EXPECT_EQ(render_pass->output_rect, gfx::Rect(kWidth, kHeight));
            EXPECT_EQ(render_pass->damage_rect,
                      gfx::Rect(kDamageWidth, kDamageHeight));

            const auto* quad = render_pass->quad_list.front();
            EXPECT_EQ(quad->material, viz::DrawQuad::TEXTURE_CONTENT);
            EXPECT_EQ(quad->rect, gfx::Rect(kWidth, kHeight));
            EXPECT_EQ(quad->visible_rect, gfx::Rect(kWidth, kHeight));

            EXPECT_EQ(quad->needs_blending, context_alpha);

            const auto* texture_quad =
                static_cast<const viz::TextureDrawQuad*>(quad);
            EXPECT_TRUE(texture_quad->premultiplied_alpha);
            EXPECT_EQ(texture_quad->uv_top_left, gfx::PointF(0.0f, 0.0f));
            EXPECT_EQ(texture_quad->uv_bottom_right, gfx::PointF(1.0f, 1.0f));
            EXPECT_THAT(texture_quad->vertex_opacity,
                        ::testing::ElementsAre(1.f, 1.f, 1.f, 1.f));
            // |y_flipped| should follow |vertical_flip| on GPU compositing; but
            // we don't have that in unit tests, so it's always false.
            EXPECT_FALSE(texture_quad->y_flipped);
          })));

  constexpr SkIRect damage_rect = SkIRect::MakeWH(kDamageWidth, kDamageHeight);
  Dispatcher()->DispatchFrame(canvas_resource, base::TimeTicks::Now(),
                              damage_rect, vertical_flip,
                              !context_alpha /* is_opaque */);
  platform->RunUntilIdle();
}

const TestParams kTestCases[] = {
    {false /* context_alpha */, false /* vertical_flip */},
    {false, true},
    {true, false},
    {true, true}};

INSTANTIATE_TEST_CASE_P(, CanvasResourceDispatcherTest, ValuesIn(kTestCases));
}  // namespace blink
