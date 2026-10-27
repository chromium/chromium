// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
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

class MockCanvasResourceDispatcherClient final
    : public GarbageCollected<MockCanvasResourceDispatcherClient>,
      public CanvasResourceDispatcherClient {
 public:
  MOCK_METHOD(bool, BeginFrame, (), (override));
  void Trace(Visitor* visitor) const override {
    CanvasResourceDispatcherClient::Trace(visitor);
  }
};

}  // namespace

class CanvasResourceDispatcherTest
    : public testing::Test,
      public ::testing::WithParamInterface<TestParams> {
 public:
  void DispatchOneFrame() {
    scoped_refptr<ExportedCanvasResource> exported_resource =
        resource_provider_->ProduceCanvasResource();
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

    client_ = MakeGarbageCollected<MockCanvasResourceDispatcherClient>();
    dispatcher_ = std::make_unique<CanvasResourceDispatcher>(
        client_.Get(),
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
  MockCanvasResourceDispatcherClient& MockClient() { return *client_; }

  test::TaskEnvironment& TaskEnvironment() { return task_environment_; }

 private:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  Persistent<MockCanvasResourceDispatcherClient> client_;
  std::unique_ptr<CanvasResourceDispatcher> dispatcher_;
  std::unique_ptr<CanvasNon2DResourceProvider> resource_provider_;
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider_;
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

namespace {

class MockBeginFrameProviderClient final
    : public GarbageCollected<MockBeginFrameProviderClient>,
      public BeginFrameProviderClient {
 public:
  MOCK_METHOD(void, BeginFrame, (const viz::BeginFrameArgs&), (override));
  scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner()
      override {
    return scheduler::GetSingleThreadTaskRunnerForTesting();
  }
  void Trace(Visitor* visitor) const override {
    BeginFrameProviderClient::Trace(visitor);
  }
};

viz::BeginFrameArgs CreateBeginFrameArgs(uint64_t sequence_number,
                                         base::TimeTicks frame_time,
                                         base::TimeDelta interval) {
  return viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /*source_id=*/1, sequence_number, frame_time,
      frame_time + interval, interval, viz::BeginFrameArgs::NORMAL);
}

}  // namespace

TEST_F(CanvasResourceDispatcherTest, ThrottlesOnBeginFrameTo60HzWhenEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kThrottleMainFrameTo60Hz,
       features::kThrottleOffscreenCanvasTo60Hz},
      {});
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
  embedded_frame_sink_provider_receiver.FlushForTesting();

  EXPECT_CALL(MockClient(), BeginFrame())
      .Times(5)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              DidNotProduceFrame(_))
      .Times(5);

  const base::TimeDelta interval_120hz = base::Hertz(120);
  for (uint64_t i = 1; i <= 10; ++i) {
    dispatcher()->OnBeginFrame(
        CreateBeginFrameArgs(i, TaskEnvironment().NowTicks(), interval_120hz),
        /*timing_details=*/{}, /*resources=*/{});
    TaskEnvironment().FastForwardBy(interval_120hz);
  }
  Mock::VerifyAndClearExpectations(&MockClient());
  Mock::VerifyAndClearExpectations(
      &mock_embedded_frame_sink_provider.mock_compositor_frame_sink());

  // Frame 10 was throttled (early return). Verify that current_begin_frame_ack_
  // was invalidated so a subsequent out-of-band frame submission uses a manual
  // ack rather than reusing the already-acked throttled frame ID.
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              SubmitCompositorFrame_(_))
      .WillOnce(::testing::WithArg<0>([](const viz::CompositorFrame* frame) {
        EXPECT_EQ(frame->metadata.begin_frame_ack.frame_id.source_id,
                  viz::BeginFrameArgs::kManualSourceId);
      }));
  DispatchOneFrame();
  embedded_frame_sink_provider_receiver.FlushForTesting();
  dispatcher()->DidReceiveCompositorFrameAck(/*resources=*/{});
  Mock::VerifyAndClearExpectations(
      &mock_embedded_frame_sink_provider.mock_compositor_frame_sink());

  // If Client()->BeginFrame() returns false (no frame submitted),
  // last_begin_frame_time_ should not advance, so the very next frame should
  // not be throttled.
  EXPECT_CALL(MockClient(), BeginFrame())
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              DidNotProduceFrame(_))
      .Times(1);
  dispatcher()->OnBeginFrame(
      CreateBeginFrameArgs(11, TaskEnvironment().NowTicks(), interval_120hz),
      /*timing_details=*/{}, /*resources=*/{});
  TaskEnvironment().FastForwardBy(interval_120hz);
  dispatcher()->OnBeginFrame(
      CreateBeginFrameArgs(12, TaskEnvironment().NowTicks(), interval_120hz),
      /*timing_details=*/{}, /*resources=*/{});
  TaskEnvironment().FastForwardBy(interval_120hz);
  Mock::VerifyAndClearExpectations(&MockClient());
  Mock::VerifyAndClearExpectations(
      &mock_embedded_frame_sink_provider.mock_compositor_frame_sink());

  // 60Hz and 90Hz intervals should not be throttled.
  const base::TimeDelta interval_90hz = base::Hertz(90);
  EXPECT_CALL(MockClient(), BeginFrame())
      .Times(4)
      .WillRepeatedly(testing::Return(true));
  for (uint64_t i = 13; i <= 16; ++i) {
    dispatcher()->OnBeginFrame(
        CreateBeginFrameArgs(i, TaskEnvironment().NowTicks(), interval_90hz),
        /*timing_details=*/{}, /*resources=*/{});
    TaskEnvironment().FastForwardBy(interval_90hz);
  }
  Mock::VerifyAndClearExpectations(&MockClient());

  // Disabling kThrottleOffscreenCanvasTo60Hz should stop throttling 120Hz
  // frames even when kThrottleMainFrameTo60Hz is enabled.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {features::kThrottleMainFrameTo60Hz},
      {features::kThrottleOffscreenCanvasTo60Hz});
  EXPECT_CALL(MockClient(), BeginFrame())
      .Times(4)
      .WillRepeatedly(testing::Return(true));
  for (uint64_t i = 17; i <= 20; ++i) {
    dispatcher()->OnBeginFrame(
        CreateBeginFrameArgs(i, TaskEnvironment().NowTicks(), interval_120hz),
        /*timing_details=*/{}, /*resources=*/{});
    TaskEnvironment().FastForwardBy(interval_120hz);
  }
}

TEST_F(CanvasResourceDispatcherTest,
       BeginFrameProviderThrottlesTo60HzWhenEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kThrottleMainFrameTo60Hz,
       features::kThrottleOffscreenCanvasTo60Hz},
      {});
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  MockEmbeddedFrameSinkProvider mock_embedded_frame_sink_provider;
  mock_embedded_frame_sink_provider
      .set_num_expected_set_needs_begin_frame_on_sink_construction(1);
  mojo::Receiver<mojom::blink::EmbeddedFrameSinkProvider>
      embedded_frame_sink_provider_receiver(&mock_embedded_frame_sink_provider);
  auto scoped_override =
      mock_embedded_frame_sink_provider.CreateScopedOverrideMojoInterface(
          &embedded_frame_sink_provider_receiver);

  auto* context = MakeGarbageCollected<MockContextLifecycleNotifier>();
  auto* client = MakeGarbageCollected<MockBeginFrameProviderClient>();
  BeginFrameProviderParams params;
  params.parent_frame_sink_id = viz::FrameSinkId(kClientId, kSinkId);
  params.frame_sink_id = viz::FrameSinkId(kClientId, kSinkId + 1);
  auto* provider =
      MakeGarbageCollected<BeginFrameProvider>(params, client, context);

  provider->RequestBeginFrame();
  embedded_frame_sink_provider_receiver.FlushForTesting();

  EXPECT_CALL(*client, BeginFrame(_))
      .Times(5)
      .WillRepeatedly([provider](const viz::BeginFrameArgs& args) {
        provider->FinishBeginFrame(args);
        provider->RequestBeginFrame();
      });
  // 5 throttled frames + 5 FinishBeginFrame calls = 10 DidNotProduceFrame acks.
  EXPECT_CALL(mock_embedded_frame_sink_provider.mock_compositor_frame_sink(),
              DidNotProduceFrame(_))
      .Times(10);

  const base::TimeDelta interval_120hz = base::Hertz(120);
  for (uint64_t i = 1; i <= 10; ++i) {
    provider->OnBeginFrame(
        CreateBeginFrameArgs(i, TaskEnvironment().NowTicks(), interval_120hz),
        /*timing_details=*/{}, /*resources=*/{});
    TaskEnvironment().FastForwardBy(interval_120hz);
  }
}

}  // namespace blink
