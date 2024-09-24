// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/web_media_player_impl.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "components/viz/test/test_context_provider.h"
#include "media/base/decoder_buffer.h"
#include "media/base/key_systems_impl.h"
#include "media/base/media_content_type.h"
#include "media/base/media_log.h"
#include "media/base/media_observer.h"
#include "media/base/media_switches.h"
#include "media/base/memory_dump_provider_proxy.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/filters/pipeline_controller.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "media/mojo/services/video_decode_stats_recorder.h"
#include "media/mojo/services/watch_time_recorder.h"
#include "media/renderers/default_decoder_factory.h"
#include "media/renderers/renderer_impl_factory.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/media/web_media_player_builder.h"
#include "third_party/blink/public/platform/media/web_media_player_delegate.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"
#include "third_party/blink/renderer/platform/media/power_status_helper.h"
#include "third_party/blink/renderer/platform/media/resource_multi_buffer_data_provider.h"
#include "third_party/blink/renderer/platform/media/testing/mock_resource_fetch_context.h"
#include "third_party/blink/renderer/platform/media/testing/mock_web_associated_url_loader.h"
#include "third_party/blink/renderer/platform/media/video_decode_stats_reporter.h"
#include "third_party/blink/renderer/platform/media/web_audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/media/web_content_decryption_module_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::media::TestAudioConfig;
using ::media::TestVideoConfig;
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

constexpr char kAudioOnlyTestFile[] = "sfx-opus-441.webm";
constexpr char kVideoOnlyTestFile[] = "bear-320x240-video-only.webm";
constexpr char kVideoAudioTestFile[] = "bear-320x240-16x9-aspect.webm";
constexpr char kEncryptedVideoOnlyTestFile[] = "bear-320x240-av_enc-v.webm";

constexpr base::TimeDelta kAudioOnlyTestFileDuration = base::Milliseconds(296);

enum class BackgroundBehaviorType { Page, Frame };

MATCHER(WmpiDestroyed, "") {
  return CONTAINS_STRING(arg, "{\"event\":\"kWebMediaPlayerDestroyed\"}");
}

MATCHER_P2(PlaybackRateChanged, old_rate_string, new_rate_string, "") {
  return CONTAINS_STRING(arg, "Effective playback rate changed from " +
                                  std::string(old_rate_string) + " to " +
                                  std::string(new_rate_string));
}

class MockMediaObserver : public media::MediaObserver {
 public:
  MOCK_METHOD1(OnBecameDominantVisibleContent, void(bool));
  MOCK_METHOD1(OnMetadataChanged, void(const media::PipelineMetadata&));
  MOCK_METHOD1(OnRemotePlaybackDisabled, void(bool));
  MOCK_METHOD0(OnMediaRemotingRequested, void());
  MOCK_METHOD0(OnHlsManifestDetected, void());
  MOCK_METHOD0(OnPlaying, void());
  MOCK_METHOD0(OnPaused, void());
  MOCK_METHOD0(OnFrozen, void());
  MOCK_METHOD1(OnDataSourceInitialized, void(const GURL&));
  MOCK_METHOD1(SetClient, void(media::MediaObserverClient*));

  base::WeakPtr<MediaObserver> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MediaObserver> weak_ptr_factory_{this};
};

class MockWebMediaPlayerClient : public WebMediaPlayerClient {
 public:
  MockWebMediaPlayerClient() = default;

  MockWebMediaPlayerClient(const MockWebMediaPlayerClient&) = delete;
  MockWebMediaPlayerClient& operator=(const MockWebMediaPlayerClient&) = delete;

  MOCK_METHOD0(NetworkStateChanged, void());
  MOCK_METHOD0(ReadyStateChanged, void());
  MOCK_METHOD0(TimeChanged, void());
  MOCK_METHOD0(Repaint, void());
  MOCK_METHOD0(DurationChanged, void());
  MOCK_METHOD0(SizeChanged, void());
  MOCK_METHOD1(SetCcLayer, void(cc::Layer*));
  MOCK_METHOD1(AddMediaTrack, void(const media::MediaTrack& track));
  MOCK_METHOD1(RemoveMediaTrack, void(const media::MediaTrack&));
  MOCK_METHOD1(MediaSourceOpened, void(std::unique_ptr<WebMediaSource>));
  MOCK_METHOD2(RemotePlaybackCompatibilityChanged, void(const WebURL&, bool));
  MOCK_METHOD0(WasAlwaysMuted, bool());
  MOCK_METHOD0(HasSelectedVideoTrack, bool());
  MOCK_METHOD0(GetSelectedVideoTrackId, WebMediaPlayer::TrackId());
  MOCK_METHOD0(HasNativeControls, bool());
  MOCK_METHOD0(IsAudioElement, bool());
  MOCK_CONST_METHOD0(GetDisplayType, DisplayType());
  MOCK_CONST_METHOD0(IsInAutoPIP, bool());
  MOCK_METHOD1(MediaRemotingStarted, void(const WebString&));
  MOCK_METHOD1(MediaRemotingStopped, void(int));
  MOCK_METHOD0(PictureInPictureStopped, void());
  MOCK_METHOD0(OnPictureInPictureStateChange, void());
  MOCK_CONST_METHOD0(CouldPlayIfEnoughData, bool());
  MOCK_METHOD0(ResumePlayback, void());
  MOCK_METHOD1(PausePlayback, void(WebMediaPlayerClient::PauseReason));
  MOCK_METHOD0(DidPlayerStartPlaying, void());
  MOCK_METHOD1(DidPlayerPaused, void(bool));
  MOCK_METHOD1(DidPlayerMutedStatusChange, void(bool));
  MOCK_METHOD6(DidMediaMetadataChange,
               void(bool,
                    bool,
                    media::AudioCodec,
                    media::VideoCodec,
                    media::MediaContentType,
                    bool));
  MOCK_METHOD4(DidPlayerMediaPositionStateChange,
               void(double,
                    base::TimeDelta,
                    base::TimeDelta position,
                    bool end_of_media));
  MOCK_METHOD0(DidDisableAudioOutputSinkChanges, void());
  MOCK_METHOD1(DidUseAudioServiceChange, void(bool uses_audio_service));
  MOCK_METHOD1(DidPlayerSizeChange, void(const gfx::Size&));
  MOCK_METHOD1(OnRemotePlaybackDisabled, void(bool));
  MOCK_METHOD0(DidBufferUnderflow, void());
  MOCK_METHOD0(DidSeek, void());
  MOCK_METHOD2(OnFirstFrame, void(base::TimeTicks, size_t));
  MOCK_METHOD0(OnRequestVideoFrameCallback, void());
  MOCK_METHOD0(GetElementId, int());
};

class MockWebMediaPlayerEncryptedMediaClient
    : public WebMediaPlayerEncryptedMediaClient {
 public:
  MockWebMediaPlayerEncryptedMediaClient() = default;

  MockWebMediaPlayerEncryptedMediaClient(
      const MockWebMediaPlayerEncryptedMediaClient&) = delete;
  MockWebMediaPlayerEncryptedMediaClient& operator=(
      const MockWebMediaPlayerEncryptedMediaClient&) = delete;

  MOCK_METHOD3(Encrypted,
               void(media::EmeInitDataType, const unsigned char*, unsigned));
  MOCK_METHOD0(DidBlockPlaybackWaitingForKey, void());
  MOCK_METHOD0(DidResumePlaybackBlockedForKey, void());
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

  MOCK_METHOD4(DidMediaMetadataChange,
               void(int, bool, bool, media::MediaContentType));

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

  void ClearStaleFlag(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    is_stale_ = false;
  }

  bool IsStale(int player_id) override {
    DCHECK_EQ(player_id_, player_id);
    return is_stale_;
  }

  bool IsPageHidden() override { return is_page_hidden_; }

  bool IsFrameHidden() override { return is_frame_hidden_; }

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

  void SetPageHiddenForTesting(bool is_page_hidden) {
    is_page_hidden_ = is_page_hidden;
  }

  void SetFrameHiddenForTesting(bool is_frame_hidden) {
    is_frame_hidden_ = is_frame_hidden;
  }

  int player_id() { return player_id_; }

 private:
  Observer* observer_ = nullptr;
  int player_id_ = 1234;
  bool is_idle_ = false;
  bool is_stale_ = false;
  bool is_page_hidden_ = false;
  bool is_frame_hidden_ = false;
};

class MockSurfaceLayerBridge : public WebSurfaceLayerBridge {
 public:
  MOCK_CONST_METHOD0(GetCcLayer, cc::Layer*());
  MOCK_CONST_METHOD0(GetFrameSinkId, const viz::FrameSinkId&());
  MOCK_CONST_METHOD0(GetSurfaceId, const viz::SurfaceId&());
  MOCK_METHOD0(ClearSurfaceId, void());
  MOCK_METHOD1(SetContentsOpaque, void(bool));
  MOCK_METHOD0(CreateSurfaceLayer, void());
  MOCK_METHOD0(ClearObserver, void());
  MOCK_METHOD0(RegisterFrameSinkHierarchy, void());
  MOCK_METHOD0(UnregisterFrameSinkHierarchy, void());
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
      std::unique_ptr<WebMediaPlayer::VideoFramePresentationMetadata>());
  MOCK_METHOD0(GetCurrentFrameOnAnyThread, scoped_refptr<media::VideoFrame>());
  MOCK_METHOD1(UpdateCurrentFrameIfStale,
               void(VideoFrameCompositor::UpdateType));
  MOCK_METHOD3(EnableSubmission,
               void(const viz::SurfaceId&, media::VideoTransformation, bool));
};

}  // namespace

class WebMediaPlayerImplTest
    : public testing::Test,
      private WebTestingSupport::WebScopedMockScrollbars {
 public:
  WebMediaPlayerImplTest()
      : media_thread_("MediaThreadForTest"),
        context_provider_(viz::TestContextProvider::Create()),
        audio_parameters_(media::TestAudioParameters::Normal()),
        memory_dump_manager_(
            base::trace_event::MemoryDumpManager::CreateInstanceForTesting()) {
    web_view_helper_.Initialize();
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

  WebMediaPlayerImplTest(const WebMediaPlayerImplTest&) = delete;
  WebMediaPlayerImplTest& operator=(const WebMediaPlayerImplTest&) = delete;

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
  }

 protected:
  void InitializeWebMediaPlayerImpl(
      std::unique_ptr<media::Demuxer> demuxer_override = nullptr) {
    auto media_log = std::make_unique<NiceMock<media::MockMediaLog>>();
    InitializeSurfaceLayerBridge();

    // Retain a raw pointer to |media_log| for use by tests. Meanwhile, give its
    // ownership to |wmpi_|. Reject attempts to reinitialize to prevent orphaned
    // expectations on previous |media_log_|.
    ASSERT_FALSE(media_log_) << "Reinitialization of media_log_ is disallowed";
    media_log_ = media_log.get();

    auto factory_selector = std::make_unique<media::RendererFactorySelector>();
    renderer_factory_selector_ = factory_selector.get();
    decoder_factory_ = std::make_unique<media::DefaultDecoderFactory>(nullptr);
    media::MediaPlayerLoggingID player_id =
        media::GetNextMediaPlayerLoggingID();
#if BUILDFLAG(IS_ANDROID)
    factory_selector->AddBaseFactory(
        media::RendererType::kRendererImpl,
        std::make_unique<media::RendererImplFactory>(
            media_log.get(), decoder_factory_.get(),
            media::RendererImplFactory::GetGpuFactoriesCB(), player_id));
    factory_selector->StartRequestRemotePlayStateCB(base::DoNothing());
#else
    factory_selector->AddBaseFactory(
        media::RendererType::kRendererImpl,
        std::make_unique<media::RendererImplFactory>(
            media_log.get(), decoder_factory_.get(),
            media::RendererImplFactory::GetGpuFactoriesCB(), player_id,
            nullptr));
#endif

    mojo::Remote<media::mojom::MediaMetricsProvider> provider;
    media::MediaMetricsProvider::Create(
        media::MediaMetricsProvider::BrowsingMode::kNormal,
        media::MediaMetricsProvider::FrameStatus::kNotTopFrame,
        ukm::kInvalidSourceId, media::learning::FeatureValue(0),
        media::VideoDecodePerfHistory::SaveCallback(),
        media::MediaMetricsProvider::GetLearningSessionCallback(),
        WTF::BindRepeating(&WebMediaPlayerImplTest::IsShuttingDown,
                           WTF::Unretained(this)),
        provider.BindNewPipeAndPassReceiver());

    // Initialize provider since none of the tests below actually go through the
    // full loading/pipeline initialize phase. If this ever changes the provider
    // will start DCHECK failing.
    provider->Initialize(false, media::mojom::MediaURLScheme::kHttp,
                         media::mojom::MediaStreamType::kNone);

    audio_sink_ =
        base::WrapRefCounted(new NiceMock<media::MockAudioRendererSink>());

    url_index_ = std::make_unique<UrlIndex>(&mock_resource_fetch_context_,
                                            media_thread_.task_runner());

    auto compositor = std::make_unique<NiceMock<MockVideoFrameCompositor>>(
        media_thread_.task_runner());
    compositor_ = compositor.get();

    wmpi_ = std::make_unique<WebMediaPlayerImpl>(
        GetWebLocalFrame(), &client_, &encrypted_client_, &delegate_,
        std::move(factory_selector), url_index_.get(), std::move(compositor),
        std::move(media_log), player_id, WebMediaPlayerBuilder::DeferLoadCB(),
        audio_sink_, media_thread_.task_runner(), media_thread_.task_runner(),
        media_thread_.task_runner(), media_thread_.task_runner(), nullptr,
        media::RequestRoutingTokenCallback(), mock_observer_.AsWeakPtr(), false,
        false, provider.Unbind(),
        WTF::BindOnce(&WebMediaPlayerImplTest::CreateMockSurfaceLayerBridge,
                      base::Unretained(this)),
        viz::TestContextProvider::Create(),
        /*use_surface_layer=*/true, is_background_suspend_enabled_,
        is_background_video_playback_enabled_, true,
        std::move(demuxer_override), nullptr);
  }

  std::unique_ptr<WebSurfaceLayerBridge> CreateMockSurfaceLayerBridge(
      WebSurfaceLayerBridgeObserver*,
      cc::UpdateSubmissionStateCB) {
    return std::move(surface_layer_bridge_);
  }

  WebLocalFrame* GetWebLocalFrame() {
    return web_view_helper_.LocalMainFrame();
  }

  void SetNetworkState(WebMediaPlayer::NetworkState state) {
    EXPECT_CALL(client_, NetworkStateChanged());
    wmpi_->SetNetworkState(state);
  }

  void SetReadyState(WebMediaPlayer::ReadyState state) {
    EXPECT_CALL(client_, ReadyStateChanged());
    wmpi_->SetReadyState(state);
  }

  void SetDuration(base::TimeDelta value) {
    wmpi_->SetPipelineMediaDurationForTest(value);
    wmpi_->OnDurationChange();
  }

  MOCK_METHOD(bool, IsShuttingDown, ());

  base::TimeDelta GetCurrentTimeInternal() {
    return wmpi_->GetCurrentTimeInternal();
  }

  void SetPaused(bool is_paused) { wmpi_->paused_ = is_paused; }
  void SetSeeking(bool is_seeking) { wmpi_->seeking_ = is_seeking; }
  void SetEnded(bool is_ended) { wmpi_->ended_ = is_ended; }
  void SetTickClock(const base::TickClock* clock) {
    wmpi_->SetTickClockForTest(clock);
  }
  void SetWasSuspendedForFrameClosed(bool is_suspended) {
    wmpi_->was_suspended_for_frame_closed_ = is_suspended;
  }

  void SetFullscreen(bool is_fullscreen) {
    wmpi_->overlay_enabled_ = is_fullscreen;
    wmpi_->overlay_info_.is_fullscreen = is_fullscreen;
  }

  void SetMetadata(bool has_audio, bool has_video) {
    wmpi_->SetNetworkState(WebMediaPlayer::kNetworkStateLoaded);

    EXPECT_CALL(client_, ReadyStateChanged());
    wmpi_->SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
    wmpi_->pipeline_metadata_.has_audio = has_audio;
    wmpi_->pipeline_metadata_.has_video = has_video;

    if (has_video) {
      wmpi_->pipeline_metadata_.video_decoder_config =
          TestVideoConfig::Normal();
    }

    if (has_audio) {
      wmpi_->pipeline_metadata_.audio_decoder_config =
          TestAudioConfig::Normal();
    }
  }

  void SetError(media::PipelineStatus status = media::PIPELINE_ERROR_DECODE) {
    wmpi_->OnError(status);
  }

  void OnMetadata(const media::PipelineMetadata& metadata) {
    wmpi_->OnMetadata(metadata);
  }

  void OnWaiting(media::WaitingReason reason) { wmpi_->OnWaiting(reason); }

  void OnVideoNaturalSizeChange(const gfx::Size& size) {
    wmpi_->OnVideoNaturalSizeChange(size);
  }

  void OnVideoConfigChange(const media::VideoDecoderConfig& config) {
    wmpi_->OnVideoConfigChange(config);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, false,
                                                   false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_FrameHidden() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, true,
                                                   false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Suspended() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, true, false,
                                                   false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_Flinging() {
    return wmpi_->UpdatePlayState_ComputePlayState(true, true, false, false,
                                                   false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_BackgroundedStreaming() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, true,
                                                   false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState_FrameHiddenPictureInPicture() {
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, true,
                                                   true);
  }

  bool IsSuspended() { return wmpi_->pipeline_controller_->IsSuspended(); }

  bool IsStreaming() const { return wmpi_->IsStreaming(); }

  int64_t GetDataSourceMemoryUsage() const {
    return wmpi_->demuxer_manager_->GetDataSourceMemoryUsage();
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

  bool IsPausedBecausePageHidden() const {
    return wmpi_->IsPausedBecausePageHidden();
  }

  bool IsPausedBecauseFrameHidden() const {
    return wmpi_->IsPausedBecauseFrameHidden();
  }

  void HidePlayerPage() {
    base::RunLoop loop;
    EXPECT_CALL(*compositor_, SetIsPageVisible(false))
        .WillOnce(RunClosure(loop.QuitClosure()));

    delegate_.SetPageHiddenForTesting(true);
    SetWasSuspendedForFrameClosed(false);

    wmpi_->OnPageHidden();

    loop.Run();

    // Clear the mock so it doesn't have a stale QuitClosure.
    testing::Mock::VerifyAndClearExpectations(compositor_);
  }

  void ShowPlayerPage() {
    base::RunLoop loop;
    EXPECT_CALL(*compositor_, SetIsPageVisible(true))
        .WillOnce(RunClosure(loop.QuitClosure()));

    delegate_.SetPageHiddenForTesting(false);
    SetWasSuspendedForFrameClosed(false);

    wmpi_->OnPageShown();

    loop.Run();

    // Clear the mock so it doesn't have a stale QuitClosure.
    testing::Mock::VerifyAndClearExpectations(compositor_);
  }

  void HidePlayerFrame() {
    delegate_.SetFrameHiddenForTesting(true);
    SetWasSuspendedForFrameClosed(false);
    wmpi_->OnFrameHidden();
  }

  void ShowPlayerFrame() {
    delegate_.SetFrameHiddenForTesting(false);
    SetWasSuspendedForFrameClosed(false);
    wmpi_->OnFrameShown();
  }

  void BackgroundPlayer(BackgroundBehaviorType type) {
    switch (type) {
      case BackgroundBehaviorType::Page:
        HidePlayerPage();
        return;
      case BackgroundBehaviorType::Frame:
        HidePlayerFrame();
        return;
    }

    NOTREACHED();
  }

  void ForegroundPlayer(BackgroundBehaviorType type) {
    switch (type) {
      case BackgroundBehaviorType::Page:
        ShowPlayerPage();
        return;
      case BackgroundBehaviorType::Frame:
        ShowPlayerFrame();
        return;
    }

    NOTREACHED();
  }

  void Play() { wmpi_->Play(); }

  void Pause() { wmpi_->Pause(); }

  void ScheduleIdlePauseTimer() { wmpi_->ScheduleIdlePauseTimer(); }
  void FireIdlePauseTimer() { wmpi_->background_pause_timer_.FireNow(); }

  bool IsIdlePauseTimerRunning() {
    return wmpi_->background_pause_timer_.IsRunning();
  }

  void SetSuspendState(bool is_suspended) {
    wmpi_->SetSuspendState(is_suspended);
  }

  void SetLoadType(WebMediaPlayer::LoadType load_type) {
    wmpi_->load_type_ = load_type;
  }

  bool IsVideoTrackDisabled() const { return wmpi_->video_track_disabled_; }

  bool IsDisableVideoTrackPending() const {
    return !wmpi_->is_background_status_change_cancelled_;
  }

  gfx::Size GetNaturalSize() const {
    return wmpi_->pipeline_metadata_.natural_size;
  }

  VideoDecodeStatsReporter* GetVideoStatsReporter() const {
    return wmpi_->video_decode_stats_reporter_.get();
  }

  media::VideoCodecProfile GetVideoStatsReporterCodecProfile() const {
    DCHECK(GetVideoStatsReporter());
    return GetVideoStatsReporter()->codec_profile_;
  }

  bool ShouldCancelUponDefer() const {
    auto* ds = wmpi_->demuxer_manager_->GetDataSourceForTesting();
    CHECK_NE(ds, nullptr);
    CHECK_NE(ds->GetAsCrossOriginDataSource(), nullptr);
    // Right now, the only implementation of DataSource that WMPI can get
    // which returns non-null from GetAsCrossOriginDataSource is
    // MultiBufferDataSource, so the CHECKs above allow us to be safe casting
    // this here.
    // TODO(crbug/1377053): Can we add |cancel_on_defer_for_testing| to
    // CrossOriginDataSource? We can't do a |GetAsMultiBufferDataSource| since
    // MBDS is in blink, and we can't import that into media.
    return static_cast<const MultiBufferDataSource*>(ds)
        ->cancel_on_defer_for_testing();
  }

  bool IsDataSourceMarkedAsPlaying() const {
    auto* ds = wmpi_->demuxer_manager_->GetDataSourceForTesting();
    CHECK_NE(ds, nullptr);
    CHECK_NE(ds->GetAsCrossOriginDataSource(), nullptr);
    // See comment in |ShouldCancelUponDefer|.
    return static_cast<const MultiBufferDataSource*>(ds)->media_has_played();
  }

  scoped_refptr<media::VideoFrame> CreateFrame() {
    gfx::Size size(8, 8);
    return media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_I420, size,
                                          gfx::Rect(size), size,
                                          base::TimeDelta());
  }

  void RequestVideoFrameCallback() { wmpi_->RequestVideoFrameCallback(); }
  void GetVideoFramePresentationMetadata() {
    wmpi_->GetVideoFramePresentationMetadata();
  }
  void UpdateFrameIfStale() { wmpi_->UpdateFrameIfStale(); }

  void OnNewFramePresentedCallback() { wmpi_->OnNewFramePresentedCallback(); }

  scoped_refptr<media::VideoFrame> GetCurrentFrameFromCompositor() {
    return wmpi_->GetCurrentFrameFromCompositor();
  }

  enum class LoadType { kFullyBuffered, kStreaming };
  void Load(std::string data_file,
            LoadType load_type = LoadType::kFullyBuffered) {
    const bool is_streaming = load_type == LoadType::kStreaming;

    // The URL is used by MultiBufferDataSource to determine if it should assume
    // the resource is fully buffered locally. We can use a fake one here since
    // we're injecting the response artificially. It's value is unknown to the
    // underlying demuxer.
    const KURL kTestURL(
        String::FromUTF8(std::string(is_streaming ? "http" : "file") +
                         "://example.com/sample.webm"));

    // This block sets up a fetch context which ultimately provides us a pointer
    // to the WebAssociatedURLLoaderClient handed out by the DataSource after it
    // requests loading of a resource. We then use that client as if we are the
    // network stack and "serve" an in memory file to the DataSource.
    const bool should_have_client =
        !wmpi_->demuxer_manager_->HasDemuxerOverride();
    WebAssociatedURLLoaderClient* client = nullptr;
    if (should_have_client) {
      EXPECT_CALL(mock_resource_fetch_context_, CreateUrlLoader(_))
          .WillRepeatedly(
              Invoke([&client](const WebAssociatedURLLoaderOptions&) {
                auto a =
                    std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
                EXPECT_CALL(*a, LoadAsynchronously(_, _))
                    .WillRepeatedly(testing::SaveArg<1>(&client));
                return a;
              }));
    }

    wmpi_->Load(WebMediaPlayer::kLoadTypeURL,
                WebMediaPlayerSource(WebURL(kTestURL)),
                WebMediaPlayer::kCorsModeUnspecified,
                /*is_cache_disabled=*/false);

    base::RunLoop().RunUntilIdle();
    if (!should_have_client) {
      return;
    }
    EXPECT_TRUE(client);

    // Load a real media file into memory.
    scoped_refptr<media::DecoderBuffer> data =
        media::ReadTestDataFile(data_file);

    // "Serve" the file to the DataSource. Note: We respond with 200 okay, which
    // will prevent range requests or partial responses from being used. For
    // streaming responses, we'll pretend we don't know the content length.
    WebURLResponse response(kTestURL);
    response.SetHttpHeaderField(
        WebString::FromUTF8("Content-Length"),
        WebString::FromUTF8(is_streaming ? "-1"
                                         : base::NumberToString(data->size())));
    response.SetExpectedContentLength(is_streaming ? -1 : data->size());
    response.SetHttpStatusCode(200);
    client->DidReceiveResponse(response);

    // Copy over the file data.
    client->DidReceiveData(base::as_chars(data->AsSpan()));

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
                                WebMediaPlayer::ReadyState ready_state) {
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
    EXPECT_TRUE(wmpi_->demuxer_manager_->HasDataSource());
    EXPECT_TRUE(wmpi_->demuxer_manager_->HasDemuxer());

    if (ready_state > WebMediaPlayer::kReadyStateHaveCurrentData)
      EXPECT_FALSE(wmpi_->seeking_);
  }

  void LoadAndWaitForCurrentData(std::string data_file) {
    LoadAndWaitForReadyState(data_file,
                             WebMediaPlayer::kReadyStateHaveCurrentData);
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
                    std::unique_ptr<WebContentDecryptionModule> cdm,
                    media::CreateCdmStatus status) {
    LOG_IF(ERROR, status != media::CreateCdmStatus::kSuccess)
        << "status = " << static_cast<int>(status);
    EXPECT_TRUE(cdm);
    web_cdm_ = std::move(cdm);
    quit_closure.Run();
  }

  void CreateCdm() {
    // Must use a supported key system on a secure context.
    media::CdmConfig cdm_config = {media::kClearKeyKeySystem, false, false,
                                   false};
    auto test_origin = WebSecurityOrigin::CreateFromString(
        WebString::FromUTF8("https://test.origin"));

    if (!key_systems_) {
      key_systems_ =
          std::make_unique<media::KeySystemsImpl>(base::NullCallback());
    }
    base::RunLoop run_loop;
    WebContentDecryptionModuleImpl::Create(
        &mock_cdm_factory_, key_systems_.get(), test_origin, cdm_config,
        WTF::BindOnce(&WebMediaPlayerImplTest::OnCdmCreated,
                      WTF::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(web_cdm_);
  }

  void SetCdm() {
    DCHECK(web_cdm_);
    EXPECT_CALL(*mock_cdm_, GetCdmContext())
        .WillRepeatedly(Return(&mock_cdm_context_));
    wmpi_->SetCdmInternal(web_cdm_.get());
  }

  media::MemoryDumpProviderProxy* GetMainThreadMemDumper() {
    return wmpi_->main_thread_mem_dumper_.get();
  }
  media::MemoryDumpProviderProxy* GetMediaThreadMemDumper() {
    return wmpi_->media_thread_mem_dumper_.get();
  }

  test::TaskEnvironment task_environment_;

  // "Media" thread. This is necessary because WMPI destruction waits on a
  // WaitableEvent.
  base::Thread media_thread_;

  // Blink state.
  frame_test_helpers::WebViewHelper web_view_helper_;

  scoped_refptr<viz::TestContextProvider> context_provider_;
  NiceMock<MockVideoFrameCompositor>* compositor_;

  scoped_refptr<NiceMock<media::MockAudioRendererSink>> audio_sink_;
  MockResourceFetchContext mock_resource_fetch_context_;
  std::unique_ptr<UrlIndex> url_index_;

  // Audio hardware configuration.
  media::AudioParameters audio_parameters_;

  bool is_background_suspend_enabled_ = false;
  bool is_background_video_playback_enabled_ = true;

  // The client interface used by |wmpi_|.
  NiceMock<MockWebMediaPlayerClient> client_;
  MockWebMediaPlayerEncryptedMediaClient encrypted_client_;

  std::unique_ptr<media::KeySystemsImpl> key_systems_;

  // Used to create the media::MockCdm to test encrypted playback.
  scoped_refptr<media::MockCdm> mock_cdm_ =
      base::MakeRefCounted<media::MockCdm>();
  media::MockCdmFactory mock_cdm_factory_{mock_cdm_};
  std::unique_ptr<WebContentDecryptionModule> web_cdm_;
  media::MockCdmContext mock_cdm_context_;

  viz::FrameSinkId frame_sink_id_ = viz::FrameSinkId(1, 1);
  viz::LocalSurfaceId local_surface_id_ = viz::LocalSurfaceId(
      11,
      base::UnguessableToken::CreateForTesting(0x111111, 0));
  viz::SurfaceId surface_id_ =
      viz::SurfaceId(frame_sink_id_, local_surface_id_);

  NiceMock<MockWebMediaPlayerDelegate> delegate_;

  // Use NiceMock since most tests do not care about this.
  std::unique_ptr<NiceMock<MockSurfaceLayerBridge>> surface_layer_bridge_;
  NiceMock<MockSurfaceLayerBridge>* surface_layer_bridge_ptr_ = nullptr;

  // Only valid once set by InitializeWebMediaPlayerImpl(), this is for
  // verifying a subset of potential media logs.
  NiceMock<media::MockMediaLog>* media_log_ = nullptr;

  // Raw pointer of the media::RendererFactorySelector owned by |wmpi_|.
  media::RendererFactorySelector* renderer_factory_selector_ = nullptr;

  // default decoder factory for WMPI
  std::unique_ptr<media::DecoderFactory> decoder_factory_;

  // The WebMediaPlayerImpl's media observer.
  NiceMock<MockMediaObserver> mock_observer_;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;

  std::unique_ptr<base::trace_event::MemoryDumpManager> memory_dump_manager_;
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
}

// Verify LoadAndWaitForCurrentData() functions without issue.
TEST_F(WebMediaPlayerImplTest, LoadAndDestroy) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);
  LoadAndWaitForCurrentData(kAudioOnlyTestFile);
  EXPECT_FALSE(IsSuspended());
  CycleThreads();

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure we're getting audio buffer and demuxer usage too.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_GT(
      task_environment_.isolate()->AdjustAmountOfExternalAllocatedMemory(0),
      data_source_size);
}

// Verify LoadAndWaitForCurrentData() functions without issue.
TEST_F(WebMediaPlayerImplTest, LoadAndDestroyDataUrl) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);

  const KURL kMp3DataUrl(
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

  wmpi_->Load(WebMediaPlayer::kLoadTypeURL,
              WebMediaPlayerSource(WebURL(kMp3DataUrl)),
              WebMediaPlayer::kCorsModeUnspecified,
              /*is_cache_disabled=*/false);

  base::RunLoop().RunUntilIdle();

  // This runs until we reach the have current data state. Attempting to wait
  // for states < kReadyStateHaveCurrentData is unreliable due to asynchronous
  // execution of tasks on the base::test:TaskEnvironment.
  while (wmpi_->GetReadyState() < WebMediaPlayer::kReadyStateHaveCurrentData) {
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
  wmpi_->SetPreload(WebMediaPlayer::kPreloadMetaData);
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveMetadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_TRUE(IsSuspended());
  EXPECT_TRUE(ShouldCancelUponDefer());

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure there's no other memory usage.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_EQ(
      task_environment_.isolate()->AdjustAmountOfExternalAllocatedMemory(0),
      data_source_size);
}

// Verify that Play() before kReadyStateHaveEnough doesn't increase buffer size.
TEST_F(WebMediaPlayerImplTest, NoBufferSizeIncreaseUntilHaveEnough) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(true));
  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveMetadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  wmpi_->Play();
  EXPECT_FALSE(IsDataSourceMarkedAsPlaying());

  while (wmpi_->GetReadyState() < WebMediaPlayer::kReadyStateHaveEnoughData) {
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
  wmpi_->SetPreload(WebMediaPlayer::kPreloadMetaData);

  // This test needs a file which is larger than the MultiBuffer block size;
  // otherwise we'll never complete initialization of the MultiBufferDataSource.
  constexpr char kLargeAudioOnlyTestFile[] = "bear_192kHz.wav";
  Load(kLargeAudioOnlyTestFile, LoadType::kStreaming);

  // This runs until we reach the metadata state.
  while (wmpi_->GetReadyState() < WebMediaPlayer::kReadyStateHaveMetadata) {
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
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(WebMediaPlayer::kPreloadMetaData);

  // Don't set poster, but ensure we still reach suspended state.

  LoadAndWaitForReadyState(kVideoOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveMetadata);
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
  EXPECT_EQ(
      task_environment_.isolate()->AdjustAmountOfExternalAllocatedMemory(0),
      data_source_size);

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

// Verify that lazy load is skipped when rVFC has been requested.
TEST_F(WebMediaPlayerImplTest, LazyLoadSkippedForRVFC) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(WebMediaPlayer::kPreloadMetaData);

  EXPECT_CALL(*compositor_, SetOnFramePresentedCallback(_));
  RequestVideoFrameCallback();

  // Ensure we don't reach the suspended state.
  LoadAndWaitForReadyState(kVideoOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveMetadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_FALSE(IsSuspended());
  EXPECT_FALSE(wmpi_->DidLazyLoad());
}

// Verify that preload=metadata suspend video w/ poster uses zero video memory.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspendNoVideoMemoryUsage) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(false));
  wmpi_->SetPreload(WebMediaPlayer::kPreloadMetaData);
  wmpi_->SetPoster(WebURL(KURL("file://example.com/sample.jpg")));

  LoadAndWaitForReadyState(kVideoOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveMetadata);
  testing::Mock::VerifyAndClearExpectations(&client_);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(AnyNumber());
  CycleThreads();
  EXPECT_TRUE(IsSuspended());

  // The data source contains the entire file, so subtract it from the memory
  // usage to ensure there's no other memory usage.
  const int64_t data_source_size = GetDataSourceMemoryUsage();
  EXPECT_GT(data_source_size, 0);
  EXPECT_EQ(
      task_environment_.isolate()->AdjustAmountOfExternalAllocatedMemory(0),
      data_source_size);

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

// Verify that preload=metadata suspend is aborted if we know the element will
// play as soon as we reach kReadyStateHaveFutureData.
TEST_F(WebMediaPlayerImplTest, LoadPreloadMetadataSuspendCouldPlay) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(client_, CouldPlayIfEnoughData()).WillRepeatedly(Return(true));
  wmpi_->SetPreload(WebMediaPlayer::kPreloadMetaData);
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
  clock.Advance(base::Seconds(1));
  SetTickClock(&clock);
  AddBufferedRanges();
  wmpi_->DidLoadingProgress();
  // Advance less than the loading timeout.
  clock.Advance(base::Seconds(1));
  EXPECT_FALSE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendIsEnabledIfLoadingHasStalled) {
  InitializeWebMediaPlayerImpl();
  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  base::SimpleTestTickClock clock;
  clock.Advance(base::Seconds(1));
  SetTickClock(&clock);
  AddBufferedRanges();
  wmpi_->DidLoadingProgress();
  // Advance more than the loading timeout.
  clock.Advance(base::Seconds(4));
  EXPECT_TRUE(delegate_.ExpireForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, DidLoadingProgressTriggersResume) {
  // Same setup as IdleSuspendIsEnabledBeforeLoadingBegins.
  InitializeWebMediaPlayerImpl();
  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  SetReadyState(WebMediaPlayer::kReadyStateHaveCurrentData);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHidden) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);

  SetMetadata(true, false);
  WebMediaPlayerImpl::PlayState state = ComputePlayState_FrameHidden();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_FALSE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHiddenVideoOnly) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(false, true);
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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

  state = ComputePlayState_FrameHiddenPictureInPicture();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_FALSE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_FrameHiddenSuspendNoResume) {
  SetUpMediaSuspend(true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(media::kResumeBackgroundVideo);

  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  scoped_feature_list.InitAndEnableFeature(media::kResumeBackgroundVideo);

  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
  SetPaused(false);
  SetWasSuspendedForFrameClosed(true);
  WebMediaPlayerImpl::PlayState state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_PausedSeek) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);

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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);

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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);

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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);

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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
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
  SetWasSuspendedForFrameClosed(true);
  state = ComputePlayState_BackgroundedStreaming();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_TRUE(state.is_idle);
  EXPECT_TRUE(state.is_suspended);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
}

TEST_F(WebMediaPlayerImplTest, IsStreamingIfDemuxerDoesntSupportSeeking) {
  std::unique_ptr<media::MockDemuxer> demuxer =
      std::make_unique<NiceMock<media::MockDemuxer>>();
  ON_CALL(*demuxer, IsSeekable()).WillByDefault(Return(false));
  InitializeWebMediaPlayerImpl(std::move(demuxer));
  Load(kVideoOnlyTestFile);
  EXPECT_TRUE(IsStreaming());
}

TEST_F(WebMediaPlayerImplTest, IsNotStreamingIfDemuxerSupportsSeeking) {
  std::unique_ptr<media::MockDemuxer> demuxer =
      std::make_unique<NiceMock<media::MockDemuxer>>();
  ON_CALL(*demuxer, IsSeekable()).WillByDefault(Return(true));
  InitializeWebMediaPlayerImpl(std::move(demuxer));
  Load(kVideoOnlyTestFile);
  EXPECT_FALSE(IsStreaming());
}

TEST_F(WebMediaPlayerImplTest, ResumeEnded) {
  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  metadata.has_audio = true;
  metadata.audio_decoder_config = TestAudioConfig::Normal();

  SetUpMediaSuspend(true);
  InitializeWebMediaPlayerImpl();

  EXPECT_CALL(delegate_, DidMediaMetadataChange(_, true, true, _)).Times(2);

  OnMetadata(metadata);
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
  Play();
  // Cause PlayerGone
  Pause();
  BackgroundPlayer(BackgroundBehaviorType::Page);

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // DidMediaMetadataChange should be called again after player gone.
  EXPECT_CALL(delegate_, DidMediaMetadataChange(_, true, true, _));

  ForegroundPlayer(BackgroundBehaviorType::Page);
  Play();
}

TEST_F(WebMediaPlayerImplTest, AutoplayMuted) {
  media::PipelineMetadata metadata;
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
                           WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(1.0);
  Play();

  EXPECT_CALL(client_, DidPlayerMediaPositionStateChange(
                           1.0, kAudioOnlyTestFileDuration, base::TimeDelta(),
                           /*end_of_media=*/false));
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, MediaPositionState_Paused) {
  InitializeWebMediaPlayerImpl();
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(1.0);

  // The effective playback rate is 0.0 while paused.
  EXPECT_CALL(client_, DidPlayerMediaPositionStateChange(
                           0.0, kAudioOnlyTestFileDuration, base::TimeDelta(),
                           /*end_of_media=*/false));
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, MediaPositionState_PositionChange) {
  InitializeWebMediaPlayerImpl();
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(0.5);
  Play();

  testing::Sequence sequence;
  EXPECT_CALL(client_, DidPlayerMediaPositionStateChange(
                           0.0, kAudioOnlyTestFileDuration, base::Seconds(0.1),
                           /*end_of_media=*/false))
      .InSequence(sequence);
  wmpi_->Seek(0.1);
  wmpi_->OnTimeUpdate();

  // If we load enough data to resume playback the position should be updated.
  EXPECT_CALL(client_, DidPlayerMediaPositionStateChange(
                           0.5, kAudioOnlyTestFileDuration, base::Seconds(0.1),
                           /*end_of_media=*/false))
      .InSequence(sequence);
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->OnTimeUpdate();

  // No media time progress -> no MediaPositionState change.
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, MediaPositionState_EndOfMedia) {
  InitializeWebMediaPlayerImpl();
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(1.0);
  Play();
  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);

  testing::Sequence sequence;
  EXPECT_CALL(client_, DidPlayerMediaPositionStateChange(
                           1.0, kAudioOnlyTestFileDuration, base::TimeDelta(),
                           /*end_of_media=*/false))
      .InSequence(sequence);
  wmpi_->OnTimeUpdate();

  // If we play through to the end of media the position should be updated.
  EXPECT_CALL(client_, DidPlayerMediaPositionStateChange(
                           1.0, kAudioOnlyTestFileDuration, base::TimeDelta(),
                           /*end_of_media=*/true))
      .InSequence(sequence);
  SetEnded(true);
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, MediaPositionState_Underflow) {
  InitializeWebMediaPlayerImpl();
  LoadAndWaitForReadyState(kAudioOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(1.0);
  Play();

  // Underflow will set the effective playback rate to 0.0.
  EXPECT_CALL(client_, DidPlayerMediaPositionStateChange(
                           0.0, kAudioOnlyTestFileDuration, base::TimeDelta(),
                           /*end_of_media=*/false));
  SetReadyState(WebMediaPlayer::kReadyStateHaveCurrentData);
  wmpi_->OnTimeUpdate();
}

// It's possible for current time to be infinite if the page seeks to
// |media::kInfiniteDuration| (2**64 - 1) when duration is infinite.
TEST_F(WebMediaPlayerImplTest, MediaPositionState_InfiniteCurrentTime) {
  InitializeWebMediaPlayerImpl();
  SetDuration(media::kInfiniteDuration);
  wmpi_->OnTimeUpdate();

  EXPECT_CALL(client_,
              DidPlayerMediaPositionStateChange(0.0, media::kInfiniteDuration,
                                                media::kInfiniteDuration,
                                                /*end_of_media=*/false));
  wmpi_->Seek(media::kInfiniteDuration.InSecondsF());
  wmpi_->OnTimeUpdate();

  testing::Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, DidPlayerMediaPositionStateChange(_, _, _, _)).Times(0);
  wmpi_->OnTimeUpdate();
}

TEST_F(WebMediaPlayerImplTest, NoStreams) {
  InitializeWebMediaPlayerImpl();
  media::PipelineMetadata metadata;

  EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
  EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer()).Times(0);
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId()).Times(0);
  EXPECT_CALL(*compositor_, EnableSubmission(_, _, _)).Times(0);

  // Since there is no audio nor video to play, OnError should occur with
  // resulting network state error update, and transition to HAVE_METADATA
  // should not occur.
  EXPECT_CALL(client_, NetworkStateChanged()).Times(1);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(0);

  // No assertions in the production code should fail.
  OnMetadata(metadata);

  EXPECT_EQ(wmpi_->GetNetworkState(), WebMediaPlayer::kNetworkStateFormatError);
  EXPECT_EQ(wmpi_->GetReadyState(), WebMediaPlayer::kReadyStateHaveNothing);
}

TEST_F(WebMediaPlayerImplTest, Encrypted) {
  InitializeWebMediaPlayerImpl();

  // To avoid PreloadMetadataLazyLoad.
  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(encrypted_client_,
                Encrypted(media::EmeInitDataType::WEBM, NotNull(), Gt(0u)));
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

  {
    // Wait for kNetworkStateFormatError caused by Renderer initialization
    // error.
    base::RunLoop run_loop;
    EXPECT_CALL(client_, NetworkStateChanged()).WillOnce(Invoke([&] {
      if (wmpi_->GetNetworkState() == WebMediaPlayer::kNetworkStateFormatError)
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

  OnWaiting(media::WaitingReason::kNoDecryptionKey);
}

TEST_F(WebMediaPlayerImplTest, Waiting_SecureSurfaceLost) {
  InitializeWebMediaPlayerImpl();

  LoadAndWaitForReadyState(kVideoOnlyTestFile,
                           WebMediaPlayer::kReadyStateHaveFutureData);
  wmpi_->SetRate(1.0);
  Play();

  EXPECT_FALSE(IsSuspended());

  OnWaiting(media::WaitingReason::kSecureSurfaceLost);
  EXPECT_TRUE(IsSuspended());
}

ACTION(ReportHaveEnough) {
  arg0->OnBufferingStateChange(media::BUFFERING_HAVE_ENOUGH,
                               media::BUFFERING_CHANGE_REASON_UNKNOWN);
}

ACTION(ReportHardwareContextReset) {
  arg0->OnError(media::PIPELINE_ERROR_HARDWARE_CONTEXT_RESET);
}

#if BUILDFLAG(IS_WIN)

// Tests that for encrypted media, when a CDM is attached that requires
// MediaFoundationRenderer, the pipeline will fallback to create a new Renderer
// for RendererType::kMediaFoundation.
TEST_F(WebMediaPlayerImplTest, FallbackToMediaFoundationRenderer) {
  InitializeWebMediaPlayerImpl();
  // To avoid PreloadMetadataLazyLoad.
  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);

  // Use MockRendererFactory for kMediaFoundation where the created Renderer
  // will take the CDM, complete Renderer initialization and report HAVE_ENOUGH
  // so that WMPI can reach kReadyStateHaveCurrentData.
  auto mock_renderer_factory = std::make_unique<media::MockRendererFactory>();
  EXPECT_CALL(*mock_renderer_factory, CreateRenderer(_, _, _, _, _, _))
      .WillOnce(testing::WithoutArgs(Invoke([]() {
        auto mock_renderer = std::make_unique<NiceMock<media::MockRenderer>>();
        EXPECT_CALL(*mock_renderer, OnSetCdm(_, _))
            .WillOnce(RunOnceCallback<1>(true));
        EXPECT_CALL(*mock_renderer, OnInitialize(_, _, _))
            .WillOnce(DoAll(RunOnceCallback<2>(media::PIPELINE_OK),
                            WithArg<1>(ReportHaveEnough())));
        return mock_renderer;
      })));

  renderer_factory_selector_->AddFactory(media::RendererType::kMediaFoundation,
                                         std::move(mock_renderer_factory));

  // Create and set CDM. The CDM doesn't support a Decryptor and requires Media
  // Foundation Renderer.
  EXPECT_CALL(mock_cdm_context_, GetDecryptor())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_cdm_context_, RequiresMediaFoundationRenderer())
      .WillRepeatedly(Return(true));

  CreateCdm();
  SetCdm();

  // Load encrypted media and expect encrypted event.
  EXPECT_CALL(encrypted_client_,
              Encrypted(media::EmeInitDataType::WEBM, NotNull(), Gt(0u)));

  base::RunLoop run_loop;
  // MediaFoundationRenderer doesn't use AudioService.
  EXPECT_CALL(client_, DidUseAudioServiceChange(/*uses_audio_service=*/false))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  Load(kEncryptedVideoOnlyTestFile);
  run_loop.Run();
}

// Tests that when PIPELINE_ERROR_HARDWARE_CONTEXT_RESET happens, the pipeline
// will suspend/resume the pipeline, which will create a new Renderer.
TEST_F(WebMediaPlayerImplTest, PipelineErrorHardwareContextReset) {
  InitializeWebMediaPlayerImpl();
  // To avoid PreloadMetadataLazyLoad.
  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);

  base::RunLoop run_loop;

  // Use MockRendererFactory which will create two Renderers. The first will
  // report a PIPELINE_ERROR_HARDWARE_CONTEXT_RESET after initialization. The
  // second one will initialize normally and quit the loop to complete the test.
  auto mock_renderer_factory = std::make_unique<media::MockRendererFactory>();
  EXPECT_CALL(*mock_renderer_factory, CreateRenderer(_, _, _, _, _, _))
      .WillOnce(testing::WithoutArgs(Invoke([]() {
        auto mock_renderer = std::make_unique<NiceMock<media::MockRenderer>>();
        EXPECT_CALL(*mock_renderer, OnInitialize(_, _, _))
            .WillOnce(DoAll(RunOnceCallback<2>(media::PIPELINE_OK),
                            WithArg<1>(ReportHardwareContextReset())));
        return mock_renderer;
      })))
      .WillOnce(testing::WithoutArgs(Invoke([&]() {
        auto mock_renderer = std::make_unique<NiceMock<media::MockRenderer>>();
        EXPECT_CALL(*mock_renderer, OnInitialize(_, _, _))
            .WillOnce(DoAll(RunOnceCallback<2>(media::PIPELINE_OK),
                            RunClosure(run_loop.QuitClosure())));
        return mock_renderer;
      })));

  renderer_factory_selector_->AddFactory(media::RendererType::kTest,
                                         std::move(mock_renderer_factory));
  renderer_factory_selector_->SetBaseRendererType(media::RendererType::kTest);

  Load(kVideoOnlyTestFile);
  run_loop.Run();
}

// Same as above, but tests that when PIPELINE_ERROR_HARDWARE_CONTEXT_RESET
// happens twice, the pipeline will always suspend/resume the pipeline, which
// will create new Renderers. See https://crbug.com/1454226 for the context.
TEST_F(WebMediaPlayerImplTest, PipelineErrorHardwareContextReset_Twice) {
  InitializeWebMediaPlayerImpl();
  // To avoid PreloadMetadataLazyLoad.
  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);

  base::RunLoop run_loop;

  // Use MockRendererFactory which will create three Renderers. The first two
  // will report a PIPELINE_ERROR_HARDWARE_CONTEXT_RESET after initialization.
  // The third one will initialize normally and quit the loop to complete the
  // test.
  auto mock_renderer_factory = std::make_unique<media::MockRendererFactory>();
  EXPECT_CALL(*mock_renderer_factory, CreateRenderer(_, _, _, _, _, _))
      .WillOnce(testing::WithoutArgs(Invoke([]() {
        auto mock_renderer = std::make_unique<NiceMock<media::MockRenderer>>();
        EXPECT_CALL(*mock_renderer, OnInitialize(_, _, _))
            .WillOnce(DoAll(RunOnceCallback<2>(media::PIPELINE_OK),
                            WithArg<1>(ReportHardwareContextReset())));
        return mock_renderer;
      })))
      .WillOnce(testing::WithoutArgs(Invoke([]() {
        auto mock_renderer = std::make_unique<NiceMock<media::MockRenderer>>();
        EXPECT_CALL(*mock_renderer, OnInitialize(_, _, _))
            .WillOnce(DoAll(RunOnceCallback<2>(media::PIPELINE_OK),
                            WithArg<1>(ReportHardwareContextReset())));
        return mock_renderer;
      })))
      .WillOnce(testing::WithoutArgs(Invoke([&]() {
        auto mock_renderer = std::make_unique<NiceMock<media::MockRenderer>>();
        EXPECT_CALL(*mock_renderer, OnInitialize(_, _, _))
            .WillOnce(DoAll(RunOnceCallback<2>(media::PIPELINE_OK),
                            RunClosure(run_loop.QuitClosure())));
        return mock_renderer;
      })));

  renderer_factory_selector_->AddFactory(media::RendererType::kTest,
                                         std::move(mock_renderer_factory));
  renderer_factory_selector_->SetBaseRendererType(media::RendererType::kTest);

  Load(kVideoOnlyTestFile);
  run_loop.Run();
}

#endif  // BUILDFLAG(IS_WIN)

TEST_F(WebMediaPlayerImplTest, VideoConfigChange) {
  InitializeWebMediaPlayerImpl();
  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::NormalCodecProfile(
      media::VideoCodec::kVP9, media::VP9PROFILE_PROFILE0);
  metadata.natural_size = gfx::Size(320, 240);

  // Arrival of metadata should trigger creation of reporter with video config
  // with profile matching test config.
  OnMetadata(metadata);
  VideoDecodeStatsReporter* last_reporter = GetVideoStatsReporter();
  ASSERT_NE(nullptr, last_reporter);
  ASSERT_EQ(media::VP9PROFILE_PROFILE0, GetVideoStatsReporterCodecProfile());

  // Changing the codec profile should trigger recreation of the reporter.
  auto new_profile_config = TestVideoConfig::NormalCodecProfile(
      media::VideoCodec::kVP9, media::VP9PROFILE_PROFILE1);
  OnVideoConfigChange(new_profile_config);
  ASSERT_EQ(media::VP9PROFILE_PROFILE1, GetVideoStatsReporterCodecProfile());
  ASSERT_NE(last_reporter, GetVideoStatsReporter());
  last_reporter = GetVideoStatsReporter();

  // Changing the codec (implies changing profile) should similarly trigger
  // recreation of the reporter.
  auto new_codec_config =
      TestVideoConfig::NormalCodecProfile(media::VideoCodec::kVP8);
  OnVideoConfigChange(new_codec_config);
  ASSERT_EQ(media::VP8PROFILE_MIN, GetVideoStatsReporterCodecProfile());
  ASSERT_NE(last_reporter, GetVideoStatsReporter());
  last_reporter = GetVideoStatsReporter();

  // Changing other aspects of the config (like colorspace) should not trigger
  // recreation of the reporter
  media::VideoDecoderConfig new_color_config =
      TestVideoConfig::NormalWithColorSpace(media::VideoCodec::kVP8,
                                            media::VideoColorSpace::REC709());
  ASSERT_EQ(media::VP8PROFILE_MIN, new_color_config.profile());
  OnVideoConfigChange(new_color_config);
  ASSERT_EQ(last_reporter, GetVideoStatsReporter());
  ASSERT_EQ(media::VP8PROFILE_MIN, GetVideoStatsReporterCodecProfile());

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange) {
  InitializeWebMediaPlayerImpl();
  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::NormalCodecProfile(
      media::VideoCodec::kVP8, media::VP8PROFILE_MIN);
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
  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config =
      TestVideoConfig::NormalRotated(media::VIDEO_ROTATION_90);
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
  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();

  OnMetadata(metadata);

  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // Backgrounding the player sets the lock.
  BackgroundPlayer(BackgroundBehaviorType::Page);
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // Play without a user gesture doesn't unlock the player.
  Play();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // With a user gesture it does unlock the player.
  GetWebLocalFrame()->NotifyUserActivation(
      mojom::UserActivationNotificationType::kTest);
  Play();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // Pause without a user gesture doesn't lock the player.
  GetWebLocalFrame()->ConsumeTransientUserActivation();
  Pause();
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  // With a user gesture, pause does lock the player.
  GetWebLocalFrame()->NotifyUserActivation(
      mojom::UserActivationNotificationType::kTest);
  Pause();
  EXPECT_TRUE(IsVideoLockedWhenPausedWhenHidden());

  // Foregrounding the player unsets the lock.
  ForegroundPlayer(BackgroundBehaviorType::Page);
  EXPECT_FALSE(IsVideoLockedWhenPausedWhenHidden());

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest,
       PageEventsHasNoEffectIfPausedDueToFrameVisibility) {
  // Adding a demuxer and loading a media is necessary to make sure that the
  // pipeline will start and that `WebMediaPlayerImpl::PauseVideoIfNeeded` won't
  // return early.
  std::unique_ptr<media::MockDemuxer> demuxer =
      std::make_unique<NiceMock<media::MockDemuxer>>();
  ON_CALL(*demuxer, IsSeekable()).WillByDefault(Return(true));
  InitializeWebMediaPlayerImpl(std::move(demuxer));
  // We need to load a media file to start the pipeline.
  Load(kVideoOnlyTestFile);
  EXPECT_FALSE(IsSuspended());

  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  EXPECT_CALL(delegate_, DidMediaMetadataChange(_, false, true, _)).Times(2);
  OnMetadata(metadata);

  wmpi_->SetShouldPauseWhenFrameIsHidden(true);

  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
  SetSeeking(false);
  Play();
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());

  BackgroundPlayer(BackgroundBehaviorType::Frame);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_TRUE(IsPausedBecauseFrameHidden());

  BackgroundPlayer(BackgroundBehaviorType::Page);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_TRUE(IsPausedBecauseFrameHidden());

  ForegroundPlayer(BackgroundBehaviorType::Page);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_TRUE(IsPausedBecauseFrameHidden());

  ForegroundPlayer(BackgroundBehaviorType::Frame);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());
}

TEST_F(WebMediaPlayerImplTest,
       FrameVisibilityEventsHavePrecedenceOverPageEvents) {
  // Adding a demuxer and loading a media is necessary to make sure that the
  // pipeline will start and that `WebMediaPlayerImpl::PauseVideoIfNeeded` won't
  // return early.
  std::unique_ptr<media::MockDemuxer> demuxer =
      std::make_unique<NiceMock<media::MockDemuxer>>();
  ON_CALL(*demuxer, IsSeekable()).WillByDefault(Return(true));
  InitializeWebMediaPlayerImpl(std::move(demuxer));
  // We need to load a media file to start the pipeline.
  Load(kVideoOnlyTestFile);
  EXPECT_FALSE(IsSuspended());

  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  EXPECT_CALL(delegate_, DidMediaMetadataChange(_, false, true, _)).Times(2);
  OnMetadata(metadata);

  wmpi_->SetShouldPauseWhenFrameIsHidden(true);

  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
  SetSeeking(false);
  Play();
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());

  BackgroundPlayer(BackgroundBehaviorType::Page);
  EXPECT_TRUE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());

  BackgroundPlayer(BackgroundBehaviorType::Frame);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_TRUE(IsPausedBecauseFrameHidden());

  ForegroundPlayer(BackgroundBehaviorType::Page);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_TRUE(IsPausedBecauseFrameHidden());

  ForegroundPlayer(BackgroundBehaviorType::Frame);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());
}

// When `WebMediaPlayerImpl::should_pause_when_frame_is_hidden` is false, frame
// visibility changes should not affect the playback state.
TEST_F(WebMediaPlayerImplTest, DisabledFlagShouldPauseWhenFrameIsHidden) {
  // Adding a demuxer and loading a media is necessary to make sure that the
  // pipeline will start and that `WebMediaPlayerImpl::PauseVideoIfNeeded` won't
  // return early.
  std::unique_ptr<media::MockDemuxer> demuxer =
      std::make_unique<NiceMock<media::MockDemuxer>>();
  ON_CALL(*demuxer, IsSeekable()).WillByDefault(Return(true));
  InitializeWebMediaPlayerImpl(std::move(demuxer));
  // We need to load a media file to start the pipeline.
  Load(kVideoOnlyTestFile);
  EXPECT_FALSE(IsSuspended());

  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config = TestVideoConfig::Normal();
  EXPECT_CALL(delegate_, DidMediaMetadataChange(_, false, true, _)).Times(2);
  OnMetadata(metadata);

  wmpi_->SetShouldPauseWhenFrameIsHidden(false);

  SetReadyState(WebMediaPlayer::kReadyStateHaveFutureData);
  SetSeeking(false);
  Play();
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());

  BackgroundPlayer(BackgroundBehaviorType::Page);
  EXPECT_TRUE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());

  BackgroundPlayer(BackgroundBehaviorType::Frame);
  EXPECT_TRUE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());

  ForegroundPlayer(BackgroundBehaviorType::Page);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());

  ForegroundPlayer(BackgroundBehaviorType::Frame);
  EXPECT_FALSE(IsPausedBecausePageHidden());
  EXPECT_FALSE(IsPausedBecauseFrameHidden());
}

TEST_F(WebMediaPlayerImplTest, NotifiesObserverWhenFrozen) {
  InitializeWebMediaPlayerImpl();
  EXPECT_CALL(mock_observer_, OnFrozen());
  wmpi_->OnFrozen();
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

  EXPECT_CALL(
      client_,
      PausePlayback(
          WebMediaPlayerClient::PauseReason::kSuspendedPlayerIdleTimeout));
  FireIdlePauseTimer();
  base::RunLoop().RunUntilIdle();
}

// Verifies that an infinite duration doesn't muck up GetCurrentTimeInternal.
TEST_F(WebMediaPlayerImplTest, InfiniteDuration) {
  InitializeWebMediaPlayerImpl();
  SetDuration(media::kInfiniteDuration);

  // Send metadata so we have a watch time reporter created.
  media::PipelineMetadata metadata;
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

  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.video_decoder_config =
      TestVideoConfig::NormalRotated(media::VIDEO_ROTATION_90);
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

  media::PipelineMetadata metadata;
  metadata.has_video = true;
  OnMetadata(metadata);

  EXPECT_CALL(client_, GetDisplayType())
      .WillRepeatedly(Return(DisplayType::kPictureInPicture));
  EXPECT_CALL(client_, OnPictureInPictureStateChange()).Times(1);

  wmpi_->OnSurfaceIdUpdated(surface_id_);

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

// Test that OnPictureInPictureStateChange is not called for audio elements.
// This test explicitly sets display type to picture in picture, for an audio
// element, for testing purposes only (See crbug.com/1403547 for reference).
TEST_F(WebMediaPlayerImplTest, OnPictureInPictureStateChangeNotCalled) {
  InitializeWebMediaPlayerImpl();

  EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
      .WillRepeatedly(ReturnRef(surface_id_));
  EXPECT_CALL(*compositor_, EnableSubmission(_, _, _));
  EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));

  media::PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.has_audio = true;
  OnMetadata(metadata);

  EXPECT_CALL(client_, IsAudioElement()).WillOnce(Return(true));
  EXPECT_CALL(client_, GetDisplayType())
      .WillRepeatedly(Return(DisplayType::kPictureInPicture));
  EXPECT_CALL(client_, OnPictureInPictureStateChange()).Times(0);

  wmpi_->OnSurfaceIdUpdated(surface_id_);

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest, DisplayTypeChange) {
  InitializeWebMediaPlayerImpl();

  scoped_refptr<cc::Layer> layer = cc::Layer::Create();

  EXPECT_CALL(*surface_layer_bridge_ptr_, CreateSurfaceLayer());
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetSurfaceId())
      .WillRepeatedly(ReturnRef(surface_id_));
  EXPECT_CALL(*compositor_, EnableSubmission(_, _, _));
  EXPECT_CALL(*surface_layer_bridge_ptr_, SetContentsOpaque(false));
  EXPECT_CALL(*surface_layer_bridge_ptr_, GetCcLayer())
      .WillRepeatedly(Return(layer.get()));

  media::PipelineMetadata metadata;
  metadata.has_video = true;
  OnMetadata(metadata);

  // When entering PIP mode the CC layer is set to null so we are not
  // compositing the video in the original window.
  EXPECT_CALL(client_, IsInAutoPIP()).WillOnce(Return(false));
  EXPECT_CALL(client_, SetCcLayer(nullptr));
  wmpi_->OnDisplayTypeChanged(DisplayType::kPictureInPicture);

  // When switching back to the inline mode the CC layer is set back to the
  // bridge CC layer.
  EXPECT_CALL(client_, SetCcLayer(testing::NotNull()));
  wmpi_->OnDisplayTypeChanged(DisplayType::kInline);

  // When in persistent state (e.g. auto-pip), video is not playing in the
  // regular Picture-in-Picture mode. Don't set the CC layer to null.
  EXPECT_CALL(client_, IsInAutoPIP()).WillOnce(Return(true));
  EXPECT_CALL(client_, SetCcLayer(_)).Times(0);
  wmpi_->OnDisplayTypeChanged(DisplayType::kPictureInPicture);

  // When switching back to fullscreen mode the CC layer is set back to the
  // bridge CC layer.
  EXPECT_CALL(client_, SetCcLayer(testing::NotNull()));
  wmpi_->OnDisplayTypeChanged(DisplayType::kFullscreen);

  EXPECT_CALL(*surface_layer_bridge_ptr_, ClearObserver());
}

TEST_F(WebMediaPlayerImplTest, RegisterFrameSinkHierarchy) {
  InitializeWebMediaPlayerImpl();
  media::PipelineMetadata metadata;
  metadata.has_video = true;
  OnMetadata(metadata);

  EXPECT_CALL(*surface_layer_bridge_ptr_, RegisterFrameSinkHierarchy());
  wmpi_->RegisterFrameSinkHierarchy();

  EXPECT_CALL(*surface_layer_bridge_ptr_, UnregisterFrameSinkHierarchy());
  wmpi_->UnregisterFrameSinkHierarchy();
}

TEST_F(WebMediaPlayerImplTest, OnProgressClearsStale) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);

  for (auto rs = WebMediaPlayer::kReadyStateHaveNothing;
       rs <= WebMediaPlayer::kReadyStateHaveEnoughData;
       rs = static_cast<WebMediaPlayer::ReadyState>(static_cast<int>(rs) + 1)) {
    SetReadyState(rs);
    delegate_.SetStaleForTesting(true);
    OnProgress();
    EXPECT_EQ(delegate_.IsStale(delegate_.player_id()),
              rs >= WebMediaPlayer::kReadyStateHaveFutureData);
  }
}

TEST_F(WebMediaPlayerImplTest, MemDumpProvidersRegistration) {
  auto* dump_manager = base::trace_event::MemoryDumpManager::GetInstance();
  InitializeWebMediaPlayerImpl();

  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);
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

  wmpi_->SetPreload(WebMediaPlayer::kPreloadAuto);
  LoadAndWaitForCurrentData(kVideoAudioTestFile);

  CycleThreads();

  base::trace_event::MemoryDumpRequestArgs args = {
      1 /* dump_guid*/, base::trace_event::MemoryDumpType::kExplicitlyTriggered,
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};

  int32_t id = media::GetNextMediaPlayerLoggingID() - 1;
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

        ASSERT_TRUE(base::ranges::any_of(entries, [](const auto& e) {
          auto* name = base::trace_event::MemoryAllocatorDump::kNameObjectCount;
          return e.name == name && e.value_uint64 == 1;
        }));

        if (args.level_of_detail ==
            base::trace_event::MemoryDumpLevelOfDetail::kDetailed) {
          ASSERT_TRUE(base::ranges::any_of(entries, [](const auto& e) {
            return e.name == "player_state" && !e.value_string.empty();
          }));
        }
        dump_count++;
      });

  auto* dump_manager = base::trace_event::MemoryDumpManager::GetInstance();

  dump_manager->CreateProcessDump(args, on_memory_dump_done);

  args.level_of_detail =
      base::trace_event::MemoryDumpLevelOfDetail::kBackground;
  args.dump_guid++;
  dump_manager->CreateProcessDump(args, on_memory_dump_done);

  args.level_of_detail = base::trace_event::MemoryDumpLevelOfDetail::kLight;
  args.dump_guid++;
  dump_manager->CreateProcessDump(args, on_memory_dump_done);

  CycleThreads();
  EXPECT_EQ(dump_count, 3);
}

// Verify that a demuxer override is used when specified.
// TODO(https://crbug.com/1084476): This test is flaky.
TEST_F(WebMediaPlayerImplTest, DISABLED_DemuxerOverride) {
  std::unique_ptr<media::MockDemuxer> demuxer =
      std::make_unique<NiceMock<media::MockDemuxer>>();
  StrictMock<media::MockDemuxerStream> stream(media::DemuxerStream::AUDIO);
  stream.set_audio_decoder_config(TestAudioConfig::Normal());
  std::vector<media::DemuxerStream*> streams;
  streams.push_back(&stream);

  EXPECT_CALL(stream, SupportsConfigChanges()).WillRepeatedly(Return(false));

  EXPECT_CALL(*demuxer.get(), OnInitialize(_, _))
      .WillOnce(RunOnceCallback<1>(media::PIPELINE_OK));
  EXPECT_CALL(*demuxer.get(), GetAllStreams()).WillRepeatedly(Return(streams));
  // Called when WebMediaPlayerImpl is destroyed.
  EXPECT_CALL(*demuxer.get(), Stop());

  InitializeWebMediaPlayerImpl(std::move(demuxer));

  EXPECT_FALSE(IsSuspended());
  wmpi_->Load(WebMediaPlayer::kLoadTypeURL,
              WebMediaPlayerSource(WebURL(KURL("data://test"))),
              WebMediaPlayer::kCorsModeUnspecified,
              /*is_cache_disabled=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

class WebMediaPlayerImplBackgroundBehaviorTest
    : public WebMediaPlayerImplTest,
      public WebAudioSourceProviderClient,
      public ::testing::WithParamInterface<
          std::tuple<bool, int, int, bool, bool, bool, bool, bool, bool>> {
 public:
  // Indices of the tuple parameters.
  static const int kIsMediaSuspendEnabled = 0;
  static const int kDurationSec = 1;
  static const int kAverageKeyframeDistanceSec = 2;
  static const int kIsResumeBackgroundVideoEnabled = 3;
  static const int kIsPictureInPictureEnabled = 4;
  static const int kIsBackgroundVideoPlaybackEnabled = 5;
  static const int kIsVideoBeingCaptured = 6;
  // If true, the player's page is hidden. Otherwise, the player's frame is
  // hidden.
  static const int kShouldHidePage = 7;
  static const int kShouldPauseWhenFrameIsHidden = 8;

  void SetUp() override {
    WebMediaPlayerImplTest::SetUp();
    SetUpMediaSuspend(IsMediaSuspendOn());
    SetUpBackgroundVideoPlayback(IsBackgroundVideoPlaybackEnabled());

    std::string enabled_features;
    std::string disabled_features;

    if (IsResumeBackgroundVideoEnabled()) {
      if (!enabled_features.empty())
        enabled_features += ",";
      enabled_features += media::kResumeBackgroundVideo.name;
    } else {
      if (!disabled_features.empty())
        disabled_features += ",";
      disabled_features += media::kResumeBackgroundVideo.name;
    }

    feature_list_.InitFromCommandLine(enabled_features, disabled_features);

    if (std::get<kShouldHidePage>(GetParam())) {
      background_type_ = BackgroundBehaviorType::Page;
    } else {
      background_type_ = BackgroundBehaviorType::Frame;
    }

    InitializeWebMediaPlayerImpl();

    // MSE or SRC doesn't matter since we artificially inject pipeline stats.
    SetLoadType(WebMediaPlayer::kLoadTypeURL);

    SetVideoKeyframeDistanceAverage(
        base::Seconds(GetAverageKeyframeDistanceSec()));
    SetDuration(base::Seconds(GetDurationSec()));

    if (IsPictureInPictureOn()) {
      SetPiPExpectations();
      wmpi_->OnSurfaceIdUpdated(surface_id_);
    }

    if (IsVideoBeingCaptured())
      wmpi_->GetCurrentFrameThenUpdate();

    wmpi_->SetShouldPauseWhenFrameIsHidden(GetShouldPauseWhenFrameIsHidden());

    BackgroundPlayer(background_type_);
  }

  void SetVideoKeyframeDistanceAverage(base::TimeDelta value) {
    media::PipelineStatistics statistics;
    statistics.video_keyframe_distance_average = value;
    wmpi_->SetPipelineStatisticsForTest(statistics);
  }

  void SetPiPExpectations() {
    if (!IsPictureInPictureOn())
      return;
    EXPECT_CALL(client_, GetDisplayType())
        .WillRepeatedly(Return(DisplayType::kPictureInPicture));
  }

  bool IsMediaSuspendOn() {
    return std::get<kIsMediaSuspendEnabled>(GetParam());
  }

  bool IsResumeBackgroundVideoEnabled() {
    return std::get<kIsResumeBackgroundVideoEnabled>(GetParam());
  }

  bool IsPictureInPictureOn() {
    return std::get<kIsPictureInPictureEnabled>(GetParam());
  }

  bool IsBackgroundVideoPlaybackEnabled() {
    return std::get<kIsBackgroundVideoPlaybackEnabled>(GetParam());
  }

  bool IsVideoBeingCaptured() {
    return std::get<kIsVideoBeingCaptured>(GetParam());
  }

  bool GetShouldPauseWhenFrameIsHidden() const {
    return std::get<kShouldPauseWhenFrameIsHidden>(GetParam());
  }

  int GetDurationSec() const { return std::get<kDurationSec>(GetParam()); }

  int GetAverageKeyframeDistanceSec() const {
    return std::get<kAverageKeyframeDistanceSec>(GetParam());
  }

  int GetMaxKeyframeDistanceSec() const {
    return WebMediaPlayerImpl::kMaxKeyframeDistanceToDisableBackgroundVideo
        .InSeconds();
  }

  bool ShouldDisableVideoWhenHidden() const {
    return wmpi_->ShouldDisableVideoWhenHidden();
  }

  bool ShouldPausePlaybackWhenHidden() const {
    return wmpi_->ShouldPausePlaybackWhenHidden();
  }

  // We should pause media playback if the media-playback-while-not-visible
  // permission policy is not enabled and the player's frame is hidden.
  bool IsFrameHiddenAndShouldPauseWhenHidden() const {
    return background_type_ == BackgroundBehaviorType::Frame &&
           GetShouldPauseWhenFrameIsHidden();
  }

  std::string PrintValues() {
    std::stringstream stream;
    stream << "is_media_suspend_enabled=" << IsMediaSuspendOn()
           << ", duration_sec=" << GetDurationSec()
           << ", average_keyframe_distance_sec="
           << GetAverageKeyframeDistanceSec()
           << ", is_resume_background_video_enabled="
           << IsResumeBackgroundVideoEnabled()
           << ", is_picture_in_picture=" << IsPictureInPictureOn()
           << ", is_background_video_playback_enabled="
           << IsBackgroundVideoPlaybackEnabled()
           << ", is_video_being_captured=" << IsVideoBeingCaptured()
           << ", should_pause_when_frame_is_hidden="
           << GetShouldPauseWhenFrameIsHidden() << ", should_hide_page="
           << (background_type_ == BackgroundBehaviorType::Page);
    return stream.str();
  }

  MOCK_METHOD2(SetFormat, void(uint32_t numberOfChannels, float sampleRate));

 protected:
  BackgroundBehaviorType background_type_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, AudioOnly) {
  SCOPED_TRACE(testing::Message() << PrintValues());
  if (base::FeatureList::IsEnabled(media::kPauseBackgroundMutedAudio)) {
    // Audio only players should pause if they are muted and not captured.
    EXPECT_CALL(client_, WasAlwaysMuted()).WillRepeatedly(Return(true));
    SetMetadata(true, false);
    EXPECT_TRUE(ShouldPausePlaybackWhenHidden());
    EXPECT_FALSE(ShouldDisableVideoWhenHidden());

    auto provider = wmpi_->GetAudioSourceProvider();
    provider->SetClient(this);
    if (IsFrameHiddenAndShouldPauseWhenHidden()) {
      EXPECT_TRUE(ShouldPausePlaybackWhenHidden());
    } else {
      EXPECT_FALSE(ShouldPausePlaybackWhenHidden());
    }
    EXPECT_FALSE(ShouldDisableVideoWhenHidden());

    provider->SetClient(nullptr);
    EXPECT_TRUE(ShouldPausePlaybackWhenHidden());
    EXPECT_FALSE(ShouldDisableVideoWhenHidden());

    provider->SetCopyAudioCallback(base::DoNothing());
    if (IsFrameHiddenAndShouldPauseWhenHidden()) {
      EXPECT_TRUE(ShouldPausePlaybackWhenHidden());
    } else {
      EXPECT_FALSE(ShouldPausePlaybackWhenHidden());
    }
    EXPECT_FALSE(ShouldDisableVideoWhenHidden());

    provider->ClearCopyAudioCallback();
    EXPECT_TRUE(ShouldPausePlaybackWhenHidden());
    EXPECT_FALSE(ShouldDisableVideoWhenHidden());

    testing::Mock::VerifyAndClearExpectations(&client_);
    SetPiPExpectations();
  } else {
    // Never optimize or pause an audio-only player.
    SetMetadata(true, false);
  }

  if (IsFrameHiddenAndShouldPauseWhenHidden()) {
    EXPECT_TRUE(ShouldPausePlaybackWhenHidden());
  } else {
    EXPECT_FALSE(ShouldPausePlaybackWhenHidden());
  }
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());
}

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, VideoOnly) {
  SCOPED_TRACE(testing::Message() << PrintValues());

  // Video only -- setting muted should do nothing.
  EXPECT_CALL(client_, WasAlwaysMuted()).WillRepeatedly(Return(true));
  SetMetadata(false, true);

  // Never disable video track for a video only stream.
  EXPECT_FALSE(ShouldDisableVideoWhenHidden());

  // There's no optimization criteria for video only in Picture-in-Picture.
  bool matches_requirements =
      !IsPictureInPictureOn() && !IsVideoBeingCaptured();

  if (IsFrameHiddenAndShouldPauseWhenHidden()) {
    EXPECT_TRUE(ShouldPausePlaybackWhenHidden());
  } else {
    // Video is always paused when suspension is on and only if matches the
    // optimization criteria if the optimization is on.
    bool should_pause = (!IsBackgroundVideoPlaybackEnabled() ||
                         IsMediaSuspendOn() || matches_requirements) &&
                        !IsPictureInPictureOn();
    EXPECT_EQ(should_pause, ShouldPausePlaybackWhenHidden());
  }
}

TEST_P(WebMediaPlayerImplBackgroundBehaviorTest, AudioVideo) {
  SCOPED_TRACE(testing::Message() << PrintValues());

  bool always_pause =
      (!IsBackgroundVideoPlaybackEnabled() ||
       (IsMediaSuspendOn() && IsResumeBackgroundVideoEnabled())) &&
      !IsPictureInPictureOn();

  bool should_pause = !IsPictureInPictureOn() &&
                      (!IsBackgroundVideoPlaybackEnabled() ||
                       IsMediaSuspendOn() || !IsVideoBeingCaptured());

  if (IsFrameHiddenAndShouldPauseWhenHidden()) {
    always_pause = true;
    should_pause = true;
  }

  if (base::FeatureList::IsEnabled(media::kPauseBackgroundMutedAudio)) {
    EXPECT_CALL(client_, WasAlwaysMuted()).WillRepeatedly(Return(true));
    SetMetadata(true, true);
    EXPECT_EQ(should_pause, ShouldPausePlaybackWhenHidden());

    auto provider = wmpi_->GetAudioSourceProvider();
    provider->SetClient(this);
    EXPECT_EQ(always_pause, ShouldPausePlaybackWhenHidden());

    provider->SetClient(nullptr);
    EXPECT_EQ(should_pause, ShouldPausePlaybackWhenHidden());

    provider->SetCopyAudioCallback(base::DoNothing());
    EXPECT_EQ(always_pause, ShouldPausePlaybackWhenHidden());

    provider->ClearCopyAudioCallback();
    EXPECT_EQ(should_pause, ShouldPausePlaybackWhenHidden());

    testing::Mock::VerifyAndClearExpectations(&client_);
    SetPiPExpectations();
  } else {
    SetMetadata(true, true);
  }

  // Only pause audible videos if both media suspend and resume background
  // videos is on and background video playback is disabled. Background video
  // playback is enabled by default. Both media suspend and resume background
  // videos are on by default on Android and off on desktop.
  EXPECT_EQ(always_pause, ShouldPausePlaybackWhenHidden());

  // Optimization requirements are the same for all platforms.
  bool matches_requirements =
      !IsPictureInPictureOn() && !IsVideoBeingCaptured() &&
      ((GetDurationSec() < GetMaxKeyframeDistanceSec()) ||
       (GetAverageKeyframeDistanceSec() < GetMaxKeyframeDistanceSec()));

  EXPECT_EQ(matches_requirements, ShouldDisableVideoWhenHidden());

  if (!matches_requirements || !ShouldDisableVideoWhenHidden() ||
      IsMediaSuspendOn()) {
    return;
  }

  ForegroundPlayer(background_type_);
  EXPECT_FALSE(IsVideoTrackDisabled());
  EXPECT_FALSE(IsDisableVideoTrackPending());

  // Should start background disable timer in case we need to pause media
  // playback, but not disable immediately.
  BackgroundPlayer(background_type_);
  switch (background_type_) {
    case BackgroundBehaviorType::Page:
      if (ShouldPausePlaybackWhenHidden()) {
        EXPECT_FALSE(IsVideoTrackDisabled());
        EXPECT_FALSE(IsDisableVideoTrackPending());
      } else {
        // Testing IsVideoTrackDisabled() leads to flakiness even though there
        // should be a 10 minutes delay until it happens. Given that it doesn't
        // provides much of a benefit at the moment, this is being ignored.
        EXPECT_TRUE(IsDisableVideoTrackPending());
      }
      break;
    case BackgroundBehaviorType::Frame:
      if (!IsFrameHiddenAndShouldPauseWhenHidden()) {
        // Nothing should happen if the frame is not hidden or if the
        // media-playback-while-not-visible permission policy is enabled.
        EXPECT_FALSE(IsVideoTrackDisabled());
        EXPECT_FALSE(IsDisableVideoTrackPending());
      } else {
        // Ignore IsVideoTrackDisabled() for the same reason as above.
        EXPECT_FALSE(IsDisableVideoTrackPending());
      }
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    BackgroundBehaviorTestInstances,
    WebMediaPlayerImplBackgroundBehaviorTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(
            WebMediaPlayerImpl::kMaxKeyframeDistanceToDisableBackgroundVideo
                    .InSeconds() -
                1,
            300),
        ::testing::Values(
            WebMediaPlayerImpl::kMaxKeyframeDistanceToDisableBackgroundVideo
                    .InSeconds() -
                1,
            100),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool()));

}  // namespace blink
