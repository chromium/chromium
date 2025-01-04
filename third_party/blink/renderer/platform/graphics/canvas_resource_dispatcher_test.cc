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
using testing::AtLeast;
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
  CanvasResourceDispatcher::AnimationState animation_state;
};

viz::ResourceId NextId(viz::ResourceId id) {
  return viz::ResourceId(id.GetUnsafeValue() + 1);
}

class MockCanvasResourceDispatcherClient
    : public CanvasResourceDispatcherClient {
 public:
  MOCK_METHOD(bool, BeginFrame, (), (override));
};

class MockCanvasResourceDispatcher : public CanvasResourceDispatcher {
 public:
  MockCanvasResourceDispatcher()
      : CanvasResourceDispatcher(
            &client_,
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

  MockCanvasResourceDispatcherClient& MockClient() { return client_; }

 private:
  MockCanvasResourceDispatcherClient client_;
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
    dispatcher_->DispatchFrame(std::move(canvas_resource), base::TimeTicks(),
                               SkIRect::MakeEmpty(), /*is_opaque=*/false);
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
        gfx::Size(kWidth, kHeight), kN32_SkColorType, kPremul_SkAlphaType,
        SkColorSpace::MakeSRGB(),
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        test_web_shared_image_interface_provider_.get());
  }

  MockCanvasResourceDispatcher* Dispatcher() { return dispatcher_.get(); }

  test::TaskEnvironment& TaskEnvironment() { return task_environment_; }

 private:
  scoped_refptr<StaticBitmapImage> PrepareStaticBitmapImage();
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

TEST_F(CanvasResourceDispatcherTest, UsesRealOnBeginFrameWhenActive) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto scoped_override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  CreateCanvasResourceDispatcher();
  Dispatcher()->SetAnimationState(
      CanvasResourceDispatcher::AnimationState::kActive);
  platform->RunUntilIdle();
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SetNeedsBeginFrame(true))
      .Times(AtLeast(1));
  Dispatcher()->SetNeedsBeginFrame(true);
  platform->RunUntilIdle();
  // Advance time, and verify that there isn't a synthetic OBF generated for the
  // client by the dispatcher.
  EXPECT_CALL(Dispatcher()->MockClient(), BeginFrame()).Times(0);
  TaskEnvironment().FastForwardBy(base::Seconds(0.25));
  platform->RunUntilIdle();

  // Verify that the client's BeginFrame is called in response to a real OBF.
  EXPECT_CALL(Dispatcher()->MockClient(), BeginFrame()).Times(1);
  Dispatcher()->OnBeginFrame(/*begin_frame_args=*/{}, /*timing details*/ {},
                             /*frame_ack=*/false, /*resources=*/{});
}

TEST_F(CanvasResourceDispatcherTest,
       UsesSyntheticOnBeginFrameWhenActiveWithSynthetic) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto scoped_override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  CreateCanvasResourceDispatcher();
  Dispatcher()->SetAnimationState(
      CanvasResourceDispatcher::AnimationState::kActiveWithSyntheticTiming);
  platform->RunUntilIdle();
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SetNeedsBeginFrame(false))
      .Times(AtLeast(1));
  Dispatcher()->SetNeedsBeginFrame(true);
  platform->RunUntilIdle();
  // Advance time and make sure that we still get a CompositorFrame, even though
  // we don't send any OBF.
  EXPECT_CALL(Dispatcher()->MockClient(), BeginFrame()).Times(AtLeast(1));
  TaskEnvironment().FastForwardBy(base::Seconds(0.25));
  platform->RunUntilIdle();
}

TEST_F(CanvasResourceDispatcherTest, UsesNoOnBeginFrameWhenSuspended) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto scoped_override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  CreateCanvasResourceDispatcher();
  Dispatcher()->SetAnimationState(
      CanvasResourceDispatcher::AnimationState::kSuspended);
  platform->RunUntilIdle();
  // Since OBF is off by default zero or more calls to turn it off is okay.  For
  // clarity, explicitly require no calls that would enable OBF.
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SetNeedsBeginFrame(false))
      .Times(AtLeast(0));
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SetNeedsBeginFrame(true))
      .Times(0);
  Dispatcher()->SetNeedsBeginFrame(true);
  platform->RunUntilIdle();
  // Advance time, and verify that there isn't a synthetic OBF generated for the
  // client by the dispatcher.
  EXPECT_CALL(Dispatcher()->MockClient(), BeginFrame()).Times(0);
  TaskEnvironment().FastForwardBy(base::Seconds(0.25));
  platform->RunUntilIdle();
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
  auto scoped_override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  CreateCanvasResourceDispatcher();
  Dispatcher()->SetAnimationState(GetParam().animation_state);
  // Throttling should be allowed if the animation is suspended.  If it's active
  // or if it's using a synthetic OBF, then the intention is that viz should not
  // throttle since the canvas might be driving some on-screen work indirectly.
  const bool expected_throttle =
      GetParam().animation_state ==
      CanvasResourceDispatcher::AnimationState::kSuspended;

  // CanvasResourceDispatcher ctor will cause a CreateCompositorFrameSink() to
  // be issued.
  EXPECT_CALL(mock_embedded_frame_sink_provider,
              CreateCompositorFrameSink_(viz::FrameSinkId(kClientId, kSinkId)));
  platform->RunUntilIdle();

  auto canvas_resource = CanvasResourceSharedBitmap::Create(
      GetSize(), viz::SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(),
      /*provider=*/nullptr, shared_image_interface_provider());
  EXPECT_TRUE(!!canvas_resource);
  EXPECT_EQ(canvas_resource->Size(), GetSize());

  const bool context_alpha = GetParam().context_alpha;

  constexpr size_t kDamageWidth = 8;
  constexpr size_t kDamageHeight = 6;
  ASSERT_LE(kDamageWidth, kWidth);
  ASSERT_LE(kDamageHeight, kHeight);

  EXPECT_CALL(*(Dispatcher()), PostImageToPlaceholder(_, _));
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SubmitCompositorFrame_(_))
      .WillOnce(::testing::WithArg<0>(
          ::testing::Invoke([context_alpha, expected_throttle](
                                const viz::CompositorFrame* frame) {
            EXPECT_EQ(frame->metadata.may_throttle_if_undrawn_frames,
                      expected_throttle);

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

            // CanvasResourceSharedBitmap origin is top-left.
            EXPECT_EQ(frame->resource_list.front().origin,
                      kTopLeft_GrSurfaceOrigin);
          })));

  constexpr SkIRect damage_rect = SkIRect::MakeWH(kDamageWidth, kDamageHeight);
  Dispatcher()->DispatchFrame(canvas_resource, base::TimeTicks::Now(),
                              damage_rect, !context_alpha /* is_opaque */);
  platform->RunUntilIdle();
}

const TestParams kTestCases[] = {
    {false /* context_alpha */,
     CanvasResourceDispatcher::AnimationState::kActive},
    {true, CanvasResourceDispatcher::AnimationState::kActive},
    // These test the requested throttling state.  Alpha doesn't matter.
    {false,
     CanvasResourceDispatcher::AnimationState::kActiveWithSyntheticTiming},
    {false, CanvasResourceDispatcher::AnimationState::kSuspended},
};

INSTANTIATE_TEST_SUITE_P(All,
                         CanvasResourceDispatcherTest,
                         ValuesIn(kTestCases));
}  // namespace blink
