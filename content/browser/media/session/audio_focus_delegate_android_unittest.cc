// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_delegate_android.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class AudioFocusDelegateAndroidTest : public RenderViewHostTestHarness {
 public:
  void TearDown() override {
    // The session and its delegate go away with the WebContents.
    delegate_ = nullptr;
    media_session_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  // Starts a playing player in a session whose AudioFocusDelegateAndroid the
  // test calls directly, as Java does on an Android audio focus change.
  void StartPlayback() {
    media_session_ = MediaSessionImpl::Get(web_contents());
    auto delegate = std::make_unique<AudioFocusDelegateAndroid>(media_session_);
    delegate->Initialize();
    delegate_ = delegate.get();
    media_session_->SetDelegateForTests(std::move(delegate));

    player_observer_ = std::make_unique<MockMediaSessionPlayerObserver>(
        main_rfh(), media::MediaContentType::kPersistent);
    player_id_ = player_observer_->StartNewPlayer();
    ASSERT_TRUE(media_session_->AddPlayer(player_observer_.get(), player_id_));
    ASSERT_TRUE(media_session_->IsActive());
  }

  bool IsPlaying() { return player_observer_->IsPlaying(player_id_); }

  raw_ptr<MediaSessionImpl> media_session_ = nullptr;
  raw_ptr<AudioFocusDelegateAndroid> delegate_ = nullptr;
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_;
  int player_id_ = -1;
};

class AudioFocusDelegateAndroidKillSwitchTest
    : public AudioFocusDelegateAndroidTest {
 public:
  AudioFocusDelegateAndroidKillSwitchTest() {
    feature_list_.InitAndDisableFeature(
        media::kFocusGainResumesOnlyFocusLossPauses);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AudioFocusDelegateAndroidTest, FocusGainResumesFocusLossPause) {
  StartPlayback();

  delegate_->OnSuspend(nullptr);
  ASSERT_TRUE(media_session_->IsSuspended());
  ASSERT_FALSE(IsPlaying());

  delegate_->OnResume(nullptr);
  ASSERT_TRUE(media_session_->IsActive());
  ASSERT_TRUE(IsPlaying());

  // The focus gain ended the focus loss pause, so a later system pause stays
  // paused on the next focus gain.
  media_session_->Suspend(MediaSession::SuspendType::kSystem);
  ASSERT_FALSE(IsPlaying());

  delegate_->OnResume(nullptr);
  EXPECT_TRUE(media_session_->IsSuspended());
  EXPECT_FALSE(IsPlaying());
}

TEST_F(AudioFocusDelegateAndroidTest, FocusGainDoesNotResumeOtherSystemPause) {
  StartPlayback();

  // A system pause that isn't a focus loss, e.g. a headphone unplug.
  media_session_->Suspend(MediaSession::SuspendType::kSystem);
  ASSERT_FALSE(IsPlaying());

  // Another app takes transient focus, then releases it.
  delegate_->OnSuspend(nullptr);
  delegate_->OnResume(nullptr);
  EXPECT_TRUE(media_session_->IsSuspended());
  EXPECT_FALSE(IsPlaying());
}

TEST_F(AudioFocusDelegateAndroidTest,
       FocusGainDoesNotResumeAfterPlaybackRestarted) {
  StartPlayback();

  // A focus loss pause, then the user resumes before the focus gain arrives.
  delegate_->OnSuspend(nullptr);
  media_session_->Resume(MediaSession::SuspendType::kUI);
  ASSERT_TRUE(IsPlaying());

  media_session_->Suspend(MediaSession::SuspendType::kSystem);
  ASSERT_FALSE(IsPlaying());

  delegate_->OnResume(nullptr);
  EXPECT_TRUE(media_session_->IsSuspended());
  EXPECT_FALSE(IsPlaying());
}

TEST_F(AudioFocusDelegateAndroidTest,
       FocusGainDoesNotResumeAfterOtherSystemResume) {
  StartPlayback();

  // Something other than a focus gain resumes a focus loss pause.
  delegate_->OnSuspend(nullptr);
  media_session_->Resume(MediaSession::SuspendType::kSystem);
  ASSERT_TRUE(IsPlaying());

  // A system pause that isn't a focus loss, then another app takes transient
  // focus and releases it.
  media_session_->Suspend(MediaSession::SuspendType::kSystem);
  ASSERT_FALSE(IsPlaying());
  delegate_->OnSuspend(nullptr);
  delegate_->OnResume(nullptr);
  EXPECT_TRUE(media_session_->IsSuspended());
  EXPECT_FALSE(IsPlaying());
}

TEST_F(AudioFocusDelegateAndroidKillSwitchTest,
       FocusGainResumesOtherSystemPause) {
  StartPlayback();

  media_session_->Suspend(MediaSession::SuspendType::kSystem);
  ASSERT_FALSE(IsPlaying());

  delegate_->OnSuspend(nullptr);
  delegate_->OnResume(nullptr);
  EXPECT_TRUE(media_session_->IsActive());
  EXPECT_TRUE(IsPlaying());
}

}  // namespace content
