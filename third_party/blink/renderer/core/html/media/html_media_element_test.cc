// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/html_media_element.h"

#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "media/mojo/mojom/media_player.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom-blink.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/media_error.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NanSensitiveDoubleEq;
using ::testing::Return;

namespace blink {

namespace {

class MockWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD0(OnTimeUpdate, void());
  MOCK_CONST_METHOD0(Seekable, WebTimeRanges());
  MOCK_CONST_METHOD0(HasAudio, bool());
  MOCK_CONST_METHOD0(HasVideo, bool());
  MOCK_CONST_METHOD0(Duration, double());
  MOCK_CONST_METHOD0(CurrentTime, double());
  MOCK_CONST_METHOD0(IsEnded, bool());
  MOCK_CONST_METHOD0(GetNetworkState, NetworkState());
  MOCK_CONST_METHOD0(WouldTaintOrigin, bool());
  MOCK_METHOD1(SetLatencyHint, void(double));
  MOCK_METHOD1(EnabledAudioTracksChanged, void(const WebVector<TrackId>&));
  MOCK_METHOD1(SelectedVideoTrackChanged, void(TrackId*));
  MOCK_METHOD4(
      Load,
      WebMediaPlayer::LoadTiming(LoadType load_type,
                                 const blink::WebMediaPlayerSource& source,
                                 CorsMode cors_mode,
                                 bool is_cache_disabled));
  MOCK_CONST_METHOD0(DidLazyLoad, bool());

  MOCK_METHOD0(GetSrcAfterRedirects, GURL());
};

class WebMediaStubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  explicit WebMediaStubLocalFrameClient(std::unique_ptr<WebMediaPlayer> player)
      : player_(std::move(player)) {}

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client) override {
    DCHECK(player_) << " Empty injected player - already used?";
    return std::move(player_);
  }

 private:
  std::unique_ptr<WebMediaPlayer> player_;
};

// Helper class that provides an implementation of the MediaPlayerObserver mojo
// interface to allow checking that messages sent over mojo are received with
// the right values in the other end.
//
// Note this relies on HTMLMediaElement::AddMediaPlayerObserverForTesting() to
// provide the HTMLMediaElement instance owned by the test with a valid mojo
// remote, that will be bound to the mojo receiver provided by this class
// instead of the real one used in production that would be owned by
// MediaSessionController instead.
class MockMediaPlayerObserverReceiverForTesting
    : public media::mojom::blink::MediaPlayerObserver {
 public:
  explicit MockMediaPlayerObserverReceiverForTesting(
      HTMLMediaElement* html_media_element) {
    // Bind the remote to the receiver, so that we can intercept incoming
    // messages sent via the different methods that use the remote.
    html_media_element->AddMediaPlayerObserverForTesting(
        receiver_.BindNewPipeAndPassRemote());
  }

  // Needs to be called from tests after invoking a method from the MediaPlayer
  // mojo interface, so that we have enough time to process the message.
  void WaitUntilReceivedMessage() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  // media::mojom::blink::MediaPlayerObserver implementation.
  void OnMediaPlaying() override {
    received_media_playing_ = true;
    run_loop_->Quit();
  }

  void OnMediaPaused(bool stream_ended) override {
    received_media_paused_stream_ended_ = stream_ended;
    run_loop_->Quit();
  }

  void OnMutedStatusChanged(bool muted) override {
    received_muted_status_type_ = muted;
    run_loop_->Quit();
  }

  void OnMediaPositionStateChanged(
      ::media_session::mojom::blink::MediaPositionPtr) override {}

  void OnMediaSizeChanged(const gfx::Size& size) override {
    received_media_size_ = size;
    run_loop_->Quit();
  }

  void OnPictureInPictureAvailabilityChanged(bool available) override {}

  void OnAudioOutputSinkChangingDisabled() override {}

  void OnBufferUnderflow() override {
    received_buffer_underflow_ = true;
    run_loop_->Quit();
  }

  void OnSeek() override {}

  // Getters used from HTMLMediaElementTest.
  bool received_media_playing() const { return received_media_playing_; }

  const base::Optional<bool>& received_media_paused_stream_ended() const {
    return received_media_paused_stream_ended_;
  }

  const base::Optional<bool>& received_muted_status() const {
    return received_muted_status_type_;
  }

  gfx::Size received_media_size() const { return received_media_size_; }

  bool received_buffer_underflow() const { return received_buffer_underflow_; }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  mojo::Receiver<media::mojom::blink::MediaPlayerObserver> receiver_{this};
  bool received_media_playing_{false};
  base::Optional<bool> received_media_paused_stream_ended_;
  base::Optional<bool> received_muted_status_type_;
  gfx::Size received_media_size_{0, 0};
  bool received_buffer_underflow_{false};
};

enum class MediaTestParam { kAudio, kVideo };

}  // namespace

class HTMLMediaElementTest : public testing::TestWithParam<MediaTestParam> {
 protected:
  void SetUp() override {
    // Sniff the media player pointer to facilitate mocking.
    auto mock_media_player = std::make_unique<MockWebMediaPlayer>();
    media_player_ = mock_media_player.get();

    // Most tests do not care about this call, nor its return value. Those that
    // do will clear this expectation and set custom expectations/returns.
    EXPECT_CALL(*mock_media_player, Seekable())
        .WillRepeatedly(Return(WebTimeRanges()));
    EXPECT_CALL(*mock_media_player, HasAudio()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_media_player, HasVideo()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_media_player, Duration()).WillRepeatedly(Return(1.0));
    EXPECT_CALL(*mock_media_player, CurrentTime()).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_media_player, Load(_, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(WebMediaPlayer::LoadTiming::kImmediate));
    EXPECT_CALL(*mock_media_player, DidLazyLoad).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_media_player, WouldTaintOrigin)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_media_player, GetNetworkState)
        .WillRepeatedly(Return(WebMediaPlayer::kNetworkStateIdle));
    EXPECT_CALL(*mock_media_player, SetLatencyHint(_)).Times(AnyNumber());

    dummy_page_holder_ = std::make_unique<DummyPageHolder>(
        IntSize(), nullptr,
        MakeGarbageCollected<WebMediaStubLocalFrameClient>(
            std::move(mock_media_player)));

    if (GetParam() == MediaTestParam::kAudio) {
      media_ = MakeGarbageCollected<HTMLAudioElement>(
          dummy_page_holder_->GetDocument());
    } else {
      media_ = MakeGarbageCollected<HTMLVideoElement>(
          dummy_page_holder_->GetDocument());
    }

    media_player_observer_receiver_ =
        std::make_unique<MockMediaPlayerObserverReceiverForTesting>(Media());
  }

  HTMLMediaElement* Media() const { return media_.Get(); }
  void SetCurrentSrc(const AtomicString& src) {
    KURL url(src);
    Media()->current_src_ = url;
  }

  MockWebMediaPlayer* MockMediaPlayer() { return media_player_; }

  bool WasAutoplayInitiated() { return Media()->WasAutoplayInitiated(); }

  bool CouldPlayIfEnoughData() { return Media()->CouldPlayIfEnoughData(); }

  bool PotentiallyPlaying() { return Media()->PotentiallyPlaying(); }

  bool ShouldDelayLoadEvent() { return Media()->should_delay_load_event_; }

  void SetReadyState(HTMLMediaElement::ReadyState state) {
    Media()->SetReadyState(state);
  }

  void SetNetworkState(WebMediaPlayer::NetworkState state) {
    Media()->SetNetworkState(state);
  }

  bool MediaShouldBeOpaque() const { return Media()->MediaShouldBeOpaque(); }

  void SetError(MediaError* err) { Media()->MediaEngineError(err); }

  void SimulateHighMediaEngagement() {
    Media()->GetDocument().GetPage()->AddAutoplayFlags(
        mojom::blink::kAutoplayFlagHighMediaEngagement);
  }

  bool HasLazyLoadObserver() const {
    return !!Media()->lazy_load_intersection_observer_;
  }

  ExecutionContext* GetExecutionContext() const {
    return dummy_page_holder_->GetFrame().DomWindow();
  }

 protected:
  // Helpers to call MediaPlayerObserver mojo methods and check their results.
  void NotifyMediaPlaying() {
    media_->DidPlayerStartPlaying();
    media_player_observer_receiver_->WaitUntilReceivedMessage();
  }

  bool ReceivedMessageMediaPlaying() {
    return media_player_observer_receiver_->received_media_playing();
  }

  void NotifyMediaPaused(bool stream_ended) {
    media_->DidPlayerPaused(stream_ended);
    media_player_observer_receiver_->WaitUntilReceivedMessage();
  }

  bool ReceivedMessageMediaPaused(bool stream_ended) {
    return media_player_observer_receiver_
               ->received_media_paused_stream_ended() == stream_ended;
  }

  void NotifyMutedStatusChange(bool muted) {
    media_->DidPlayerMutedStatusChange(muted);
    media_player_observer_receiver_->WaitUntilReceivedMessage();
  }

  bool ReceivedMessageMutedStatusChange(bool muted) {
    return media_player_observer_receiver_->received_muted_status() == muted;
  }

  void NotifyMediaSizeChange(const gfx::Size& size) {
    media_->DidPlayerSizeChange(size);
    media_player_observer_receiver_->WaitUntilReceivedMessage();
  }

  bool ReceivedMessageMediaSizeChange(const gfx::Size& size) {
    return media_player_observer_receiver_->received_media_size() == size;
  }

  void NotifyBufferUnderflowEvent() {
    media_->DidBufferUnderflow();
    media_player_observer_receiver_->WaitUntilReceivedMessage();
  }

  bool ReceivedMessageBufferUnderflowEvent() {
    return media_player_observer_receiver_->received_buffer_underflow();
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<HTMLMediaElement> media_;

  // Owned by WebMediaStubLocalFrameClient.
  MockWebMediaPlayer* media_player_;

  // Used to check that mojo messages are received in the other end.
  std::unique_ptr<MockMediaPlayerObserverReceiverForTesting>
      media_player_observer_receiver_;
};

INSTANTIATE_TEST_SUITE_P(Audio,
                         HTMLMediaElementTest,
                         testing::Values(MediaTestParam::kAudio));
INSTANTIATE_TEST_SUITE_P(Video,
                         HTMLMediaElementTest,
                         testing::Values(MediaTestParam::kVideo));

TEST_P(HTMLMediaElementTest, effectiveMediaVolume) {
  struct TestData {
    double volume;
    bool muted;
    double effective_volume;
  } test_data[] = {
      {0.0, false, 0.0}, {0.5, false, 0.5}, {1.0, false, 1.0},
      {0.0, true, 0.0},  {0.5, true, 0.0},  {1.0, true, 0.0},
  };

  for (const auto& data : test_data) {
    Media()->setVolume(data.volume);
    Media()->setMuted(data.muted);
    EXPECT_EQ(data.effective_volume, Media()->EffectiveMediaVolume());
  }
}

enum class TestURLScheme {
  kHttp,
  kHttps,
  kFtp,
  kFile,
  kData,
  kBlob,
};

AtomicString SrcSchemeToURL(TestURLScheme scheme) {
  switch (scheme) {
    case TestURLScheme::kHttp:
      return "http://example.com/foo.mp4";
    case TestURLScheme::kHttps:
      return "https://example.com/foo.mp4";
    case TestURLScheme::kFtp:
      return "ftp://example.com/foo.mp4";
    case TestURLScheme::kFile:
      return "file:///foo/bar.mp4";
    case TestURLScheme::kData:
      return "data:video/mp4;base64,XXXXXXX";
    case TestURLScheme::kBlob:
      return "blob:http://example.com/00000000-0000-0000-0000-000000000000";
    default:
      NOTREACHED();
  }
  return g_empty_atom;
}

TEST_P(HTMLMediaElementTest, preloadType) {
  struct TestData {
    bool data_saver_enabled;
    bool is_cellular;
    TestURLScheme src_scheme;
    AtomicString preload_to_set;
    AtomicString preload_expected;
  } test_data[] = {
      // Tests for conditions in which preload type should be overriden to
      // "none".
      {false, false, TestURLScheme::kHttp, "auto", "auto"},
      {true, false, TestURLScheme::kHttps, "auto", "auto"},
      {true, false, TestURLScheme::kFtp, "metadata", "metadata"},
      {false, false, TestURLScheme::kHttps, "auto", "auto"},
      {false, false, TestURLScheme::kFile, "auto", "auto"},
      {false, false, TestURLScheme::kData, "metadata", "metadata"},
      {false, false, TestURLScheme::kBlob, "auto", "auto"},
      {false, false, TestURLScheme::kFile, "none", "none"},
      // Tests for conditions in which preload type should be overriden to
      // "metadata".
      {false, true, TestURLScheme::kHttp, "auto", "metadata"},
      {false, true, TestURLScheme::kHttp, "scheme", "metadata"},
      {false, true, TestURLScheme::kHttp, "none", "none"},
      // Tests that the preload is overriden to "metadata".
      {false, false, TestURLScheme::kHttp, "foo", "metadata"},
  };

  int index = 0;
  for (const auto& data : test_data) {
    GetNetworkStateNotifier().SetSaveDataEnabledOverride(
        data.data_saver_enabled);
    if (data.is_cellular) {
      GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
          true, WebConnectionType::kWebConnectionTypeCellular3G,
          WebEffectiveConnectionType::kTypeUnknown, 1.0, 2.0);
    } else {
      GetNetworkStateNotifier().ClearOverride();
    }
    SetCurrentSrc(SrcSchemeToURL(data.src_scheme));
    Media()->setPreload(data.preload_to_set);

    EXPECT_EQ(data.preload_expected, Media()->preload())
        << "preload type differs at index" << index;
    ++index;
  }
}

TEST_P(HTMLMediaElementTest, CouldPlayIfEnoughDataRespondsToPlay) {
  EXPECT_FALSE(CouldPlayIfEnoughData());
  Media()->Play();
  EXPECT_TRUE(CouldPlayIfEnoughData());
}

TEST_P(HTMLMediaElementTest, CouldPlayIfEnoughDataRespondsToEnded) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();

  MockWebMediaPlayer* mock_wmpi =
      reinterpret_cast<MockWebMediaPlayer*>(Media()->GetWebMediaPlayer());
  ASSERT_NE(mock_wmpi, nullptr);
  EXPECT_CALL(*mock_wmpi, IsEnded()).WillRepeatedly(Return(false));
  EXPECT_TRUE(CouldPlayIfEnoughData());

  // Playback can only end once the ready state is above kHaveMetadata.
  SetReadyState(HTMLMediaElement::kHaveFutureData);
  EXPECT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->ended());
  EXPECT_TRUE(CouldPlayIfEnoughData());

  // Now advance current time to duration and verify ended state.
  testing::Mock::VerifyAndClearExpectations(mock_wmpi);
  EXPECT_CALL(*mock_wmpi, CurrentTime())
      .WillRepeatedly(Return(Media()->duration()));
  EXPECT_CALL(*mock_wmpi, IsEnded()).WillRepeatedly(Return(true));
  EXPECT_FALSE(CouldPlayIfEnoughData());
  EXPECT_TRUE(Media()->ended());
}

TEST_P(HTMLMediaElementTest, CouldPlayIfEnoughDataRespondsToError) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();

  MockWebMediaPlayer* mock_wmpi =
      reinterpret_cast<MockWebMediaPlayer*>(Media()->GetWebMediaPlayer());
  EXPECT_NE(mock_wmpi, nullptr);
  EXPECT_TRUE(CouldPlayIfEnoughData());

  SetReadyState(HTMLMediaElement::kHaveMetadata);
  EXPECT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->ended());
  EXPECT_TRUE(CouldPlayIfEnoughData());

  SetError(MakeGarbageCollected<MediaError>(MediaError::kMediaErrDecode, ""));
  EXPECT_FALSE(CouldPlayIfEnoughData());
}

TEST_P(HTMLMediaElementTest, SetLatencyHint) {
  const double kNan = std::numeric_limits<double>::quiet_NaN();

  // Initial value.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  EXPECT_CALL(*MockMediaPlayer(), SetLatencyHint(NanSensitiveDoubleEq(kNan)));

  test::RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // Valid value.
  EXPECT_CALL(*MockMediaPlayer(), SetLatencyHint(NanSensitiveDoubleEq(1.0)));
  Media()->setLatencyHint(1.0);

  test::RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // Invalid value.
  EXPECT_CALL(*MockMediaPlayer(), SetLatencyHint(NanSensitiveDoubleEq(kNan)));
  Media()->setLatencyHint(-1.0);
}

TEST_P(HTMLMediaElementTest, CouldPlayIfEnoughDataInfiniteStreamNeverEnds) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), Duration())
      .WillRepeatedly(Return(std::numeric_limits<double>::infinity()));
  EXPECT_CALL(*MockMediaPlayer(), CurrentTime())
      .WillRepeatedly(Return(std::numeric_limits<double>::infinity()));

  SetReadyState(HTMLMediaElement::kHaveMetadata);
  EXPECT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->ended());
  EXPECT_TRUE(CouldPlayIfEnoughData());
}

TEST_P(HTMLMediaElementTest, AutoplayInitiated_DocumentActivation_Low_Gesture) {
  // Setup is the following:
  // - Policy: DocumentUserActivation (aka. unified autoplay)
  // - MEI: low;
  // - Frame received user gesture.
  ScopedMediaEngagementBypassAutoplayPoliciesForTest scoped_feature(true);
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kDocumentUserActivationRequired);
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  Media()->Play();

  EXPECT_FALSE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest,
       AutoplayInitiated_DocumentActivation_High_Gesture) {
  // Setup is the following:
  // - Policy: DocumentUserActivation (aka. unified autoplay)
  // - MEI: high;
  // - Frame received user gesture.
  ScopedMediaEngagementBypassAutoplayPoliciesForTest scoped_feature(true);
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kDocumentUserActivationRequired);
  SimulateHighMediaEngagement();
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  Media()->Play();

  EXPECT_FALSE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest,
       AutoplayInitiated_DocumentActivation_High_NoGesture) {
  // Setup is the following:
  // - Policy: DocumentUserActivation (aka. unified autoplay)
  // - MEI: high;
  // - Frame did not receive user gesture.
  ScopedMediaEngagementBypassAutoplayPoliciesForTest scoped_feature(true);
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kDocumentUserActivationRequired);
  SimulateHighMediaEngagement();

  Media()->Play();

  EXPECT_TRUE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest, AutoplayInitiated_GestureRequired_Gesture) {
  // Setup is the following:
  // - Policy: user gesture is required.
  // - Frame received a user gesture.
  // - MEI doesn't matter as it's not used by the policy.
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kUserGestureRequired);
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  Media()->Play();

  EXPECT_FALSE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest, AutoplayInitiated_NoGestureRequired_Gesture) {
  // Setup is the following:
  // - Policy: no user gesture is required.
  // - Frame received a user gesture.
  // - MEI doesn't matter as it's not used by the policy.
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kNoUserGestureRequired);
  LocalFrame::NotifyUserActivation(
      Media()->GetDocument().GetFrame(),
      mojom::UserActivationNotificationType::kTest);

  Media()->Play();

  EXPECT_FALSE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest, AutoplayInitiated_NoGestureRequired_NoGesture) {
  // Setup is the following:
  // - Policy: no user gesture is required.
  // - Frame did not receive a user gesture.
  // - MEI doesn't matter as it's not used by the policy.
  Media()->GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kNoUserGestureRequired);

  Media()->Play();

  EXPECT_TRUE(WasAutoplayInitiated());
}

TEST_P(HTMLMediaElementTest,
       DeferredMediaPlayerLoadDoesNotDelayWindowLoadEvent) {
  // Source isn't really important, we just need something to let load algorithm
  // run up to the point of calling WebMediaPlayer::Load().
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));

  // WebMediaPlayer will signal that it will defer loading to some later time.
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());
  EXPECT_CALL(*MockMediaPlayer(), Load(_, _, _, _))
      .WillOnce(Return(WebMediaPlayer::LoadTiming::kDeferred));

  // Window's 'load' event starts out "delayed".
  EXPECT_TRUE(ShouldDelayLoadEvent());
  Media()->load();
  test::RunPendingTasks();

  // No longer delayed because WMP loading is deferred.
  EXPECT_FALSE(ShouldDelayLoadEvent());
}

TEST_P(HTMLMediaElementTest, ImmediateMediaPlayerLoadDoesDelayWindowLoadEvent) {
  // Source isn't really important, we just need something to let load algorithm
  // run up to the point of calling WebMediaPlayer::Load().
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));

  // WebMediaPlayer will signal that it will do the load immediately.
  EXPECT_CALL(*MockMediaPlayer(), Load(_, _, _, _))
      .WillOnce(Return(WebMediaPlayer::LoadTiming::kImmediate));

  // Window's 'load' event starts out "delayed".
  EXPECT_TRUE(ShouldDelayLoadEvent());
  Media()->load();
  test::RunPendingTasks();

  // Still delayed because WMP loading is not deferred.
  EXPECT_TRUE(ShouldDelayLoadEvent());
}

TEST_P(HTMLMediaElementTest, DefaultTracksAreEnabled) {
  // Default tracks should start enabled, not be created then enabled.
  EXPECT_CALL(*MockMediaPlayer(), EnabledAudioTracksChanged(_)).Times(0);
  EXPECT_CALL(*MockMediaPlayer(), SelectedVideoTrackChanged(_)).Times(0);

  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();
  SetReadyState(HTMLMediaElement::kHaveFutureData);

  ASSERT_EQ(1u, Media()->audioTracks().length());
  ASSERT_EQ(1u, Media()->videoTracks().length());
  EXPECT_TRUE(Media()->audioTracks().AnonymousIndexedGetter(0)->enabled());
  EXPECT_TRUE(Media()->videoTracks().AnonymousIndexedGetter(0)->selected());
}

// Ensure a visibility observer is created for lazy loading.
TEST_P(HTMLMediaElementTest, VisibilityObserverCreatedForLazyLoad) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), DidLazyLoad()).WillRepeatedly(Return(true));

  SetReadyState(HTMLMediaElement::kHaveFutureData);
  EXPECT_EQ(HasLazyLoadObserver(), GetParam() == MediaTestParam::kVideo);
}

TEST_P(HTMLMediaElementTest, DomInteractive) {
  EXPECT_FALSE(Media()->GetDocument().GetTiming().DomInteractive().is_null());
}

TEST_P(HTMLMediaElementTest, ContextPaused) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();
  SetReadyState(HTMLMediaElement::kHaveFutureData);

  EXPECT_FALSE(Media()->paused());
  GetExecutionContext()->SetLifecycleState(
      mojom::FrameLifecycleState::kFrozenAutoResumeMedia);
  EXPECT_TRUE(Media()->paused());
  GetExecutionContext()->SetLifecycleState(
      mojom::FrameLifecycleState::kRunning);
  EXPECT_FALSE(Media()->paused());
  GetExecutionContext()->SetLifecycleState(mojom::FrameLifecycleState::kFrozen);
  EXPECT_TRUE(Media()->paused());
  GetExecutionContext()->SetLifecycleState(
      mojom::FrameLifecycleState::kRunning);
  EXPECT_TRUE(Media()->paused());
}

TEST_P(HTMLMediaElementTest, GcMarkingNoAllocWebTimeRanges) {
  auto* thread_state = ThreadState::Current();
  ThreadState::NoAllocationScope no_allocation_scope(thread_state);
  EXPECT_FALSE(thread_state->IsAllocationAllowed());
  // Use of TimeRanges is not allowed during GC marking (crbug.com/970150)
  EXPECT_DCHECK_DEATH(MakeGarbageCollected<TimeRanges>(0, 0));
  // Instead of using TimeRanges, WebTimeRanges can be used without GC
  Vector<WebTimeRanges> ranges;
  ranges.emplace_back();
  ranges[0].emplace_back(0, 0);
}

// Reproduce crbug.com/970150
TEST_P(HTMLMediaElementTest, GcMarkingNoAllocHasActivity) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();

  test::RunPendingTasks();
  SetReadyState(HTMLMediaElement::kHaveFutureData);
  SetError(MakeGarbageCollected<MediaError>(MediaError::kMediaErrDecode, ""));

  EXPECT_FALSE(Media()->paused());

  auto* thread_state = ThreadState::Current();
  ThreadState::NoAllocationScope no_allocation_scope(thread_state);
  EXPECT_FALSE(thread_state->IsAllocationAllowed());
  Media()->HasPendingActivity();
}

TEST_P(HTMLMediaElementTest, CapturesRedirectedSrc) {
  // Verify that the element captures the redirected URL.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();
  test::RunPendingTasks();

  // Should start at the original.
  EXPECT_EQ(Media()->downloadURL(), Media()->currentSrc());

  GURL redirected_url("https://redirected.com");
  EXPECT_CALL(*MockMediaPlayer(), GetSrcAfterRedirects())
      .WillRepeatedly(Return(redirected_url));
  SetReadyState(HTMLMediaElement::kHaveFutureData);

  EXPECT_EQ(Media()->downloadURL(), redirected_url);
}

TEST_P(HTMLMediaElementTest, EmptyRedirectedSrcUsesOriginal) {
  // If the player returns an empty URL for the redirected src, then the element
  // should continue using currentSrc().
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  Media()->Play();
  test::RunPendingTasks();
  EXPECT_EQ(Media()->downloadURL(), Media()->currentSrc());
  SetReadyState(HTMLMediaElement::kHaveFutureData);
  EXPECT_EQ(Media()->downloadURL(), Media()->currentSrc());
}

TEST_P(HTMLMediaElementTest, NoPendingActivityEvenIfBeforeMetadata) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  MockWebMediaPlayer* mock_wmpi =
      reinterpret_cast<MockWebMediaPlayer*>(Media()->GetWebMediaPlayer());
  EXPECT_CALL(*mock_wmpi, WouldTaintOrigin()).WillRepeatedly(Return(true));
  EXPECT_NE(mock_wmpi, nullptr);

  EXPECT_TRUE(MediaShouldBeOpaque());
  EXPECT_TRUE(Media()->HasPendingActivity());
  SetNetworkState(WebMediaPlayer::kNetworkStateIdle);
  EXPECT_FALSE(Media()->HasPendingActivity());
  EXPECT_TRUE(MediaShouldBeOpaque());
}

TEST_P(HTMLMediaElementTest, OnTimeUpdate_DurationChange) {
  // Prepare the player.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  // Change from no duration to 1s will trigger OnTimeUpdate().
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->DurationChanged(1, false);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // Change from 1s to 2s will trigger OnTimeUpdate().
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->DurationChanged(2, false);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  // No duration change -> no OnTimeUpdate().
  Media()->DurationChanged(2, false);
}

TEST_P(HTMLMediaElementTest, OnTimeUpdate_PlayPauseSetRate) {
  // Prepare the player.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->Play();
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->setPlaybackRate(0.5);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate()).Times(testing::AtLeast(1));
  Media()->pause();
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->setPlaybackRate(1.5);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->Play();
}

TEST_P(HTMLMediaElementTest, OnTimeUpdate_ReadyState) {
  // Prepare the player.
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  // The ready state affects the progress of media time, so the player should
  // be kept informed.
  EXPECT_CALL(*MockMediaPlayer(), GetSrcAfterRedirects)
      .WillRepeatedly(Return(GURL()));
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  SetReadyState(HTMLMediaElement::kHaveCurrentData);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  SetReadyState(HTMLMediaElement::kHaveFutureData);
}

TEST_P(HTMLMediaElementTest, OnTimeUpdate_Seeking) {
  // Prepare the player and seekable ranges -- setCurrentTime()'s prerequisites.
  WebTimeRanges seekable;
  seekable.Add(0, 3);
  EXPECT_CALL(*MockMediaPlayer(), Seekable).WillRepeatedly(Return(seekable));
  EXPECT_CALL(*MockMediaPlayer(), GetSrcAfterRedirects)
      .WillRepeatedly(Return(GURL()));
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();
  SetReadyState(HTMLMediaElement::kHaveCurrentData);

  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->setCurrentTime(1);
  testing::Mock::VerifyAndClearExpectations(MockMediaPlayer());

  EXPECT_CALL(*MockMediaPlayer(), Seekable).WillRepeatedly(Return(seekable));
  EXPECT_CALL(*MockMediaPlayer(), OnTimeUpdate());
  Media()->setCurrentTime(2);
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_InitiallyTrue) {
  // ShowPosterFlag should be true upon initialization
  EXPECT_TRUE(Media()->IsShowPosterFlagSet());

  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  EXPECT_TRUE(Media()->IsShowPosterFlagSet());

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  // ShowPosterFlag should still be true once video is ready to play
  EXPECT_TRUE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_FalseAfterPlay) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  Media()->Play();
  test::RunPendingTasks();

  // ShowPosterFlag should be false once video is playing
  ASSERT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_FalseAfterSeek) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  ASSERT_NE(Media()->duration(), 0.0);
  Media()->setCurrentTime(Media()->duration() / 2);
  test::RunPendingTasks();

  EXPECT_FALSE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_FalseAfterAutoPlay) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));
  test::RunPendingTasks();

  Media()->SetBooleanAttribute(html_names::kAutoplayAttr, true);
  test::RunPendingTasks();

  SetReadyState(HTMLMediaElement::kHaveEnoughData);
  test::RunPendingTasks();

  ASSERT_TRUE(WasAutoplayInitiated());
  ASSERT_FALSE(Media()->paused());
  EXPECT_FALSE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, ShowPosterFlag_FalseAfterPlayBeforeReady) {
  Media()->SetSrc(SrcSchemeToURL(TestURLScheme::kHttp));

  // Initially we have nothing, we're not playing, trying to play, and the 'show
  // poster' flag is set
  EXPECT_EQ(Media()->getReadyState(), HTMLMediaElement::kHaveNothing);
  EXPECT_TRUE(Media()->paused());
  EXPECT_FALSE(PotentiallyPlaying());
  EXPECT_TRUE(Media()->IsShowPosterFlagSet());

  // Attempt to begin playback
  Media()->Play();
  test::RunPendingTasks();

  // We still have no data, but we're not paused, and the 'show poster' flag is
  // not set
  EXPECT_EQ(Media()->getReadyState(), HTMLMediaElement::kHaveNothing);
  EXPECT_FALSE(Media()->paused());
  EXPECT_FALSE(PotentiallyPlaying());
  EXPECT_FALSE(Media()->IsShowPosterFlagSet());

  // Pretend we have data to begin playback
  SetReadyState(HTMLMediaElement::kHaveFutureData);

  // We should have data, be playing, and the show poster flag should be unset
  EXPECT_EQ(Media()->getReadyState(), HTMLMediaElement::kHaveFutureData);
  EXPECT_FALSE(Media()->paused());
  EXPECT_TRUE(PotentiallyPlaying());
  EXPECT_FALSE(Media()->IsShowPosterFlagSet());
}

TEST_P(HTMLMediaElementTest, SendMediaPlayingToObserver) {
  NotifyMediaPlaying();
  EXPECT_TRUE(ReceivedMessageMediaPlaying());
}

TEST_P(HTMLMediaElementTest, SendMediaPausedToObserver) {
  NotifyMediaPaused(true);
  EXPECT_TRUE(ReceivedMessageMediaPaused(true));

  NotifyMediaPaused(false);
  EXPECT_TRUE(ReceivedMessageMediaPaused(false));
}

TEST_P(HTMLMediaElementTest, SendMutedStatusChangeToObserver) {
  NotifyMutedStatusChange(true);
  EXPECT_TRUE(ReceivedMessageMutedStatusChange(true));

  NotifyMutedStatusChange(false);
  EXPECT_TRUE(ReceivedMessageMutedStatusChange(false));
}

TEST_P(HTMLMediaElementTest, SendMediaSizeChangeToObserver) {
  const gfx::Size kTestMediaSizeChangedValue(16, 9);
  NotifyMediaSizeChange(kTestMediaSizeChangedValue);
  EXPECT_TRUE(ReceivedMessageMediaSizeChange(kTestMediaSizeChangedValue));
}

TEST_P(HTMLMediaElementTest, SendBufferOverflowToObserver) {
  NotifyBufferUnderflowEvent();
  EXPECT_TRUE(ReceivedMessageBufferUnderflowEvent());
}

}  // namespace blink
