// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/system_media_controls_notifier.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/system_media_controls/mock_system_media_controls.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_media_session_client.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/idle/scoped_set_idle_state.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {
std::u16string hidden_metadata_placeholder_title = u"placeholder_title";
std::u16string hidden_metadata_placeholder_artist = u"placeholder_artist";
std::u16string hidden_metadata_placeholder_album = u"placeholder_album";
int hidden_metadata_placeholder_thumbnail_size = 42;
}  // namespace

namespace content {

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using PlaybackStatus =
    system_media_controls::SystemMediaControls::PlaybackStatus;
using testing::_;
using testing::Expectation;
using testing::WithArg;

class SystemMediaControlsNotifierTest : public testing::Test {
 public:
  SystemMediaControlsNotifierTest() = default;

  SystemMediaControlsNotifierTest(const SystemMediaControlsNotifierTest&) =
      delete;
  SystemMediaControlsNotifierTest& operator=(
      const SystemMediaControlsNotifierTest&) = delete;

  ~SystemMediaControlsNotifierTest() override = default;

  void SetUp() override {
    notifier_ = std::make_unique<SystemMediaControlsNotifier>(
        &mock_system_media_controls_, base::UnguessableToken::Null());
    SetupMediaSessionClient();
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

  void SimulateMetadataChanged(std::u16string title,
                               std::u16string artist,
                               std::u16string album) {
    media_session::MediaMetadata metadata;
    metadata.title = title;
    metadata.artist = artist;
    metadata.album = album;
    notifier_->MediaSessionMetadataChanged(
        std::optional<media_session::MediaMetadata>(metadata));
  }

  void SimulateHidden() {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->hide_metadata = true;
    notifier_->MediaSessionInfoChanged(std::move(session_info));
  }

  void SimulateEmptyMetadata() {
    notifier_->MediaSessionMetadataChanged(std::nullopt);
  }

  void SimulatePositionChanged(const media_session::MediaPosition& position) {
    notifier_->MediaSessionPositionChanged(
        std::optional<media_session::MediaPosition>(position));
  }

  void SimulateEmptyPosition() {
    notifier_->MediaSessionPositionChanged(std::nullopt);
  }

  void SimulateImageChanged(int image_size) {
    // Need a non-empty SkBitmap so MediaControllerImageChanged doesn't try to
    // get the icon from ChromeContentBrowserClient.
    SkBitmap bitmap;
    bitmap.allocN32Pixels(image_size, image_size);
    notifier_->MediaControllerImageChanged(
        media_session::mojom::MediaSessionImageType::kArtwork, bitmap);
  }

  void SimulateIsSeekToEnabledChanged(bool is_seek_to_enabled) {
    std::vector<media_session::mojom::MediaSessionAction> actions;

    if (is_seek_to_enabled) {
      actions.push_back(media_session::mojom::MediaSessionAction::kSeekTo);
    }

    notifier_->MediaSessionActionsChanged(actions);
  }

  void SetupMediaSessionClient() {
    client_.SetTitlePlaceholder(hidden_metadata_placeholder_title);
    client_.SetArtistPlaceholder(hidden_metadata_placeholder_artist);
    client_.SetAlbumPlaceholder(hidden_metadata_placeholder_album);

    SkBitmap placeholder_bitmap;
    placeholder_bitmap.allocN32Pixels(
        hidden_metadata_placeholder_thumbnail_size,
        hidden_metadata_placeholder_thumbnail_size);
    client_.SetThumbnailPlaceholder(placeholder_bitmap);
  }

  SystemMediaControlsNotifier& notifier() { return *notifier_; }
  system_media_controls::testing::MockSystemMediaControls&
  mock_system_media_controls() {
    return mock_system_media_controls_;
  }

  media_session::MediaPosition GetTestMediaPosition(
      base::TimeDelta position = base::Seconds(10)) {
    constexpr double kPlaybackRate = 1.0;
    constexpr base::TimeDelta kDuration = base::Seconds(300);

    return media_session::MediaPosition(kPlaybackRate, kDuration, position,
                                        false);
  }

  base::OneShotTimer& metadata_update_timer() {
    return notifier_->metadata_update_timer_;
  }

  base::OneShotTimer& icon_update_timer() {
    return notifier_->icon_update_timer_;
  }

  base::OneShotTimer& actions_update_timer() {
    return notifier_->actions_update_timer_;
  }

#if BUILDFLAG(IS_WIN)
  base::RepeatingTimer& lock_polling_timer() {
    return notifier_->lock_polling_timer_;
  }

  base::OneShotTimer& hide_smtc_timer() { return notifier_->hide_smtc_timer_; }
#endif  // BUILDFLAG(IS_WIN)

  TestMediaSessionClient client_;

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<SystemMediaControlsNotifier> notifier_;
  system_media_controls::testing::MockSystemMediaControls
      mock_system_media_controls_;
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
  metadata_update_timer().FireNow();

  SimulatePaused();
  metadata_update_timer().FireNow();

  SimulateStopped();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyDebouncesPlaybackState) {
  EXPECT_CALL(mock_system_media_controls(),
              SetPlaybackStatus(PlaybackStatus::kPlaying))
      .Times(0);
  EXPECT_CALL(mock_system_media_controls(),
              SetPlaybackStatus(PlaybackStatus::kPaused));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);

  SimulatePlaying();
  SimulatePaused();
  metadata_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, StopClearsPendingPlaybackState) {
  EXPECT_CALL(mock_system_media_controls(),
              SetPlaybackStatus(PlaybackStatus::kPlaying))
      .Times(0);
  EXPECT_CALL(mock_system_media_controls(),
              SetPlaybackStatus(PlaybackStatus::kPaused))
      .Times(0);
  EXPECT_CALL(mock_system_media_controls(),
              SetPlaybackStatus(PlaybackStatus::kStopped));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata());

  SimulatePlaying();
  SimulatePaused();
  SimulateStopped();
  EXPECT_FALSE(metadata_update_timer().IsRunning());
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesMetadata) {
  std::u16string title = u"title";
  std::u16string artist = u"artist";
  std::u16string album = u"album";

  EXPECT_CALL(mock_system_media_controls(), SetTitle(title));
  EXPECT_CALL(mock_system_media_controls(), SetArtist(artist));
  EXPECT_CALL(mock_system_media_controls(), SetAlbum(album));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);
  EXPECT_CALL(mock_system_media_controls(), UpdateDisplay());

  SimulateMetadataChanged(title, artist, album);
  metadata_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesNullMetadata) {
  EXPECT_CALL(mock_system_media_controls(), SetTitle(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), SetArtist(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), SetAlbum(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata());

  SimulateEmptyMetadata();
  EXPECT_FALSE(metadata_update_timer().IsRunning());
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyDebouncesMetadataUpdates) {
  std::u16string dropped_title = u"dropped_title";
  std::u16string dropped_artist = u"dropped_artist";
  std::u16string dropped_album = u"dropped_album";

  std::u16string title = u"title";
  std::u16string artist = u"artist";
  std::u16string album = u"album";

  EXPECT_CALL(mock_system_media_controls(), SetTitle(title));
  EXPECT_CALL(mock_system_media_controls(), SetArtist(artist));
  EXPECT_CALL(mock_system_media_controls(), SetAlbum(album));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);
  EXPECT_CALL(mock_system_media_controls(), UpdateDisplay());

  // When there are two calls in quick succession, only the last one should be
  // applied.
  SimulateMetadataChanged(dropped_title, dropped_artist, dropped_album);
  SimulateMetadataChanged(title, artist, album);
  metadata_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesMetadaBetweenDebounces) {
  std::u16string title = u"title";
  std::u16string artist = u"artist";
  std::u16string album = u"album";

  EXPECT_CALL(mock_system_media_controls(), SetTitle(title));
  EXPECT_CALL(mock_system_media_controls(), SetArtist(artist));
  EXPECT_CALL(mock_system_media_controls(), SetAlbum(album));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);
  EXPECT_CALL(mock_system_media_controls(), UpdateDisplay());

  SimulateMetadataChanged(title, artist, album);
  metadata_update_timer().FireNow();

  testing::Mock::VerifyAndClearExpectations(&mock_system_media_controls());

  std::u16string other_title = u"other_title";
  std::u16string other_artist = u"other_artist";
  std::u16string other_album = u"other_album";

  EXPECT_CALL(mock_system_media_controls(), SetTitle(other_title));
  EXPECT_CALL(mock_system_media_controls(), SetArtist(other_artist));
  EXPECT_CALL(mock_system_media_controls(), SetAlbum(other_album));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);
  EXPECT_CALL(mock_system_media_controls(), UpdateDisplay());

  SimulateMetadataChanged(other_title, other_artist, other_album);
  metadata_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, EmptyMetadataClearsPendingMetadata) {
  std::u16string title = u"title";
  std::u16string artist = u"artist";
  std::u16string album = u"album";

  EXPECT_CALL(mock_system_media_controls(), SetTitle(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), SetArtist(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), SetAlbum(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata());

  SimulateMetadataChanged(title, artist, album);
  SimulateEmptyMetadata();
  EXPECT_FALSE(metadata_update_timer().IsRunning());
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesPosition) {
  auto position = GetTestMediaPosition();

  EXPECT_CALL(mock_system_media_controls(), SetPosition(position));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);

  SimulatePositionChanged(position);
  metadata_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyHandlesNullPosition) {
  EXPECT_CALL(mock_system_media_controls(), SetPosition(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata());

  SimulateEmptyPosition();
  EXPECT_FALSE(metadata_update_timer().IsRunning());
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyDebouncesPositionUpdates) {
  auto dropped_position = GetTestMediaPosition(base::Seconds(10));
  auto position = GetTestMediaPosition(base::Seconds(20));

  EXPECT_CALL(mock_system_media_controls(), SetPosition(position));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);

  SimulatePositionChanged(dropped_position);
  SimulatePositionChanged(position);
  metadata_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest,
       ProperlyUpdatesPositionBetweenDebounces) {
  auto first_position = GetTestMediaPosition(base::Seconds(10));

  EXPECT_CALL(mock_system_media_controls(), SetPosition(first_position));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);

  SimulatePositionChanged(first_position);
  metadata_update_timer().FireNow();

  testing::Mock::VerifyAndClearExpectations(&mock_system_media_controls());

  auto second_position = GetTestMediaPosition(base::Seconds(20));

  EXPECT_CALL(mock_system_media_controls(), SetPosition(second_position));
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata()).Times(0);

  SimulatePositionChanged(second_position);
  metadata_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, NullPositionClearsPendingPosition) {
  EXPECT_CALL(mock_system_media_controls(), SetPosition(_)).Times(0);
  EXPECT_CALL(mock_system_media_controls(), ClearMetadata());

  SimulatePositionChanged(GetTestMediaPosition());
  SimulateEmptyPosition();
  EXPECT_FALSE(metadata_update_timer().IsRunning());
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesImage) {
  constexpr int kIconSize = 1;
  EXPECT_CALL(mock_system_media_controls(), SetThumbnail(_));

  SimulateImageChanged(kIconSize);
  icon_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyDebouncesImage) {
  constexpr int kDroppedIconSize = 1;
  constexpr int kIconSize = 2;
  EXPECT_CALL(mock_system_media_controls(), SetThumbnail(_))
      .WillOnce(testing::Invoke([kIconSize](const SkBitmap& bitmap) {
        EXPECT_EQ(bitmap.width(), kIconSize);
        EXPECT_EQ(bitmap.height(), kIconSize);
      }));

  SimulateImageChanged(kDroppedIconSize);
  SimulateImageChanged(kIconSize);
  icon_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesIsSeekTooEnabled) {
  EXPECT_CALL(mock_system_media_controls(), SetIsSeekToEnabled(true));

  SimulateIsSeekToEnabledChanged(true);
  actions_update_timer().FireNow();

  testing::Mock::VerifyAndClearExpectations(&mock_system_media_controls());

  EXPECT_CALL(mock_system_media_controls(), SetIsSeekToEnabled(false));

  SimulateIsSeekToEnabledChanged(false);
  actions_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyDebouncesIsSeekTooEnabled) {
  EXPECT_CALL(mock_system_media_controls(), SetIsSeekToEnabled(true)).Times(1);
  EXPECT_CALL(mock_system_media_controls(), SetIsSeekToEnabled(false)).Times(0);

  SimulateIsSeekToEnabledChanged(true);
  SimulateIsSeekToEnabledChanged(false);
  SimulateIsSeekToEnabledChanged(false);
  SimulateIsSeekToEnabledChanged(true);
  actions_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, ProperlyUpdatesID) {
  // When a request ID is set, the system media controls should receive that ID.
  auto request_id = base::UnguessableToken::Create();
  EXPECT_CALL(mock_system_media_controls(), SetID(_))
      .WillOnce(WithArg<0>([request_id](const std::string* value) {
        ASSERT_NE(nullptr, value);
        EXPECT_EQ(request_id.ToString(), *value);
      }));
  notifier().MediaSessionChanged(request_id);
  testing::Mock::VerifyAndClearExpectations(&mock_system_media_controls());

  // When the request ID is cleared, the system media controls should receive
  // null.
  EXPECT_CALL(mock_system_media_controls(), SetID(nullptr));
  notifier().MediaSessionChanged(std::nullopt);
}

TEST_F(SystemMediaControlsNotifierTest, DontHideMediaMetadataIfNotNeeded) {
  std::u16string title = u"original_title";
  std::u16string artist = u"original_artist";
  std::u16string album = u"original_album";
  int thumbnail_size = 1;

  EXPECT_CALL(mock_system_media_controls(), SetThumbnail(_))
      .WillOnce(testing::Invoke([thumbnail_size](const SkBitmap& bitmap) {
        EXPECT_EQ(bitmap.width(), thumbnail_size);
        EXPECT_EQ(bitmap.height(), thumbnail_size);
      }));

  EXPECT_CALL(mock_system_media_controls(), SetTitle(title));
  EXPECT_CALL(mock_system_media_controls(), SetArtist(artist));
  EXPECT_CALL(mock_system_media_controls(), SetAlbum(album));

  // NOTE: As we don't call SimulateHidden(), the MediaSessionInfo's
  // `hide_metadata` field will be false, and hence the updated metadata
  // shouldn't be modified.

  SimulateMetadataChanged(title, artist, album);
  SimulateImageChanged(thumbnail_size);
  metadata_update_timer().FireNow();
  icon_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, HideMediaMetadataIfNeeded) {
  std::u16string title = u"original_title";
  std::u16string artist = u"original_artist";
  std::u16string album = u"original_album";

  EXPECT_CALL(mock_system_media_controls(), SetThumbnail(_))
      .WillOnce(testing::Invoke([this](const SkBitmap& bitmap) {
        SkBitmap placeholder_bitmap = client_.GetThumbnailPlaceholder();
        EXPECT_EQ(bitmap.width(), placeholder_bitmap.width());
        EXPECT_EQ(bitmap.height(), placeholder_bitmap.height());
      }));

  EXPECT_CALL(mock_system_media_controls(),
              SetTitle(client_.GetTitlePlaceholder()));
  EXPECT_CALL(mock_system_media_controls(),
              SetArtist(client_.GetArtistPlaceholder()));
  EXPECT_CALL(mock_system_media_controls(),
              SetAlbum(client_.GetAlbumPlaceholder()));

  SimulateHidden();
  // Just metadata changed should trigger an icon change as well.
  SimulateMetadataChanged(title, artist, album);
  metadata_update_timer().FireNow();
}

TEST_F(SystemMediaControlsNotifierTest, HideMediaImageIfNeeded) {
  int thumbnail_size = 1;

  EXPECT_CALL(mock_system_media_controls(), SetThumbnail(_))
      .WillOnce(testing::Invoke([this](const SkBitmap& bitmap) {
        SkBitmap placeholder_bitmap = client_.GetThumbnailPlaceholder();
        EXPECT_EQ(bitmap.width(), placeholder_bitmap.width());
        EXPECT_EQ(bitmap.height(), placeholder_bitmap.height());
      }));

  SimulateHidden();
  SimulateImageChanged(thumbnail_size);
  icon_update_timer().FireNow();
}

#if BUILDFLAG(IS_WIN)
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
  metadata_update_timer().FireNow();

  // Lock the screen.
  ui::ScopedSetIdleState locked(ui::IDLE_STATE_LOCKED);

  // Make sure that the lock polling timer is running and then force it to
  // fire so that we don't need to wait. This should not disable the service.
  EXPECT_TRUE(lock_polling_timer().IsRunning());
  lock_polling_timer().user_task().Run();

  // Since we're playing, the timer to hide the SMTC should not be running.
  EXPECT_FALSE(hide_smtc_timer().IsRunning());

  SimulatePaused();
  metadata_update_timer().FireNow();

  // Now that we're paused, the timer to hide the SMTC should be running.
  EXPECT_TRUE(hide_smtc_timer().IsRunning());

  // Force the timer to fire now. This should disable the service.
  hide_smtc_timer().FireNow();
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
