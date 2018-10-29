// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_submitter.h"

#include <memory>
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/layers/video_frame_provider.h"
#include "cc/test/layer_test_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/test_context_provider.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_resource_provider.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::StrictMock;

namespace blink {

namespace {

class MockVideoFrameProvider : public cc::VideoFrameProvider {
 public:
  MockVideoFrameProvider() = default;
  ~MockVideoFrameProvider() override = default;

  MOCK_METHOD1(SetVideoFrameProviderClient, void(Client*));
  MOCK_METHOD2(UpdateCurrentFrame, bool(base::TimeTicks, base::TimeTicks));
  MOCK_METHOD0(HasCurrentFrame, bool());
  MOCK_METHOD0(GetCurrentFrame, scoped_refptr<media::VideoFrame>());
  MOCK_METHOD0(PutCurrentFrame, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoFrameProvider);
};

class MockCompositorFrameSink : public viz::mojom::blink::CompositorFrameSink {
 public:
  MockCompositorFrameSink(
      viz::mojom::blink::CompositorFrameSinkRequest* request)
      : binding_(this, std::move(*request)) {}
  ~MockCompositorFrameSink() override = default;

  MOCK_METHOD1(SetNeedsBeginFrame, void(bool));
  MOCK_METHOD0(SetWantsAnimateOnlyBeginFrames, void());

  MOCK_METHOD2(DoSubmitCompositorFrame,
               void(const viz::LocalSurfaceId&, viz::CompositorFrame*));
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& id,
      viz::CompositorFrame frame,
      viz::mojom::blink::HitTestRegionListPtr hit_test_region_list,
      uint64_t submit_time) override {
    DoSubmitCompositorFrame(id, &frame);
  }
  void SubmitCompositorFrameSync(
      const viz::LocalSurfaceId& id,
      viz::CompositorFrame frame,
      viz::mojom::blink::HitTestRegionListPtr hit_test_region_list,
      uint64_t submit_time,
      const SubmitCompositorFrameSyncCallback callback) override {
    DoSubmitCompositorFrame(id, &frame);
  }

  MOCK_METHOD1(DidNotProduceFrame, void(const viz::BeginFrameAck&));

  MOCK_METHOD2(DidAllocateSharedBitmap_,
               void(mojo::ScopedSharedBufferHandle* buffer,
                    gpu::mojom::blink::MailboxPtr* id));
  void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               gpu::mojom::blink::MailboxPtr id) override {
    DidAllocateSharedBitmap_(&buffer, &id);
  }

  MOCK_METHOD1(DidDeleteSharedBitmap_, void(gpu::mojom::blink::MailboxPtr* id));
  void DidDeleteSharedBitmap(gpu::mojom::blink::MailboxPtr id) override {
    DidDeleteSharedBitmap_(&id);
  }

 private:
  mojo::Binding<viz::mojom::blink::CompositorFrameSink> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockCompositorFrameSink);
};

class MockVideoFrameResourceProvider
    : public blink::VideoFrameResourceProvider {
 public:
  MockVideoFrameResourceProvider(
      viz::ContextProvider* context_provider,
      viz::SharedBitmapReporter* shared_bitmap_reporter)
      : blink::VideoFrameResourceProvider(cc::LayerTreeSettings(), false) {
    blink::VideoFrameResourceProvider::Initialize(context_provider,
                                                  shared_bitmap_reporter);
  }
  ~MockVideoFrameResourceProvider() override = default;

  MOCK_METHOD2(Initialize,
               void(viz::ContextProvider*, viz::SharedBitmapReporter*));
  MOCK_METHOD4(AppendQuads,
               void(viz::RenderPass*,
                    scoped_refptr<media::VideoFrame>,
                    media::VideoRotation,
                    bool));
  MOCK_METHOD0(ReleaseFrameResources, void());
  MOCK_METHOD2(PrepareSendToParent,
               void(const std::vector<viz::ResourceId>&,
                    std::vector<viz::TransferableResource>*));
  MOCK_METHOD1(
      ReceiveReturnsFromParent,
      void(const std::vector<viz::ReturnedResource>& transferable_resources));
  MOCK_METHOD0(ObtainContextProvider, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoFrameResourceProvider);
};
}  // namespace

class VideoFrameSubmitterTest : public testing::Test {
 public:
  VideoFrameSubmitterTest()
      : now_src_(new base::SimpleTestTickClock()),
        begin_frame_source_(new viz::FakeExternalBeginFrameSource(0.f, false)),
        provider_(new StrictMock<MockVideoFrameProvider>()),
        context_provider_(viz::TestContextProvider::Create()) {
    context_provider_->BindToCurrentThread();
  }

  void SetUp() override {
    MakeSubmitter();
    scoped_task_environment_.RunUntilIdle();
  }

  void MakeSubmitter() {
    resource_provider_ = new StrictMock<MockVideoFrameResourceProvider>(
        context_provider_.get(), nullptr);
    submitter_ = std::make_unique<VideoFrameSubmitter>(
        base::BindRepeating(
            [](scoped_refptr<viz::ContextProvider>,
               base::OnceCallback<void(
                   bool, scoped_refptr<viz::ContextProvider>)>) {}),
        base::WrapUnique<MockVideoFrameResourceProvider>(resource_provider_));

    submitter_->Initialize(provider_.get());
    viz::mojom::blink::CompositorFrameSinkPtr submitter_sink;
    viz::mojom::blink::CompositorFrameSinkRequest request =
        mojo::MakeRequest(&submitter_sink);
    sink_ = std::make_unique<StrictMock<MockCompositorFrameSink>>(&request);

    // By setting the submission state before we set the sink, we can make
    // testing easier without having to worry about the first sent frame.
    submitter_->UpdateSubmissionState(true);
    submitter_->SetCompositorFrameSinkPtrForTesting(&submitter_sink);
    submitter_->SetSurfaceIdForTesting(
        viz::SurfaceId(
            viz::FrameSinkId(1, 1),
            viz::LocalSurfaceId(
                11, base::UnguessableToken::Deserialize(0x111111, 0))),
        base::TimeTicks());
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  std::unique_ptr<viz::FakeExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<StrictMock<MockCompositorFrameSink>> sink_;
  std::unique_ptr<StrictMock<MockVideoFrameProvider>> provider_;
  StrictMock<MockVideoFrameResourceProvider>* resource_provider_;
  scoped_refptr<viz::TestContextProvider> context_provider_;
  std::unique_ptr<VideoFrameSubmitter> submitter_;
};

TEST_F(VideoFrameSubmitterTest, StatRenderingFlipsBits) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->Rendering());
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  submitter_->StartRendering();

  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(submitter_->Rendering());
}

TEST_F(VideoFrameSubmitterTest, StopUsingProviderNullsProvider) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->Rendering());
  EXPECT_EQ(provider_.get(), submitter_->Provider());

  submitter_->StopUsingProvider();

  EXPECT_EQ(nullptr, submitter_->Provider());
}

TEST_F(VideoFrameSubmitterTest,
       StopUsingProviderSubmitsFrameAndStopsRendering) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(submitter_->Rendering());

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->StopUsingProvider();

  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->Rendering());
}

TEST_F(VideoFrameSubmitterTest, DidReceiveFrameDoesNothingIfRendering) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(submitter_->Rendering());

  submitter_->DidReceiveFrame();
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameSubmitterTest, DidReceiveFrameSubmitsFrame) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->Rendering());

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->DidReceiveFrame();
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameSubmitterTest, ShouldSubmitPreventsSubmission) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));
  submitter_->UpdateSubmissionState(false);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->ShouldSubmit());

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));
  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _)).Times(1);
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());
  submitter_->UpdateSubmissionState(true);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(submitter_->ShouldSubmit());

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _)).Times(1);
  EXPECT_CALL(*provider_, GetCurrentFrame()).Times(0);
  submitter_->UpdateSubmissionState(false);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->ShouldSubmit());

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*provider_, PutCurrentFrame());

  submitter_->SubmitSingleFrame();
}

// Tests that when set to true SetForceSubmit forces frame submissions.
// regardless of the internal submit state.
TEST_F(VideoFrameSubmitterTest, SetForceSubmitForcesSubmission) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));
  submitter_->UpdateSubmissionState(false);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->ShouldSubmit());

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  submitter_->SetForceSubmit(true);
  EXPECT_TRUE(submitter_->ShouldSubmit());

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _)).Times(1);
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));
  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _)).Times(1);
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());
  submitter_->UpdateSubmissionState(true);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(submitter_->ShouldSubmit());

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _)).Times(1);
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());
  submitter_->UpdateSubmissionState(false);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(submitter_->ShouldSubmit());

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*provider_, PutCurrentFrame());

  submitter_->SubmitSingleFrame();
}

TEST_F(VideoFrameSubmitterTest, RotationInformationPassedToResourceProvider) {
  // Check to see if rotation is communicated pre-rendering.
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->Rendering());

  submitter_->SetRotation(media::VideoRotation::VIDEO_ROTATION_90);

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_,
              AppendQuads(_, _, media::VideoRotation::VIDEO_ROTATION_90, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->DidReceiveFrame();
  scoped_task_environment_.RunUntilIdle();

  {
    WTF::Vector<viz::ReturnedResource> resources;
    EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
    submitter_->DidReceiveCompositorFrameAck(resources);
  }

  // Check to see if an update to rotation just before rendering is
  // communicated.
  submitter_->SetRotation(media::VideoRotation::VIDEO_ROTATION_180);

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));
  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  {
    WTF::Vector<viz::ReturnedResource> resources;
    EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
    submitter_->DidReceiveCompositorFrameAck(resources);
  }

  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_,
              AppendQuads(_, _, media::VideoRotation::VIDEO_ROTATION_180, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();

  {
    WTF::Vector<viz::ReturnedResource> resources;
    EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
    submitter_->DidReceiveCompositorFrameAck(resources);
  }

  // Check to see if changing rotation while rendering is handled.
  submitter_->SetRotation(media::VideoRotation::VIDEO_ROTATION_270);

  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_,
              AppendQuads(_, _, media::VideoRotation::VIDEO_ROTATION_270, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameSubmitterTest, IsOpaquePassedToResourceProvider) {
  // Check to see if is_opaque is communicated pre-rendering.
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(submitter_->Rendering());

  // We submit a frame on opacity change.
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _)).Times(1);
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->SetIsOpaque(false);
  scoped_task_environment_.RunUntilIdle();

  {
    WTF::Vector<viz::ReturnedResource> resources;
    EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
    submitter_->DidReceiveCompositorFrameAck(resources);
  }

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, false));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->DidReceiveFrame();
  scoped_task_environment_.RunUntilIdle();

  {
    WTF::Vector<viz::ReturnedResource> resources;
    EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
    submitter_->DidReceiveCompositorFrameAck(resources);
  }

  // Check to see if an update to is_opaque just before rendering is
  // communicated.
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(false));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _)).Times(1);
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());
  submitter_->SetIsOpaque(true);
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));
  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  {
    WTF::Vector<viz::ReturnedResource> resources;
    EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
    submitter_->DidReceiveCompositorFrameAck(resources);
  }

  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, true));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();

  {
    WTF::Vector<viz::ReturnedResource> resources;
    EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
    submitter_->DidReceiveCompositorFrameAck(resources);
  }

  // Check to see if changing is_opaque while rendering is handled.
  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _)).Times(1);
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->SetIsOpaque(false);
  scoped_task_environment_.RunUntilIdle();

  {
    WTF::Vector<viz::ReturnedResource> resources;
    EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
    submitter_->DidReceiveCompositorFrameAck(resources);
  }

  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, false));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();

  // Updating |is_opaque_| with the same value should not cause a frame submit.
  submitter_->SetIsOpaque(false);
}

TEST_F(VideoFrameSubmitterTest, OnBeginFrameSubmitsFrame) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameSubmitterTest, MissedFrameArgDoesNotProduceFrame) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, DidNotProduceFrame(_));

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  args.type = viz::BeginFrameArgs::MISSED;
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameSubmitterTest, MissingProviderDoesNotProduceFrame) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  submitter_->StopUsingProvider();

  EXPECT_CALL(*sink_, DidNotProduceFrame(_));

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameSubmitterTest, NoUpdateOnFrameDoesNotProduceFrame) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));
  submitter_->StartRendering();

  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).WillOnce(Return(false));
  EXPECT_CALL(*sink_, DidNotProduceFrame(_));

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameSubmitterTest, NotRenderingDoesNotProduceFrame) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  // We don't care if UpdateCurrentFrame is called or not; it doesn't matter
  // if we're not rendering.
  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).Times(AnyNumber());
  EXPECT_CALL(*sink_, DidNotProduceFrame(_));

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(VideoFrameSubmitterTest, ReturnsResourceOnCompositorAck) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  WTF::Vector<viz::ReturnedResource> resources;
  EXPECT_CALL(*resource_provider_, ReceiveReturnsFromParent(_));
  submitter_->DidReceiveCompositorFrameAck(resources);
  scoped_task_environment_.RunUntilIdle();
}

// Tests that after submitting a frame, no frame will be submitted until an ACK
// was received. This is tested by simulating another BeginFrame message.
TEST_F(VideoFrameSubmitterTest, WaitingForAckPreventsNewFrame) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .Times(1)
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));

  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  viz::BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
      BEGINFRAME_FROM_HERE, now_src_.get());
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();

  // DidNotProduceFrame should be called because no frame will be submitted
  // given that the ACK is still pending.
  EXPECT_CALL(*sink_, DidNotProduceFrame(_)).Times(1);

  // UpdateCurrentFrame should still be called, however, so that the compositor
  // knows that we missed a frame.
  EXPECT_CALL(*provider_, UpdateCurrentFrame(_, _)).Times(1);

  std::unique_ptr<base::SimpleTestTickClock> new_time =
      std::make_unique<base::SimpleTestTickClock>();
  args = begin_frame_source_->CreateBeginFrameArgs(BEGINFRAME_FROM_HERE,
                                                   new_time.get());
  submitter_->OnBeginFrame(args);
  scoped_task_environment_.RunUntilIdle();
}

// Test that no crash happens if the context is lost during a frame submission.
TEST_F(VideoFrameSubmitterTest, ContextLostDuringSubmit) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*provider_, PutCurrentFrame());

  // This will post a task that will later call SubmitFrame(). The call will
  // happen after OnContextLost().
  submitter_->SubmitSingleFrame();

  submitter_->OnContextLost();

  scoped_task_environment_.RunUntilIdle();
}

// Test the behaviour of the ChildLocalSurfaceIdAllocator instance. It checks
// that the LocalSurfaceId is propoerly set at creation and updated when the
// video frames change.
TEST_F(VideoFrameSubmitterTest, FrameSizeChangeUpdatesLocalSurfaceId) {
  MakeSubmitter();
  scoped_task_environment_.RunUntilIdle();

  {
    viz::LocalSurfaceId local_surface_id =
        submitter_->child_local_surface_id_allocator_
            .GetCurrentLocalSurfaceId();
    EXPECT_TRUE(local_surface_id.is_valid());
    EXPECT_EQ(11u, local_surface_id.parent_sequence_number());
    EXPECT_EQ(viz::kInitialChildSequenceNumber,
              local_surface_id.child_sequence_number());
    EXPECT_TRUE(submitter_->frame_size_.IsEmpty());
  }

  EXPECT_CALL(*sink_, SetNeedsBeginFrame(true));

  submitter_->StartRendering();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(8, 8), gfx::Rect(gfx::Size(8, 8)),
          gfx::Size(8, 8), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->SubmitSingleFrame();
  scoped_task_environment_.RunUntilIdle();

  {
    viz::LocalSurfaceId local_surface_id =
        submitter_->child_local_surface_id_allocator_
            .GetCurrentLocalSurfaceId();
    EXPECT_TRUE(local_surface_id.is_valid());
    EXPECT_EQ(11u, local_surface_id.parent_sequence_number());
    EXPECT_EQ(viz::kInitialChildSequenceNumber,
              local_surface_id.child_sequence_number());
    EXPECT_EQ(gfx::Rect(8, 8), submitter_->frame_size_);
  }

  EXPECT_CALL(*provider_, GetCurrentFrame())
      .WillOnce(Return(media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_YV12, gfx::Size(2, 2), gfx::Rect(gfx::Size(2, 2)),
          gfx::Size(2, 2), base::TimeDelta())));
  EXPECT_CALL(*sink_, DoSubmitCompositorFrame(_, _));
  EXPECT_CALL(*provider_, PutCurrentFrame());
  EXPECT_CALL(*resource_provider_, AppendQuads(_, _, _, _));
  EXPECT_CALL(*resource_provider_, PrepareSendToParent(_, _));
  EXPECT_CALL(*resource_provider_, ReleaseFrameResources());

  submitter_->SubmitSingleFrame();
  scoped_task_environment_.RunUntilIdle();

  {
    viz::LocalSurfaceId local_surface_id =
        submitter_->child_local_surface_id_allocator_
            .GetCurrentLocalSurfaceId();
    EXPECT_TRUE(local_surface_id.is_valid());
    EXPECT_EQ(11u, local_surface_id.parent_sequence_number());
    EXPECT_EQ(viz::kInitialChildSequenceNumber + 1,
              local_surface_id.child_sequence_number());
    EXPECT_EQ(gfx::Rect(2, 2), submitter_->frame_size_);
  }
}

}  // namespace blink
