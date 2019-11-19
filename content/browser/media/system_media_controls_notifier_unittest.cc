// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/system_media_controls_notifier.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/system_media_controls/mock_system_media_controls.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "ui/base/idle/scoped_set_idle_state.h"
#endif  // defined(OS_WIN)

namespace content {

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using PlaybackStatus =
    system_media_controls::SystemMediaControls::PlaybackStatus;
using testing::_;
using testing::Expectation;

class SystemMediaControlsNotifierTest : public testing::Test {
 public:
  SystemMediaControlsNotifierTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}
  ~SystemMediaControlsNotifierTest() override = default;

  void SetUp() override {
    notifier_ = std::make_unique<SystemMediaControlsNotifier>(
        /*connector=*/nullptr, &mock_system_media_controls_);
  }

 protected:
  void SimulatePlaying() {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPlaying;
    notifier_->MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulatePaused() {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->playback_state = MediaPlaybackState::kPaused;
    notifier_->MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulateStopped() { notifier_->MediaSessionInfoChanged(nullptr); }

  void SimulateMetadataChanged(bool empty,
                               base::string16 title,
                               base::string16 artist) {
    if (!empty) {
      media_session::MediaMetadata metadata;
      metadata.title = title;
      metadata.artist = artist;
      notifier_->MediaSessionMetadataChanged(
          base::Optional<media_session::MediaMetadata>(metadata));
    } else {
      notifier_->MediaSessionMetadataChanged(base::nullopt);
    }
  }

  void SimulateImageChanged() {
    // Need a non-empty SkBitmap so MediaControllerImageChanged doesn't try to
    // get the icon from ChromeContentBrowserClient.
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    notifier_->MediaControllerImageChanged(
        media_session::mojom::MediaSessionImageType::kArtwork, bitmap);
  }

  SystemMediaControlsNotifier& notifier() { return *notifier_; }
  system_media_controls::testing::MockSystemMediaControls&
  mock_system_media_controls() {
    return mock_system_media_controls_;
  }

#if defined(OS_WIN)
  base::RepeatingTimer& lock_polling_timer() {
    return notifier_->lock_polling_timer_;
  }

  base::OneShotTimer& hide_smtc_timer() { return notifier_->hide_smtc_timer_; }
#endif  // defined(OS_WIN)

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SystemMediaControlsNotifier> notifier_;
  system_media_controls::testing::MockSystemMediaControls
      mock_system_media_controls_;

  DISALLOW_COPY_AND_ASSIGN(SystemMediaControlsNotifierTest);
};

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesPlaybackState) {
  Expectation playing =
      EXPECT_CALL(mock_system_media_controls(),
                  SetPlaybackStatus(PlaybackStatus::kPlaying));
  Expectation paused = EXPECT_CALL(mock_system_media_controls(),
                                   SetPlaybackStatus(PlaybackStatus::kPaused))
                           .After(playing);
  Expectation stopped = EXPECT_CALL(mock_system_media_controls(),
                                    SetPlaybackStatus(PlaybackStatus::kStopped))
                            .After(paused);
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).After(stopped);

  SimulatePlaying();
  SimulatePaused();
  SimulateStopped();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesMetadata) {
  base::string16 title = base::ASCIIToUTF16("title");
  base::string16 artist = base::ASCIIToUTF16("artist");

  EXPECT_CALL(mock_system_media_controls(), SetTitle(title));
  EXPECT_CALL(mock_system_media_controls(), SetArtist(artist));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);
  EXPECT_CALL(mock_system_media_controls(), UpdateDisplay());

  SimulateMetadataChanged(false, title, artist);
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesNullMetadata) {
  EXPECT_CALL(mock_system_media_controls(), SetTitle(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), SetArtist(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata());

  SimulateMetadataChanged(true, base::string16(), base::string16());
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesImage) {
  EXPECT_CALL(mock_system_media_controls(), SetThumbnail(_));

  SimulateImageChanged();
}

#if defined(OS_WIN)
TEST_F(SystemMediaControlsNotifierTest, DisablesOnLockAndEnablesOnUnlock) {
  EXPECT_CALL(mock_system_media_controls(), SetEnabled(false));

  {
    // Lock the screen.
    ui::ScopedSetIdleState locked(ui::IDLE_STATE_LOCKED);

    // Make sure that the lock polling timer is running and then force it to
    // fire so that we don't need to wait. This should disable the service.
    EXPECT_TRUE(lock_polling_timer().IsRunning());
    lock_polling_timer().user_task().Run();
  }

  // Ensure that the service was disabled.
  testing::Mock::VerifyAndClearExpectations(&mock_system_media_controls());

  // The service should be reenabled on unlock.
  EXPECT_CALL(mock_system_media_controls(), SetEnabled(true));

  {
    // Unlock the screen.
    ui::ScopedSetIdleState unlocked(ui::IDLE_STATE_ACTIVE);

    // Make sure that the lock polling timer is running and then force it to
    // fire so that we don't need to wait. This should enable the service.
    EXPECT_TRUE(lock_polling_timer().IsRunning());
    lock_polling_timer().user_task().Run();
  }
}

TEST_F(SystemMediaControlsNotifierTest, DoesNotDisableOnLockWhenPlaying) {
  EXPECT_CALL(mock_system_media_controls(), SetEnabled(_)).Times(0);

  SimulatePlaying();

  // Lock the screen.
  ui::ScopedSetIdleState locked(ui::IDLE_STATE_LOCKED);

  // Make sure that the lock polling timer is running and then force it to
  // fire so that we don't need to wait. This should not disable the service.
  EXPECT_TRUE(lock_polling_timer().IsRunning());
  lock_polling_timer().user_task().Run();
}

TEST_F(SystemMediaControlsNotifierTest, DisablesAfterPausingOnLockScreen) {
  Expectation playing =
      EXPECT_CALL(mock_system_media_controls(),
                  SetPlaybackStatus(PlaybackStatus::kPlaying));
  Expectation paused = EXPECT_CALL(mock_system_media_controls(),
                                   SetPlaybackStatus(PlaybackStatus::kPaused))
                           .After(playing);
  EXPECT_CALL(mock_system_media_controls(), SetEnabled(false)).After(paused);

  SimulatePlaying();

  // Lock the screen.
  ui::ScopedSetIdleState locked(ui::IDLE_STATE_LOCKED);

  // Make sure that the lock polling timer is running and then force it to
  // fire so that we don't need to wait. This should not disable the service.
  EXPECT_TRUE(lock_polling_timer().IsRunning());
  lock_polling_timer().user_task().Run();

  // Since we're playing, the timer to hide the SMTC should not be running.
  EXPECT_FALSE(hide_smtc_timer().IsRunning());

  SimulatePaused();

  // Now that we're paused, the timer to hide the SMTC should be running.
  EXPECT_TRUE(hide_smtc_timer().IsRunning());

  // Force the timer to fire now. This should disable the service.
  hide_smtc_timer().FireNow();
}
#endif  // defined(OS_WIN)

}  // namespace content
