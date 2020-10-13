// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "components/viz/test/test_context_provider.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/memory_dump_provider_proxy.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/blink/mock_resource_fetch_context.h"
#include "media/blink/mock_webassociatedurlloader.h"
#include "media/blink/resource_multibuffer_data_provider.h"
#include "media/blink/video_decode_stats_reporter.h"
#include "media/blink/webcontentdecryptionmodule_impl.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "media/mojo/services/video_decode_stats_recorder.h"
#include "media/mojo/services/watch_time_recorder.h"
#include "media/renderers/default_decoder_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/media/webmediaplayer_delegate.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "url/gurl.h"

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::WithoutArgs;

namespace media {

constexpr char kAudioOnlyTestFile[] = "sfx-opus-441.webm";
constexpr char kVideoOnlyTestFile[] = "bear-320x240-video-only.webm";
constexpr char kVideoAudioTestFile[] = "bear-320x240-16x9-aspect.webm";
constexpr char kEncryptedVideoOnlyTestFile[] = "bear-320x240-av_enc-v.webm";

constexpr base::TimeDelta kAudioOnlyTestFileDuration =
    base::TimeDelta::FromMilliseconds(296);

MATCHER(WmpiDestroyed, "") {
  return CONTAINS_STRING(arg, "{\"event\":\"kWebMediaPlayerDestroyed\"}");
}

MATCHER_P2(PlaybackRateChanged, old_rate_string, new_rate_string, "") {
  return CONTAINS_STRING(arg, "Effective playback rate changed from " +
                                  std::string(old_rate_string) + " to " +
                                  std::string(new_rate_string));
}

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
  MOCK_METHOD1(MediaRemotingStarted, void(const blink::WebString&));
  MOCK_METHOD1(MediaRemotingStopped, void(int));
  MOCK_METHOD0(PictureInPictureStopped, void());
  MOCK_METHOD0(OnPictureInPictureStateChange, void());
  MOCK_CONST_METHOD0(CouldPlayIfEnoughData, bool());
  MOCK_METHOD0(RequestPlay, void());
  MOCK_METHOD0(RequestPause, void());
  MOCK_METHOD1(RequestMuted, void(bool));
  MOCK_METHOD0(RequestEnterPictureInPicture, void());
  MOCK_METHOD0(RequestExitPictureInPicture, void());
  MOCK_METHOD0(GetFeatures, Features(void));
  MOCK_METHOD0(OnRequestVideoFrameCallback, void());
  MOCK_METHOD0(GetTextTrackMetadata, std::vector<blink::TextTrackMetadata>());

  bool was_always_muted_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebMediaPlayerClient);
};

class MockWebMediaPlayerEncryptedMediaClient
    : public blink::WebMediaPlayerEncryptedMediaClient {
 public:
  MockWebMediaPlayerEncryptedMediaClient() = default;

  MOCK_METHOD3(Encrypted,
               void(EmeInitDataType, const unsigned char*, unsigned));
  MOCK_METHOD0(DidBlockPlaybackWaitingForKey, void());
  MOCK_METHOD0(DidResumePlaybackBlockedForKey, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebMediaPlayerEncryptedMediaClient);
};

class MockWebMediaPlayerDelegate : public blink::WebMediaPlayerDelegate {
 public:
  MockWebMediaPlayerDelegate() = default;
  ~MockWebMediaPlayerDelegate() override = default;

  // blink::WebMediaPlayerDelegate implementation.
  int AddObserver(Observer* observer) override {
    DCHECK_EQ(nullptr, observer_);
    observer_ = observer;
    return player_id_;
  }

  void RemoveObserver(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    observer_ = nullptr;
  }

  MOCK_METHOD4(DidMediaMetadataChange, void(int, bool, bool, MediaContentType));

  void DidPlay(int player_id) override { DCHECK_EQ(player_id_, player_id); }

  void DidPause(int player_id, bool reached_end_of_stream) override {
    DCHECK_EQ(player_id_, player_id);
  }

  void PlayerGone(int player_id) override { DCHECK_EQ(player_id_, player_id); }

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

  void DidBufferUnderflow(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
  }

  bool IsFrameHidden() override { return is_hidden_; }

  bool IsFrameClosed() override { return is_closed_; }

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

  int player_id() { return player_id_; }

  MOCK_METHOD2(DidPlayerMediaPositionStateChange,
               void(int, const media_session::MediaPosition&));

  MOCK_METHOD2(DidPictureInPictureAvailabilityChange, void(int, bool));

  MOCK_METHOD2(DidAudioOutputSinkChange, void(int, const std::string&));

  MOCK_METHOD1(DidDisableAudioOutputSinkChanges, void(int));

 private:
  Observer* observer_ = nullptr;
  int player_id_ = 1234;
  bool is_idle_ = false;
  bool is_stale_ = false;
  bool is_hidden_ = false;
  bool is_closed_ = false;
};

class MockSurfaceLayerBridge : public blink::WebSurfaceLayerBridge {
 public:
  MOCK_CONST_METHOD0(GetCcLayer, cc::Layer*());
  MOCK_CONST_METHOD0(GetFrameSinkId, const viz::FrameSinkId&());
  MOCK_CONST_METHOD0(GetSurfaceId, const viz::SurfaceId&());
  MOCK_METHOD0(ClearSurfaceId, void());
  MOCK_METHOD1(SetContentsOpaque, void(bool));
  MOCK_METHOD0(CreateSurfaceLayer, void());
  MOCK_METHOD0(ClearObserver, void());
};

class MockVideoFrameCompositor : public VideoFrameCompositor {
 public:
  MockVideoFrameCompositor(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : VideoFrameCompositor(task_runner, nullptr) {}
  ~MockVideoFrameCompositor() override = default;

  // MOCK_METHOD doesn't like OnceCallback.
  MOCK_METHOD1(SetOnFramePresentedCallback, void(OnNewFramePresentedCB));
  MOCK_METHOD1(SetIsPageVisible, void(bool));
  MOCK_METHOD0(
      GetLastPresentedFrameMetadata,
      std::unique_ptr<blink::WebMediaPlayer::VideoFramePresentationMetadata>());
  MOCK_METHOD0(GetCurrentFrameOnAnyThread, scoped_refptr<VideoFrame>());
  MOCK_METHOD1(UpdateCurrentFrameIfStale,
               void(VideoFrameCompositor::UpdateType));
  MOCK_METHOD3(EnableSubmission,
               void(const viz::SurfaceId&, media::VideoRotation, bool));
};

class WebMediaPlayerImplTest
    : public testing::Test,
      private blink::WebTestingSupport::WebScopedMockScrollbars {
 public:
  WebMediaPlayerImplTest()
      : media_thread_("MediaThreadForTest"),
        web_view_(blink::WebView::Create(/*client=*/nullptr,
                                         /*is_hidden=*/false,
                                         /*is_inside_portal=*/false,
                                         /*compositing_enabled=*/false,
                                         nullptr,
                                         mojo::NullAssociatedReceiver())),
        web_local_frame_(blink::WebLocalFrame::CreateMainFrame(
            web_view_,
            &web_frame_client_,
            nullptr,
            base::UnguessableToken::Create(),
            nullptr)),
        context_provider_(viz::TestContextProvider::Create()),
        audio_parameters_(TestAudioParameters::Normal()),
        memory_dump_manager_(
            base::trace_event::MemoryDumpManager::CreateInstanceForTesting()) {
    media_thread_.StartAndWaitForTesting();
  }

  void InitializeSurfaceLayerBridge() {
    surface_layer_bridge_ =
        std::make_unique<NiceMock<MockSurfaceLayerBridge>>();
    surface_layer_bridge_ptr_ = surface_layer_bridge_.get();

    EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
    ON_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
        .WillByDefault(ReturnRef(surface_id_));
  }

  void InitializeWebMediaPlayerImpl() {
    InitializeWebMediaPlayerImplInternal(nullptr);
  }

  ~WebMediaPlayerImplTest() override {
    if (!wmpi_)
      return;
    EXPECT_CALL(client_, SetCcLayer(nullptr));
    EXPECT_CALL(client_, MediaRemotingStopped(_));

    // Destruct WebMediaPlayerImpl and pump the message loop to ensure that
    // objects passed to the message loop for destruction are released.
    //
    // NOTE: This should be done before any other member variables are
    // destructed since WMPI may reference them during destruction.
    wmpi_.reset();

    CycleThreads();

    web_view_->Close();
  }

 protected:
  void InitializeWebMediaPlayerImplInternal(
      std::unique_ptr<media::Demuxer> demuxer_override) {
    auto media_log = std::make_unique<NiceMock<MockMediaLog>>();
    InitializeSurfaceLayerBridge();

    // Retain a raw pointer to |media_log| for use by tests. Meanwhile, give its
    // ownership to |wmpi_|. Reject attempts to reinitialize to prevent orphaned
    // expectations on previous |media_log_|.
    ASSERT_FALSE(media_log_) << "Reinitialization of media_log_ is disallowed";
    media_log_ = media_log.get();

    auto factory_selector = std::make_unique<RendererFactorySelector>();
    renderer_factory_selector_ = factory_selector.get();
    decoder_factory_.reset(new media::DefaultDecoderFactory(nullptr));
#if defined(OS_ANDROID)
    factory_selector->AddBaseFactory(
        RendererFactoryType::kDefault,
        std::make_unique<DefaultRendererFactory>(
            media_log.get(), decoder_factory_.get(),
            DefaultRendererFactory::GetGpuFactoriesCB()));
    factory_selector->StartRequestRemotePlayStateCB(base::DoNothing());
#else
    factory_selector->AddBaseFactory(
        RendererFactoryType::kDefault,
        std::make_unique<DefaultRendererFactory>(
            media_log.get(), decoder_factory_.get(),
            DefaultRendererFactory::GetGpuFactoriesCB(), nullptr));
#endif

    mojo::Remote<mojom::MediaMetricsProvider> provider;
    MediaMetricsProvider::Create(
        MediaMetricsProvider::BrowsingMode::kNormal,
        MediaMetricsProvider::FrameStatus::kNotTopFrame,
        base::BindRepeating([]() { return ukm::kInvalidSourceId; }),
        base::BindRepeating([]() { return learning::FeatureValue(0); }),
        VideoDecodePerfHistory::SaveCallback(),
        MediaMetricsProvider::GetLearningSessionCallback(),
        base::BindRepeating(
            &WebMediaPlayerImplTest::GetRecordAggregateWatchTimeCallback,
            base::Unretained(this)),
        provider.BindNewPipeAndPassReceiver());

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
        provider.Unbind(),
        base::BindOnce(&WebMediaPlayerImplTest::CreateMockSurfaceLayerBridge,
                       base::Unretained(this)),
        viz::TestContextProvider::Create(),
        blink::WebMediaPlayer::SurfaceLayerMode::kAlways,
        is_background_suspend_enabled_, is_background_video_playback_enabled_,
        true, std::move(demuxer_override), nullptr);

    auto compositor = std::make_unique<NiceMock<MockVideoFrameCompositor>>(
        params->video_frame_compositor_task_runner());
    compositor_ = compositor.get();

    wmpi_ = std::make_unique<WebMediaPlayerImpl>(
        web_local_frame_, &client_, &encrypted_client_, &delegate_,
        std::move(factory_selector), url_index_.get(), std::move(compositor),
        std::move(params));
  }

  std::unique_ptr<blink::WebSurfaceLayerBridge> CreateMockSurfaceLayerBridge(
      blink::WebSurfaceLayerBridgeObserver*,
      cc::UpdateSubmissionStateCB) {
    return std::move(surface_layer_bridge_);
  }

  blink::WebLocalFrame* GetWebLocalFrame() { return web_local_frame_; }

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
    wmpi_->OnDurationChange();
  }

  MediaMetricsProvider::RecordAggregateWatchTimeCallback
  GetRecordAggregateWatchTimeCallback() {
    return base::NullCallback();
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

  void OnMetadata(const PipelineMetadata& metadata) {
    wmpi_->OnMetadata(metadata);
  }

  void OnWaiting(WaitingReason reason) { wmpi_->OnWaiting(reason); }

  void OnVideoNaturalSizeChange(const gfx::Size& size) {
    wmpi_->OnVideoNaturalSizeChange(size);
  }

  void OnVideoConfigChange(const VideoDecoderConfig& config) {
    wmpi_->OnVideoConfigChange(config);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_FrameHidden() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, true);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Suspended() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, true, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Flinging() {
    return wmpi_->UpdatePlayState_ComputePlayState(true, true, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_BackgroundedStreaming() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, true);
  }

  bool IsSuspended() { return wmpi_->pipeline_controller_->IsSuspended(); }

  int64_t GetDataSourceMemoryUsage() const {
    return wmpi_->data_source_->GetMemoryUsage();
  }

  void AddBufferedRanges() {
    wmpi_->buffered_data_source_host_->AddBufferedByteRange(0, 1);
  }

  void SetDelegateState(WebMediaPlayerImpl::DelegateState state) {
    wmpi_->SetDelegateState(state, false);
  }

  void SetUpMediaSuspend(bool enable) {
    is_background_suspend_enabled_ = enable;
  }

  void SetUpBackgroundVideoPlayback(bool enable) {
    is_background_video_playback_enabled_ = enable;
  }

  bool IsVideoLockedWhenPausedWhenHidden() const {
    return wmpi_->video_locked_when_paused_when_hidden_;
  }

  void BackgroundPlayer() {
    base::RunLoop loop;
    EXPECT_CALL(*compositor_, SetIsPageVisible(false))
        .WillOnce(RunClosure(loop.QuitClosure()));

    delegate_.SetFrameHiddenForTesting(true);
    delegate_.SetFrameClosedForTesting(false);

    wmpi_->OnFrameHidden();

    loop.Run();

    // Clear the mock so it doesn't have a stale QuitClosure.
    testing::Mock::VerifyAndClearExpectations(compositor_);
  }

  void ForegroundPlayer() {
    base::RunLoop loop;
    EXPECT_CALL(*compositor_, SetIsPageVisible(true))
        .WillOnce(RunClosure(loop.QuitClosure()));

    delegate_.SetFrameHiddenForTesting(false);
    delegate_.SetFrameClosedForTesting(false);

    wmpi_->OnFrameShown();

    loop.Run();

    // Clear the mock so it doesn't have a stale QuitClosure.
    testing::Mock::VerifyAndClearExpectations(compositor_);
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

  VideoDecodeStatsReporter* GetVideoStatsReporter() const {
    return wmpi_->video_decode_stats_reporter_.get();
  }

  VideoCodecProfile GetVideoStatsReporterCodecProfile() const {
    DCHECK(GetVideoStatsReporter());
    return GetVideoStatsReporter()->codec_profile_;
  }

  bool ShouldCancelUponDefer() const {
    return wmpi_->mb_data_source_->cancel_on_defer_for_testing();
  }

  bool IsDataSourceMarkedAsPlaying() const {
    return wmpi_->mb_data_source_->media_has_played();
  }

  scoped_refptr<VideoFrame> CreateFrame() {
    gfx::Size size(8, 8);
    return VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                   size, base::TimeDelta());
  }

  void RequestVideoFrameCallback() { wmpi_->RequestVideoFrameCallback(); }
  void GetVideoFramePresentationMetadata() {
    wmpi_->GetVideoFramePresentationMetadata();
  }
  void UpdateFrameIfStale() { wmpi_->UpdateFrameIfStale(); }

  void OnNewFramePresentedCallback() { wmpi_->OnNewFramePresentedCallback(); }

  scoped_refptr<VideoFrame> GetCurrentFrameFromCompositor() {
    return wmpi_->GetCurrentFrameFromCompositor();
  }

  enum class LoadType { kFullyBuffered, kStreaming };
  void Load(std::string data_file,
            LoadType load_type = LoadType::kFullyBuffered) {
    const bool is_streaming = load_type == LoadType::kStreaming;

    // The URL is used by MultibufferDataSource to determine if it should assume
    // the resource is fully buffered locally. We can use a fake one here since
    // we're injecting the response artificially. It's value is unknown to the
    // underlying demuxer.
    const GURL kTestURL(std::string(is_streaming ? "http" : "file") +
                        "://example.com/sample.webm");

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
                blink::WebMediaPlayer::kCorsModeUnspecified);

    base::RunLoop().RunUntilIdle();

    // Load a real media file into memory.
    scoped_refptr<DecoderBuffer> data = ReadTestDataFile(data_file);

    // "Serve" the file to the DataSource. Note: We respond with 200 okay, which
    // will prevent range requests or partial responses from being used. For
    // streaming responses, we'll pretend we don't know the content length.
    blink::WebURLResponse response(kTestURL);
    response.SetHttpHeaderField(
        blink::WebString::FromUTF8("Content-Length"),
        blink::WebString::FromUTF8(
            is_streaming ? "-1" : base::NumberToString(data->data_size())));
    response.SetExpectedContentLength(is_streaming ? -1 : data->data_size());
    response.SetHttpStatusCode(200);
    client->DidReceiveResponse(response);

    // Copy over the file data.
    client->DidReceiveData(reinterpret_cast<const char*>(data->data()),
                           data->data_size());

    // If we're pretending to be a streaming resource, don't complete the load;
    // otherwise the DataSource will not be marked as streaming.
    if (!is_streaming)
      client->DidFinishLoading();
  }

  // This runs until we reach the |ready_state_|. Attempting to wait for ready
  // states < kReadyStateHaveCurrentData in non-startup-suspend test cases is
  // unreliable due to asynchronous execution of tasks on the
  // base::test:TaskEnvironment.
  void LoadAndWaitForReadyState(std::string data_file,
                                blink::WebMediaPlayer::ReadyState ready_state) {
    Load(data_file);
    while (wmpi_->GetReadyState() < ready_state) {
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

    if (ready_state > blink::WebMediaPlayer::kReadyStateHaveCurrentData)
      EXPECT_FALSE(wmpi_->seeking_);
  }

  void LoadAndWaitForCurrentData(std::string data_file) {
    LoadAndWaitForReadyState(data_file,
                             blink::WebMediaPlayer::kReadyStateHaveCurrentData);
  }

  void CycleThreads() {
    // Ensure any tasks waiting to be posted to the media thread are posted.
    base::RunLoop().RunUntilIdle();

    // Flush all media tasks.
    media_thread_.FlushForTesting();

    // Cycle anything that was posted back from the media thread.
    base::RunLoop().RunUntilIdle();
  }

  void OnProgress() { wmpi_->OnProgress(); }

  void OnCdmCreated(base::RepeatingClosure quit_closure,
                    blink::WebContentDecryptionModule* cdm,
                    const std::string& error_message) {
    LOG_IF(ERROR, !error_message.empty()) << error_message;
    EXPECT_TRUE(cdm);
    web_cdm_.reset(cdm);
    quit_closure.Run();
  }

  void CreateCdm() {
    // Must use a supported key system on a secure context.
    auto key_system = base::ASCIIToUTF16("org.w3.clearkey");
    auto test_origin = blink::WebSecurityOrigin::CreateFromString(
        blink::WebString::FromUTF8("https://test.origin"));

    base::RunLoop run_loop;
    WebContentDecryptionModuleImpl::Create(
        &mock_cdm_factory_, key_system, test_origin, CdmConfig(),
        base::BindOnce(&WebMediaPlayerImplTest::OnCdmCreated,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(web_cdm_);
  }

  void SetCdm() {
    DCHECK(web_cdm_);
    auto* mock_cdm = mock_cdm_factory_.GetCreatedCdm();
    EXPECT_CALL(*mock_cdm, GetCdmContext())
        .WillRepeatedly(Return(&mock_cdm_context_));

    wmpi_->SetCdmInternal(web_cdm_.get());
  }

  MemoryDumpProviderProxy* GetMainThreadMemDumper() {
    return wmpi_->main_thread_mem_dumper_.get();
  }
  MemoryDumpProviderProxy* GetMediaThreadMemDumper() {
    return wmpi_->media_thread_mem_dumper_.get();
  }

  int32_t GetMediaLogId() { return media_log_->id(); }

  // "Media" thread. This is necessary because WMPI destruction waits on a
  // WaitableEvent.
  base::Thread media_thread_;

  // Blink state.
  blink::WebLocalFrameClient web_frame_client_;
  blink::WebView* web_view_;
  blink::WebLocalFrame* web_local_frame_;

  scoped_refptr<viz::TestContextProvider> context_provider_;
  NiceMock<MockVideoFrameCompositor>* compositor_;

  scoped_refptr<NiceMock<MockAudioRendererSink>> audio_sink_;
  MockResourceFetchContext mock_resource_fetch_context_;
  std::unique_ptr<media::UrlIndex> url_index_;

  // Audio hardware configuration.
  AudioParameters audio_parameters_;

  bool is_background_suspend_enabled_ = false;
  bool is_background_video_playback_enabled_ = true;

  // The client interface used by |wmpi_|.
  NiceMock<MockWebMediaPlayerClient> client_;
  MockWebMediaPlayerEncryptedMediaClient encrypted_client_;

  // Used to create the MockCdm to test encrypted playback.
  MockCdmFactory mock_cdm_factory_;
  std::unique_ptr<blink::WebContentDecryptionModule> web_cdm_;
  MockCdmContext mock_cdm_context_;

  viz::FrameSinkId frame_sink_id_ = viz::FrameSinkId(1, 1);
  viz::LocalSurfaceId local_surface_id_ =
      viz::LocalSurfaceId(11, base::UnguessableToken::Deserialize(0x111111, 0));
  viz::SurfaceId surface_id_ =
      viz::SurfaceId(frame_sink_id_, local_surface_id_);

  NiceMock<MockWebMediaPlayerDelegate> delegate_;

  // Use NiceMock since most tests do not care about this.
  std::unique_ptr<NiceMock<MockSurfaceLayerBridge>> surface_layer_bridge_;
  NiceMock<MockSurfaceLayerBridge>* surface_layer_bridge_ptr_ = nullptr;

  // Only valid once set by InitializeWebMediaPlayerImpl(), this is for
  // verifying a subset of potential media logs.
  NiceMock<MockMediaLog>* media_log_ = nullptr;

  // Total memory in bytes allocated by the WebMediaPlayerImpl instance.
  int64_t reported_memory_ = 0;

  // Raw pointer of the RendererFactorySelector owned by |wmpi_|.
  RendererFactorySelector* renderer_factory_selector_ = nullptr;

  // default decoder factory for WMPI
  std::unique_ptr<DecoderFactory> decoder_factory_;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;

  std::unique_ptr<base::trace_event::MemoryDumpManager> memory_dump_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImplTest);
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
}

// Verify LoadAndWaitForCurrentData() functions without issue.
TEST_F(WebMediaPlayerImplTest, LoadAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadAuto);
  LoadAndWaitForCurrentData(kAudioOnlyTestFile);
  EXPECT_FALSE(IsSuspended());
  CycleThreads();

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure we're getting audio buffer and demuxer usage too.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_GT(reported_memory_ - data_source_size, 0);
}

// Verify LoadAndWaitForCurrentData() functions without issue.
TEST_F(WebMediaPlayerImplTest, LoadAndDestroyDataUrl) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadAuto);

  const GURL kMp3DataUrl(
      "data://audio/mp3;base64,SUQzAwAAAAAAFlRFTkMAAAAMAAAAQW1hZGV1cyBQcm//"
      "+5DEAAAAAAAAAAAAAAAAAAAAAABYaW5nAAAADwAAAAwAAAftABwcHBwcHBwcMTExMTExMTFG"
      "RkZGRkZGRlpaWlpaWlpaWm9vb29vb29vhISEhISEhISYmJiYmJiYmJitra2tra2trcLCwsLC"
      "wsLC3t7e3t7e3t7e7+/v7+/v7+///////////"
      "wAAADxMQU1FMy45OHIErwAAAAAudQAANCAkCK9BAAHMAAAHbZV/"
      "jdYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD/"
      "+0DEAAABcAOH1AAAIhcbaLc80EgAAAAAAPAAAP/"
      "0QfAAQdBwGBwf8AwGAgAAMBYxgxBgaTANj4NBIJgwVX+"
      "jXKCAMFgC8LgBGBmB3KTAhAT8wIQFjATAWhyLf4TUFdHcW4WkdwwxdMT3EaJEeo4UknR8dww"
      "wlxIj1RZJJ98S0khhhhiaPX/8LqO4YYS4kRhRhf/8nD2HsYj1HqZF4vf9YKiIKgqIlQAMA3/"
      "kQAMHsToyV0cDdv/"
      "7IMQHgEgUaSld4QAg1A0lde4cXDskP7w0MysiKzEUARMCEBQwLQPAwC8VADBwCsOwF+v///"
      "6ydVW3tR1HNzg22xv+3Z9gAAOgA//"
      "pg1gxGG0G6aJdDp5LCgnFycZmDJi0ADQhRrZGzKGQAqP//3t3Xe3pUv19yF6v7FIAAiMAb/"
      "3/"
      "+yDEAwBGpBsprn9gIN0NZOn9lFyAAGa1QaI6ZhLqtGY3QFgnJ4BlymYWTBYNQ4LcX88rfX/"
      "1Yu+8WKLoSm09u7Fd1QADgbfwwBECUMBpB+TDDGAUySsMLO80jP18xowMNGTBgotYkm3gPv/"
      "/6P1v2pspRShZJjXgT7V1AAAoAG/9//"
      "sgxAMCxzRpKa9k5KDODOUR7ihciAAsEwYdoVZqATrn1uJSYowIBg9gKn0MboJlBF3Fh4YAfX"
      "//9+52v6qhZt7o244rX/JfRoADB+B5MPsQ401sRj4pGKOeGUzuJDGwHEhUhAvBuMNAM1b//"
      "t9kSl70NlDrbJecU/t99aoAACMAD//7IMQCggY0Gymuf2Ag7A0k9f2UXPwAAGaFSZ/"
      "7BhFSu4Yy2FjHCYZlKoYQTiEMTLaGxV5nNu/8UddjmtWbl6r/SYAN/pAADACAI8wHQHwMM/"
      "XrDJuAv48nRNEXDHS8w4YMJCy0aSDbgm3//26S0noiIgkPfZn1Sa9V16dNAAAgAA//"
      "+yDEAoBHCGkpr2SkoMgDZXW/cAT4iAA8FEYeASxqGx/H20IYYpYHJg+AHH2GbgBlgl/"
      "1yQ2AFP///YpK32okeasc/f/+xXsAAJ1AA/"
      "9Ntaj1Pc0K7Yzw6FrOHlozEHzFYEEg6NANZbIn9a8p//j7HC6VvlmStt3o+pUAACMADfyA//"
      "sgxAOCRkwbKa5/YCDUDWU1/ZxcAGZVQZ27Zg/KweYuMFmm74hkSqYKUCINS0ZoxZ5XOv/"
      "8X7EgE4lCZDu7fc4AN/6BQHQwG0GpMMAUczI/wpM7iuM9TTGCQwsRMEBi8Cl7yAnv//"
      "2+belL59SGkk1ENqvyagAAKAAP/aAAEBGmGv/"
      "7IMQGAobYaSuvcOLgzA1lNe4cXGDeaOzj56RhnnIBMZrA4GMAKF4GBCJjK4gC+v///"
      "uh3b1WWRQNv2e/syS7ABAADCACBMPUSw0sNqj23G4OZHMzmKjGgLDBMkAzxpMNAE1b///"
      "od72VdCOtlpw1/764AAhwAf/0AAGUkeZb0Bgz/"
      "+yDEB4CGMBsrrn9gINgNJXX9qFxCcAYkOE7GsVJi6QBCEZCEEav2owqE3f4+KbGKLWKN29/"
      "YsAAC0AUAARAL5gMgLQYWGjRGQkBGh1MmZseGKjpgwUYCBoprUgcDlG//7372tX0y/"
      "zl33dN2ugIf/yIADoERhDlqm9CtAfsRzhlK//"
      "tAxAoAB7RpKPXhACHRkia3PPAAEkGL4EUFgCTA3BTMDkAcEgMgoCeefz/////"
      "oxOy73ryRx97nI2//YryIAhX0mveu/"
      "3tEgAAAABh2nnnBAAOYOK6ZtxB4mEYkiaDwX5gzgHGAkAUYGwB0kMGQFaKGBEAwDgHAUAcvP"
      "KwDfJeHEGqcMk3iN5blKocU8c6FA4FxhTqXf/OtXzv37ErkOYWXP/"
      "93kTV91+YNo3Lh8ECwliUABv7/"
      "+xDEAYPIREMrXcMAKAAAP8AAAARfwAADHinN1RU5NKTjkHN1Mc08dTJQjL4GBwgYEAK/"
      "X2a8/1qZjMtcFCUTiSXmteUeFNBWIqEKCioLiKyO10VVTEFNRTMuOTguMlVVVVVVVVVVVf/"
      "7EMQJg8AAAaQAAAAgAAA0gAAABFVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
      "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVEFHAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAP8=");

  wmpi_->Load(blink::WebMediaPlayer::kLoadTypeURL,
              blink::WebMediaPlayerSource(blink::WebURL(kMp3DataUrl)),
              blink::WebMediaPlayer::kCorsModeUnspecified);

  base::RunLoop().RunUntilIdle();

  // This runs until we reach the have current data state. Attempting to wait
  // for states < kReadyStateHaveCurrentData is unreliable due to asynchronous
  // execution of tasks on the base::test:TaskEnvironment.
  while (wmpi_->GetReadyState() <
         blink::WebMediaPlayer::kReadyStateHaveCurrentData) {
    base::RunLoop loop;
    EXPECT_CALL(client_, ReadyStateChanged())
        .WillRepeatedly(RunClosure(loop.QuitClosure()));
    loop.Run();

    // Clear the mock so it doesn't have a stale QuitClosure.
    testing::Mock::VerifyAndClearExpectations(&client_);
  }

  EXPECT_FALSE(IsSuspended());
  CycleThreads();
}

// Verify that preload=metadata suspend works properly.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspend) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveMetadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_TRUE(IsSuspended());
  EXPECT_TRUE(ShouldCancelUponDefer());

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure there's no other memory usage.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_EQ(reported_memory_ - data_source_size, 0);
}

// Verify that Play() before kReadyStateHaveEnough doesn't increase buffer size.
TEST_F(WebMediaPlayerImplTest, NoBufferSizeIncreaseUntilHaveEnough) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(true));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadAuto);
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveMetadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  wmpi_->Play();
  EXPECT_FALSE(IsDataSourceMarkedAsPlaying());

  while (wmpi_->GetReadyState() <
         blink::WebMediaPlayer::kReadyStateHaveEnoughData) {
    // Clear the mock so it doesn't have a stale QuitClosure.
    testing::Mock::VerifyAndClearExpectations(&client_);

    base::RunLoop loop;
    EXPECT_CALL(client_, ReadyStateChanged())
        .WillRepeatedly(RunClosure(loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_TRUE(IsDataSourceMarkedAsPlaying());
}

// Verify that preload=metadata suspend works properly for streaming sources.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspendNoStreaming) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);

  // This test needs a file which is larger than the MultiBuffer block size;
  // otherwise we'll never complete initialization of the MultiBufferDataSource.
  constexpr char kLargeAudioOnlyTestFile[] = "bear_192kHz.wav";
  Load(kLargeAudioOnlyTestFile, LoadType::kStreaming);

  // This runs until we reach the metadata state.
  while (wmpi_->GetReadyState() <
         blink::WebMediaPlayer::kReadyStateHaveMetadata) {
    base::RunLoop loop;
    EXPECT_CALL(client_, ReadyStateChanged())
        .WillRepeatedly(RunClosure(loop.QuitClosure()));
    loop.Run();

    // Clear the mock so it doesn't have a stale QuitClosure.
    testing::Mock::VerifyAndClearExpectations(&client_);
  }

  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_FALSE(IsSuspended());
}

// Verify that lazy load for preload=metadata works properly.
TEST_F(WebMediaPlayerImplTest, LazyLoadPreloadMetadataSuspend) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPreloadMetadataLazyLoad);
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);

  // Don't set poster, but ensure we still reach suspended state.

  LoadAndWaitForReadyState(kVideoOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveMetadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_TRUE(IsSuspended());
  EXPECT_TRUE(wmpi_->DidLazyLoad());
  EXPECT_FALSE(ShouldCancelUponDefer());

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure there's no other memory usage.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_EQ(reported_memory_ - data_source_size, 0);

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

// Verify that preload=metadata suspend video w/ poster uses zero video memory.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspendNoVideoMemoryUsage) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);
  wmpi_->SetPoster(blink::WebURL(GURL("file://example.com/sample.jpg")));

  LoadAndWaitForReadyState(kVideoOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveMetadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_TRUE(IsSuspended());

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure there's no other memory usage.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_EQ(reported_memory_ - data_source_size, 0);

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

// Verify that preload=metadata suspend is aborted if we know the element will
// play as soon as we reach kReadyStateHaveFutureData.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspendCouldPlay) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(true));
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadMetaData);
  LoadAndWaitForCurrentData(kAudioOnlyTestFile);
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

TEST_F(WebMediaPlayerImplTest, RequestVideoFrameCallback) {
  InitializeWebMediaPlayerImpl();

  EXPECT_CALL(*compositor_, SetOnFramePresentedCallback(_));
  RequestVideoFrameCallback();
}

TEST_F(WebMediaPlayerImplTest, UpdateFrameIfStale) {
  InitializeWebMediaPlayerImpl();

  base::RunLoop loop;
  EXPECT_CALL(*compositor_,
              UpdateCurrentFrameIfStale(
                  VideoFrameCompositor::UpdateType::kBypassClient))
      .WillOnce(RunClosure(loop.QuitClosure()));

  UpdateFrameIfStale();

  loop.Run();

  testing::Mock::VerifyAndClearExpectations(compositor_);
}

TEST_F(WebMediaPlayerImplTest, GetVideoFramePresentationMetadata) {
  InitializeWebMediaPlayerImpl();

  EXPECT_CALL(*compositor_, GetLastPresentedFrameMetadata());
  GetVideoFramePresentationMetadata();
}

TEST_F(WebMediaPlayerImplTest, OnNewFramePresentedCallback) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, OnRequestVideoFrameCallback());

  OnNewFramePresentedCallback();
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
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_DoesNotStaySuspended) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveMetadata);

  // Should stay suspended even though not stale or backgrounded.
  WebMediaPlayerImpl::PlayState state = ComputePlayState_Suspended();
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

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Flinging) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);

  // Remote media via the FlingingRenderer should not be idle.
  WebMediaPlayerImpl::PlayState state = ComputePlayState_Flinging();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
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

TEST_F(WebMediaPlayerImplTest, AutoplayMuted) {
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  metadata.has_audio = true;
  metadata.audio_decoder_config = TestAudioConfig::Normal();

  EXPECT_CALL(client_, WasAlwaysMuted()).WillRepeatedly(Return(true));

  InitializeWebMediaPlayerImpl();
  SetPaused(false);

  EXPECT_CALL(delegate_, DidMediaMetadataChange(_, false, true, _));
  OnMetadata(metadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(client_, WasAlwaysMuted()).WillRepeatedly(Return(false));
  EXPECT_CALL(delegate_, DidMediaMetadataChange(_, true, true, _));
  wmpi_->SetVolume(1.0);
}

TEST_F(WebMediaPlayerImplTest, MediaPositionState_Playing) {
  InitializeWebMediaPlayerImpl();
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(1.0);
  Play();

  EXPECT_CALL(delegate_,
              DidPlayerMediaPositionStateChange(
                  delegate_.player_id(),
                  media_session::MediaPosition(1.0, kAudioOnlyTestFileDuration,
                                               base::TimeDelta())));
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, MediaPositionState_Paused) {
  InitializeWebMediaPlayerImpl();
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(1.0);

  // The effective playback rate is 0.0 while paused.
  EXPECT_CALL(delegate_,
              DidPlayerMediaPositionStateChange(
                  delegate_.player_id(),
                  media_session::MediaPosition(0.0, kAudioOnlyTestFileDuration,
                                               base::TimeDelta())));
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, MediaPositionState_PositionChange) {
  InitializeWebMediaPlayerImpl();
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(0.5);
  Play();

  testing::Sequence sequence;
  EXPECT_CALL(delegate_, DidPlayerMediaPositionStateChange(
                             delegate_.player_id(),
                             media_session::MediaPosition(
                                 0.0, kAudioOnlyTestFileDuration,
                                 base::TimeDelta::FromSecondsD(0.1))))
      .InSequence(sequence);
  wmpi_->Seek(0.1);
  wmpi_->OnTimeUpdate();

  // If we load enough data to resume playback the position should be updated.
  EXPECT_CALL(delegate_, DidPlayerMediaPositionStateChange(
                             delegate_.player_id(),
                             media_session::MediaPosition(
                                 0.5, kAudioOnlyTestFileDuration,
                                 base::TimeDelta::FromSecondsD(0.1))))
      .InSequence(sequence);
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->OnTimeUpdate();

  // No media time progress -> no MediaPositionState change.
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, MediaPositionState_Underflow) {
  InitializeWebMediaPlayerImpl();
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(1.0);
  Play();

  // Underflow will set the effective playback rate to 0.0.
  EXPECT_CALL(delegate_,
              DidPlayerMediaPositionStateChange(
                  delegate_.player_id(),
                  media_session::MediaPosition(0.0, kAudioOnlyTestFileDuration,
                                               base::TimeDelta())));
  SetReadyState(blink::WebMediaPlayer::kReadyStateHaveCurrentData);
  wmpi_->OnTimeUpdate();
}

// It's possible for current time to be infinite if the page seeks to
// |kInfiniteDuration| (2**64 - 1) when duration is infinite.
TEST_F(WebMediaPlayerImplTest, MediaPositionState_InfiniteCurrentTime) {
  InitializeWebMediaPlayerImpl();
  SetDuration(kInfiniteDuration);
  wmpi_->OnTimeUpdate();

  EXPECT_CALL(delegate_, DidPlayerMediaPositionStateChange(
                             delegate_.player_id(),
                             media_session::MediaPosition(
                                 0.0, kInfiniteDuration, kInfiniteDuration)));
  wmpi_->Seek(kInfiniteDuration.InSecondsF());
  wmpi_->OnTimeUpdate();

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, DidPlayerMediaPositionStateChange(_, _)).Times(0);
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, NoStreams) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;

  EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
  EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer()).Times(0);
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId()).Times(0);
  EXPECT_CALL(*compositor_, EnableSubmission(_, _, _)).Times(0);

  // Nothing should happen.  In particular, no assertions should fail.
  OnMetadata(metadata);
}

TEST_F(WebMediaPlayerImplTest, Encrypted) {
  InitializeWebMediaPlayerImpl();

  // To avoid PreloadMetadataLazyLoad.
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadAuto);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(encrypted_client_,
                Encrypted(EmeInitDataType::WEBM, NotNull(), Gt(0u)));
    EXPECT_CALL(encrypted_client_, DidBlockPlaybackWaitingForKey());
    EXPECT_CALL(encrypted_client_, DidResumePlaybackBlockedForKey())
        .WillRepeatedly(RunClosure(run_loop.QuitClosure()));
    Load(kEncryptedVideoOnlyTestFile);
    run_loop.Run();
  }

  CreateCdm();

  // The CDM doesn't support Decryptor nor CDM ID. Pipeline startup will fail.
  EXPECT_CALL(mock_cdm_context_, GetDecryptor())
      .Times(AnyNumber())
      .WillRepeatedly(Return(nullptr));
  mock_cdm_context_.set_cdm_id(nullptr);

  {
    // Wait for kNetworkStateFormatError caused by Renderer initialization
    // error.
    base::RunLoop run_loop;
    EXPECT_CALL(client_, NetworkStateChanged()).WillOnce(Invoke([&] {
      if (wmpi_->GetNetworkState() ==
          blink::WebMediaPlayer::kNetworkStateFormatError)
        run_loop.QuitClosure().Run();
    }));
    SetCdm();
    run_loop.Run();
  }
}

TEST_F(WebMediaPlayerImplTest, Waiting_NoDecryptionKey) {
  InitializeWebMediaPlayerImpl();

  // Use non-encrypted file here since we don't have a CDM. Otherwise pipeline
  // initialization will stall waiting for a CDM to be set.
  LoadAndWaitForCurrentData(kVideoOnlyTestFile);

  EXPECT_CALL(encrypted_client_, DidBlockPlaybackWaitingForKey());
  EXPECT_CALL(encrypted_client_, DidResumePlaybackBlockedForKey());

  OnWaiting(WaitingReason::kNoDecryptionKey);
}

ACTION(ReportHaveEnough) {
  arg0->OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                               BUFFERING_CHANGE_REASON_UNKNOWN);
}

TEST_F(WebMediaPlayerImplTest, FallbackToMediaFoundationRenderer) {
  InitializeWebMediaPlayerImpl();
  // To avoid PreloadMetadataLazyLoad.
  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadAuto);

  // Use MockRendererFactory for kMediaFoundation where the created Renderer
  // will take the CDM, complete Renderer initialization and report HAVE_ENOUGH
  // so that WMPI can reach kReadyStateHaveCurrentData.
  auto mock_renderer_factory = std::make_unique<MockRendererFactory>();
  EXPECT_CALL(*mock_renderer_factory, CreateRenderer(_, _, _, _, _, _))
      .WillOnce(testing::WithoutArgs(Invoke([]() {
        auto mock_renderer = std::make_unique<NiceMock<MockRenderer>>();
        EXPECT_CALL(*mock_renderer, OnSetCdm(_, _))
            .WillOnce(RunOnceCallback<1>(true));
        EXPECT_CALL(*mock_renderer, OnInitialize(_, _, _))
            .WillOnce(DoAll(RunOnceCallback<2>(PIPELINE_OK),
                            WithArg<1>(ReportHaveEnough())));
        return mock_renderer;
      })));

  renderer_factory_selector_->AddFactory(RendererFactoryType::kMediaFoundation,
                                         std::move(mock_renderer_factory));

  // Create and set CDM. The CDM doesn't support a Decryptor and requires Media
  // Foundation Renderer.
  EXPECT_CALL(mock_cdm_context_, GetDecryptor())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_cdm_context_, RequiresMediaFoundationRenderer())
      .WillRepeatedly(Return(true));

  CreateCdm();
  SetCdm();

  // Load encrypted media and wait for HaveCurrentData.
  EXPECT_CALL(encrypted_client_,
              Encrypted(EmeInitDataType::WEBM, NotNull(), Gt(0u)));
  LoadAndWaitForReadyState(kEncryptedVideoOnlyTestFile,
                           blink::WebMediaPlayer::kReadyStateHaveCurrentData);
}

TEST_F(WebMediaPlayerImplTest, VideoConfigChange) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config =
      TestVideoConfig::NormalCodecProfile(kCodecVP9, VP9PROFILE_PROFILE0);
  metadata.natural_size = gfx::Size(320, 240);

  // Arrival of metadata should trigger creation of reporter with video config
  // with profile matching test config.
  OnMetadata(metadata);
  VideoDecodeStatsReporter* last_reporter = GetVideoStatsReporter();
  ASSERT_NE(nullptr, last_reporter);
  ASSERT_EQ(VP9PROFILE_PROFILE0, GetVideoStatsReporterCodecProfile());

  // Changing the codec profile should trigger recreation of the reporter.
  auto new_profile_config =
      TestVideoConfig::NormalCodecProfile(kCodecVP9, VP9PROFILE_PROFILE1);
  OnVideoConfigChange(new_profile_config);
  ASSERT_EQ(VP9PROFILE_PROFILE1, GetVideoStatsReporterCodecProfile());
  ASSERT_NE(last_reporter, GetVideoStatsReporter());
  last_reporter = GetVideoStatsReporter();

  // Changing the codec (implies changing profile) should similarly trigger
  // recreation of the reporter.
  auto new_codec_config = TestVideoConfig::NormalCodecProfile(kCodecVP8);
  OnVideoConfigChange(new_codec_config);
  ASSERT_EQ(VP8PROFILE_MIN, GetVideoStatsReporterCodecProfile());
  ASSERT_NE(last_reporter, GetVideoStatsReporter());
  last_reporter = GetVideoStatsReporter();

  // Changing other aspects of the config (like colorspace) should not trigger
  // recreation of the reporter
  VideoDecoderConfig new_color_config = TestVideoConfig::NormalWithColorSpace(
      kCodecVP8, VideoColorSpace::REC709());
  ASSERT_EQ(VP8PROFILE_MIN, new_color_config.profile());
  OnVideoConfigChange(new_color_config);
  ASSERT_EQ(last_reporter, GetVideoStatsReporter());
  ASSERT_EQ(VP8PROFILE_MIN, GetVideoStatsReporterCodecProfile());

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config =
      TestVideoConfig::NormalCodecProfile(kCodecVP8, VP8PROFILE_MIN);
  metadata.natural_size = gfx::Size(320, 240);

  OnMetadata(metadata);
  ASSERT_EQ(gfx::Size(320, 240), wmpi_->NaturalSize());

  // Arrival of metadata should trigger creation of reporter with original size.
  VideoDecodeStatsReporter* orig_stats_reporter = GetVideoStatsReporter();
  ASSERT_NE(nullptr, orig_stats_reporter);
  ASSERT_TRUE(
      orig_stats_reporter->MatchesBucketedNaturalSize(gfx::Size(320, 240)));

  EXPECT_CALL(client_, SizeChanged());
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(gfx::Size(1920, 1080), wmpi_->NaturalSize());

  // New natural size triggers new reporter to be created.
  ASSERT_NE(orig_stats_reporter, GetVideoStatsReporter());
  ASSERT_TRUE(GetVideoStatsReporter()->MatchesBucketedNaturalSize(
      gfx::Size(1920, 1080)));

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange_Rotated) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config =
      TestVideoConfig::NormalRotated(VIDEO_ROTATION_90);
  metadata.natural_size = gfx::Size(320, 240);

  OnMetadata(metadata);
  ASSERT_EQ(gfx::Size(320, 240), wmpi_->NaturalSize());

  // Arrival of metadata should trigger creation of reporter with original size.
  VideoDecodeStatsReporter* orig_stats_reporter = GetVideoStatsReporter();
  ASSERT_NE(nullptr, orig_stats_reporter);
  ASSERT_TRUE(
      orig_stats_reporter->MatchesBucketedNaturalSize(gfx::Size(320, 240)));

  EXPECT_CALL(client_, SizeChanged());
  // For 90/270deg rotations, the natural size should be transposed.
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(gfx::Size(1080, 1920), wmpi_->NaturalSize());

  // New natural size triggers new reporter to be created.
  ASSERT_NE(orig_stats_reporter, GetVideoStatsReporter());
  ASSERT_TRUE(GetVideoStatsReporter()->MatchesBucketedNaturalSize(
      gfx::Size(1080, 1920)));

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest, VideoLockedWhenPausedWhenHidden) {
  InitializeWebMediaPlayerImpl();

  // Setting metadata initializes |watch_time_reporter_| used in play().
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();

  OnMetadata(metadata);

  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // Backgrounding the player sets the lock.
  BackgroundPlayer();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // Play without a user gesture doesn't unlock the player.
  Play();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // With a user gesture it does unlock the player.
  GetWebLocalFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  Play();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // Pause without a user gesture doesn't lock the player.
  GetWebLocalFrame()->ConsumeTransientUserActivation();
  Pause();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // With a user gesture, pause does lock the player.
  GetWebLocalFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  Pause();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // Foregrounding the player unsets the lock.
  ForegroundPlayer();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
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

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest, SetContentsLayerGetsWebLayerFromBridge) {
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
  EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  EXPECT_CALL(*compositor_, EnableSubmission(_, _, _));

  // We only call the callback to create the bridge in OnMetadata, so we need
  // to call it.
  OnMetadata(metadata);

  scoped_refptr<cc::Layer> layer = cc::Layer::Create();

  EXPECT_CALL(*surface_layer_bridge_ptr_, GetCcLayer())
      .WillRepeatedly(Return(layer.get()));
  EXPECT_CALL(client_, SetCcLayer(Eq(layer.get())));
  EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  wmpi_->RegisterContentsLayer(layer.get());

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
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

// Tests that updating the surface id calls OnPictureInPictureStateChange.
TEST_F(WebMediaPlayerImplTest, PictureInPictureStateChange) {
  InitializeWebMediaPlayerImpl();

  EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
      .WillRepeatedly(ReturnRef(surface_id_));
  EXPECT_CALL(*compositor_, EnableSubmission(_, _, _));
  EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));

  PipelineMetadata metadata;
  metadata.has_video = true;
  OnMetadata(metadata);

  EXPECT_CALL(client_, DisplayType())
      .WillRepeatedly(
          Return(blink::WebMediaPlayer::DisplayType::kPictureInPicture));
  EXPECT_CALL(client_, OnPictureInPictureStateChange()).Times(1);

  wmpi_->OnSurfaceIdUpdated(surface_id_);

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest, OnProgressClearsStale) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);

  for (auto rs = blink::WebMediaPlayer::kReadyStateHaveNothing;
       rs <= blink::WebMediaPlayer::kReadyStateHaveEnoughData;
       rs = static_cast<blink::WebMediaPlayer::ReadyState>(
           static_cast<int>(rs) + 1)) {
    SetReadyState(rs);
    delegate_.SetStaleForTesting(true);
    OnProgress();
    EXPECT_EQ(delegate_.IsStale(delegate_.player_id()),
              rs >= blink::WebMediaPlayer::kReadyStateHaveFutureData);
  }
}

TEST_F(WebMediaPlayerImplTest, MemDumpProvidersRegistration) {
  auto* dump_manager = base::trace_event::MemoryDumpManager::GetInstance();
  InitializeWebMediaPlayerImpl();

  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadAuto);
  auto* main_dumper = GetMainThreadMemDumper();
  EXPECT_TRUE(dump_manager->IsDumpProviderRegisteredForTesting(main_dumper));
  LoadAndWaitForCurrentData(kVideoAudioTestFile);

  auto* media_dumper = GetMediaThreadMemDumper();
  EXPECT_TRUE(dump_manager->IsDumpProviderRegisteredForTesting(media_dumper));
  CycleThreads();

  wmpi_.reset();
  CycleThreads();

  EXPECT_FALSE(dump_manager->IsDumpProviderRegisteredForTesting(main_dumper));
  EXPECT_FALSE(dump_manager->IsDumpProviderRegisteredForTesting(media_dumper));
}

TEST_F(WebMediaPlayerImplTest, MemDumpReporting) {
  InitializeWebMediaPlayerImpl();

  wmpi_->SetPreload(blink::WebMediaPlayer::kPreloadAuto);
  LoadAndWaitForCurrentData(kVideoAudioTestFile);

  CycleThreads();

  base::trace_event::MemoryDumpRequestArgs args = {
      1 /* dump_guid*/, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};

  int32_t id = GetMediaLogId();
  int dump_count = 0;

  auto on_memory_dump_done = base::BindLambdaForTesting(
      [&](bool success, uint64_t dump_guid,
          std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd) {
        ASSERT_TRUE(success);
        const auto& dumps = pmd->allocator_dumps();

        std::vector<const char*> allocations = {"audio", "video", "data_source",
                                                "demuxer"};

        for (const char* name : allocations) {
          auto it = dumps.find(base::StringPrintf(
              "media/webmediaplayer/%s/player_0x%x", name, id));
          ASSERT_NE(dumps.end(), it) << name;
          ASSERT_GT(it->second->GetSizeInternal(), 0u) << name;
        }

        auto it = dumps.find(
            base::StringPrintf("media/webmediaplayer/player_0x%x", id));
        ASSERT_NE(dumps.end(), it);
        auto* player_dump = it->second.get();
        const auto& entries = player_dump->entries();

        auto instance_counter_it =
            std::find_if(entries.begin(), entries.end(), [](const auto& e) {
              auto* name =
                  base::trace_event::MemoryAllocatorDump::kNameObjectCount;
              return e.name == name && e.value_uint64 == 1;
            });
        ASSERT_NE(entries.end(), instance_counter_it);

        if (args.level_of_detail ==
            base::trace_event::MemoryDumpLevelOfDetail::DETAILED) {
          auto player_state_it =
              std::find_if(entries.begin(), entries.end(), [](const auto& e) {
                return e.name == "player_state" && !e.value_string.empty();
              });
          ASSERT_NE(entries.end(), player_state_it);
        }
        dump_count++;
      });

  auto* dump_manager = base::trace_event::MemoryDumpManager::GetInstance();

  dump_manager->CreateProcessDump(args, on_memory_dump_done);

  args.level_of_detail = base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND;
  args.dump_guid++;
  dump_manager->CreateProcessDump(args, on_memory_dump_done);

  args.level_of_detail = base::trace_event::MemoryDumpLevelOfDetail::LIGHT;
  args.dump_guid++;
  dump_manager->CreateProcessDump(args, on_memory_dump_done);

  CycleThreads();
  EXPECT_EQ(dump_count, 3);
}

// Verify that a demuxer override is used when specified.
// TODO(https://crbug.com/1084476): This test is flaky.
TEST_F(WebMediaPlayerImplTest, DISABLED_DemuxerOverride) {
  std::unique_ptr<MockDemuxer> demuxer =
      std::make_unique<NiceMock<MockDemuxer>>();
  StrictMock<MockDemuxerStream> stream(DemuxerStream::AUDIO);
  stream.set_audio_decoder_config(TestAudioConfig::Normal());
  std::vector<DemuxerStream*> streams;
  streams.push_back(&stream);

  EXPECT_CALL(stream, SupportsConfigChanges()).WillRepeatedly(Return(false));

  EXPECT_CALL(*demuxer.get(), OnInitialize(_, _))
      .WillOnce(RunOnceCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*demuxer.get(), GetAllStreams()).WillRepeatedly(Return(streams));
  // Called when WebMediaPlayerImpl is destroyed.
  EXPECT_CALL(*demuxer.get(), Stop());

  InitializeWebMediaPlayerImplInternal(std::move(demuxer));

  EXPECT_FALSE(IsSuspended());
  wmpi_->Load(blink::WebMediaPlayer::kLoadTypeURL,
              blink::WebMediaPlayerSource(blink::WebURL(GURL("data://test"))),
              blink::WebMediaPlayer::kCorsModeUnspecified);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

class WebMediaPlayerImplBackgroundBehaviorTest
    : public WebMediaPlayerImplTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, int, int, bool, bool, bool, bool, bool>> {
 public:
  // Indices of the tuple parameters.
  static const int kIsMediaSuspendEnabled = 0;
  static const int kDurationSec = 1;
  static const int kAverageKeyframeDistanceSec = 2;
  static const int kIsResumeBackgroundVideoEnabled = 3;
  static const int kIsMediaSource = 4;
  static const int kIsBackgroundPauseEnabled = 5;
  static const int kIsPictureInPictureEnabled = 6;
  static const int kIsBackgroundVideoPlaybackEnabled = 7;

  void SetUp() override {
    WebMediaPlayerImplTest::SetUp();
    SetUpMediaSuspend(IsMediaSuspendOn());
    SetUpBackgroundVideoPlayback(IsBackgroundVideoPlaybackEnabled());

    std::string enabled_features;
    std::string disabled_features;

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

  bool IsResumeBackgroundVideoEnabled() {
    return std::get<kIsResumeBackgroundVideoEnabled>(GetParam());
  }

  bool IsBackgroundPauseOn() {
    return std::get<kIsBackgroundPauseEnabled>(GetParam());
  }

  bool IsPictureInPictureOn() {
    return std::get<kIsPictureInPictureEnabled>(GetParam());
  }

  bool IsBackgroundVideoPlaybackEnabled() {
    return std::get<kIsBackgroundVideoPlaybackEnabled>(GetParam());
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

  bool ShouldPausePlaybackWhenHidden() const {
    return wmpi_->ShouldPausePlaybackWhenHidden();
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
  EXPECT_FALSE(ShouldPausePlaybackWhenHidden());
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
  bool should_pause = !IsBackgroundVideoPlaybackEnabled() ||
                      IsMediaSuspendOn() ||
                      (IsBackgroundPauseOn() && matches_requirements);
  EXPECT_EQ(should_pause, ShouldPausePlaybackWhenHidden());
}

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, AudioVideo) {
  SetMetadata(true, true);

  // Optimization requirements are the same for all platforms.
  bool matches_requirements =
      !IsPictureInPictureOn() &&
      ((GetDurationSec() < GetMaxKeyframeDistanceSec()) ||
       (GetAverageKeyframeDistanceSec() < GetMaxKeyframeDistanceSec()));

  EXPECT_EQ(matches_requirements, IsBackgroundOptimizationCandidate());
  EXPECT_EQ(matches_requirements, ShouldDisableVideoWhenHidden());

  // Only pause audible videos if both media suspend and resume background
  // videos is on and background video playback is disabled. Background video
  // playback is enabled by default. Both media suspend and resume background
  // videos are on by default on Android and off on desktop.
  EXPECT_EQ(!IsBackgroundVideoPlaybackEnabled() ||
                (IsMediaSuspendOn() && IsResumeBackgroundVideoEnabled()),
            ShouldPausePlaybackWhenHidden());

  if (!matches_requirements || !ShouldDisableVideoWhenHidden() ||
      IsMediaSuspendOn()) {
    return;
  }

  // These tests start in background mode prior to having metadata, so put the
  // test back into a normal state.
  EXPECT_TRUE(IsDisableVideoTrackPending());

  ForegroundPlayer();
  EXPECT_FALSE(IsVideoTrackDisabled());
  EXPECT_FALSE(IsDisableVideoTrackPending());

  // Should start background disable timer, but not disable immediately.
  BackgroundPlayer();
  if (ShouldPausePlaybackWhenHidden()) {
    EXPECT_FALSE(IsVideoTrackDisabled());
    EXPECT_FALSE(IsDisableVideoTrackPending());
  } else {
    // Testing IsVideoTrackDisabled() leads to flakyness even though there
    // should be a 10 minutes delay until it happens. Given that it doesn't
    // provides much of a benefit at the moment, this is being ignored.
    EXPECT_TRUE(IsDisableVideoTrackPending());
  }
}

INSTANTIATE_TEST_SUITE_P(
    BackgroundBehaviorTestInstances,
    WebMediaPlayerImplBackgroundBehaviorTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(
            WebMediaPlayerImpl::kMaxKeyframeDistanceToDisableBackgroundVideoMs /
                    base::Time::kMillisecondsPerSecond -
                1,
            300),
        ::testing::Values(
            WebMediaPlayerImpl::kMaxKeyframeDistanceToDisableBackgroundVideoMs /
                    base::Time::kMillisecondsPerSecond -
                1,
            100),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool()));

}  // namespace media
