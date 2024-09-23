// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_media_session_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/blink/public/common/features.h"

using ::testing::_;

namespace content {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using media_session::test::MockMediaSessionMojoObserver;
using media_session::test::TestAudioFocusObserver;

namespace {

class MockAudioFocusDelegate : public AudioFocusDelegate {
 public:
  MockAudioFocusDelegate() = default;

  MockAudioFocusDelegate(const MockAudioFocusDelegate&) = delete;
  MockAudioFocusDelegate& operator=(const MockAudioFocusDelegate&) = delete;

  ~MockAudioFocusDelegate() override = default;

  void AbandonAudioFocus() override {}

  AudioFocusResult RequestAudioFocus(AudioFocusType type) override {
    request_audio_focus_count_++;
    return AudioFocusResult::kSuccess;
  }

  std::optional<AudioFocusType> GetCurrentFocusType() const override {
    return AudioFocusType::kGain;
  }

  void MediaSessionInfoChanged(
      const MediaSessionInfoPtr& session_info) override {
    session_info_ = session_info.Clone();
  }

  MOCK_CONST_METHOD0(request_id, const base::UnguessableToken&());

  MOCK_METHOD(void, ReleaseRequestId, (), (override));

  MediaSessionInfo::SessionState GetState() const {
    DCHECK(!session_info_.is_null());
    return session_info_->state;
  }

  int request_audio_focus_count() const { return request_audio_focus_count_; }

 private:
  int request_audio_focus_count_ = 0;

  MediaSessionInfoPtr session_info_;
};

// A mock WebContentsDelegate which listens to |ActivateContents()| calls.
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  // content::WebContentsDelegate:
  MOCK_METHOD(void, ActivateContents, (content::WebContents*), (override));
};

}  // anonymous namespace

class MediaSessionImplTest : public RenderViewHostTestHarness {
 public:
  MediaSessionImplTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    default_actions_.insert(MediaSessionAction::kPlay);
    default_actions_.insert(MediaSessionAction::kPause);
    default_actions_.insert(MediaSessionAction::kStop);
    default_actions_.insert(MediaSessionAction::kSeekTo);
    default_actions_.insert(MediaSessionAction::kScrubTo);
    default_actions_.insert(MediaSessionAction::kSeekForward);
    default_actions_.insert(MediaSessionAction::kSeekBackward);
  }

  MediaSessionImplTest(const MediaSessionImplTest&) = delete;
  MediaSessionImplTest& operator=(const MediaSessionImplTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media_session::features::kMediaSessionService,
         media_session::features::kAudioFocusEnforcement,
         media::kGlobalMediaControlsPictureInPicture,
         blink::features::kMediaSessionEnterPictureInPicture},
        {});

    RenderViewHostTestHarness::SetUp();

    player_observer_ = std::make_unique<MockMediaSessionPlayerObserver>(
        main_rfh(), media::MediaContentType::kPersistent);
    mock_media_session_service_ =
        std::make_unique<testing::NiceMock<MockMediaSessionServiceImpl>>(
            main_rfh());

    // Connect to the Media Session service and bind |audio_focus_remote_| to
    // it.
    GetMediaSessionService().BindAudioFocusManager(
        audio_focus_remote_.BindNewPipeAndPassReceiver());
    audio_focus_remote_->SetEnforcementMode(
        media_session::mojom::EnforcementMode::kDefault);
  }

  void TearDown() override {
    mock_media_session_service_.reset();

    scoped_feature_list_.Reset();
    audio_focus_remote_->SetEnforcementMode(
        media_session::mojom::EnforcementMode::kDefault);

    RenderViewHostTestHarness::TearDown();
  }

  void RequestAudioFocus(MediaSessionImpl* session,
                         AudioFocusType audio_focus_type) {
    session->RequestSystemAudioFocus(audio_focus_type);
  }

  void AbandonAudioFocus(MediaSessionImpl* session) {
    session->AbandonSystemAudioFocusIfNeeded();
  }

  bool GetForceDuck(MediaSessionImpl* session) {
    return media_session::test::GetMediaSessionInfoSync(session)->force_duck;
  }

  MediaSessionInfo::SessionState GetState(MediaSessionImpl* session) {
    return media_session::test::GetMediaSessionInfoSync(session)->state;
  }

  void ClearObservers(MediaSessionImpl* session) {
    session->observers_.Clear();
  }

  bool HasObservers(MediaSessionImpl* session) {
    return !session->observers_.empty();
  }

  void FlushForTesting(MediaSessionImpl* session) {
    session->FlushForTesting();
  }

  std::unique_ptr<TestAudioFocusObserver> CreateAudioFocusObserver() {
    std::unique_ptr<TestAudioFocusObserver> observer =
        std::make_unique<TestAudioFocusObserver>();

    audio_focus_remote_->AddObserver(observer->BindNewPipeAndPassRemote());
    audio_focus_remote_.FlushForTesting();

    return observer;
  }

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_;

  void SetDelegateForTests(MediaSessionImpl* session,
                           AudioFocusDelegate* delegate) {
    session->SetDelegateForTests(base::WrapUnique(delegate));
  }

  MockMediaSessionServiceImpl& mock_media_session_service() const {
    return *mock_media_session_service_.get();
  }

  MediaSessionImpl* GetMediaSession() {
    return MediaSessionImpl::Get(web_contents());
  }

  // Returns the player ID.
  int StartNewPlayer() {
    int player_id;
    GetMediaSession()->AddPlayer(
        player_observer_.get(), player_id = player_observer_->StartNewPlayer());
    return player_id;
  }

  MockMediaSessionPlayerObserver* player_observer() {
    return player_observer_.get();
  }

  const std::set<MediaSessionAction>& default_actions() const {
    return default_actions_;
  }

  void OnVideoVisibilityChanged() {
    GetMediaSession()->OnVideoVisibilityChanged();
  }

  bool HasSufficientlyVisibleVideo() {
    return GetMediaSession()->HasSufficientlyVisibleVideo();
  }

 private:
  std::set<MediaSessionAction> default_actions_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<MockMediaSessionServiceImpl> mock_media_session_service_;

  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
};

TEST_F(MediaSessionImplTest, SessionInfoState) {
  EXPECT_EQ(MediaSessionInfo::SessionState::kInactive,
            GetState(GetMediaSession()));

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    RequestAudioFocus(GetMediaSession(), AudioFocusType::kGain);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(GetMediaSession())));
  }

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->StartDucking();
    observer.WaitForState(MediaSessionInfo::SessionState::kDucking);
  }

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->StopDucking();
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(GetMediaSession())));
  }

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(GetMediaSession())));
  }

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->Resume(MediaSession::SuspendType::kSystem);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(GetMediaSession())));
  }

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    AbandonAudioFocus(GetMediaSession());
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(GetMediaSession())));
  }
}

TEST_F(MediaSessionImplTest, NotifyDelegateOnStateChange) {
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(GetMediaSession(), delegate);

  RequestAudioFocus(GetMediaSession(), AudioFocusType::kGain);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, delegate->GetState());

  GetMediaSession()->StartDucking();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kDucking, delegate->GetState());

  GetMediaSession()->StopDucking();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, delegate->GetState());

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kSuspended, delegate->GetState());

  GetMediaSession()->Resume(MediaSession::SuspendType::kSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kActive, delegate->GetState());

  AbandonAudioFocus(GetMediaSession());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaSessionInfo::SessionState::kInactive, delegate->GetState());
}

TEST_F(MediaSessionImplTest, PepperForcesDuckAndRequestsFocus) {
  int player_id = player_observer_->StartNewPlayer();

  {
    player_observer_->SetMediaContentType(media::MediaContentType::kPepper);
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->AddPlayer(player_observer_.get(), player_id);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    player_observer_->SetMediaContentType(media::MediaContentType::kPersistent);
  }

  EXPECT_TRUE(GetForceDuck(GetMediaSession()));

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->RemovePlayer(player_observer_.get(), player_id);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
  }

  EXPECT_FALSE(GetForceDuck(GetMediaSession()));
}

TEST_F(MediaSessionImplTest, RegisterObserver) {
  // There is no way to get the number of mojo observers so we should just
  // remove them all and check if the mojo observers interface ptr set is
  // empty or not.
  ClearObservers(GetMediaSession());
  EXPECT_FALSE(HasObservers(GetMediaSession()));

  MockMediaSessionMojoObserver observer(*GetMediaSession());
  FlushForTesting(GetMediaSession());

  EXPECT_TRUE(HasObservers(GetMediaSession()));
}

TEST_F(MediaSessionImplTest, SessionInfo_PlaybackState) {
  EXPECT_EQ(MediaPlaybackState::kPaused,
            media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                ->playback_state);

  int player_id = player_observer_->StartNewPlayer();

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->AddPlayer(player_observer_.get(), player_id);
    observer.WaitForPlaybackState(MediaPlaybackState::kPlaying);
  }

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->OnPlayerPaused(player_observer_.get(), player_id);
    observer.WaitForPlaybackState(MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaSessionImplTest, SuspendUI) {
  EXPECT_CALL(mock_media_session_service().mock_client(),
              DidReceiveAction(MediaSessionAction::kPause, _))
      .Times(0);

  StartNewPlayer();

  GetMediaSession()->Suspend(MediaSession::SuspendType::kUI);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, SuspendContent_WithAction) {
  EXPECT_CALL(mock_media_session_service().mock_client(),
              DidReceiveAction(MediaSessionAction::kPause, _))
      .Times(0);

  StartNewPlayer();
  mock_media_session_service().EnableAction(MediaSessionAction::kPause);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kContent);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, SuspendSystem_WithAction) {
  EXPECT_CALL(mock_media_session_service().mock_client(),
              DidReceiveAction(MediaSessionAction::kPause, _))
      .Times(0);

  StartNewPlayer();
  mock_media_session_service().EnableAction(MediaSessionAction::kPause);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, SuspendUI_WithAction) {
  EXPECT_CALL(mock_media_session_service().mock_client(),
              DidReceiveAction(MediaSessionAction::kPause, _));

  StartNewPlayer();
  mock_media_session_service().EnableAction(MediaSessionAction::kPause);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kUI);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, ResumeUI) {
  EXPECT_CALL(mock_media_session_service().mock_client(),
              DidReceiveAction(MediaSessionAction::kPlay, _))
      .Times(0);

  StartNewPlayer();

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  GetMediaSession()->Resume(MediaSession::SuspendType::kUI);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, ResumeContent_WithAction) {
  EXPECT_CALL(mock_media_session_service().mock_client(),
              DidReceiveAction(MediaSessionAction::kPlay, _))
      .Times(0);

  StartNewPlayer();
  mock_media_session_service().EnableAction(MediaSessionAction::kPlay);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  GetMediaSession()->Resume(MediaSession::SuspendType::kContent);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, ResumeSystem_WithAction) {
  EXPECT_CALL(mock_media_session_service().mock_client(),
              DidReceiveAction(MediaSessionAction::kPlay, _))
      .Times(0);

  StartNewPlayer();
  mock_media_session_service().EnableAction(MediaSessionAction::kPlay);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  GetMediaSession()->Resume(MediaSession::SuspendType::kSystem);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, ResumeUI_WithAction) {
  EXPECT_CALL(mock_media_session_service().mock_client(),
              DidReceiveAction(MediaSessionAction::kPlay, _));

  StartNewPlayer();
  mock_media_session_service().EnableAction(MediaSessionAction::kPlay);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  GetMediaSession()->Resume(MediaSession::SuspendType::kUI);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

#if !BUILDFLAG(IS_ANDROID)

TEST_F(MediaSessionImplTest, WebContentsDestroyed_ReleasesFocus) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    RequestAudioFocus(media_session, AudioFocusType::kGain);
    observer->WaitForGainedEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    web_contents.reset();
    observer->WaitForLostEvent();
  }
}

TEST_F(MediaSessionImplTest, WebContentsDestroyed_ReleasesTransients) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());
  MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents.get());

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    RequestAudioFocus(media_session, AudioFocusType::kGainTransientMayDuck);
    observer->WaitForGainedEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    web_contents.reset();
    observer->WaitForLostEvent();
  }
}

TEST_F(MediaSessionImplTest, WebContentsDestroyed_StopsDucking) {
  std::unique_ptr<WebContents> web_contents_1(CreateTestWebContents());
  MediaSessionImpl* media_session_1 =
      MediaSessionImpl::Get(web_contents_1.get());

  std::unique_ptr<WebContents> web_contents_2(CreateTestWebContents());
  MediaSessionImpl* media_session_2 =
      MediaSessionImpl::Get(web_contents_2.get());

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    RequestAudioFocus(media_session_1, AudioFocusType::kGain);
    observer->WaitForGainedEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session_1);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    RequestAudioFocus(media_session_2, AudioFocusType::kGainTransientMayDuck);
    observer->WaitForGainedEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session_1);
    observer.WaitForState(MediaSessionInfo::SessionState::kDucking);
  }

  {
    std::unique_ptr<TestAudioFocusObserver> observer =
        CreateAudioFocusObserver();
    web_contents_2.reset();
    observer->WaitForLostEvent();
  }

  {
    MockMediaSessionMojoObserver observer(*media_session_1);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }
}

#if BUILDFLAG(IS_MAC)

TEST_F(MediaSessionImplTest, TabFocusDoesNotCauseAudioFocus) {
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(GetMediaSession(), delegate);

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    RequestAudioFocus(GetMediaSession(), AudioFocusType::kGain);
    FlushForTesting(GetMediaSession());
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  EXPECT_EQ(1, delegate->request_audio_focus_count());
  GetMediaSession()->OnWebContentsFocused(nullptr);
  EXPECT_EQ(1, delegate->request_audio_focus_count());
}

#else  // BUILDFLAG(IS_MAC)

TEST_F(MediaSessionImplTest, RequestAudioFocus_OnFocus_Active) {
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(GetMediaSession(), delegate);

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    RequestAudioFocus(GetMediaSession(), AudioFocusType::kGain);
    FlushForTesting(GetMediaSession());
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  EXPECT_EQ(1, delegate->request_audio_focus_count());
  GetMediaSession()->OnWebContentsFocused(nullptr);
  EXPECT_EQ(2, delegate->request_audio_focus_count());
}

TEST_F(MediaSessionImplTest, RequestAudioFocus_OnFocus_Inactive) {
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(GetMediaSession(), delegate);
  EXPECT_EQ(MediaSessionInfo::SessionState::kInactive,
            GetState(GetMediaSession()));

  EXPECT_EQ(0, delegate->request_audio_focus_count());
  GetMediaSession()->OnWebContentsFocused(nullptr);
  EXPECT_EQ(0, delegate->request_audio_focus_count());
}

TEST_F(MediaSessionImplTest, RequestAudioFocus_OnFocus_Suspended) {
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(GetMediaSession(), delegate);

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    RequestAudioFocus(GetMediaSession(), AudioFocusType::kGain);
    FlushForTesting(GetMediaSession());
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
  }

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
  }

  EXPECT_EQ(1, delegate->request_audio_focus_count());
  GetMediaSession()->OnWebContentsFocused(nullptr);
  EXPECT_EQ(1, delegate->request_audio_focus_count());
}

#endif  // BUILDFLAG(IS_MAC)

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(MediaSessionImplTest, SourceId_SameBrowserContext) {
  auto other_contents = TestWebContents::Create(browser_context(), nullptr);
  MediaSessionImpl* other_session = MediaSessionImpl::Get(other_contents.get());

  EXPECT_EQ(GetMediaSession()->GetSourceId(), other_session->GetSourceId());
}

TEST_F(MediaSessionImplTest, SourceId_DifferentBrowserContext) {
  auto other_context = CreateBrowserContext();
  auto other_contents = TestWebContents::Create(other_context.get(), nullptr);
  MediaSessionImpl* other_session = MediaSessionImpl::Get(other_contents.get());

  EXPECT_NE(GetMediaSession()->GetSourceId(), other_session->GetSourceId());
}


TEST_F(MediaSessionImplTest, SessionInfoPictureInPicture) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());

  EXPECT_EQ(
      media_session::test::GetMediaSessionInfoSync(GetMediaSession())
          ->picture_in_picture_state,
      media_session::mojom::MediaPictureInPictureState::kNotInPictureInPicture);

  web_contents_impl->SetHasPictureInPictureVideo(true);
  EXPECT_EQ(
      media_session::test::GetMediaSessionInfoSync(GetMediaSession())
          ->picture_in_picture_state,
      media_session::mojom::MediaPictureInPictureState::kInPictureInPicture);

  web_contents_impl->SetHasPictureInPictureVideo(false);
  EXPECT_EQ(
      media_session::test::GetMediaSessionInfoSync(GetMediaSession())
          ->picture_in_picture_state,
      media_session::mojom::MediaPictureInPictureState::kNotInPictureInPicture);
}

TEST_F(MediaSessionImplTest, SessionInfoAudioSink) {
  // When the session is created it should be using the default audio device.
  // When the default audio device is in use, the |audio_sink_id| attribute
  // should be unset.
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                   ->audio_sink_id.has_value());
  int player1 = player_observer_->StartNewPlayer();
  int player2 = player_observer_->StartNewPlayer();
  GetMediaSession()->AddPlayer(player_observer_.get(), player1);
  GetMediaSession()->AddPlayer(player_observer_.get(), player2);
  player_observer_->SetAudioSinkId(player1, "1");
  player_observer_->SetAudioSinkId(player2, "1");

  auto info = media_session::test::GetMediaSessionInfoSync(GetMediaSession());
  ASSERT_TRUE(info->audio_sink_id.has_value());
  EXPECT_EQ(info->audio_sink_id.value(), "1");

  // If multiple audio devices are being used the audio sink id attribute should
  // be unset.
  player_observer_->SetAudioSinkId(player2, "2");
  info = media_session::test::GetMediaSessionInfoSync(GetMediaSession());
  EXPECT_FALSE(info->audio_sink_id.has_value());
}

TEST_F(MediaSessionImplTest, SessionInfoPresentation) {
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                   ->has_presentation);

  GetMediaSession()->OnPresentationsChanged(true);
  EXPECT_TRUE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                  ->has_presentation);

  GetMediaSession()->OnPresentationsChanged(false);
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                   ->has_presentation);
}

TEST_F(MediaSessionImplTest, SessionInfoRemotePlaybackMetadata) {
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                   ->remote_playback_metadata);

  int player1 = player_observer_->StartNewPlayer();
  GetMediaSession()->AddPlayer(player_observer_.get(), player1);
  GetMediaSession()->SetRemotePlaybackMetadata(
      media_session::mojom::RemotePlaybackMetadata::New(
          "video_codec", "audio_codec", false, true, "device_friendly_name",
          false));
  EXPECT_TRUE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                  ->remote_playback_metadata);

  int player2 = player_observer_->StartNewPlayer();
  GetMediaSession()->AddPlayer(player_observer_.get(), player2);

  EXPECT_TRUE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                  ->remote_playback_metadata->remote_playback_disabled);
}

TEST_F(MediaSessionImplTest, RaiseActivatesWebContents) {
  MockWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  // When the WebContents has a delegate, |Raise()| should activate the
  // WebContents.
  EXPECT_CALL(delegate, ActivateContents(web_contents()));
  GetMediaSession()->Raise();
  testing::Mock::VerifyAndClearExpectations(&delegate);

  // When the WebContents does not have a delegate, |Raise()| should not crash.
  web_contents()->SetDelegate(nullptr);
  GetMediaSession()->Raise();
}

TEST_F(MediaSessionImplTest,
       RegisteredEnterPictureInPictureExposesAutoPictureInPicture) {
  // When the website has registered for 'enterpictureinpicture',
  // MediaSessionImpl should expose the kEnterPictureInPicture,
  // kEnterAutoPictureInPicture, and kExitPictureInPicture to the internal
  // MediaSession service.
  StartNewPlayer();
  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  mock_media_session_service().EnableAction(
      MediaSessionAction::kEnterPictureInPicture);
  mock_media_session_service().FlushForTesting();

  EXPECT_TRUE(base::Contains(observer.actions(),
                             MediaSessionAction::kEnterPictureInPicture));
  EXPECT_TRUE(base::Contains(observer.actions(),
                             MediaSessionAction::kEnterAutoPictureInPicture));
  EXPECT_TRUE(base::Contains(observer.actions(),
                             MediaSessionAction::kExitPictureInPicture));
}

TEST_F(MediaSessionImplTest, WebContentsHasPictureInPictureVideo) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetHasPictureInPictureVideo(true);

  StartNewPlayer();
  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  mock_media_session_service().EnableAction(MediaSessionAction::kPause);
  mock_media_session_service().FlushForTesting();

  EXPECT_FALSE(base::Contains(observer.actions(),
                              MediaSessionAction::kEnterPictureInPicture));
  EXPECT_TRUE(base::Contains(observer.actions(),
                             MediaSessionAction::kExitPictureInPicture));
}

TEST_F(MediaSessionImplTest, WebContentsHasPictureInPictureDocument) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetHasPictureInPictureDocument(true);

  StartNewPlayer();
  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  mock_media_session_service().EnableAction(MediaSessionAction::kPause);
  mock_media_session_service().FlushForTesting();

  EXPECT_FALSE(base::Contains(observer.actions(),
                              MediaSessionAction::kEnterPictureInPicture));
  EXPECT_TRUE(base::Contains(observer.actions(),
                             MediaSessionAction::kExitPictureInPicture));
}

TEST_F(MediaSessionImplTest, SufficientlyVisibleVideo_NoPlayer) {
  OnVideoVisibilityChanged();

  EXPECT_FALSE(HasSufficientlyVisibleVideo());
}

TEST_F(MediaSessionImplTest, SufficientlyVisibleVideo_MultiplePlayers) {
  // Start with a single player with a video reporting as sufficiently visible.
  int player1 = player_observer_->StartNewPlayer();
  player_observer_->SetHasSufficientlyVisibleVideo(player1, true);
  GetMediaSession()->AddPlayer(player_observer_.get(), player1);

  OnVideoVisibilityChanged();
  EXPECT_TRUE(HasSufficientlyVisibleVideo());

  // Add a second player with with a video reporting as sufficiently visible,
  // and make player1 report that its video is not sufficiently visible.
  int player2 = player_observer_->StartNewPlayer();
  player_observer_->SetHasSufficientlyVisibleVideo(player2, true);
  GetMediaSession()->AddPlayer(player_observer_.get(), player2);

  player_observer_->SetHasSufficientlyVisibleVideo(player1, false);

  OnVideoVisibilityChanged();
  EXPECT_TRUE(HasSufficientlyVisibleVideo());

  // Make player2 report that its video is not sufficiently visible.
  player_observer_->SetHasSufficientlyVisibleVideo(player2, false);

  OnVideoVisibilityChanged();
  EXPECT_FALSE(HasSufficientlyVisibleVideo());
}

TEST_F(MediaSessionImplTest, SessionInfoDontHideMetadataByDefault) {
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                   ->hide_metadata);
}

TEST_F(MediaSessionImplTest, PausedPlayersDoNotRequestFocus) {
  // If a player is paused when it's added, it should be controllable but should
  // not request audio focus.
  MockAudioFocusDelegate* delegate = new MockAudioFocusDelegate();
  SetDelegateForTests(GetMediaSession(), delegate);
  int player_id = StartNewPlayer();
  EXPECT_TRUE(GetMediaSession()->IsActive());
  EXPECT_TRUE(GetMediaSession()->IsControllable());
  EXPECT_EQ(delegate->request_audio_focus_count(), 1);
  player_observer()->SetPlaying(player_id, false);
  // Remember that the player still has the audio focus.  Re-adding the paused
  // player should neither lose the focus, nor re-request it.
  GetMediaSession()->AddPlayer(player_observer(), player_id);
  EXPECT_EQ(delegate->request_audio_focus_count(), 1);
  EXPECT_TRUE(GetMediaSession()->IsActive());

  // Give up audio focus.
  GetMediaSession()->RemovePlayer(player_observer(), player_id);
  EXPECT_FALSE(GetMediaSession()->IsActive());
  EXPECT_FALSE(GetMediaSession()->IsControllable());

  // Adding should now result in an inactive session that is controllable,
  // without requesting focus again.
  GetMediaSession()->AddPlayer(player_observer(), player_id);
  EXPECT_EQ(delegate->request_audio_focus_count(), 1);
  EXPECT_FALSE(GetMediaSession()->IsActive());
  EXPECT_TRUE(GetMediaSession()->IsControllable());
}

TEST_F(MediaSessionImplTest, SeekingAndScrubbingNotAllowedWithMaxDuration) {
  MockMediaSessionMojoObserver observer(*GetMediaSession());
  int player_id = player_observer_->StartNewPlayer();
  GetMediaSession()->AddPlayer(player_observer_.get(), player_id);

  media_session::MediaPosition pos;
  pos = media_session::MediaPosition(
      /*playback_rate=*/1.0,
      /*duration=*/base::TimeDelta::Max(),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);

  player_observer_->SetPosition(player_id, pos);
  GetMediaSession()->RebuildAndNotifyMediaPositionChanged();
  FlushForTesting(GetMediaSession());

  // With a max duration, we should be considered live media and should not
  // allow seeking and scrubbing actions by default.
  EXPECT_FALSE(base::Contains(observer.actions(), MediaSessionAction::kSeekTo));
  EXPECT_FALSE(
      base::Contains(observer.actions(), MediaSessionAction::kScrubTo));
  EXPECT_FALSE(
      base::Contains(observer.actions(), MediaSessionAction::kSeekForward));
  EXPECT_FALSE(
      base::Contains(observer.actions(), MediaSessionAction::kSeekBackward));

  // However, if the website explicitly supports the action, then we will still
  // route it.
  mock_media_session_service().EnableAction(MediaSessionAction::kSeekTo);
  FlushForTesting(GetMediaSession());

  EXPECT_TRUE(base::Contains(observer.actions(), MediaSessionAction::kSeekTo));
  EXPECT_FALSE(
      base::Contains(observer.actions(), MediaSessionAction::kScrubTo));
  EXPECT_FALSE(
      base::Contains(observer.actions(), MediaSessionAction::kSeekForward));
  EXPECT_FALSE(
      base::Contains(observer.actions(), MediaSessionAction::kSeekBackward));
}

class MediaSessionImplWithMediaSessionClientTest : public MediaSessionImplTest {
 protected:
  TestMediaSessionClient client_;
};

TEST_F(MediaSessionImplWithMediaSessionClientTest,
       SessionInfoDontHideMetadata) {
  client_.SetShouldHideMetadata(false);
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                   ->hide_metadata);
}

TEST_F(MediaSessionImplWithMediaSessionClientTest, SessionInfoHideMetadata) {
  client_.SetShouldHideMetadata(true);
  EXPECT_TRUE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                  ->hide_metadata);
}

class MediaSessionImplIsSensitive : public MediaSessionImplTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  MediaSessionImplIsSensitive()
      : is_incognito_media_feature_enabled_(GetParam()) {}

  void SetUp() override {
    MediaSessionImplTest::SetUp();

    if (is_incognito_media_feature_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          media::kHideIncognitoMediaMetadata);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          media::kHideIncognitoMediaMetadata);
    }
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    MediaSessionImplTest::TearDown();
  }

  bool is_incognito_media_feature_enabled_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(MediaSessionImplIsSensitive,
                         MediaSessionImplIsSensitive,
                         testing::Bool());

TEST_P(MediaSessionImplIsSensitive, SessionInfo_IsSensitive_NormalSession) {
  EXPECT_FALSE(browser_context()->IsOffTheRecord());

  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                   ->is_sensitive);
}

TEST_P(MediaSessionImplIsSensitive, SessionInfo_IsSensitive_OffTheRecord) {
  auto other_context = std::make_unique<TestBrowserContext>();
  other_context->set_is_off_the_record(true);
  auto other_contents = TestWebContents::Create(other_context.get(), nullptr);
  MediaSessionImpl* other_session = MediaSessionImpl::Get(other_contents.get());

  EXPECT_TRUE(other_context->IsOffTheRecord());
  EXPECT_EQ(!is_incognito_media_feature_enabled_,
            media_session::test::GetMediaSessionInfoSync(other_session)
                ->is_sensitive);
}

// Tests for throttling duration updates.
// TODO (jazzhsu): Remove these tests once media session supports livestream.
class MediaSessionImplDurationThrottleTest : public MediaSessionImplTest {
 public:
  void SetUp() override {
    MediaSessionImplTest::SetUp();
    GetMediaSession()->SetShouldThrottleDurationUpdateForTest(true);
  }

  void FastForwardBy(base::TimeDelta delay) {
    task_environment()->FastForwardBy(delay);
  }

  int GetDurationUpdateMaxAllowance() {
    return MediaSessionImpl::kDurationUpdateMaxAllowance;
  }

  base::TimeDelta GetDurationUpdateAllowanceIncreaseInterval() {
    return MediaSessionImpl::kDurationUpdateAllowanceIncreaseInterval;
  }
};

TEST_F(MediaSessionImplDurationThrottleTest, ThrottleDurationUpdate) {
  MockMediaSessionMojoObserver observer(*GetMediaSession());
  int player_id = player_observer_->StartNewPlayer();
  GetMediaSession()->AddPlayer(player_observer_.get(), player_id);

  media_session::MediaPosition pos;
  for (int duration = 0; duration <= GetDurationUpdateMaxAllowance();
       ++duration) {
    pos = media_session::MediaPosition(
        /*playback_rate=*/0.0,
        /*duration=*/base::Seconds(duration),
        /*position=*/base::TimeDelta(), /*end_of_media=*/false);

    player_observer_->SetPosition(player_id, pos);
    GetMediaSession()->RebuildAndNotifyMediaPositionChanged();
    FlushForTesting(GetMediaSession());

    if (duration == GetDurationUpdateMaxAllowance()) {
      // Last update should be throttle and marked as +INF duration.
      EXPECT_EQ(**observer.session_position(),
                media_session::MediaPosition(
                    /*playback_rate=*/0.0,
                    /*duration=*/base::TimeDelta::Max(),
                    /*position=*/base::TimeDelta(), /*end_of_media=*/false));

      // Since we're now considered live, the seeking and scrubbing actions
      // should no longer be available.
      EXPECT_FALSE(
          base::Contains(observer.actions(), MediaSessionAction::kSeekTo));
      EXPECT_FALSE(
          base::Contains(observer.actions(), MediaSessionAction::kScrubTo));
      EXPECT_FALSE(
          base::Contains(observer.actions(), MediaSessionAction::kSeekForward));
      EXPECT_FALSE(base::Contains(observer.actions(),
                                  MediaSessionAction::kSeekBackward));
    } else {
      EXPECT_EQ(**observer.session_position(), pos);

      // If we're not considered live, then the seeking and scrubbing actions
      // should still be available.
      EXPECT_TRUE(
          base::Contains(observer.actions(), MediaSessionAction::kSeekTo));
      EXPECT_TRUE(
          base::Contains(observer.actions(), MediaSessionAction::kScrubTo));
      EXPECT_TRUE(
          base::Contains(observer.actions(), MediaSessionAction::kSeekForward));
      EXPECT_TRUE(base::Contains(observer.actions(),
                                 MediaSessionAction::kSeekBackward));
    }
  }

  // Media session should send last throttled duration update after certain
  // delay.
  FastForwardBy(GetDurationUpdateAllowanceIncreaseInterval());
  FlushForTesting(GetMediaSession());
  EXPECT_EQ(**observer.session_position(), pos);
}

TEST_F(MediaSessionImplDurationThrottleTest, ThrottleResetOnPlayerChange) {
  MockMediaSessionMojoObserver observer(*GetMediaSession());

  // Duration updates caused by player switch should not be throttled.
  media_session::MediaPosition pos;
  for (int duration = 0; duration <= GetDurationUpdateMaxAllowance();
       ++duration) {
    int player_id = player_observer_->StartNewPlayer();
    GetMediaSession()->AddPlayer(player_observer_.get(), player_id);

    pos = media_session::MediaPosition(
        /*playback_rate=*/0.0,
        /*duration=*/base::Seconds(duration),
        /*position=*/base::TimeDelta(), /*end_of_media=*/false);

    player_observer_->SetPosition(player_id, pos);
    GetMediaSession()->RebuildAndNotifyMediaPositionChanged();
    FlushForTesting(GetMediaSession());
    EXPECT_EQ(**observer.session_position(), pos);

    GetMediaSession()->RemovePlayer(player_observer_.get(), player_id);
  }
}

}  // namespace content
