// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/win/media_foundation_renderer_client.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/renderer/media/win/overlay_state_service_provider.h"
#include "media/base/fake_demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/mock_media_log.h"
#include "media/base/video_renderer_sink.h"
#include "media/base/win/dcomp_texture_wrapper.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class FakeMojomRenderer : public mojom::Renderer {
 public:
  FakeMojomRenderer() = default;
  ~FakeMojomRenderer() override = default;
  // mojom::Renderer implementations.
  void Initialize(
      mojo::PendingAssociatedRemote<mojom::RendererClient>,
      std::optional<std::vector<mojo::PendingRemote<mojom::DemuxerStream>>>,
      mojom::MediaUrlParamsPtr,
      InitializeCallback cb) override {
    std::move(cb).Run(true);
  }
  // mojom::Renderer mocks.
  MOCK_METHOD(void, Flush, (FlushCallback), (override));
  MOCK_METHOD(void, StartPlayingFrom, (base::TimeDelta), (override));
  MOCK_METHOD(void, SetPlaybackRate, (double), (override));
  MOCK_METHOD(void, SetVolume, (float), (override));
  MOCK_METHOD(void,
              SetCdm,
              (const std::optional<base::UnguessableToken>&, SetCdmCallback),
              (override));
  MOCK_METHOD(void,
              SetLatencyHint,
              (std::optional<base::TimeDelta>),
              (override));
};

class FakeMediaFoundationRendererExtension
    : public mojom::MediaFoundationRendererExtension {
 public:
  FakeMediaFoundationRendererExtension() = default;
  ~FakeMediaFoundationRendererExtension() override = default;

  // MediaFoundationRendererExtension implementations.
  void GetDCOMPSurface(GetDCOMPSurfaceCallback cb) override {
    // Generate random token
    base::UnguessableToken token = base::UnguessableToken::Create();
    std::move(cb).Run(token, "");
  }

  void SetOutputRect(const gfx::Rect&, SetOutputRectCallback cb) override {
    std::move(cb).Run(true);
  }

  // MediaFoundationRendererExtension mocks.
  MOCK_METHOD(void, SetVideoStreamEnabled, (bool), (override));
  MOCK_METHOD(void,
              NotifyFrameReleased,
              (const base::UnguessableToken&),
              (override));
  MOCK_METHOD(void, RequestNextFrame, (), (override));
  MOCK_METHOD(void,
              SetMediaFoundationRenderingMode,
              (MediaFoundationRenderingMode),
              (override));
};

class FakeDCOMPTextureWrapper : public DCOMPTextureWrapper {
 public:
  FakeDCOMPTextureWrapper(gpu::Mailbox frame_server_mailbox,
                          gpu::Mailbox dcomp_mailbox)
      : frame_server_mailbox_(frame_server_mailbox),
        dcomp_mailbox_(dcomp_mailbox) {}
  ~FakeDCOMPTextureWrapper() override = default;

  // DCompTextureWrapper implementations.
  bool Initialize(const gfx::Size&, OutputRectChangeCB cb) override {
    gfx::Rect rect(1920, 1080);
    std::move(cb).Run(rect);
    return true;
  }

  void SetDCOMPSurfaceHandle(const base::UnguessableToken&,
                             SetDCOMPSurfaceHandleCB cb) override {
    std::move(cb).Run(true);
  }

  void CreateVideoFrame(const gfx::Size& size, CreateVideoFrameCB cb) override {
    // DComp CreateVideoFrame
    scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateBlackFrame(size);
    std::move(cb).Run(std::move(video_frame), std::move(frame_server_mailbox_));
  }

  void CreateVideoFrame(const gfx::Size& size,
                        gfx::GpuMemoryBufferHandle,
                        CreateDXVideoFrameCB cb) override {
    // Frame Server CreateVideoFrame
    scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateBlackFrame(size);
    std::move(cb).Run(std::move(video_frame), std::move(dcomp_mailbox_));
  }

  // DCompTextureWrapper mocks.
  MOCK_METHOD(void, UpdateTextureSize, (const gfx::Size&), (override));

 private:
  gpu::Mailbox frame_server_mailbox_;
  gpu::Mailbox dcomp_mailbox_;
};

class MockVideoRendererSink : public VideoRendererSink {
 public:
  MockVideoRendererSink() = default;
  ~MockVideoRendererSink() override = default;

  // VideoRendererSink mocks.
  MOCK_METHOD(void, Start, (RenderCallback*));
  MOCK_METHOD(void, Stop, ());
  MOCK_METHOD(void, PaintSingleFrame, (scoped_refptr<VideoFrame>, bool));
};

class MockOverlayStateObserverSubscription
    : public OverlayStateObserverSubscription {
 public:
  MockOverlayStateObserverSubscription() = default;
  ~MockOverlayStateObserverSubscription() override = default;
};

class MockRendererClientMF : public RendererClient {
 public:
  MockRendererClientMF() = default;
  ~MockRendererClientMF() = default;

  // RendererClient mocks.
  MOCK_METHOD(void, OnError, (PipelineStatus), (override));
  MOCK_METHOD(void, OnFallback, (PipelineStatus), (override));
  MOCK_METHOD(void, OnEnded, (), (override));
  MOCK_METHOD(void,
              OnStatisticsUpdate,
              (const PipelineStatistics&),
              (override));
  MOCK_METHOD(void,
              OnBufferingStateChange,
              (BufferingState, BufferingStateChangeReason),
              (override));
  MOCK_METHOD(void, OnWaiting, (WaitingReason), (override));
  MOCK_METHOD(void,
              OnAudioConfigChange,
              (const AudioDecoderConfig&),
              (override));
  MOCK_METHOD(void,
              OnVideoConfigChange,
              (const VideoDecoderConfig&),
              (override));
  MOCK_METHOD(void, OnVideoNaturalSizeChange, (const gfx::Size&), (override));
  MOCK_METHOD(void, OnVideoOpacityChange, (bool), (override));
  MOCK_METHOD(void, OnVideoFrameRateChange, (std::optional<int>), (override));
};

class MediaFoundationRendererClientTest
    : public ::testing::Test,
      public media::mojom::MediaFoundationRendererObserver {
 public:
  MediaFoundationRendererClientTest() = default;
  ~MediaFoundationRendererClientTest() override = default;

  void SetUp() override {
    // Create the necessary mocks & fakes to create a
    // MediaFoundationRendererClient.
    mojo::PendingRemote<mojom::Renderer> renderer_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FakeMojomRenderer>(),
        renderer_remote.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<mojom::MediaFoundationRendererExtension>
        mf_renderer_extensions_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FakeMediaFoundationRendererExtension>(),
        mf_renderer_extensions_remote.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
        client_extension_remote;
    auto client_extension_receiver =
        client_extension_remote.InitWithNewPipeAndPassReceiver();

    auto mojo_renderer = std::make_unique<MojoRenderer>(
        task_environment_.GetMainThreadTaskRunner(),
        /*video_overlay_factory*/ nullptr,
        /*video_renderer_sink*/ nullptr, std::move(renderer_remote));

    frame_server_mailbox_ = gpu::Mailbox::Generate();
    dcomp_mailbox_ = gpu::Mailbox::Generate();
    dcomp_texture_wrapper_ = std::make_unique<FakeDCOMPTextureWrapper>(
        frame_server_mailbox_, dcomp_mailbox_);

    ObserveOverlayStateCB observe_overlay_state_cb = base::BindRepeating(
        &MediaFoundationRendererClientTest::ObserveOverlaySupscription,
        base::Unretained(this));

    mojo::PendingRemote<media::mojom::MediaFoundationRendererObserver>
        media_foundation_renderer_observer_remote;

    media_foundation_renderer_client_ =
        std::make_unique<MediaFoundationRendererClient>(
            task_environment_.GetMainThreadTaskRunner(), media_log_.Clone(),
            std::move(mojo_renderer), std::move(mf_renderer_extensions_remote),
            std::move(client_extension_receiver),
            std::move(dcomp_texture_wrapper_), observe_overlay_state_cb,
            &mock_video_renderer_sink_,
            std::move(media_foundation_renderer_observer_remote));
  }

  void OnPipelineStatus(PipelineStatus status) { last_status_ = status; }

  std::unique_ptr<media::OverlayStateObserverSubscription>
  ObserveOverlaySupscription(
      const gpu::Mailbox& mailbox,
      OverlayStateObserverSubscription::StateChangedCB cb) {
    if (mailbox == frame_server_mailbox_) {
      frame_server_on_state_change_cb_ = cb;
    } else if (mailbox == dcomp_mailbox_) {
      dcomp_on_state_change_cb_ = cb;
    } else {
      // Unexpected
      NOTREACHED_IN_MIGRATION();
    }

    return std::make_unique<MockOverlayStateObserverSubscription>();
  }

  void InitializeMediaFoundationRendererClient() {
    fake_media_resource_ = std::make_unique<FakeMediaResource>(3, 9, false);
    renderer_client_ = new MockRendererClientMF();
    media_foundation_renderer_client_->Initialize(
        fake_media_resource_.get(), renderer_client_,
        base::BindOnce(&MediaFoundationRendererClientTest::OnPipelineStatus,
                       base::Unretained(this)));
    // Force a call to OnVideoNaturalSizeChange to force creation of a
    // DComp video frame.
    media_foundation_renderer_client_->OnVideoNaturalSizeChange(
        gfx::Size(1920, 1080));
  }

  void InitializeFramePool() {
    gfx::GpuMemoryBufferHandle gpu_handle;
    HANDLE shared_texture_handle = INVALID_HANDLE_VALUE;
    base::win::ScopedHandle scoped_shared_texture_handle;
    scoped_shared_texture_handle.Set(shared_texture_handle);

    gpu_handle.dxgi_handle = std::move(scoped_shared_texture_handle);
    gpu_handle.dxgi_token = gfx::DXGIHandleToken();
    gpu_handle.type = gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;

    auto frame_info = media::mojom::FrameTextureInfo::New();
    frame_info->token = base::UnguessableToken::Create();
    frame_info->texture_handle = std::move(gpu_handle);

    auto pool_params = media::mojom::FramePoolInitializationParameters::New();
    pool_params->frame_textures.emplace_back(std::move(frame_info));
    pool_params->texture_size = gfx::Size(1920, 1080);

    media_foundation_renderer_client_->InitializeFramePool(
        std::move(pool_params));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;

  MockMediaLog media_log_;
  MockVideoRendererSink mock_video_renderer_sink_;
  raw_ptr<RendererClient> renderer_client_;
  std::unique_ptr<FakeMediaResource> fake_media_resource_;
  std::unique_ptr<MediaFoundationRendererClient>
      media_foundation_renderer_client_;

  gpu::Mailbox dcomp_mailbox_;
  gpu::Mailbox frame_server_mailbox_;
  OverlayStateObserverSubscription::StateChangedCB
      frame_server_on_state_change_cb_;
  OverlayStateObserverSubscription::StateChangedCB dcomp_on_state_change_cb_;

  PipelineStatus last_status_ = PIPELINE_STATUS_MAX;
  std::unique_ptr<DCOMPTextureWrapper> dcomp_texture_wrapper_;
};

TEST_F(MediaFoundationRendererClientTest, CreateAndDestroy) {}

TEST_F(MediaFoundationRendererClientTest, CreateInitializeAndDestroy) {
  feature_list_.InitWithFeatures({kMediaFoundationClearPlayback}, {});
  InitializeMediaFoundationRendererClient();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(last_status_, PIPELINE_OK);
}

TEST_F(MediaFoundationRendererClientTest, InitializeFrameServer) {
  base::FieldTrialParams params;
  params["strategy"] = "frame-server";
  feature_list_.InitWithFeaturesAndParameters(
      {{kMediaFoundationClearRendering, params},
       {kMediaFoundationClearPlayback, {}}},
      {});
  InitializeMediaFoundationRendererClient();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(last_status_, PIPELINE_OK);
  EXPECT_TRUE(media_foundation_renderer_client_->IsFrameServerMode());
}

TEST_F(MediaFoundationRendererClientTest, InitializeDComp) {
  base::FieldTrialParams params;
  params["strategy"] = "direct-composition";
  feature_list_.InitWithFeaturesAndParameters(
      {{kMediaFoundationClearRendering, params},
       {kMediaFoundationClearPlayback, {}}},
      {});
  InitializeMediaFoundationRendererClient();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(last_status_, PIPELINE_OK);
  EXPECT_FALSE(media_foundation_renderer_client_->IsFrameServerMode());
}

TEST_F(MediaFoundationRendererClientTest, InitializeDynamic) {
  base::FieldTrialParams params;
  params["strategy"] = "dynamic";
  feature_list_.InitWithFeaturesAndParameters(
      {{kMediaFoundationClearRendering, params},
       {kMediaFoundationClearPlayback, {}}},
      {});
  InitializeMediaFoundationRendererClient();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(last_status_, PIPELINE_OK);
  // Dynamic mode should start in Frame Server until a successful overlay
  // promotion hint.
  EXPECT_TRUE(media_foundation_renderer_client_->IsFrameServerMode());
}

TEST_F(MediaFoundationRendererClientTest, VerifyDynamicPromotionAndDemotion) {
  base::FieldTrialParams params;
  params["strategy"] = "dynamic";
  feature_list_.InitWithFeaturesAndParameters(
      {{kMediaFoundationClearRendering, params},
       {kMediaFoundationClearPlayback, {}}},
      {});
  InitializeMediaFoundationRendererClient();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(last_status_, PIPELINE_OK);
  // Dynamic mode should start in Frame Server until a successful overlay
  // promotion hint.
  EXPECT_TRUE(media_foundation_renderer_client_->IsFrameServerMode());

  // Initialize Frame Pool
  InitializeFramePool();
  base::RunLoop().RunUntilIdle();
  // We should still be in Frame Server mode.
  EXPECT_TRUE(media_foundation_renderer_client_->IsFrameServerMode());

  DCHECK(frame_server_on_state_change_cb_);
  // Promote
  std::move(frame_server_on_state_change_cb_).Run(true);
  base::RunLoop().RunUntilIdle();

  // Verify we're now in Direct Composition mode
  EXPECT_FALSE(media_foundation_renderer_client_->IsFrameServerMode());

  DCHECK(dcomp_on_state_change_cb_);
  // Demote
  std::move(dcomp_on_state_change_cb_).Run(false);
  base::RunLoop().RunUntilIdle();

  // Verify we're back in Frame Server mode
  EXPECT_TRUE(media_foundation_renderer_client_->IsFrameServerMode());
}

}  // namespace media
