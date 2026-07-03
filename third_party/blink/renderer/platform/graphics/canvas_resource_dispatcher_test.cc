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
#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
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
  OffscreenCanvasPlaceholder::AnimationState animation_state;
};

class MockCanvasResourceDispatcherClient
    : public CanvasResourceDispatcherClient {
 public:
  MOCK_METHOD(bool, BeginFrame, (), (override));
};

}  // namespace

class CanvasResourceDispatcherTest
    : public testing::Test,
      public ::testing::WithParamInterface<TestParams> {
 public:
  void DispatchOneFrame() {
    scoped_refptr<CanvasResource> canvas_resource =
        resource_provider_->ProduceCanvasResource();
    auto exported_resource = base::MakeRefCounted<ExportedCanvasResource>(
        std::move(canvas_resource));
    dispatcher_->DispatchFrame(std::move(exported_resource), gfx::Rect(),
                               /*is_opaque=*/false);
  }

  const gfx::Size& GetSize() const { return dispatcher_->size_; }

  base::WeakPtr<WebGraphicsSharedImageInterfaceProvider>
  shared_image_interface_provider() {
    return test_web_shared_image_interface_provider_->GetWeakPtr();
  }

 protected:
  CanvasResourceDispatcherTest() = default;

  void CreateDispatcher() {
    test_web_shared_image_interface_provider_ =
        TestWebGraphicsSharedImageInterfaceProvider::Create();

    dispatcher_ = std::make_unique<CanvasResourceDispatcher>(
        &client_,
        /*task_runner=*/scheduler::GetSingleThreadTaskRunnerForTesting(),
        kClientId, kSinkId,
        /*canvas_size=*/gfx::Size(kWidth, kHeight));
    resource_provider_ =
        CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
            gfx::Size(kWidth, kHeight), GetN32FormatForCanvas(),
            kPremul_SkAlphaType, gfx::ColorSpace::CreateSRGB(),
            gfx::HDRMetadata(),
            test_web_shared_image_interface_provider_.get());
  }

  CanvasResourceDispatcher* dispatcher() { return dispatcher_.get(); }
  MockCanvasResourceDispatcherClient& MockClient() { return client_; }

  test::TaskEnvironment& TaskEnvironment() { return task_environment_; }

 private:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<CanvasResourceDispatcher> dispatcher_;
  std::unique_ptr<CanvasNon2DResourceProvider> resource_provider_;
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider_;
  MockCanvasResourceDispatcherClient client_;
};

TEST_F(CanvasResourceDispatcherTest, UsesRealOnBeginFrameWhenActive) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto scoped_override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  CreateDispatcher();
  dispatcher()->SetAnimationState(
      OffscreenCanvasPlaceholder::AnimationState::kActive);
  platform->RunUntilIdle();
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SetNeedsBeginFrame(true))
      .Times(AtLeast(1));
  dispatcher()->SetNeedsBeginFrame(true);
  platform->RunUntilIdle();
  // Advance time, and verify that there isn't a synthetic OBF generated for the
  // client by the dispatcher.
  EXPECT_CALL(MockClient(), BeginFrame()).Times(0);
  TaskEnvironment().FastForwardBy(base::Seconds(0.25));
  platform->RunUntilIdle();

  // Verify that the client's BeginFrame is called in response to a real OBF.
  EXPECT_CALL(MockClient(), BeginFrame()).Times(1);
  dispatcher()->OnBeginFrame(/*begin_frame_args=*/{}, /*timing details*/ {},
                             /*resources=*/{});
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

  CreateDispatcher();
  dispatcher()->SetAnimationState(
      OffscreenCanvasPlaceholder::AnimationState::kActiveWithSyntheticTiming);
  platform->RunUntilIdle();
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SetNeedsBeginFrame(false))
      .Times(AtLeast(1));
  dispatcher()->SetNeedsBeginFrame(true);
  platform->RunUntilIdle();
  // Advance time and make sure that we still get a CompositorFrame, even though
  // we don't send any OBF.
  EXPECT_CALL(MockClient(), BeginFrame()).Times(AtLeast(1));
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

  CreateDispatcher();
  dispatcher()->SetAnimationState(
      OffscreenCanvasPlaceholder::AnimationState::kSuspended);
  platform->RunUntilIdle();
  // Since OBF is off by default zero or more calls to turn it off is okay.  For
  // clarity, explicitly require no calls that would enable OBF.
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SetNeedsBeginFrame(false))
      .Times(AtLeast(0));
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SetNeedsBeginFrame(true))
      .Times(0);
  dispatcher()->SetNeedsBeginFrame(true);
  platform->RunUntilIdle();
  // Advance time, and verify that there isn't a synthetic OBF generated for the
  // client by the dispatcher.
  EXPECT_CALL(MockClient(), BeginFrame()).Times(0);
  TaskEnvironment().FastForwardBy(base::Seconds(0.25));
  platform->RunUntilIdle();
}

TEST_P(CanvasResourceDispatcherTest, DispatchFrame) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  ::testing::InSequence s;

  // To intercept SubmitCompositorFrame messages sent by
  // theCanvasResourceDispatcher, we have to override the Mojo
  // EmbeddedFrameSinkProvider interface impl and its
  // CompositorFrameSinkClient.
  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto scoped_override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  CreateDispatcher();
  dispatcher()->SetAnimationState(GetParam().animation_state);
  // Throttling should be allowed if the animation is suspended.  If it's active
  // or if it's using a synthetic OBF, then the intention is that viz should not
  // throttle since the canvas might be driving some on-screen work indirectly.
  const bool expected_throttle =
      GetParam().animation_state ==
      OffscreenCanvasPlaceholder::AnimationState::kSuspended;

  // CanvasResourceDispatcher ctor will cause a CreateCompositorFrameSink() to
  // be issued.
  EXPECT_CALL(mock_embedded_frame_sink_provider,
              CreateCompositorFrameSink_(viz::FrameSinkId(kClientId, kSinkId)));
  platform->RunUntilIdle();

  auto canvas_resource = CanvasResourceSharedImage::CreateForTesting(
      GetSize(), viz::SinglePlaneFormat::kBGRA_8888, kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(), gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
      /*is_software=*/true,
      /*is_accelerated=*/false, /*provider=*/nullptr,
      /*context_provider_wrapper=*/nullptr, shared_image_interface_provider());

  EXPECT_TRUE(!!canvas_resource);
  EXPECT_EQ(canvas_resource->Size(), GetSize());

  const bool context_alpha = GetParam().context_alpha;

  constexpr size_t kDamageWidth = 8;
  constexpr size_t kDamageHeight = 6;
  ASSERT_LE(kDamageWidth, kWidth);
  ASSERT_LE(kDamageHeight, kHeight);

  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SubmitCompositorFrame_(_))
      .WillOnce(
          ::testing::WithArg<0>([context_alpha, expected_throttle](
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
            EXPECT_EQ(texture_quad->GetNormalizedTexCoords(
                          gfx::Size(kWidth, kHeight)),
                      gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f));

            // CanvasResourceSharedImage::CreateSoftware() creates a resource
            // whose origin is top-left.
            EXPECT_EQ(frame->resource_list.front().GetOrigin(),
                      kTopLeft_GrSurfaceOrigin);
            EXPECT_EQ(frame->resource_list.front().GetAlphaType(),
                      kPremul_SkAlphaType);
          }));

  const gfx::Rect damage_rect(kDamageWidth, kDamageHeight);
  auto exported_resource =
      base::MakeRefCounted<ExportedCanvasResource>(std::move(canvas_resource));
  dispatcher()->DispatchFrame(std::move(exported_resource), damage_rect,
                              !context_alpha /* is_opaque */);
  platform->RunUntilIdle();
}

const TestParams kTestCases[] = {
    {false /* context_alpha */,
     OffscreenCanvasPlaceholder::AnimationState::kActive},
    {true, OffscreenCanvasPlaceholder::AnimationState::kActive},
    // These test the requested throttling state.  Alpha doesn't matter.
    {false,
     OffscreenCanvasPlaceholder::AnimationState::kActiveWithSyntheticTiming},
    {false, OffscreenCanvasPlaceholder::AnimationState::kSuspended},
};

INSTANTIATE_TEST_SUITE_P(All,
                         CanvasResourceDispatcherTest,
                         ValuesIn(kTestCases));
}  // namespace blink
