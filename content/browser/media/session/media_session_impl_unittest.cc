// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/system_connector.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

using ::testing::_;

namespace content {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using media_session::mojom::MediaPlaybackState;
using media_session::test::MockMediaSessionMojoObserver;
using media_session::test::TestAudioFocusObserver;

namespace {

class MockAudioFocusDelegate : public AudioFocusDelegate {
 public:
  MockAudioFocusDelegate() = default;
  ~MockAudioFocusDelegate() override = default;

  void AbandonAudioFocus() override {}

  AudioFocusResult RequestAudioFocus(AudioFocusType type) override {
    request_audio_focus_count_++;
    return AudioFocusResult::kSuccess;
  }

  base::Optional<AudioFocusType> GetCurrentFocusType() const override {
    return AudioFocusType::kGain;
  }

  void MediaSessionInfoChanged(MediaSessionInfoPtr session_info) override {
    session_info_ = std::move(session_info);
  }

  MOCK_CONST_METHOD0(request_id, const base::UnguessableToken&());

  MediaSessionInfo::SessionState GetState() const {
    DCHECK(!session_info_.is_null());
    return session_info_->state;
  }

  int request_audio_focus_count() const { return request_audio_focus_count_; }

 private:
  int request_audio_focus_count_ = 0;

  MediaSessionInfoPtr session_info_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioFocusDelegate);
};

}  // anonymous namespace

class MediaSessionImplTest : public RenderViewHostTestHarness {
 public:
  MediaSessionImplTest() {
    default_actions_.insert(media_session::mojom::MediaSessionAction::kPlay);
    default_actions_.insert(media_session::mojom::MediaSessionAction::kPause);
    default_actions_.insert(media_session::mojom::MediaSessionAction::kStop);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media_session::features::kMediaSessionService,
         media_session::features::kAudioFocusEnforcement},
        {});

    RenderViewHostTestHarness::SetUp();

    player_observer_.reset(new MockMediaSessionPlayerObserver(main_rfh()));
    mock_media_session_service_.reset(
        new testing::NiceMock<MockMediaSessionServiceImpl>(main_rfh()));

    // Connect to the Media Session service and bind |audio_focus_remote_| to
    // it.
    service_manager_context_ = std::make_unique<TestServiceManagerContext>();
    GetSystemConnector()->Connect(
        media_session::mojom::kServiceName,
        audio_focus_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    mock_media_session_service_.reset();
    service_manager_context_.reset();

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

  void StartNewPlayer() {
    GetMediaSession()->AddPlayer(player_observer_.get(),
                                 player_observer_->StartNewPlayer(),
                                 media::MediaContentType::Persistent);
  }

  const std::set<media_session::mojom::MediaSessionAction>& default_actions()
      const {
    return default_actions_;
  }

 private:
  std::set<media_session::mojom::MediaSessionAction> default_actions_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<MockMediaSessionServiceImpl> mock_media_session_service_;

  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;

  std::unique_ptr<TestServiceManagerContext> service_manager_context_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionImplTest);
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

    EXPECT_TRUE(observer.session_info().Equals(
        media_session::test::GetMediaSessionInfoSync(GetMediaSession())));
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
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->AddPlayer(player_observer_.get(), player_id,
                                 media::MediaContentType::Pepper);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
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
    GetMediaSession()->AddPlayer(player_observer_.get(), player_id,
                                 media::MediaContentType::Persistent);
    observer.WaitForPlaybackState(MediaPlaybackState::kPlaying);
  }

  {
    MockMediaSessionMojoObserver observer(*GetMediaSession());
    GetMediaSession()->OnPlayerPaused(player_observer_.get(), player_id);
    observer.WaitForPlaybackState(MediaPlaybackState::kPaused);
  }
}

TEST_F(MediaSessionImplTest, SuspendUI) {
  EXPECT_CALL(
      mock_media_session_service().mock_client(),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPause, _))
      .Times(0);

  StartNewPlayer();

  GetMediaSession()->Suspend(MediaSession::SuspendType::kUI);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, SuspendContent_WithAction) {
  EXPECT_CALL(
      mock_media_session_service().mock_client(),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPause, _))
      .Times(0);

  StartNewPlayer();
  mock_media_session_service().EnableAction(
      media_session::mojom::MediaSessionAction::kPause);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kContent);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, SuspendSystem_WithAction) {
  EXPECT_CALL(
      mock_media_session_service().mock_client(),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPause, _))
      .Times(0);

  StartNewPlayer();
  mock_media_session_service().EnableAction(
      media_session::mojom::MediaSessionAction::kPause);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, SuspendUI_WithAction) {
  EXPECT_CALL(
      mock_media_session_service().mock_client(),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPause, _));

  StartNewPlayer();
  mock_media_session_service().EnableAction(
      media_session::mojom::MediaSessionAction::kPause);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kUI);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, ResumeUI) {
  EXPECT_CALL(
      mock_media_session_service().mock_client(),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPlay, _))
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
  EXPECT_CALL(
      mock_media_session_service().mock_client(),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPlay, _))
      .Times(0);

  StartNewPlayer();
  mock_media_session_service().EnableAction(
      media_session::mojom::MediaSessionAction::kPlay);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  GetMediaSession()->Resume(MediaSession::SuspendType::kContent);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, ResumeSystem_WithAction) {
  EXPECT_CALL(
      mock_media_session_service().mock_client(),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPlay, _))
      .Times(0);

  StartNewPlayer();
  mock_media_session_service().EnableAction(
      media_session::mojom::MediaSessionAction::kPlay);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  GetMediaSession()->Resume(MediaSession::SuspendType::kSystem);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplTest, ResumeUI_WithAction) {
  EXPECT_CALL(
      mock_media_session_service().mock_client(),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPlay, _));

  StartNewPlayer();
  mock_media_session_service().EnableAction(
      media_session::mojom::MediaSessionAction::kPlay);

  GetMediaSession()->Suspend(MediaSession::SuspendType::kSystem);
  GetMediaSession()->Resume(MediaSession::SuspendType::kUI);
  mock_media_session_service().FlushForTesting();

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

#if !defined(OS_ANDROID)

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

#if defined(OS_MACOSX)

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

#else  // defined(OS_MACOSX)

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

#endif  // defined(OS_MACOSX)

#endif  // !defined(OS_ANDROID)

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

TEST_F(MediaSessionImplTest, SessionInfoSensitive) {
  EXPECT_FALSE(browser_context()->IsOffTheRecord());
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(GetMediaSession())
                   ->is_sensitive);
}

TEST_F(MediaSessionImplTest, SessionInfoSensitive_OffTheRecord) {
  auto other_context = std::make_unique<TestBrowserContext>();
  other_context->set_is_off_the_record(true);
  auto other_contents = TestWebContents::Create(other_context.get(), nullptr);
  MediaSessionImpl* other_session = MediaSessionImpl::Get(other_contents.get());

  EXPECT_TRUE(other_context->IsOffTheRecord());
  EXPECT_TRUE(media_session::test::GetMediaSessionInfoSync(other_session)
                  ->is_sensitive);
}

}  // namespace content
