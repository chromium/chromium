// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"

#include <memory>

#include "components/viz/common/quads/texture_draw_quad.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/mojom/presentation_feedback.mojom-blink.h"

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

viz::ResourceId NextId(viz::ResourceId id) {
  return viz::ResourceId(id.GetUnsafeValue() + 1);
}

class MockCanvasResourceDispatcher : public CanvasResourceDispatcher {
 public:
  MockCanvasResourceDispatcher()
      : CanvasResourceDispatcher(
            /*client=*/nullptr,
            /*task_runner=*/scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*agent_group_scheduler_compositor_task_runner=*/
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            kClientId,
            kSinkId,
            /*placeholder_canvas_id=*/0,
            /*canvas_size=*/{kWidth, kHeight}) {}

  MOCK_METHOD2(PostImageToPlaceholder,
               void(scoped_refptr<CanvasResource>&&,
                    viz::ResourceId resource_id));
};

}  // namespace

class CanvasResourceDispatcherTest
    : public testing::Test,
      public ::testing::WithParamInterface<TestParams> {
 public:
  scoped_refptr<CanvasResource> DispatchOneFrame() {
    scoped_refptr<CanvasResource> canvas_resource =
        resource_provider_->ProduceCanvasResource(FlushReason::kTesting);
    auto canvas_resource_extra = canvas_resource;
    dispatcher_->DispatchFrame(
        std::move(canvas_resource), base::TimeTicks(), SkIRect::MakeEmpty(),
        false /* needs_vertical_flip */, false /* is-opaque */);
    return canvas_resource_extra;
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

  viz::ResourceId PeekNextResourceId() {
    return dispatcher_->id_generator_.PeekNextValueForTesting();
  }

  const gfx::Size& GetSize() const { return dispatcher_->size_; }

  base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
  shared_image_interface_provider() {
    return test_web_shared_image_interface_provider_->GetWeakPtr();
  }

 protected:
  CanvasResourceDispatcherTest() = default;

  void CreateCanvasResourceDispatcher() {
    test_web_shared_image_interface_provider_ =
        TestWebGraphicsSharedImageInterfaceProvider::Create();

    dispatcher_ = std::make_unique<MockCanvasResourceDispatcher>();
    resource_provider_ = CanvasResourceProvider::CreateSharedBitmapProvider(
        SkImageInfo::MakeN32Premul(kWidth, kHeight),
        cc::PaintFlags::FilterQuality::kLow,
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        dispatcher_->GetWeakPtr(),
        test_web_shared_image_interface_provider_.get());
  }

  MockCanvasResourceDispatcher* Dispatcher() { return dispatcher_.get(); }

 private:
  scoped_refptr<StaticBitmapImage> PrepareStaticBitmapImage();
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockCanvasResourceDispatcher> dispatcher_;
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider_;
};

TEST_F(CanvasResourceDispatcherTest, PlaceholderRunsNormally) {
  CreateCanvasResourceDispatcher();
  /* We allow OffscreenCanvas to post up to 3 frames without hearing a response
   * from placeholder. */
  // Post first frame
  viz::ResourceId post_resource_id(1u);
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, post_resource_id));
  auto frame1 = DispatchOneFrame();
  EXPECT_EQ(1u, GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(NextId(post_resource_id), PeekNextResourceId());
  Mock::VerifyAndClearExpectations(Dispatcher());

  // Post second frame
  post_resource_id = NextId(post_resource_id);
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, post_resource_id));
  auto frame2 = DispatchOneFrame();
  EXPECT_EQ(2u, GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(NextId(post_resource_id), PeekNextResourceId());
  Mock::VerifyAndClearExpectations(Dispatcher());

  // Post third frame
  post_resource_id = NextId(post_resource_id);
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, post_resource_id));
  auto frame3 = DispatchOneFrame();
  EXPECT_EQ(3u, GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(NextId(post_resource_id), PeekNextResourceId());
  EXPECT_EQ(nullptr, GetLatestUnpostedImage());
  Mock::VerifyAndClearExpectations(Dispatcher());

  /* We mock the behavior of placeholder on main thread here, by reclaiming
   * the resources in order. */
  // Reclaim first frame
  viz::ResourceId reclaim_resource_id(1u);
  Dispatcher()->ReclaimResource(reclaim_resource_id, std::move(frame1));
  EXPECT_EQ(2u, GetNumUnreclaimedFramesPosted());

  // Reclaim second frame
  reclaim_resource_id = NextId(reclaim_resource_id);
  Dispatcher()->ReclaimResource(reclaim_resource_id, std::move(frame2));
  EXPECT_EQ(1u, GetNumUnreclaimedFramesPosted());

  // Reclaim third frame
  reclaim_resource_id = NextId(reclaim_resource_id);
  Dispatcher()->ReclaimResource(reclaim_resource_id, std::move(frame3));
  EXPECT_EQ(0u, GetNumUnreclaimedFramesPosted());
}

TEST_F(CanvasResourceDispatcherTest, PlaceholderBeingBlocked) {
  CreateCanvasResourceDispatcher();
  /* When main thread is blocked, attempting to post more than 3 frames will
   * result in only 3 PostImageToPlaceholder. The latest unposted image will
   * be saved. */
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, _))
      .Times(CanvasResourceDispatcher::kMaxUnreclaimedPlaceholderFrames);

  // Attempt to post kMaxUnreclaimedPlaceholderFrames+1 times
  auto frame1 = DispatchOneFrame();
  auto frame2 = DispatchOneFrame();
  for (unsigned i = 0;
       i < CanvasResourceDispatcher::kMaxUnreclaimedPlaceholderFrames - 1;
       i++) {
    DispatchOneFrame();
  }
  viz::ResourceId post_resource_id(
      CanvasResourceDispatcher::kMaxUnreclaimedPlaceholderFrames + 1);
  EXPECT_EQ(CanvasResourceDispatcher::kMaxUnreclaimedPlaceholderFrames,
            GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(NextId(post_resource_id), PeekNextResourceId());
  EXPECT_TRUE(GetLatestUnpostedImage());
  EXPECT_EQ(post_resource_id, GetLatestUnpostedResourceId());

  // Attempt to post the 5th time. The latest unposted image will be replaced.
  post_resource_id = NextId(post_resource_id);
  DispatchOneFrame();
  EXPECT_EQ(CanvasResourceDispatcher::kMaxUnreclaimedPlaceholderFrames,
            GetNumUnreclaimedFramesPosted());
  EXPECT_EQ(NextId(post_resource_id), PeekNextResourceId());
  EXPECT_TRUE(GetLatestUnpostedImage());
  EXPECT_EQ(post_resource_id, GetLatestUnpostedResourceId());

  Mock::VerifyAndClearExpectations(Dispatcher());

  /* When main thread becomes unblocked, the first reclaim called by placeholder
   * will trigger CanvasResourceDispatcher to post the last saved image.
   * Resource reclaim happens in the same order as frame posting. */
  viz::ResourceId reclaim_resource_id(1u);
  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, post_resource_id));
  Dispatcher()->ReclaimResource(reclaim_resource_id, std::move(frame1));
  // Reclaim 1 frame and post 1 frame, so numPostImagesUnresponded remains as 3
  EXPECT_EQ(CanvasResourceDispatcher::kMaxUnreclaimedPlaceholderFrames,
            GetNumUnreclaimedFramesPosted());
  // Not generating new resource Id
  EXPECT_EQ(NextId(post_resource_id), PeekNextResourceId());
  EXPECT_FALSE(GetLatestUnpostedImage());
  EXPECT_EQ(viz::kInvalidResourceId, GetLatestUnpostedResourceId());
  Mock::VerifyAndClearExpectations(Dispatcher());

  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, _)).Times(0);
  reclaim_resource_id = NextId(reclaim_resource_id);
  Dispatcher()->ReclaimResource(reclaim_resource_id, std::move(frame2));
  EXPECT_EQ(CanvasResourceDispatcher::kMaxUnreclaimedPlaceholderFrames - 1,
            GetNumUnreclaimedFramesPosted());
  Mock::VerifyAndClearExpectations(Dispatcher());
}

TEST_P(CanvasResourceDispatcherTest, DispatchFrame) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  ::testing::InSequence s;

  // To intercept SubmitCompositorFrame/SubmitCompositorFrameSync messages sent
  // by theCanvasResourceDispatcher, we have to override the Mojo
  // EmbeddedFrameSinkProvider interface impl and its CompositorFrameSinkClient.
  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  CreateCanvasResourceDispatcher();

  // CanvasResourceDispatcher ctor will cause a CreateCompositorFrameSink() to
  // be issued.
  EXPECT_CALL(mock_embedded_frame_sink_provider,
              CreateCompositorFrameSink_(viz::FrameSinkId(kClientId, kSinkId)));
  platform->RunUntilIdle();

  auto canvas_resource = CanvasResourceSharedBitmap::Create(
      SkImageInfo::MakeN32Premul(GetSize().width(), GetSize().height()),
      /*provider=*/nullptr, shared_image_interface_provider(),
      cc::PaintFlags::FilterQuality::kLow);
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
            const viz::CompositorRenderPass* render_pass =
                frame->render_pass_list[0].get();

            EXPECT_EQ(render_pass->transform_to_root_target, gfx::Transform());
            EXPECT_EQ(render_pass->output_rect, gfx::Rect(kWidth, kHeight));
            EXPECT_EQ(render_pass->damage_rect,
                      gfx::Rect(kDamageWidth, kDamageHeight));

            const auto* quad = render_pass->quad_list.front();
            EXPECT_EQ(quad->material, viz::DrawQuad::Material::kTextureContent);
            EXPECT_EQ(quad->rect, gfx::Rect(kWidth, kHeight));
            EXPECT_EQ(quad->visible_rect, gfx::Rect(kWidth, kHeight));

            EXPECT_EQ(quad->needs_blending, context_alpha);

            const auto* texture_quad =
                static_cast<const viz::TextureDrawQuad*>(quad);
            EXPECT_TRUE(texture_quad->premultiplied_alpha);
            EXPECT_EQ(texture_quad->uv_top_left, gfx::PointF(0.0f, 0.0f));
            EXPECT_EQ(texture_quad->uv_bottom_right, gfx::PointF(1.0f, 1.0f));
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

INSTANTIATE_TEST_SUITE_P(All,
                         CanvasResourceDispatcherTest,
                         ValuesIn(kTestCases));
}  // namespace blink
