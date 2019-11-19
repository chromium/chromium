// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_keys_listener_manager_impl.h"

#include "base/containers/flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/hardware_key_media_controller.h"
#include "content/public/test/content_browser_test.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/media_keys_listener.h"

namespace content {

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using media_session::test::TestMediaController;

namespace {

class MockMediaKeysListener : public ui::MediaKeysListener {
 public:
  explicit MockMediaKeysListener(ui::MediaKeysListener::Delegate* delegate)
      : delegate_(delegate) {}
  ~MockMediaKeysListener() override = default;

  // MediaKeysListener implementation.
  bool StartWatchingMediaKey(ui::KeyboardCode key_code) override {
    key_codes_.insert(key_code);
    return true;
  }
  void StopWatchingMediaKey(ui::KeyboardCode key_code) override {
    key_codes_.erase(key_code);
  }
  void SetIsMediaPlaying(bool is_playing) override {
    is_media_playing_ = is_playing;
  }

  void SimulateAccelerator(ui::Accelerator accelerator) {
    if (IsWatching(accelerator.key_code()))
      delegate_->OnMediaKeysAccelerator(accelerator);
  }

  bool IsWatching(ui::KeyboardCode key_code) const {
    return key_codes_.contains(key_code);
  }

  bool is_media_playing() const { return is_media_playing_; }

 private:
  ui::MediaKeysListener::Delegate* delegate_;
  base::flat_set<ui::KeyboardCode> key_codes_;
  bool is_media_playing_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockMediaKeysListener);
};

class MockMediaKeysListenerDelegate : public ui::MediaKeysListener::Delegate {
 public:
  MockMediaKeysListenerDelegate() = default;
  ~MockMediaKeysListenerDelegate() override = default;

  // MediaKeysListener::Delegate implementation.
  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override {
    received_keys_.push_back(accelerator.key_code());
  }

  // Expect that we have received the correct number of key events.
  void ExpectReceivedKeysCount(uint32_t count) {
    EXPECT_EQ(count, received_keys_.size());
  }

  // Expect that the key event received at |index| has the specified key code.
  void ExpectReceivedKey(uint32_t index, ui::KeyboardCode code) {
    ASSERT_LT(index, received_keys_.size());
    EXPECT_EQ(code, received_keys_[index]);
  }

 private:
  std::vector<ui::KeyboardCode> received_keys_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaKeysListenerDelegate);
};

}  // anonymous namespace

class MediaKeysListenerManagerImplTest : public ContentBrowserTest {
 public:
  MediaKeysListenerManagerImplTest() = default;
  ~MediaKeysListenerManagerImplTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndEnableFeature(media::kHardwareMediaKeyHandling);
  }

  void SetUpOnMainThread() override {
    media_keys_listener_manager_ =
        BrowserMainLoop::GetInstance()->media_keys_listener_manager();

    std::unique_ptr<MockMediaKeysListener> listener =
        std::make_unique<MockMediaKeysListener>(media_keys_listener_manager_);
    media_keys_listener_ = listener.get();
    media_keys_listener_manager_->SetMediaKeysListenerForTesting(
        std::move(listener));

    media_controller_ = std::make_unique<TestMediaController>();
    media_keys_listener_manager_->hardware_key_media_controller_for_testing()
        ->SetMediaControllerForTesting(
            media_controller_->CreateMediaControllerRemote());

    ContentBrowserTest::SetUpOnMainThread();
  }

  void SetMediaSessionInfo(MediaSessionInfoPtr session_info) {
    media_keys_listener_manager_->hardware_key_media_controller_for_testing()
        ->MediaSessionInfoChanged(std::move(session_info));
  }
  void SetSupportedMediaSessionActions(
      const std::vector<MediaSessionAction>& actions) {
    media_keys_listener_manager_->hardware_key_media_controller_for_testing()
        ->MediaSessionActionsChanged(actions);
  }
  void FlushForTesting() {
    media_keys_listener_manager_->hardware_key_media_controller_for_testing()
        ->FlushForTesting();
  }

  MediaKeysListenerManagerImpl* media_keys_listener_manager() {
    return media_keys_listener_manager_;
  }
  MockMediaKeysListener* media_keys_listener() { return media_keys_listener_; }
  TestMediaController* media_controller() { return media_controller_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MediaKeysListenerManagerImpl* media_keys_listener_manager_;
  MockMediaKeysListener* media_keys_listener_;
  std::unique_ptr<TestMediaController> media_controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaKeysListenerManagerImplTest);
};

IN_PROC_BROWSER_TEST_F(MediaKeysListenerManagerImplTest, PressPlayPauseKey) {
  // Tell the HardwareKeyMediaController that there is media playing that can be
  // paused.
  {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPlaying;
    SetMediaSessionInfo(std::move(session_info));
    SetSupportedMediaSessionActions({MediaSessionAction::kPause});
  }

  // There should not have been any calls to the media controller yet.
  EXPECT_EQ(0, media_controller()->suspend_count());
  EXPECT_EQ(0, media_controller()->resume_count());

  // The MediaKeysListener should know that media is playing.
  EXPECT_TRUE(media_keys_listener()->is_media_playing());

  // Press the play/pause media key.
  media_keys_listener()->SimulateAccelerator(
      ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, 0));
  FlushForTesting();

  // The media controller should have been told to pause.
  EXPECT_EQ(1, media_controller()->suspend_count());
  EXPECT_EQ(0, media_controller()->resume_count());

  // Tell the HardwareKeyMediaController that the media is now paused and can be
  // played.
  {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPaused;
    SetMediaSessionInfo(std::move(session_info));
    SetSupportedMediaSessionActions({MediaSessionAction::kPlay});
  }

  // The MediaKeysListener should know that media is paused.
  EXPECT_FALSE(media_keys_listener()->is_media_playing());

  // Press play/pause.
  media_keys_listener()->SimulateAccelerator(
      ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, 0));
  FlushForTesting();

  // The media controller should have been told to play.
  EXPECT_EQ(1, media_controller()->suspend_count());
  EXPECT_EQ(1, media_controller()->resume_count());
}

IN_PROC_BROWSER_TEST_F(MediaKeysListenerManagerImplTest,
                       ListensToTheCorrectMediaKeys) {
  // Before any media session starts, we should not be listening for key input.
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));

  // Tell the HardwareKeyMediaController that there is media playing that can be
  // paused.
  {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPlaying;
    SetMediaSessionInfo(std::move(session_info));
    SetSupportedMediaSessionActions({MediaSessionAction::kPause});
  }

  // We should now be listening for the play/pause key, but no others.
  EXPECT_TRUE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));

  // Update the list of supported actions.
  SetSupportedMediaSessionActions({MediaSessionAction::kPause,
                                   MediaSessionAction::kStop,
                                   MediaSessionAction::kNextTrack});

  // We should now be listening for the correct media keys.
  EXPECT_TRUE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_TRUE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_TRUE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));

  // Update the list of supported actions.
  SetSupportedMediaSessionActions({MediaSessionAction::kStop});

  // We should now be listening for the correct media keys.
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_TRUE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));

  // Disable media key handling for the HardwareKeyMediaController.
  media_keys_listener_manager()->DisableInternalMediaKeyHandling();

  // We should no longer be listening for key input.
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));

  // Re-enable media key handling for the HardwareKeyMediaController.
  media_keys_listener_manager()->EnableInternalMediaKeyHandling();

  // We should now be listening for the correct media keys.
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_TRUE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));

  // Have a different delegate besides the HardwareKeyMediaController request
  // keys.
  MockMediaKeysListenerDelegate delegate;
  media_keys_listener_manager()->StartWatchingMediaKey(
      ui::VKEY_MEDIA_PLAY_PAUSE, &delegate);

  // We should now be listening for only the new delegate's keys.
  EXPECT_TRUE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));

  // Unregister the delegate.
  media_keys_listener_manager()->StopWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE,
                                                      &delegate);

  // We should now be listening for the HardwareKeyMediaController's keys.
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_TRUE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));

  // Tell the HardwareKeyMediaController there is no longer an active session.
  SetMediaSessionInfo(nullptr);
  SetSupportedMediaSessionActions({});

  // We should no longer be listening for key input.
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PLAY_PAUSE));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_STOP));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_NEXT_TRACK));
  EXPECT_FALSE(media_keys_listener()->IsWatching(ui::VKEY_MEDIA_PREV_TRACK));
}

IN_PROC_BROWSER_TEST_F(MediaKeysListenerManagerImplTest,
                       OtherDelegatesPreemptHardwareKeyMediaController) {
  // Tell the HardwareKeyMediaController that there is media playing that can be
  // paused or sent to the next track.
  {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPlaying;
    SetMediaSessionInfo(std::move(session_info));
    SetSupportedMediaSessionActions({MediaSessionAction::kPause});
  }

  // Set up a delegate that listens to Play/Pause.
  MockMediaKeysListenerDelegate delegate;
  media_keys_listener_manager()->StartWatchingMediaKey(
      ui::VKEY_MEDIA_PLAY_PAUSE, &delegate);

  // There should not have been any calls to the media controller or the
  // delegate yet.
  EXPECT_EQ(0, media_controller()->suspend_count());
  EXPECT_EQ(0, media_controller()->next_track_count());
  delegate.ExpectReceivedKeysCount(0);

  // Press play/pause.
  media_keys_listener()->SimulateAccelerator(
      ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, 0));
  FlushForTesting();

  // The media controller should not have been told to pause.
  EXPECT_EQ(0, media_controller()->suspend_count());
  EXPECT_EQ(0, media_controller()->next_track_count());

  // The delegate should have received the event instead.
  delegate.ExpectReceivedKeysCount(1);
  delegate.ExpectReceivedKey(/*index=*/0, ui::VKEY_MEDIA_PLAY_PAUSE);

  // Unregister the delegate.
  media_keys_listener_manager()->StopWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE,
                                                      &delegate);

  // Press play/pause.
  media_keys_listener()->SimulateAccelerator(
      ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, 0));
  FlushForTesting();

  // The media controller should have been told to pause.
  EXPECT_EQ(1, media_controller()->suspend_count());

  // The delegate should not have been told to pause, since it was unregistered.
  delegate.ExpectReceivedKeysCount(1);
}

}  // namespace content
