// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "components/viz/test/test_context_provider.h"
#include "media/base/decoder_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/blink/mock_resource_fetch_context.h"
#include "media/blink/mock_webassociatedurlloader.h"
#include "media/blink/resource_multibuffer_data_provider.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "media/mojo/services/video_decode_stats_recorder.h"
#include "media/mojo/services/watch_time_recorder.h"
#include "media/renderers/default_decoder_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/picture_in_picture/picture_in_picture_control_info.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_scoped_user_gesture.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "media/blink/renderer_media_player_interface.h"
#endif

using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::_;

namespace media {

constexpr char kAudioOnlyTestFile[] = "sfx-opus-441.webm";
constexpr char kVideoOnlyTestFile[] = "bear-320x240-video-only.webm";

MATCHER(WmpiDestroyed, "") {
  return CONTAINS_STRING(arg, "WEBMEDIAPLAYER_DESTROYED {}");
}

MATCHER_P2(PlaybackRateChanged, old_rate_string, new_rate_string, "") {
  return CONTAINS_STRING(arg, "Effective playback rate changed from " +
                                  std::string(old_rate_string) + " to " +
                                  std::string(new_rate_string));
}

#if defined(OS_ANDROID)
class MockRendererMediaPlayerManager
    : public RendererMediaPlayerManagerInterface {
 public:
  MOCK_METHOD7(Initialize,
               void(MediaPlayerHostMsg_Initialize_Type type,
                    int player_id,
                    const GURL& url,
                    const GURL& site_for_cookies,
                    const GURL& frame_url,
                    bool allow_credentials,
                    int delegate_id));
  MOCK_METHOD1(Start, void(int player_id));
  MOCK_METHOD2(Pause, void(int player_id, bool is_media_related_action));
  MOCK_METHOD2(Seek, void(int player_id, base::TimeDelta time));
  MOCK_METHOD2(SetVolume, void(int player_id, double volume));
  MOCK_METHOD2(SetPoster, void(int player_id, const GURL& poster));
  MOCK_METHOD1(SuspendAndReleaseResources, void(int player_id));
  MOCK_METHOD1(DestroyPlayer, void(int player_id));
  MOCK_METHOD1(RequestRemotePlayback, void(int player_id));
  MOCK_METHOD1(RequestRemotePlaybackControl, void(int player_id));
  MOCK_METHOD1(RequestRemotePlaybackStop, void(int player_id));
  MOCK_METHOD1(RegisterMediaPlayer, int(RendererMediaPlayerInterface* player));
  MOCK_METHOD1(UnregisterMediaPlayer, void(int player_id));
};
#endif

class MockWebMediaPlayerClient : public blink::WebMediaPlayerClient {
 public:
  MockWebMediaPlayerClient() = default;

  MOCK_METHOD0(NetworkStateChanged, void());
  MOCK_METHOD0(ReadyStateChanged, void());
  MOCK_METHOD0(TimeChanged, void());
  MOCK_METHOD0(Repaint, void());
  MOCK_METHOD0(DurationChanged, void());
  MOCK_METHOD0(SizeChanged, void());
  MOCK_METHOD1(SetCcLayer, void(cc::Layer*));
  MOCK_METHOD5(AddAudioTrack,
               blink::WebMediaPlayer::TrackId(
                   const blink::WebString&,
                   blink::WebMediaPlayerClient::AudioTrackKind,
                   const blink::WebString&,
                   const blink::WebString&,
                   bool));
  MOCK_METHOD1(RemoveAudioTrack, void(blink::WebMediaPlayer::TrackId));
  MOCK_METHOD5(AddVideoTrack,
               blink::WebMediaPlayer::TrackId(
                   const blink::WebString&,
                   blink::WebMediaPlayerClient::VideoTrackKind,
                   const blink::WebString&,
                   const blink::WebString&,
                   bool));
  MOCK_METHOD1(RemoveVideoTrack, void(blink::WebMediaPlayer::TrackId));
  MOCK_METHOD1(AddTextTrack, void(blink::WebInbandTextTrack*));
  MOCK_METHOD1(RemoveTextTrack, void(blink::WebInbandTextTrack*));
  MOCK_METHOD1(MediaSourceOpened, void(blink::WebMediaSource*));
  MOCK_METHOD1(RequestSeek, void(double));
  MOCK_METHOD1(RemoteRouteAvailabilityChanged,
               void(blink::WebRemotePlaybackAvailability));
  MOCK_METHOD0(ConnectedToRemoteDevice, void());
  MOCK_METHOD0(DisconnectedFromRemoteDevice, void());
  MOCK_METHOD0(CancelledRemotePlaybackRequest, void());
  MOCK_METHOD0(RemotePlaybackStarted, void());
  MOCK_METHOD2(RemotePlaybackCompatibilityChanged,
               void(const blink::WebURL&, bool));
  MOCK_METHOD1(OnBecamePersistentVideo, void(bool));
  MOCK_METHOD0(WasAlwaysMuted, bool());
  MOCK_METHOD0(HasSelectedVideoTrack, bool());
  MOCK_METHOD0(GetSelectedVideoTrackId, blink::WebMediaPlayer::TrackId());
  MOCK_METHOD0(HasNativeControls, bool());
  MOCK_METHOD0(IsAudioElement, bool());
  MOCK_CONST_METHOD0(DisplayType, blink::WebMediaPlayer::DisplayType());
  MOCK_CONST_METHOD0(IsInAutoPIP, bool());
  MOCK_METHOD1(ActivateViewportIntersectionMonitoring, void(bool));
  MOCK_METHOD1(MediaRemotingStarted, void(const blink::WebString&));
  MOCK_METHOD1(MediaRemotingStopped, void(blink::WebLocalizedString::Name));
  MOCK_METHOD0(PictureInPictureStarted, void());
  MOCK_METHOD0(PictureInPictureStopped, void());
  MOCK_METHOD1(PictureInPictureControlClicked, void(const blink::WebString&));
  MOCK_CONST_METHOD0(CouldPlayIfEnoughData, bool());
  MOCK_METHOD0(RequestPlay, void());
  MOCK_METHOD0(RequestPause, void());

  void set_was_always_muted(bool value) { was_always_muted_ = value; }

  bool was_always_muted_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebMediaPlayerClient);
};

class MockWebMediaPlayerDelegate : public WebMediaPlayerDelegate {
 public:
  MockWebMediaPlayerDelegate() = default;
  ~MockWebMediaPlayerDelegate() override = default;

  // WebMediaPlayerDelegate implementation.
  int AddObserver(Observer* observer) override {
    DCHECK_EQ(nullptr, observer_);
    observer_ = observer;
    return player_id_;
  }

  void RemoveObserver(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    observer_ = nullptr;
  }

  MOCK_METHOD4(DidPlay, void(int, bool, bool, MediaContentType));
  MOCK_METHOD1(DidPause, void(int));
  MOCK_METHOD1(PlayerGone, void(int));

  void SetIdle(int player_id, bool is_idle) override {
    DCHECK_EQ(player_id_, player_id);
    is_idle_ = is_idle;
    is_stale_ &= is_idle;
  }

  bool IsIdle(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    return is_idle_;
  }

  void DidPlayerMutedStatusChange(int delegate_id, bool muted) override {
    DCHECK_EQ(player_id_, delegate_id);
  }

  MOCK_METHOD5(DidPictureInPictureModeStart,
               void(int,
                    const viz::SurfaceId&,
                    const gfx::Size&,
                    blink::WebMediaPlayer::PipWindowOpenedCallback,
                    bool));
  MOCK_METHOD2(DidPictureInPictureModeEnd,
               void(int, blink::WebMediaPlayer::PipWindowClosedCallback));
  MOCK_METHOD2(DidSetPictureInPictureCustomControls,
               void(int,
                    const std::vector<blink::PictureInPictureControlInfo>&));
  MOCK_METHOD4(DidPictureInPictureSurfaceChange,
               void(int, const viz::SurfaceId&, const gfx::Size&, bool));
  MOCK_METHOD2(RegisterPictureInPictureWindowResizeCallback,
               void(int, blink::WebMediaPlayer::PipWindowResizedCallback));

  void ClearStaleFlag(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    is_stale_ = false;
  }

  bool IsStale(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    return is_stale_;
  }

  void SetIsEffectivelyFullscreen(
      int player_id,
      blink::WebFullscreenVideoStatus fullscreen_video_status) override {
    DCHECK_EQ(player_id_, player_id);
  }

  void DidPlayerSizeChange(int player_id, const gfx::Size& size) override {
    DCHECK_EQ(player_id_, player_id);
  }

  bool IsFrameHidden() override { return is_hidden_; }

  bool IsFrameClosed() override { return is_closed_; }

  bool IsBackgroundMediaSuspendEnabled() override {
    return is_background_media_suspend_enabled_;
  }

  void SetIdleForTesting(bool is_idle) { is_idle_ = is_idle; }

  void SetStaleForTesting(bool is_stale) {
    is_idle_ |= is_stale;
    is_stale_ = is_stale;
  }

  // Returns true if the player does in fact expire.
  bool ExpireForTesting() {
    if (is_idle_ && !is_stale_) {
      is_stale_ = true;
      observer_->OnIdleTimeout();
    }

    return is_stale_;
  }

  void SetFrameHiddenForTesting(bool is_hidden) { is_hidden_ = is_hidden; }

  void SetFrameClosedForTesting(bool is_closed) { is_closed_ = is_closed; }

  void SetBackgroundMediaSuspendEnabledForTesting(bool enable) {
    is_background_media_suspend_enabled_ = enable;
  }

  int player_id() { return player_id_; }

 private:
  Observer* observer_ = nullptr;
  int player_id_ = 1234;
  bool is_idle_ = false;
  bool is_stale_ = false;
  bool is_hidden_ = false;
  bool is_closed_ = false;
  bool is_background_media_suspend_enabled_ = false;
};

class MockSurfaceLayerBridge : public blink::WebSurfaceLayerBridge {
 public:
  MOCK_CONST_METHOD0(GetCcLayer, cc::Layer*());
  MOCK_CONST_METHOD0(GetFrameSinkId, const viz::FrameSinkId&());
  MOCK_CONST_METHOD0(GetSurfaceId, const viz::SurfaceId&());
  MOCK_CONST_METHOD0(GetLocalSurfaceIdAllocationTime, base::TimeTicks());
  MOCK_METHOD0(ClearSurfaceId, void());
  MOCK_METHOD1(SetContentsOpaque, void(bool));
  MOCK_METHOD0(CreateSurfaceLayer, void());
};

class MockVideoFrameCompositor : public VideoFrameCompositor {
 public:
  MockVideoFrameCompositor(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : VideoFrameCompositor(task_runner, nullptr) {}
  ~MockVideoFrameCompositor() override = default;

  // MOCK_METHOD doesn't like OnceCallback.
  void SetOnNewProcessedFrameCallback(OnNewProcessedFrameCB cb) override {}
  MOCK_METHOD0(GetCurrentFrameAndUpdateIfStale, scoped_refptr<VideoFrame>());
  MOCK_METHOD6(EnableSubmission,
               void(const viz::SurfaceId&,
                    base::TimeTicks,
                    media::VideoRotation,
                    bool,
                    bool,
                    blink::WebFrameSinkDestroyedCallback));
};

class WebMediaPlayerImplTest : public testing::Test {
 public:
  WebMediaPlayerImplTest()
      : media_thread_("MediaThreadForTest"),
        web_view_(
            blink::WebView::Create(/*client=*/nullptr,
                                   /*widget_client=*/nullptr,
                                   blink::mojom::PageVisibilityState::kVisible,
                                   nullptr)),
        web_local_frame_(
            blink::WebLocalFrame::CreateMainFrame(web_view_,
                                                  &web_frame_client_,
                                                  nullptr,
                                                  nullptr)),
        context_provider_(viz::TestContextProvider::Create()),
        audio_parameters_(TestAudioParameters::Normal()) {
    media_thread_.StartAndWaitForTesting();
  }

  void InitializeWebMediaPlayerImpl() {
    auto media_log = std::make_unique<NiceMock<MockMediaLog>>();
    surface_layer_bridge_ =
        std::make_unique<StrictMock<MockSurfaceLayerBridge>>();
    surface_layer_bridge_ptr_ = surface_layer_bridge_.get();

    // Retain a raw pointer to |media_log| for use by tests. Meanwhile, give its
    // ownership to |wmpi_|. Reject attempts to reinitialize to prevent orphaned
    // expectations on previous |media_log_|.
    ASSERT_FALSE(media_log_) << "Reinitialization of media_log_ is disallowed";
    media_log_ = media_log.get();

    decoder_factory_.reset(new media::DefaultDecoderFactory(nullptr));
    auto factory_selector = std::make_unique<RendererFactorySelector>();
    factory_selector->AddFactory(
        RendererFactorySelector::FactoryType::DEFAULT,
        std::make_unique<DefaultRendererFactory>(
            media_log.get(), decoder_factory_.get(),
            DefaultRendererFactory::GetGpuFactoriesCB()));
    factory_selector->SetBaseFactoryType(
        RendererFactorySelector::FactoryType::DEFAULT);

    mojom::MediaMetricsProviderPtr provider;
    MediaMetricsProvider::Create(
        false, base::BindRepeating([]() { return ukm::kInvalidSourceId; }),
        VideoDecodePerfHistory::SaveCallback(), mojo::MakeRequest(&provider));

    // Initialize provider since none of the tests below actually go through the
    // full loading/pipeline initialize phase. If this ever changes the provider
    // will start DCHECK failing.
    provider->Initialize(false, mojom::MediaURLScheme::kHttp);

    audio_sink_ = base::WrapRefCounted(new NiceMock<MockAudioRendererSink>());

    url_index_.reset(new UrlIndex(&mock_resource_fetch_context_));

    auto params = std::make_unique<WebMediaPlayerParams>(
        std::move(media_log), WebMediaPlayerParams::DeferLoadCB(), audio_sink_,
        media_thread_.task_runner(), base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(), media_thread_.task_runner(),
        base::BindRepeating(&WebMediaPlayerImplTest::OnAdjustAllocatedMemory,
                            base::Unretained(this)),
        nullptr, RequestRoutingTokenCallback(), nullptr, false, false,
        std::move(provider),
        base::BindOnce(&WebMediaPlayerImplTest::CreateMockSurfaceLayerBridge,
                       base::Unretained(this)),
        viz::TestContextProvider::Create(),
        base::FeatureList::IsEnabled(media::kUseSurfaceLayerForVideo)
            ? blink::WebMediaPlayer::SurfaceLayerMode::kAlways
            : blink::WebMediaPlayer::SurfaceLayerMode::kNever);

    auto compositor = std::make_unique<StrictMock<MockVideoFrameCompositor>>(
        params->video_frame_compositor_task_runner());
    compositor_ = compositor.get();

    wmpi_ = std::make_unique<WebMediaPlayerImpl>(
        web_local_frame_, &client_, nullptr, &delegate_,
        std::move(factory_selector), url_index_.get(), std::move(compositor),
        std::move(params));

#if defined(OS_ANDROID)
    wmpi_->SetMediaPlayerManager(&mock_media_player_manager_);
#endif
  }

  ~WebMediaPlayerImplTest() override {
    EXPECT_CALL(client_, SetCcLayer(nullptr));
    EXPECT_CALL(client_, MediaRemotingStopped(_));
    // Destruct WebMediaPlayerImpl and pump the message loop to ensure that
    // objects passed to the message loop for destruction are released.
    //
    // NOTE: This should be done before any other member variables are
    // destructed since WMPI may reference them during destruction.
    wmpi_.reset();

    base::RunLoop().RunUntilIdle();

    web_view_->Close();
  }

 protected:
  std::unique_ptr<blink::WebSurfaceLayerBridge> CreateMockSurfaceLayerBridge(
      blink::WebSurfaceLayerBridgeObserver*,
      cc::UpdateSubmissionStateCB) {
    return std::move(surface_layer_bridge_);
  }

  int64_t OnAdjustAllocatedMemory(int64_t delta) {
    reported_memory_ += delta;
    return 0;
  }

  void SetNetworkState(blink::WebMediaPlayer::NetworkState state) {
    EXPECT_CALL(client_, NetworkStateChanged());
    wmpi_->SetNetworkState(state);
  }

  void SetReadyState(blink::WebMediaPlayer::ReadyState state) {
    EXPECT_CALL(client_, ReadyStateChanged());
    wmpi_->SetReadyState(state);
  }

  void SetDuration(base::TimeDelta value) {
    wmpi_->SetPipelineMediaDurationForTest(value);
  }

  base::TimeDelta GetCurrentTimeInternal() {
    return wmpi_->GetCurrentTimeInternal();
  }

  void SetPaused(bool is_paused) { wmpi_->paused_ = is_paused; }
  void SetSeeking(bool is_seeking) { wmpi_->seeking_ = is_seeking; }
  void SetEnded(bool is_ended) { wmpi_->ended_ = is_ended; }
  void SetTickClock(const base::TickClock* clock) {
    wmpi_->SetTickClockForTest(clock);
  }

  void SetFullscreen(bool is_fullscreen) {
    wmpi_->overlay_enabled_ = is_fullscreen;
  }

  void SetMetadata(bool has_audio, bool has_video) {
    wmpi_->SetNetworkState(blink::WebMediaPlayer::kNetworkStateLoaded);

    EXPECT_CALL(client_, ReadyStateChanged());
    wmpi_->SetReadyState(blink::WebMediaPlayer::kReadyStateHaveMetadata);
    EXPECT_CALL(client_, WasAlwaysMuted())
        .WillRepeatedly(Return(client_.was_always_muted_));
    wmpi_->pipeline_metadata_.has_audio = has_audio;
    wmpi_->pipeline_metadata_.has_video = has_video;

    if (has_video)
      wmpi_->pipeline_metadata_.video_decoder_config =
          TestVideoConfig::Normal();

    if (has_audio)
      wmpi_->pipeline_metadata_.audio_decoder_config =
          TestAudioConfig::Normal();
  }

  void SetError(PipelineStatus status = PIPELINE_ERROR_DECODE) {
    wmpi_->OnError(status);
  }

  void OnMetadata(PipelineMetadata metadata) { wmpi_->OnMetadata(metadata); }

  void OnVideoNaturalSizeChange(const gfx::Size& size) {
    wmpi_->OnVideoNaturalSizeChange(size);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState() {
    EXPECT_CALL(client_, WasAlwaysMuted())
        .WillRepeatedly(Return(client_.was_always_muted_));
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_FrameHidden() {
    EXPECT_CALL(client_, WasAlwaysMuted())
        .WillRepeatedly(Return(client_.was_always_muted_));
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, true);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Suspended() {
    EXPECT_CALL(client_, WasAlwaysMuted())
        .WillRepeatedly(Return(client_.was_always_muted_));
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, true, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Remote() {
    EXPECT_CALL(client_, WasAlwaysMuted())
        .WillRepeatedly(Return(client_.was_always_muted_));
    return wmpi_->UpdatePlayState_ComputePlayState(true, true, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_BackgroundedStreaming() {
    EXPECT_CALL(client_, WasAlwaysMuted())
        .WillRepeatedly(Return(client_.was_always_muted_));
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, true);
  }

  bool IsSuspended() { return wmpi_->pipeline_controller_.IsSuspended(); }

  int64_t GetDataSourceMemoryUsage() const {
    return wmpi_->data_source_->GetMemoryUsage();
  }

  void AddBufferedRanges() {
    wmpi_->buffered_data_source_host_.AddBufferedByteRange(0, 1);
  }

  void SetDelegateState(WebMediaPlayerImpl::DelegateState state) {
    wmpi_->SetDelegateState(state, false);
  }

  void SetUpMediaSuspend(bool enable) {
    delegate_.SetBackgroundMediaSuspendEnabledForTesting(enable);
  }

  bool IsVideoLockedWhenPausedWhenHidden() const {
    return wmpi_->video_locked_when_paused_when_hidden_;
  }

  void BackgroundPlayer() {
    delegate_.SetFrameHiddenForTesting(true);
    delegate_.SetFrameClosedForTesting(false);
    wmpi_->OnFrameHidden();
  }

  void ForegroundPlayer() {
    delegate_.SetFrameHiddenForTesting(false);
    delegate_.SetFrameClosedForTesting(false);
    wmpi_->OnFrameShown();
  }

  void Play() { wmpi_->Play(); }

  void Pause() { wmpi_->Pause(); }

  void ScheduleIdlePauseTimer() { wmpi_->ScheduleIdlePauseTimer(); }

  bool IsIdlePauseTimerRunning() {
    return wmpi_->background_pause_timer_.IsRunning();
  }

  void SetSuspendState(bool is_suspended) {
    wmpi_->SetSuspendState(is_suspended);
  }

  void SetLoadType(blink::WebMediaPlayer::LoadType load_type) {
    wmpi_->load_type_ = load_type;
  }

  bool IsVideoTrackDisabled() const { return wmpi_->video_track_disabled_; }

  bool IsDisableVideoTrackPending() const {
    return !wmpi_->update_background_status_cb_.IsCancelled();
  }

  gfx::Size GetNaturalSize() const {
    return wmpi_->pipeline_metadata_.natural_size;
  }

  void LoadAndWaitForMetadata(std::string data_file) {
    // URL doesn't matter, it's value is unknown to the underlying demuxer.
    const GURL kTestURL("file://example.com/sample.webm");

    // This block sets up a fetch context which ultimately provides us a pointer
    // to the WebAssociatedURLLoaderClient handed out by the DataSource after it
    // requests loading of a resource. We then use that client as if we are the
    // network stack and "serve" an in memory file to the DataSource.
    blink::WebAssociatedURLLoaderClient* client = nullptr;
    EXPECT_CALL(mock_resource_fetch_context_, CreateUrlLoader(_))
        .WillRepeatedly(
            Invoke([&client](const blink::WebAssociatedURLLoaderOptions&) {
              auto a = std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
              EXPECT_CALL(*a, LoadAsynchronously(_, _))
                  .WillRepeatedly(testing::SaveArg<1>(&client));
              return a;
            }));

    wmpi_->Load(blink::WebMediaPlayer::kLoadTypeURL,
                blink::WebMediaPlayerSource(blink::WebURL(kTestURL)),
                blink::WebMediaPlayer::kCORSModeUnspecified);

    base::RunLoop().RunUntilIdle();

    // Load a real media file into memory.
    scoped_refptr<DecoderBuffer> data = ReadTestDataFile(data_file);

    // "Serve" the file to the DataSource. Note: We respond with 200 okay, which
    // will prevent range requests or partial responses from being used.
    blink::WebURLResponse response(kTestURL);
    response.SetHTTPHeaderField(
        blink::WebString::FromUTF8("Content-Length"),
        blink::WebString::FromUTF8(base::NumberToString(data->data_size())));
    response.SetExpectedContentLength(data->data_size());
    response.SetHTTPStatusCode(200);
    client->DidReceiveResponse(response);

    // Copy over the file data and indicate that's everything.
    client->DidReceiveData(reinterpret_cast<const char*>(data->data()),
                           data->data_size());
    client->DidFinishLoading();

    // This runs until we reach the have current data state. Attempting to wait
    // for states < kReadyStateHaveCurrentData is unreliable due to asynchronous
    // execution of tasks on the base::test:ScopedTaskEnvironment.
    while (wmpi_->GetReadyState() <
           blink::WebMediaPlayer::kReadyStateHaveCurrentData) {
      base::RunLoop loop;
      EXPECT_CALL(client_, ReadyStateChanged())
          .WillRepeatedly(RunClosure(loop.QuitClosure()));
      loop.Run();

      // Clear the mock so it doesn't have a stale QuitClosure.
      testing::Mock::VerifyAndClearExpectations(&client_);
    }

    // Verify we made it through pipeline startup.
    EXPECT_TRUE(wmpi_->data_source_);
    EXPECT_TRUE(wmpi_->demuxer_);
    EXPECT_FALSE(wmpi_->seeking_);
  }

  void CycleThreads() {
    // Ensure any tasks waiting to be posted to the media thread are posted.
    base::RunLoop().RunUntilIdle();

    // Cycle media thread.
    {
      base::RunLoop loop;
      media_thread_.task_runner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), loop.QuitClosure());
      loop.Run();
    }

    // Cycle anything that was posted back from the media thread.
    base::RunLoop().RunUntilIdle();
  }

  // "Media" thread. This is necessary because WMPI destruction waits on a
  // WaitableEvent.
  base::Thread media_thread_;

  // Blink state.
  blink::WebLocalFrameClient web_frame_client_;
  blink::WebView* web_view_;
  blink::WebLocalFrame* web_local_frame_;

  scoped_refptr<viz::TestContextProvider> context_provider_;
  StrictMock<MockVideoFrameCompositor>* compositor_;

  scoped_refptr<NiceMock<MockAudioRendererSink>> audio_sink_;
  MockResourceFetchContext mock_resource_fetch_context_;
  std::unique_ptr<media::UrlIndex> url_index_;

  // Audio hardware configuration.
  AudioParameters audio_parameters_;

  // The client interface used by |wmpi_|.
  NiceMock<MockWebMediaPlayerClient> client_;

#if defined(OS_ANDROID)
  NiceMock<MockRendererMediaPlayerManager> mock_media_player_manager_;
#endif

  viz::FrameSinkId frame_sink_id_ = viz::FrameSinkId(1, 1);
  viz::LocalSurfaceId local_surface_id_ =
      viz::LocalSurfaceId(11, base::UnguessableToken::Deserialize(0x111111, 0));
  viz::SurfaceId surface_id_ =
      viz::SurfaceId(frame_sink_id_, local_surface_id_);

  NiceMock<MockWebMediaPlayerDelegate> delegate_;

  std::unique_ptr<StrictMock<MockSurfaceLayerBridge>> surface_layer_bridge_;
  StrictMock<MockSurfaceLayerBridge>* surface_layer_bridge_ptr_ = nullptr;

  // Only valid once set by InitializeWebMediaPlayerImpl(), this is for
  // verifying a subset of potential media logs.
  NiceMock<MockMediaLog>* media_log_ = nullptr;

  // Total memory in bytes allocated by the WebMediaPlayerImpl instance.
  int64_t reported_memory_ = 0;

  // default decoder factory for WMPI
  std::unique_ptr<DecoderFactory> decoder_factory_;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImplTest);
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
}

// Verify LoadAndWaitForMetadata() functions without issue.
TEST_F(WebMediaPlayerImplTest, LoadAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadAuto);
  LoadAndWaitForMetadata(kAudioOnlyTestFile);
  EXPECT_FALSE(IsSuspended());
  CycleThreads();

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure we're getting audio buffer and demuxer usage too.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_GT(reported_memory_ - data_source_size, 0);
}

// Verify that preload=metadata suspend works properly.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspend) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);
  LoadAndWaitForMetadata(kAudioOnlyTestFile);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_TRUE(IsSuspended());

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure there's no other memory usage.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_EQ(reported_memory_ - data_source_size, 0);
}

// Verify that lazy load for preload=metadata works properly.
TEST_F(WebMediaPlayerImplTest, LazyLoadPreloadMetadataSuspend) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPreloadMetadataLazyLoad);
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);

  // Don't set poster, but ensure we still reach suspended state.

  if (base::FeatureList::IsEnabled(kUseSurfaceLayerForVideo)) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
        .WillOnce(ReturnRef(surface_id_));
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetLocalSurfaceIdAllocationTime())
        .WillOnce(Return(base::TimeTicks()));
    EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, false, _));
    EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  }

  LoadAndWaitForMetadata(kVideoOnlyTestFile);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_TRUE(IsSuspended());
  EXPECT_TRUE(wmpi_->DidLazyLoad());

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure there's no other memory usage.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_EQ(reported_memory_ - data_source_size, 0);
}

// Verify that preload=metadata suspend video w/ poster uses zero video memory.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspendNoVideoMemoryUsage) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);
  wmpi_->SetPoster(blink::WebURL(GURL("file://example.com/sample.jpg")));

  if (base::FeatureList::IsEnabled(kUseSurfaceLayerForVideo)) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
        .WillOnce(ReturnRef(surface_id_));
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetLocalSurfaceIdAllocationTime())
        .WillOnce(Return(base::TimeTicks()));
    EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, false, _));
    EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  }

  LoadAndWaitForMetadata(kVideoOnlyTestFile);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_TRUE(IsSuspended());

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure there's no other memory usage.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_EQ(reported_memory_ - data_source_size, 0);
}

// Verify that preload=metadata suspend is aborted if we know the element will
// play as soon as we reach kReadyStateHaveFutureData.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspendCouldPlay) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(true));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);
  LoadAndWaitForMetadata(kAudioOnlyTestFile);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendBeforeLoadingBegins) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(delegate_.ExpireForTesting());
}

TEST_F(WebMediaPlayerImplTest,
       IdleSuspendIsDisabledIfLoadingProgressedRecently) {
  InitializeWebMediaPlayerImpl();
  base::SimpleTestTickClock clock;
  clock.Advance(base::TimeDelta::FromSeconds(1));
  SetTickClock(&clock);
  AddBufferedRanges();
  wmpi_->DidLoadingProgress();
  // Advance less than the loading timeout.
  clock.Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendIsEnabledIfLoadingHasStalled) {
  InitializeWebMediaPlayerImpl();
  SetNetworkState(blink::WebMediaPlayer::kNetworkStateLoading);
  base::SimpleTestTickClock clock;
  clock.Advance(base::TimeDelta::FromSeconds(1));
  SetTickClock(&clock);
  AddBufferedRanges();
  wmpi_->DidLoadingProgress();
  // Advance more than the loading timeout.
  clock.Advance(base::TimeDelta::FromSeconds(4));
  EXPECT_TRUE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, DidLoadingProgressTriggersResume) {
  // Same setup as IdleSuspendIsEnabledBeforeLoadingBegins.
  InitializeWebMediaPlayerImpl();
  SetNetworkState(blink::WebMediaPlayer::kNetworkStateLoading);
  EXPECT_TRUE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());

  // Like IdleSuspendIsDisabledIfLoadingProgressedRecently, the idle timeout
  // should be rejected if it hasn't been long enough.
  AddBufferedRanges();
  wmpi_->DidLoadingProgress();
  EXPECT_FALSE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Constructed) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_HaveMetadata) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_HaveFutureData) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

// Ensure memory reporting is not running after an error.
TEST_F(WebMediaPlayerImplTest, ComputePlayState_PlayingError) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  SetError();
  state = ComputePlayState();
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Playing) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_PlayingVideoOnly) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(false, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Underflow) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveCurrentData);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHidden) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);

  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHiddenAudioOnly) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);

  SetMetadata(true, false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHiddenSuspendNoResume) {
  SetUpMediaSuspend(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kResumeBackgroundVideo);

  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);

  SetPaused(true);
  state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHiddenSuspendWithResume) {
  SetUpMediaSuspend(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kResumeBackgroundVideo);

  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);

  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameClosed) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  delegate_.SetFrameClosedForTesting(true);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_PausedSeek) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetSeeking(true);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Ended) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  SetEnded(true);

  // Before Blink pauses us (or seeks for looping content), the media session
  // should be preserved.
  WebMediaPlayerImpl::PlayState state;
  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);

  SetPaused(true);
  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_StaysSuspended) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);

  // Should stay suspended even though not stale or backgrounded.
  WebMediaPlayerImpl::PlayState state = ComputePlayState_Suspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_ResumeForNeedFirstFrame) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);

  // Should stay suspended even though not stale or backgrounded.
  WebMediaPlayerImpl::PlayState state = ComputePlayState_Suspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);

  wmpi_->OnBecameVisible();
  state = ComputePlayState_Suspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Remote) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);

  // Remote media is always suspended.
  // TODO(sandersd): Decide whether this should count as idle or not.
  WebMediaPlayerImpl::PlayState state = ComputePlayState_Remote();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Fullscreen) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetFullscreen(true);
  SetPaused(true);
  delegate_.SetStaleForTesting(true);

  // Fullscreen media is never suspended (Android only behavior).
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Streaming) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(true);
  delegate_.SetStaleForTesting(true);

  // Streaming media should not suspend, even if paused, stale, and
  // backgrounded.
  WebMediaPlayerImpl::PlayState state;
  state = ComputePlayState_BackgroundedStreaming();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);

  // Streaming media should suspend when the tab is closed, regardless.
  delegate_.SetFrameClosedForTesting(true);
  state = ComputePlayState_BackgroundedStreaming();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, AutoplayMuted_StartsAndStops) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  client_.set_was_always_muted(true);

  EXPECT_CALL(delegate_, DidPlay(_, true, false, _));
  EXPECT_CALL(client_, WasAlwaysMuted())
      .WillOnce(Return(client_.was_always_muted_));
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);

  client_.set_was_always_muted(false);
  EXPECT_CALL(delegate_, DidPlay(_, true, true, _));
  EXPECT_CALL(client_, WasAlwaysMuted())
      .WillOnce(Return(client_.was_always_muted_));
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);
}

TEST_F(WebMediaPlayerImplTest, AutoplayMuted_SetVolume) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  client_.set_was_always_muted(true);

  EXPECT_CALL(delegate_, DidPlay(_, true, false, _));
  EXPECT_CALL(client_, WasAlwaysMuted())
      .WillOnce(Return(client_.was_always_muted_));
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);

  client_.set_was_always_muted(false);
  EXPECT_CALL(client_, WasAlwaysMuted())
      .WillOnce(Return(client_.was_always_muted_));
  EXPECT_CALL(delegate_, DidPlay(_, true, true, _));
  wmpi_->SetVolume(1.0);
}

TEST_F(WebMediaPlayerImplTest, NoStreams) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;

  EXPECT_CALL(client_, SetCcLayer(_)).Times(0);

  if (base::FeatureList::IsEnabled(media::kUseSurfaceLayerForVideo)) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer()).Times(0);
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId()).Times(0);
    EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, _, _)).Times(0);
  }

  // Nothing should happen.  In particular, no assertions should fail.
  OnMetadata(metadata);
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  metadata.natural_size = gfx::Size(320, 240);

  if (base::FeatureList::IsEnabled(kUseSurfaceLayerForVideo)) {
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
        .WillOnce(ReturnRef(surface_id_));
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetLocalSurfaceIdAllocationTime())
        .WillOnce(Return(base::TimeTicks()));
    EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, false, _));
    EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  } else {
    EXPECT_CALL(client_, SetCcLayer(NotNull()));
  }

  OnMetadata(metadata);
  ASSERT_EQ(blink::WebSize(320, 240), wmpi_->NaturalSize());

  EXPECT_CALL(client_, SizeChanged());
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(blink::WebSize(1920, 1080), wmpi_->NaturalSize());
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange_Rotated) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config =
      TestVideoConfig::NormalRotated(VIDEO_ROTATION_90);
  metadata.natural_size = gfx::Size(320, 240);

  if (base::FeatureList::IsEnabled(kUseSurfaceLayerForVideo)) {
    EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
        .WillOnce(ReturnRef(surface_id_));
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetLocalSurfaceIdAllocationTime())
        .WillOnce(Return(base::TimeTicks()));
    EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, false, _));
    EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  } else {
    EXPECT_CALL(client_, SetCcLayer(NotNull()));
  }

  OnMetadata(metadata);
  ASSERT_EQ(blink::WebSize(320, 240), wmpi_->NaturalSize());

  EXPECT_CALL(client_, SizeChanged());
  // For 90/270deg rotations, the natural size should be transposed.
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(blink::WebSize(1080, 1920), wmpi_->NaturalSize());
}

TEST_F(WebMediaPlayerImplTest, VideoLockedWhenPausedWhenHidden) {
  InitializeWebMediaPlayerImpl();

  // Setting metadata initializes |watch_time_reporter_| used in play().
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();

  if (base::FeatureList::IsEnabled(kUseSurfaceLayerForVideo)) {
    EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
        .WillOnce(ReturnRef(surface_id_));
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetLocalSurfaceIdAllocationTime())
        .WillOnce(Return(base::TimeTicks()));
    EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, false, _));
    EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  } else {
    EXPECT_CALL(client_, SetCcLayer(NotNull()));
  }

  OnMetadata(metadata);

  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // Backgrounding the player sets the lock.
  BackgroundPlayer();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // Play without a user gesture doesn't unlock the player.
  Play();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // With a user gesture it does unlock the player.
  {
    blink::WebScopedUserGesture user_gesture(nullptr);
    Play();
    EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());
  }

  // Pause without a user gesture doesn't lock the player.
  Pause();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // With a user gesture, pause does lock the player.
  {
    blink::WebScopedUserGesture user_gesture(nullptr);
    Pause();
    EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());
  }

  // Foregrounding the player unsets the lock.
  ForegroundPlayer();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());
}

TEST_F(WebMediaPlayerImplTest, BackgroundIdlePauseTimerDependsOnAudio) {
  InitializeWebMediaPlayerImpl();
  SetSuspendState(true);
  SetPaused(false);

  ASSERT_TRUE(IsSuspended());

  // Video-only players are not paused when suspended.
  SetMetadata(false, true);
  ScheduleIdlePauseTimer();
  EXPECT_FALSE(IsIdlePauseTimerRunning());

  SetMetadata(true, true);
  ScheduleIdlePauseTimer();
  EXPECT_TRUE(IsIdlePauseTimerRunning());
}

// Verifies that an infinite duration doesn't muck up GetCurrentTimeInternal.
TEST_F(WebMediaPlayerImplTest, InfiniteDuration) {
  InitializeWebMediaPlayerImpl();
  SetDuration(kInfiniteDuration);

  // Send metadata so we have a watch time reporter created.
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  metadata.has_audio = true;
  metadata.audio_decoder_config = TestAudioConfig::Normal();
  metadata.natural_size = gfx::Size(400, 400);

  if (base::FeatureList::IsEnabled(kUseSurfaceLayerForVideo)) {
    EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
    EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
        .WillOnce(ReturnRef(surface_id_));
    EXPECT_CALL(*surface_layer_bridge_ptr_, GetLocalSurfaceIdAllocationTime())
        .WillOnce(Return(base::TimeTicks()));
    EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, false, _));
    EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  } else {
    EXPECT_CALL(client_, SetCcLayer(NotNull()));
  }

  OnMetadata(metadata);

  EXPECT_EQ(std::numeric_limits<double>::infinity(), wmpi_->Duration());
  EXPECT_EQ(0, wmpi_->CurrentTime());
  EXPECT_EQ(base::TimeDelta(), GetCurrentTimeInternal());

  SetEnded(true);
  EXPECT_EQ(0, wmpi_->CurrentTime());
  EXPECT_EQ(base::TimeDelta(), GetCurrentTimeInternal());

  // Pause should not pick up infinity for the current time.
  wmpi_->Pause();
  EXPECT_EQ(0, wmpi_->CurrentTime());
  EXPECT_EQ(base::TimeDelta(), GetCurrentTimeInternal());
}

TEST_F(WebMediaPlayerImplTest, SetContentsLayerGetsWebLayerFromBridge) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(kUseSurfaceLayerForVideo.name, "");

  InitializeWebMediaPlayerImpl();

  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config =
      TestVideoConfig::NormalRotated(VIDEO_ROTATION_90);
  metadata.natural_size = gfx::Size(320, 240);

  EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
  EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
      .WillOnce(ReturnRef(surface_id_));
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetLocalSurfaceIdAllocationTime())
      .WillOnce(Return(base::TimeTicks()));
  EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, false, _));

  // We only call the callback to create the bridge in OnMetadata, so we need
  // to call it.
  OnMetadata(metadata);

  scoped_refptr<cc::Layer> layer = cc::Layer::Create();

  EXPECT_CALL(*surface_layer_bridge_ptr_, GetCcLayer())
      .WillRepeatedly(Return(layer.get()));
  EXPECT_CALL(client_, SetCcLayer(Eq(layer.get())));
  EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  wmpi_->RegisterContentsLayer(layer.get());
}

TEST_F(WebMediaPlayerImplTest, PlaybackRateChangeMediaLogs) {
  InitializeWebMediaPlayerImpl();

  {
    InSequence s;

    // Expect precisely one rate change log from this test case.
    EXPECT_MEDIA_LOG_ON(*media_log_, PlaybackRateChanged("0", "0.8"));
    EXPECT_MEDIA_LOG_ON(*media_log_, WmpiDestroyed());

    wmpi_->SetRate(0.0);  // No change from initial rate, so no log.
    wmpi_->SetRate(0.8);  // This should log change from 0 -> 0.8
    wmpi_->SetRate(0.8);  // No change from previous rate, so no log.
  }
}

// Tests delegate methods are called when Picture-in-Picture is triggered.
TEST_F(WebMediaPlayerImplTest, PictureInPictureTriggerCallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(kUseSurfaceLayerForVideo.name, "");

  InitializeWebMediaPlayerImpl();

  EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
      .WillRepeatedly(ReturnRef(surface_id_));
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetLocalSurfaceIdAllocationTime())
      .WillRepeatedly(Return(base::TimeTicks()));
  EXPECT_CALL(*compositor_, EnableSubmission(_, _, _, _, false, _));
  EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));

  PipelineMetadata metadata;
  metadata.has_video = true;
  OnMetadata(metadata);

  EXPECT_CALL(client_, DisplayType())
      .WillRepeatedly(
          Return(blink::WebMediaPlayer::DisplayType::kPictureInPicture));
  EXPECT_CALL(delegate_,
              DidPictureInPictureSurfaceChange(
                  delegate_.player_id(), surface_id_, GetNaturalSize(), true))
      .Times(2);

  wmpi_->OnSurfaceIdUpdated(surface_id_);

  EXPECT_CALL(delegate_,
              DidPictureInPictureModeStart(delegate_.player_id(), surface_id_,
                                           GetNaturalSize(), _, true));

  wmpi_->EnterPictureInPicture(base::DoNothing());
  wmpi_->OnSurfaceIdUpdated(surface_id_);

  // Updating SurfaceId should NOT exit Picture-in-Picture.
  EXPECT_CALL(delegate_, DidPictureInPictureModeEnd(delegate_.player_id(), _))
      .Times(0);
}

class WebMediaPlayerImplBackgroundBehaviorTest
    : public WebMediaPlayerImplTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, int, int, bool, bool, bool, bool>> {
 public:
  // Indices of the tuple parameters.
  static const int kIsMediaSuspendEnabled = 0;
  static const int kIsBackgroundOptimizationEnabled = 1;
  static const int kDurationSec = 2;
  static const int kAverageKeyframeDistanceSec = 3;
  static const int kIsResumeBackgroundVideoEnabled = 4;
  static const int kIsMediaSource = 5;
  static const int kIsBackgroundPauseEnabled = 6;
  static const int kIsPictureInPictureEnabled = 7;

  void SetUp() override {
    WebMediaPlayerImplTest::SetUp();
    SetUpMediaSuspend(IsMediaSuspendOn());

    std::string enabled_features;
    std::string disabled_features;
    if (IsBackgroundOptimizationOn()) {
      enabled_features += kBackgroundSrcVideoTrackOptimization.name;
    } else {
      disabled_features += kBackgroundSrcVideoTrackOptimization.name;
    }

    if (IsBackgroundPauseOn()) {
      if (!enabled_features.empty())
        enabled_features += ",";
      enabled_features += kBackgroundVideoPauseOptimization.name;
    } else {
      if (!disabled_features.empty())
        disabled_features += ",";
      disabled_features += kBackgroundVideoPauseOptimization.name;
    }

    if (IsResumeBackgroundVideoEnabled()) {
      if (!enabled_features.empty())
        enabled_features += ",";
      enabled_features += kResumeBackgroundVideo.name;
    } else {
      if (!disabled_features.empty())
        disabled_features += ",";
      disabled_features += kResumeBackgroundVideo.name;
    }

    feature_list_.InitFromCommandLine(enabled_features, disabled_features);

    InitializeWebMediaPlayerImpl();
    bool is_media_source = std::get<kIsMediaSource>(GetParam());
    SetLoadType(is_media_source ? blink::WebMediaPlayer::kLoadTypeMediaSource
                                : blink::WebMediaPlayer::kLoadTypeURL);
    SetVideoKeyframeDistanceAverage(
        base::TimeDelta::FromSeconds(GetAverageKeyframeDistanceSec()));
    SetDuration(base::TimeDelta::FromSeconds(GetDurationSec()));

    if (IsPictureInPictureOn()) {
      EXPECT_CALL(client_, DisplayType())
          .WillRepeatedly(
              Return(blink::WebMediaPlayer::DisplayType::kPictureInPicture));

      wmpi_->OnSurfaceIdUpdated(surface_id_);
    }

    BackgroundPlayer();
  }

  void SetVideoKeyframeDistanceAverage(base::TimeDelta value) {
    PipelineStatistics statistics;
    statistics.video_keyframe_distance_average = value;
    wmpi_->SetPipelineStatisticsForTest(statistics);
  }

  bool IsMediaSuspendOn() {
    return std::get<kIsMediaSuspendEnabled>(GetParam());
  }

  bool IsBackgroundOptimizationOn() {
    return std::get<kIsBackgroundOptimizationEnabled>(GetParam());
  }

  bool IsResumeBackgroundVideoEnabled() {
    return std::get<kIsResumeBackgroundVideoEnabled>(GetParam());
  }

  bool IsBackgroundPauseOn() {
    return std::get<kIsBackgroundPauseEnabled>(GetParam());
  }

  bool IsPictureInPictureOn() {
    return std::get<kIsPictureInPictureEnabled>(GetParam());
  }

  int GetDurationSec() const { return std::get<kDurationSec>(GetParam()); }

  int GetAverageKeyframeDistanceSec() const {
    return std::get<kAverageKeyframeDistanceSec>(GetParam());
  }

  int GetMaxKeyframeDistanceSec() const {
    return WebMediaPlayerImpl::kMaxKeyframeDistanceToDisableBackgroundVideoMs /
           base::Time::kMillisecondsPerSecond;
  }

  bool IsAndroid() {
#if defined(OS_ANDROID)
    return true;
#else
    return false;
#endif
  }

  bool ShouldDisableVideoWhenHidden() const {
    return wmpi_->ShouldDisableVideoWhenHidden();
  }

  bool ShouldPauseVideoWhenHidden() const {
    return wmpi_->ShouldPauseVideoWhenHidden();
  }

  bool IsBackgroundOptimizationCandidate() const {
    return wmpi_->IsBackgroundOptimizationCandidate();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, AudioOnly) {
  // Never optimize or pause an audio-only player.
  SetMetadata(true, false);
  EXPECT_FALSE(IsBackgroundOptimizationCandidate());
  EXPECT_FALSE(ShouldPauseVideoWhenHidden());
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());
}

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, VideoOnly) {
  // Video only.
  SetMetadata(false, true);

  // Never disable video track for a video only stream.
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());

  // There's no optimization criteria for video only in Picture-in-Picture.
  bool matches_requirements = !IsPictureInPictureOn();
  EXPECT_EQ(matches_requirements, IsBackgroundOptimizationCandidate());

  // Video is always paused when suspension is on and only if matches the
  // optimization criteria if the optimization is on.
  bool should_pause =
      IsMediaSuspendOn() || (IsBackgroundPauseOn() && matches_requirements);
  EXPECT_EQ(should_pause, ShouldPauseVideoWhenHidden());
}

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, AudioVideo) {
  SetMetadata(true, true);

  // Optimization requirements are the same for all platforms.
  bool matches_requirements =
      !IsPictureInPictureOn() &&
      ((GetDurationSec() < GetMaxKeyframeDistanceSec()) ||
       (GetAverageKeyframeDistanceSec() < GetMaxKeyframeDistanceSec()));

  EXPECT_EQ(matches_requirements, IsBackgroundOptimizationCandidate());
  EXPECT_EQ(IsBackgroundOptimizationOn() && matches_requirements,
            ShouldDisableVideoWhenHidden());

  // Only pause audible videos if both media suspend and resume background
  // videos is on. Both are on by default on Android and off on desktop.
  EXPECT_EQ(IsMediaSuspendOn() && IsResumeBackgroundVideoEnabled(),
            ShouldPauseVideoWhenHidden());

  if (!IsBackgroundOptimizationOn() || !matches_requirements ||
      !ShouldDisableVideoWhenHidden() || IsMediaSuspendOn()) {
    return;
  }

  // These tests start in background mode prior to having metadata, so put the
  // test back into a normal state.
  EXPECT_TRUE(IsDisableVideoTrackPending());

  EXPECT_CALL(client_, WasAlwaysMuted())
      .WillRepeatedly(Return(client_.was_always_muted_));
  ForegroundPlayer();
  EXPECT_FALSE(IsVideoTrackDisabled());
  EXPECT_FALSE(IsDisableVideoTrackPending());

  // Should start background disable timer, but not disable immediately.
  BackgroundPlayer();
  EXPECT_FALSE(IsVideoTrackDisabled());
  EXPECT_TRUE(IsDisableVideoTrackPending());
}

INSTANTIATE_TEST_CASE_P(
    BackgroundBehaviorTestInstances,
    WebMediaPlayerImplBackgroundBehaviorTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Values(
            WebMediaPlayerImpl::kMaxKeyframeDistanceToDisableBackgroundVideoMs /
                    base::Time::kMillisecondsPerSecond +
                1,
            300),
        ::testing::Values(
            WebMediaPlayerImpl::kMaxKeyframeDistanceToDisableBackgroundVideoMs /
                    base::Time::kMillisecondsPerSecond +
                1,
            100),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool()));

}  // namespace media
