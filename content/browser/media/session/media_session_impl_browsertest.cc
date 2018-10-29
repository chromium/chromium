// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <stddef.h>

#include <list>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/media/session/mock_media_session_observer.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::WebContents;
using content::MediaSession;
using content::MediaSessionImpl;
using content::MediaSessionObserver;
using content::AudioFocusDelegate;
using content::MediaSessionPlayerObserver;
using content::MediaSessionUmaHelper;
using content::MockMediaSessionPlayerObserver;

using media_session::mojom::AudioFocusType;

using ::testing::Eq;
using ::testing::Expectation;
using ::testing::NiceMock;
using ::testing::_;

namespace {

const double kDefaultVolumeMultiplier = 1.0;
const double kDuckingVolumeMultiplier = 0.2;
const double kDifferentDuckingVolumeMultiplier = 0.018;

class MockAudioFocusDelegate : public AudioFocusDelegate {
 public:
  MockAudioFocusDelegate(MediaSessionImpl* media_session, bool async_mode)
      : media_session_(media_session), async_mode_(async_mode) {}

  MOCK_METHOD0(AbandonAudioFocus, void());

  AudioFocusDelegate::AudioFocusResult RequestAudioFocus(
      AudioFocusType audio_focus_type) {
    if (async_mode_) {
      requests_.push_back(audio_focus_type);
      return AudioFocusDelegate::AudioFocusResult::kDelayed;
    } else {
      audio_focus_type_ = audio_focus_type;
      return AudioFocusDelegate::AudioFocusResult::kSuccess;
    }
  }

  base::Optional<AudioFocusType> GetCurrentFocusType() const {
    return audio_focus_type_;
  }

  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override {}

  void ResolveRequest(bool result) {
    if (!async_mode_)
      return;

    audio_focus_type_ = requests_.front();
    requests_.pop_front();

    media_session_->FinishSystemAudioFocusRequest(audio_focus_type_.value(),
                                                  result);
  }

  bool HasRequests() const { return !requests_.empty(); }

 private:
  MediaSessionImpl* media_session_;
  const bool async_mode_ = false;

  std::list<AudioFocusType> requests_;
  base::Optional<AudioFocusType> audio_focus_type_;
};

class MockMediaSessionServiceImpl : public content::MediaSessionServiceImpl {
 public:
  explicit MockMediaSessionServiceImpl(content::RenderFrameHost* rfh)
      : MediaSessionServiceImpl(rfh) {}
  ~MockMediaSessionServiceImpl() override = default;
};

}  // namespace

class MediaSessionImplBrowserTest : public content::ContentBrowserTest {
 protected:
  MediaSessionImplBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    media_session_ = MediaSessionImpl::Get(shell()->web_contents());
    mock_media_session_observer_.reset(
        new NiceMock<content::MockMediaSessionObserver>(media_session_));
    mock_audio_focus_delegate_ = new NiceMock<MockAudioFocusDelegate>(
        media_session_, true /* async_mode */);
    media_session_->SetDelegateForTests(
        base::WrapUnique(mock_audio_focus_delegate_));
    ASSERT_TRUE(media_session_);
  }

  void TearDownOnMainThread() override {
    mock_media_session_observer_.reset();
    media_session_->RemoveAllPlayersForTest();
    mock_media_session_service_.reset();

    media_session_ = nullptr;

    ContentBrowserTest::TearDownOnMainThread();
  }

  void StartNewPlayer(MockMediaSessionPlayerObserver* player_observer,
                      media::MediaContentType media_content_type) {
    int player_id = player_observer->StartNewPlayer();

    bool result = AddPlayer(player_observer, player_id, media_content_type);

    EXPECT_TRUE(result);
  }

  bool AddPlayer(MockMediaSessionPlayerObserver* player_observer,
                 int player_id,
                 media::MediaContentType type) {
    return media_session_->AddPlayer(player_observer, player_id, type);
  }

  void RemovePlayer(MockMediaSessionPlayerObserver* player_observer,
                    int player_id) {
    media_session_->RemovePlayer(player_observer, player_id);
  }

  void RemovePlayers(MockMediaSessionPlayerObserver* player_observer) {
    media_session_->RemovePlayers(player_observer);
  }

  void OnPlayerPaused(MockMediaSessionPlayerObserver* player_observer,
                      int player_id) {
    media_session_->OnPlayerPaused(player_observer, player_id);
  }

  bool IsActive() { return media_session_->IsActive(); }

  base::Optional<AudioFocusType> GetSessionAudioFocusType() {
    return mock_audio_focus_delegate_->GetCurrentFocusType();
  }

  bool IsControllable() { return media_session_->IsControllable(); }

  void UIResume() { media_session_->Resume(MediaSession::SuspendType::kUI); }

  void SystemResume() {
    media_session_->OnResumeInternal(MediaSession::SuspendType::kSystem);
  }

  void UISuspend() { media_session_->Suspend(MediaSession::SuspendType::kUI); }

  void SystemSuspend(bool temporary) {
    media_session_->OnSuspendInternal(MediaSession::SuspendType::kSystem,
                                      temporary
                                          ? MediaSessionImpl::State::SUSPENDED
                                          : MediaSessionImpl::State::INACTIVE);
  }

  void UISeekForward() {
    media_session_->SeekForward(base::TimeDelta::FromSeconds(1));
  }

  void UISeekBackward() {
    media_session_->SeekBackward(base::TimeDelta::FromSeconds(1));
  }

  void SystemStartDucking() { media_session_->StartDucking(); }

  void SystemStopDucking() { media_session_->StopDucking(); }

  void EnsureMediaSessionService() {
    mock_media_session_service_.reset(new NiceMock<MockMediaSessionServiceImpl>(
        shell()->web_contents()->GetMainFrame()));
  }

  void SetPlaybackState(blink::mojom::MediaSessionPlaybackState state) {
    mock_media_session_service_->SetPlaybackState(state);
  }

  void ResolveAudioFocusSuccess() {
    mock_audio_focus_delegate()->ResolveRequest(true /* result */);
  }

  void ResolveAudioFocusFailure() {
    mock_audio_focus_delegate()->ResolveRequest(false /* result */);
  }

  bool HasUnresolvedAudioFocusRequest() {
    return mock_audio_focus_delegate()->HasRequests();
  }

  content::MockMediaSessionObserver* mock_media_session_observer() {
    return mock_media_session_observer_.get();
  }

  MockAudioFocusDelegate* mock_audio_focus_delegate() {
    return mock_audio_focus_delegate_;
  }

  std::unique_ptr<MediaSessionImpl> CreateDummyMediaSession() {
    return base::WrapUnique<MediaSessionImpl>(new MediaSessionImpl(nullptr));
  }

  MediaSessionUmaHelper* GetMediaSessionUMAHelper() {
    return media_session_->uma_helper_for_test();
  }

  void SetAudioFocusDelegateForTests(MockAudioFocusDelegate* delegate) {
    mock_audio_focus_delegate_ = delegate;
    media_session_->SetDelegateForTests(
        base::WrapUnique(mock_audio_focus_delegate_));
  }

  bool IsDucking() const { return media_session_->is_ducking_; }

 protected:
  MediaSessionImpl* media_session_;
  std::unique_ptr<content::MockMediaSessionObserver>
      mock_media_session_observer_;
  MockAudioFocusDelegate* mock_audio_focus_delegate_;
  std::unique_ptr<MockMediaSessionServiceImpl> mock_media_session_service_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionImplBrowserTest);
};

class MediaSessionImplParamBrowserTest
    : public MediaSessionImplBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  MediaSessionImplParamBrowserTest() = default;

  void SetUpOnMainThread() override {
    MediaSessionImplBrowserTest::SetUpOnMainThread();

    SetAudioFocusDelegateForTests(
        new NiceMock<MockAudioFocusDelegate>(media_session_, GetParam()));
  }
};

INSTANTIATE_TEST_CASE_P(, MediaSessionImplParamBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       PlayersFromSameObserverDoNotStopEachOtherInSameSession) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       PlayersFromManyObserverDoNotStopEachOtherInSameSession) {
  auto player_observer_1 = std::make_unique<MockMediaSessionPlayerObserver>();
  auto player_observer_2 = std::make_unique<MockMediaSessionPlayerObserver>();
  auto player_observer_3 = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer_1.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_2.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_3.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(player_observer_1->IsPlaying(0));
  EXPECT_TRUE(player_observer_2->IsPlaying(0));
  EXPECT_TRUE(player_observer_3->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SuspendedMediaSessionStopsPlayers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumedMediaSessionRestartsPlayers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StartedPlayerOnSuspendedSessionPlaysAlone) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       InitialVolumeMultiplier) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));

  ResolveAudioFocusSuccess();

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StartDuckingReducesVolumeMultiplier) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemStartDucking();

  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(1));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StopDuckingRecoversVolumeMultiplier) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemStartDucking();
  SystemStopDucking();

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       DuckingUsesConfiguredMultiplier) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  media_session_->SetDuckingVolumeMultiplier(kDifferentDuckingVolumeMultiplier);
  SystemStartDucking();
  EXPECT_EQ(kDifferentDuckingVolumeMultiplier,
            player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDifferentDuckingVolumeMultiplier,
            player_observer->GetVolumeMultiplier(1));
  SystemStopDucking();
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AudioFocusInitialState) {
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AddPlayerOnSuspendedFocusUnducks) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  UISuspend();
  EXPECT_FALSE(IsActive());

  SystemStartDucking();
  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(0));

  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 0, media::MediaContentType::Persistent));
  ResolveAudioFocusSuccess();
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       CanRequestFocusBeforePlayerCreation) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  media_session_->RequestSystemAudioFocus(AudioFocusType::kGain);
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StartPlayerGivesFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SuspendGivesAwayAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(true);

  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StopGivesAwayAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  media_session_->Stop(MediaSession::SuspendType::kUI);

  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SystemResumeGivesBackAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UIResumeGivesBackAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  UISuspend();

  UIResume();
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingLastPlayerDropsAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);
  EXPECT_TRUE(IsActive());
  RemovePlayer(player_observer.get(), 1);
  EXPECT_TRUE(IsActive());
  RemovePlayer(player_observer.get(), 2);
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingLastPlayerFromManyObserversDropsAudioFocus) {
  auto player_observer_1 = std::make_unique<MockMediaSessionPlayerObserver>();
  auto player_observer_2 = std::make_unique<MockMediaSessionPlayerObserver>();
  auto player_observer_3 = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer_1.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_2.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_3.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer_1.get(), 0);
  EXPECT_TRUE(IsActive());
  RemovePlayer(player_observer_2.get(), 0);
  EXPECT_TRUE(IsActive());
  RemovePlayer(player_observer_3.get(), 0);
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingAllPlayersFromObserversDropsAudioFocus) {
  auto player_observer_1 = std::make_unique<MockMediaSessionPlayerObserver>();
  auto player_observer_2 = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer_1.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_1.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_2.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_2.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayers(player_observer_1.get());
  EXPECT_TRUE(IsActive());
  RemovePlayers(player_observer_2.get());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumePlayGivesAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);
  EXPECT_FALSE(IsActive());

  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 0, media::MediaContentType::Persistent));
  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumeSuspendSeekAreSentOnlyOncePerPlayers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  ResolveAudioFocusSuccess();

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());
  EXPECT_EQ(0, player_observer->received_seek_forward_calls());
  EXPECT_EQ(0, player_observer->received_seek_backward_calls());

  SystemSuspend(true);
  EXPECT_EQ(3, player_observer->received_suspend_calls());

  SystemResume();
  EXPECT_EQ(3, player_observer->received_resume_calls());

  UISeekForward();
  EXPECT_EQ(3, player_observer->received_seek_forward_calls());

  UISeekBackward();
  EXPECT_EQ(3, player_observer->received_seek_backward_calls());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumeSuspendSeekAreSentOnlyOncePerPlayersAddedTwice) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  ResolveAudioFocusSuccess();

  // Adding the three players above again.
  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 0, media::MediaContentType::Persistent));
  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 1, media::MediaContentType::Persistent));
  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 2, media::MediaContentType::Persistent));

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());
  EXPECT_EQ(0, player_observer->received_seek_forward_calls());
  EXPECT_EQ(0, player_observer->received_seek_backward_calls());

  SystemSuspend(true);
  EXPECT_EQ(3, player_observer->received_suspend_calls());

  SystemResume();
  EXPECT_EQ(3, player_observer->received_resume_calls());

  UISeekForward();
  EXPECT_EQ(3, player_observer->received_seek_forward_calls());

  UISeekBackward();
  EXPECT_EQ(3, player_observer->received_seek_backward_calls());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingTheSamePlayerTwiceIsANoop) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);
  RemovePlayer(player_observer.get(), 0);
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest, AudioFocusType) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  // Starting a player with a given type should set the session to that type.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  ResolveAudioFocusSuccess();
  EXPECT_EQ(AudioFocusType::kGainTransientMayDuck, GetSessionAudioFocusType());

  // Adding a player of the same type should have no effect on the type.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());
  EXPECT_EQ(AudioFocusType::kGainTransientMayDuck, GetSessionAudioFocusType());

  // Adding a player of Content type should override the current type.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  EXPECT_EQ(AudioFocusType::kGain, GetSessionAudioFocusType());

  // Adding a player of the Transient type should have no effect on the type.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());
  EXPECT_EQ(AudioFocusType::kGain, GetSessionAudioFocusType());

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
  EXPECT_TRUE(player_observer->IsPlaying(3));

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(player_observer->IsPlaying(2));
  EXPECT_FALSE(player_observer->IsPlaying(3));

  EXPECT_EQ(AudioFocusType::kGain, GetSessionAudioFocusType());

  SystemResume();

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
  EXPECT_TRUE(player_observer->IsPlaying(3));

  EXPECT_EQ(AudioFocusType::kGain, GetSessionAudioFocusType());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShowForContent) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false));

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  // Starting a player with a content type should show the media controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsNoShowForTransient) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, false));

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  // Starting a player with a transient type should not show the media controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  ResolveAudioFocusSuccess();

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenStopped) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayers(player_observer.get());

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShownAcceptTransient) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false));

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  // Transient player join the session without affecting the controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShownAfterContentAdded) {
  Expectation dontShowControls = EXPECT_CALL(
      *mock_media_session_observer(), MediaSessionStateChanged(false, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(dontShowControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  ResolveAudioFocusSuccess();

  // The controls are shown when the content player is added.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsStayIfOnlyOnePlayerHasBeenPaused) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false));

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  // Removing only content player doesn't hide the controls since the session
  // is still active.
  RemovePlayer(player_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenTheLastPlayerIsRemoved) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());

  RemovePlayer(player_observer.get(), 1);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenAllThePlayersAreRemoved) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayers(player_observer.get());

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsNotHideWhenTheLastPlayerIsPaused) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .After(showControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  OnPlayerPaused(player_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());

  OnPlayerPaused(player_observer.get(), 1);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SuspendTemporaryUpdatesControls) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .After(showControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(true);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsUpdatedWhenResumed) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenSessionSuspendedPermanently) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(false);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenSessionStops) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(pauseControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  media_session_->Stop(MediaSession::SuspendType::kUI);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenSessionChangesFromContentToTransient) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, false))
      .After(pauseControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  SystemSuspend(true);

  // This should reset the session and change it to a transient, so
  // hide the controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  ResolveAudioFocusSuccess();

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsUpdatedWhenNewPlayerResetsSession) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  SystemSuspend(true);

  // This should reset the session and update the controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsResumedWhenPlayerIsResumed) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  SystemSuspend(true);

  // This should resume the session and update the controls.
  AddPlayer(player_observer.get(), 0, media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsUpdatedDueToResumeSessionAction) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .After(showControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  UISuspend();

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsUpdatedDueToSuspendSessionAction) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  UISuspend();

  UIResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsDontShowWhenOneShotIsPresent) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, false));

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::OneShot);
  ResolveAudioFocusSuccess();

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());

  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHiddenAfterRemoveOneShotWithoutOtherPlayers) {
  Expectation expect_1 = EXPECT_CALL(*mock_media_session_observer(),
                                     MediaSessionStateChanged(false, false));
  Expectation expect_2 = EXPECT_CALL(*mock_media_session_observer(),
                                     MediaSessionStateChanged(false, true))
                             .After(expect_1);
  EXPECT_CALL(*mock_media_session_observer(), MediaSessionStateChanged(true, _))
      .Times(0)
      .After(expect_2);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::OneShot);
  ResolveAudioFocusSuccess();
  RemovePlayer(player_observer.get(), 0);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShowAfterRemoveOneShotWithPersistentPresent) {
  Expectation uncontrollable = EXPECT_CALL(
      *mock_media_session_observer(), MediaSessionStateChanged(false, false));

  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(uncontrollable);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::OneShot);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       DontSuspendWhenOneShotIsPresent) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::OneShot);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(false);

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());

  EXPECT_EQ(0, player_observer->received_suspend_calls());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       DontResumeBySystemUISuspendedSessions) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  UISuspend();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());

  SystemResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AllowUIResumeForSystemSuspend) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());

  UIResume();
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest, ResumeSuspendFromUI) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  UISuspend();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());

  UIResume();
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumeSuspendFromSystem) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());

  SystemResume();
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       OneShotTakesGainFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::OneShot);
  ResolveAudioFocusSuccess();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());

  EXPECT_EQ(AudioFocusType::kGain,
            mock_audio_focus_delegate()->GetCurrentFocusType());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingOneShotDropsFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  EXPECT_CALL(*mock_audio_focus_delegate(), AbandonAudioFocus());
  StartNewPlayer(player_observer.get(), media::MediaContentType::OneShot);
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingOneShotWhileStillHavingOtherPlayersKeepsFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  EXPECT_CALL(*mock_audio_focus_delegate(), AbandonAudioFocus())
      .Times(1);  // Called in TearDown
  StartNewPlayer(player_observer.get(), media::MediaContentType::OneShot);
  ResolveAudioFocusSuccess();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());

  RemovePlayer(player_observer.get(), 0);
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ActualPlaybackStateWhilePlayerPaused) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetMainFrame());

  ::testing::Sequence s;
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .InSequence(s);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .InSequence(s);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .InSequence(s);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .InSequence(s);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .InSequence(s);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  OnPlayerPaused(player_observer.get(), 0);
  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PLAYING);
  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PAUSED);
  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::NONE);

  // Verify before test exists. Otherwise the sequence will expire and cause
  // weird problems.
  ::testing::Mock::VerifyAndClear(mock_media_session_observer());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ActualPlaybackStateWhilePlayerPlaying) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetMainFrame());
  ::testing::Sequence s;
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .InSequence(s);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .InSequence(s);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .InSequence(s);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .InSequence(s);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PLAYING);
  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PAUSED);
  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::NONE);

  // Verify before test exists. Otherwise the sequence will expire and cause
  // weird problems.
  ::testing::Mock::VerifyAndClear(mock_media_session_observer());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ActualPlaybackStateWhilePlayerRemoved) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetMainFrame());

  ::testing::Sequence s;
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .InSequence(s);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, _))
      .InSequence(s);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  RemovePlayer(player_observer.get(), 0);

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PLAYING);
  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PAUSED);
  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::NONE);

  // Verify before test exists. Otherwise the sequence will expire and cause
  // weird problems.
  ::testing::Mock::VerifyAndClear(mock_media_session_observer());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_Suspended_SystemTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  SystemSuspend(true);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(0, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_Suspended_SystemPermantent) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  SystemSuspend(false);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(1, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(0, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest, UMA_Suspended_UI) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  UISuspend();

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_Suspended_Multiple) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  UISuspend();
  UIResume();
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemResume();

  UISuspend();
  UIResume();
  ResolveAudioFocusSuccess();

  SystemSuspend(false);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(4, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(1, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(2, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_Suspended_Crossing) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  UISuspend();
  SystemSuspend(true);
  SystemSuspend(false);
  UIResume();
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemSuspend(true);
  SystemSuspend(false);
  SystemResume();

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest, UMA_Suspended_Stop) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_NoActivation) {
  base::HistogramTester tester;

  std::unique_ptr<MediaSessionImpl> media_session = CreateDummyMediaSession();
  media_session.reset();

  // A MediaSession that wasn't active doesn't register an active time.
  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(0, samples->TotalCount());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_SimpleActivation) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_ActivationWithUISuspension) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  UISuspend();

  clock.Advance(base::TimeDelta::FromMilliseconds(2000));
  UIResume();
  ResolveAudioFocusSuccess();

  clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(2000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_ActivationWithSystemSuspension) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  SystemSuspend(true);

  clock.Advance(base::TimeDelta::FromMilliseconds(2000));
  SystemResume();

  clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(2000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_ActivateSuspendedButNotStopped) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  clock.Advance(base::TimeDelta::FromMilliseconds(500));
  SystemSuspend(true);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
    EXPECT_EQ(0, samples->TotalCount());
  }

  SystemResume();
  clock.Advance(base::TimeDelta::FromMilliseconds(5000));
  UISuspend();

  {
    std::unique_ptr<base::HistogramSamples> samples(
        tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
    EXPECT_EQ(0, samples->TotalCount());
  }
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_ActivateSuspendStopTwice) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  clock.Advance(base::TimeDelta::FromMilliseconds(500));
  SystemSuspend(true);
  media_session_->Stop(MediaSession::SuspendType::kUI);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  clock.Advance(base::TimeDelta::FromMilliseconds(5000));
  SystemResume();
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(500));
  EXPECT_EQ(1, samples->GetCount(5000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_MultipleActivations) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  clock.Advance(base::TimeDelta::FromMilliseconds(10000));
  RemovePlayer(player_observer.get(), 0);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();
  clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
  EXPECT_EQ(1, samples->GetCount(10000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AddingObserverNotifiesCurrentInformation_EmptyInfo) {
  media_session_->RemoveObserver(mock_media_session_observer());
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(base::nullopt)));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(
                  Eq(std::set<blink::mojom::MediaSessionAction>())));
  media_session_->AddObserver(mock_media_session_observer());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AddingObserverNotifiesCurrentInformation_WithInfo) {
  // Set up the service and information.
  EnsureMediaSessionService();

  content::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");
  metadata.album = base::ASCIIToUTF16("album");
  mock_media_session_service_->SetMetadata(metadata);

  mock_media_session_service_->EnableAction(
      blink::mojom::MediaSessionAction::PLAY);
  std::set<blink::mojom::MediaSessionAction> expectedActions =
      mock_media_session_service_->actions();

  // Make sure the service is routed,
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetMainFrame());
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  ResolveAudioFocusSuccess();

  // Check if the expectations are met when the observer is newly added.
  media_session_->RemoveObserver(mock_media_session_observer());
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(metadata)));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(expectedActions)));
  media_session_->AddObserver(mock_media_session_observer());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_RequestFailure_Gain) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(IsActive());

  // The gain request failed so we should suspend the whole session.
  ResolveAudioFocusFailure();
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       Async_RequestFailure_GainTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(IsActive());

  // A transient audio focus failure should only affect transient players.
  ResolveAudioFocusFailure();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_GainThenTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_TransientThenGain) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       Async_SuspendBeforeResolve) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemSuspend(true);
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(IsActive());

  SystemResume();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_ResumeBeforeResolve) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  UISuspend();
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(player_observer->IsPlaying(0));

  UIResume();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  ResolveAudioFocusFailure();
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(player_observer->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_RemoveBeforeResolve) {
  {
    auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

    EXPECT_CALL(*mock_audio_focus_delegate(), AbandonAudioFocus());
    StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
    EXPECT_TRUE(player_observer->IsPlaying(0));

    RemovePlayer(player_observer.get(), 0);
  }

  ResolveAudioFocusSuccess();
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_StopBeforeResolve) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(player_observer->IsPlaying(1));

  media_session_->Stop(MediaSession::SuspendType::kUI);
  ResolveAudioFocusSuccess();

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_Unducking_Failure) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemStartDucking();
  EXPECT_TRUE(IsDucking());

  ResolveAudioFocusFailure();
  EXPECT_TRUE(IsDucking());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_Unducking_Inactive) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  media_session_->Stop(MediaSession::SuspendType::kUI);
  SystemStartDucking();
  EXPECT_TRUE(IsDucking());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsDucking());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_Unducking_Success) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemStartDucking();
  EXPECT_TRUE(IsDucking());

  ResolveAudioFocusSuccess();
  EXPECT_FALSE(IsDucking());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_Unducking_Suspended) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>();

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  UISuspend();
  SystemStartDucking();
  EXPECT_TRUE(IsDucking());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsDucking());
}
