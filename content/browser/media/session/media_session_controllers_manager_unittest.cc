// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_controllers_manager.h"

#include <set>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_controller.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"

namespace content {

namespace {

std::set<media_session::mojom::MediaSessionAction> GetDefaultActions() {
  return {media_session::mojom::MediaSessionAction::kPlay,
          media_session::mojom::MediaSessionAction::kPause,
          media_session::mojom::MediaSessionAction::kStop,
          media_session::mojom::MediaSessionAction::kSeekTo,
          media_session::mojom::MediaSessionAction::kScrubTo,
          media_session::mojom::MediaSessionAction::kSeekForward,
          media_session::mojom::MediaSessionAction::kSeekBackward};
}

std::set<media_session::mojom::MediaSessionAction>
AppendPictureInPictureActionsTo(
    std::set<media_session::mojom::MediaSessionAction> actions) {
  actions.insert(
      {media_session::mojom::MediaSessionAction::kEnterPictureInPicture,
       media_session::mojom::MediaSessionAction::kExitPictureInPicture});
  return actions;
}

}  // namespace

class MediaSessionControllersManagerTest
    : public RenderViewHostImplTestHarness,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  // Indices of the tuple parameters.
  static const int kIsInternalMediaSessionEnabled = 0;
  static const int kIsAudioFocusEnabled = 1;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(media::kGlobalMediaControlsPictureInPicture);

    // Based on the parameters, switch them on.
    if (IsInternalMediaSessionEnabled()) {
      enabled_features.push_back(media::kInternalMediaSession);
    } else {
      disabled_features.push_back(media::kInternalMediaSession);
    }

    if (IsAudioFocusEnabled()) {
      enabled_features.push_back(media_session::features::kMediaSessionService);
      enabled_features.push_back(
          media_session::features::kAudioFocusEnforcement);
    } else {
      disabled_features.push_back(
          media_session::features::kMediaSessionService);
      disabled_features.push_back(
          media_session::features::kAudioFocusEnforcement);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    RenderViewHostImplTestHarness::SetUp();

    GlobalRenderFrameHostId frame_routing_id =
        contents()->GetPrimaryMainFrame()->GetGlobalId();
    media_player_id_ = MediaPlayerId(frame_routing_id, 1);
    media_player_id2_ = MediaPlayerId(frame_routing_id, 2);
    manager_ = std::make_unique<MediaSessionControllersManager>(contents());
  }

  MediaSessionImpl* media_session() {
    return MediaSessionImpl::Get(contents());
  }

  bool IsInternalMediaSessionEnabled() const {
    return std::get<kIsInternalMediaSessionEnabled>(GetParam());
  }

  bool IsAudioFocusEnabled() const {
    return std::get<kIsAudioFocusEnabled>(GetParam());
  }

  bool IsMediaSessionEnabled() const {
    return IsInternalMediaSessionEnabled() || IsAudioFocusEnabled();
  }

  void TearDown() override {
    manager_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  MediaPlayerId media_player_id_ = MediaPlayerId::CreateMediaPlayerIdForTests();
  MediaPlayerId media_player_id2_ =
      MediaPlayerId::CreateMediaPlayerIdForTests();
  std::unique_ptr<MediaSessionControllersManager> manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(MediaSessionControllersManagerTest, ActivateDeactivateSession) {
  ASSERT_FALSE(media_session()->IsActive());

  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  manager_->OnMetadata(media_player_id2_, true, false,
                       media::MediaContentType::kTransient);
  EXPECT_FALSE(media_session()->IsActive());

  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));
  EXPECT_EQ(media_session()->IsActive(), IsMediaSessionEnabled());

  // RequestPlay() for the same player has no effect.
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));
  EXPECT_EQ(media_session()->IsActive(), IsMediaSessionEnabled());

  // RequestPlay() for another player should keep the session active until both
  // are stopped.
  EXPECT_TRUE(manager_->RequestPlay(media_player_id2_));
  EXPECT_EQ(media_session()->IsActive(), IsMediaSessionEnabled());

  manager_->OnPause(media_player_id_, true);
  EXPECT_EQ(media_session()->IsActive(), IsMediaSessionEnabled());

  manager_->OnEnd(media_player_id2_);
  EXPECT_FALSE(media_session()->IsActive());
}

TEST_P(MediaSessionControllersManagerTest, RenderFrameDeletedRemovesHost) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));
  ASSERT_EQ(media_session()->IsActive(), IsMediaSessionEnabled());

  manager_->RenderFrameDeleted(contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(media_session()->IsActive());
}

TEST_P(MediaSessionControllersManagerTest, OnPauseSuspends) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));
  ASSERT_FALSE(media_session()->IsSuspended());

  manager_->OnPause(media_player_id_, false);
  EXPECT_EQ(media_session()->IsSuspended(), IsMediaSessionEnabled());
}

TEST_P(MediaSessionControllersManagerTest, OnPauseIdNotFound) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));
  ASSERT_FALSE(media_session()->IsSuspended());

  manager_->OnPause(media_player_id2_, false);
  EXPECT_FALSE(media_session()->IsSuspended());
}

TEST_P(MediaSessionControllersManagerTest, PositionState) {
  // If not enabled, no adds will occur, as RequestPlay returns early.
  if (!IsMediaSessionEnabled())
    return;

  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *media_session());

    const media_session::MediaPosition expected_position(
        /*playback_rate=*/1.0, /*duration=*/base::TimeDelta(),
        /*position=*/base::TimeDelta(), /*end_of_media=*/false);

    manager_->OnMediaPositionStateChanged(media_player_id_, expected_position);

    EXPECT_TRUE(manager_->RequestPlay(media_player_id_));

    // Media session should be updated with the last received position for that
    // player.
    observer.WaitForExpectedPosition(expected_position);
  }

  {
    auto observer =
        std::make_unique<media_session::test::MockMediaSessionMojoObserver>(
            *media_session());

    media_session::MediaPosition expected_position(
        /*playback_rate=*/0.0, /*duration=*/base::Seconds(10),
        /*position=*/base::TimeDelta(), /*end_of_media=*/false);

    manager_->OnMediaPositionStateChanged(media_player_id_, expected_position);

    // Media session should be updated with the new position.
    observer->WaitForExpectedPosition(expected_position);

    // Replay the current player.
    manager_->OnPause(media_player_id_, true);
    observer =
        std::make_unique<media_session::test::MockMediaSessionMojoObserver>(
            *media_session());
    EXPECT_TRUE(manager_->RequestPlay(media_player_id_));

    // Media session should still see the last received position for that
    // player.
    observer->WaitForExpectedPosition(expected_position);
  }
}

TEST_P(MediaSessionControllersManagerTest, MultiplePlayersWithPositionState) {
  // If not enabled, no adds will occur, as RequestPlay returns early.
  if (!IsMediaSessionEnabled())
    return;

  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  manager_->OnMetadata(media_player_id2_, true, false,
                       media::MediaContentType::kTransient);

  media_session::MediaPosition expected_position1(
      /*playback_rate=*/1.0, /*duration=*/base::TimeDelta(),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);
  media_session::MediaPosition expected_position2(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(10),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);

  media_session::test::MockMediaSessionMojoObserver observer(*media_session());

  manager_->OnMediaPositionStateChanged(media_player_id_, expected_position1);
  manager_->OnMediaPositionStateChanged(media_player_id2_, expected_position2);

  // If there is exactly one player, media session uses its position.
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));
  observer.WaitForExpectedPosition(expected_position1);

  // If there is more than one player, media session doesn't know about
  // position.
  EXPECT_TRUE(manager_->RequestPlay(media_player_id2_));
  observer.WaitForEmptyPosition();

  // Change the position of the second player.
  media_session::MediaPosition new_position(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(20),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);
  manager_->OnMediaPositionStateChanged(media_player_id2_, new_position);

  // Stop the first player.
  manager_->OnPause(media_player_id_, true);

  // There is exactly one player again (the second one). Media session should
  // use its updated position.
  observer.WaitForExpectedPosition(new_position);
}

TEST_P(MediaSessionControllersManagerTest, PictureInPictureAvailability) {
  if (!IsMediaSessionEnabled())
    return;

  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);

  media_session::test::MockMediaSessionMojoObserver observer(*media_session());

  manager_->OnPictureInPictureAvailabilityChanged(media_player_id_, true);
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));

  observer.WaitForExpectedActions(AppendPictureInPictureActionsTo({}));

  manager_->OnPictureInPictureAvailabilityChanged(media_player_id_, false);

  observer.WaitForEmptyActions();
}

TEST_P(MediaSessionControllersManagerTest,
       PictureInPictureAvailabilityMultiplePlayers) {
  if (!IsMediaSessionEnabled())
    return;

  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kPersistent);
  manager_->OnMetadata(media_player_id2_, true, false,
                       media::MediaContentType::kPersistent);

  media_session::test::MockMediaSessionMojoObserver observer(*media_session());

  manager_->OnPictureInPictureAvailabilityChanged(media_player_id_, true);
  manager_->OnPictureInPictureAvailabilityChanged(media_player_id2_, true);

  // If there is exactly one player, media session uses its Picture-In-Picture
  // availability.
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));
  observer.WaitForExpectedActions(
      AppendPictureInPictureActionsTo(GetDefaultActions()));

  // If there is more than one player, media session doesn't know about
  // Picture-In-Picture availability.
  EXPECT_TRUE(manager_->RequestPlay(media_player_id2_));
  observer.WaitForExpectedActions(GetDefaultActions());

  // Change the Picture-In-Picture availability of the first player.
  manager_->OnPictureInPictureAvailabilityChanged(media_player_id_, false);

  // Stop the second player.
  manager_->OnPause(media_player_id2_, true);

  // There is exactly one player again (the second one). Media session should
  // use its updated Picture-In-Picture availability.
  observer.WaitForExpectedActions(GetDefaultActions());
}

TEST_P(MediaSessionControllersManagerTest, SufficientlyVisibleVideo) {
  if (!IsMediaSessionEnabled()) {
    return;
  }

  manager_->OnMetadata(media_player_id_, true, true,
                       media::MediaContentType::kTransient);

  media_session::test::MockMediaSessionMojoObserver observer(*media_session());

  manager_->OnVideoVisibilityChanged(media_player_id_, true);
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));

  // Verify that media session reports video is sufficiently visible.
  EXPECT_TRUE(observer.WaitForMeetsVisibilityThreshold(true));

  // Update video visibility to not sufficiently visible, and verify that media
  // session reports video is not sufficiently visible.
  manager_->OnVideoVisibilityChanged(media_player_id_, false);
  EXPECT_FALSE(observer.WaitForMeetsVisibilityThreshold(false));
}

TEST_P(MediaSessionControllersManagerTest,
       SufficientlyVisibleVideoMultiplePlayers) {
  if (!IsMediaSessionEnabled()) {
    return;
  }

  manager_->OnMetadata(media_player_id_, true, true,
                       media::MediaContentType::kPersistent);
  manager_->OnMetadata(media_player_id2_, true, true,
                       media::MediaContentType::kPersistent);

  media_session::test::MockMediaSessionMojoObserver observer(*media_session());

  manager_->OnVideoVisibilityChanged(media_player_id_, true);
  manager_->OnVideoVisibilityChanged(media_player_id2_, true);

  // If there is exactly one player, media session reports its video visibility.
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_));
  EXPECT_TRUE(observer.WaitForMeetsVisibilityThreshold(true));

  // Change the video visibility of the first player's video.
  manager_->OnVideoVisibilityChanged(media_player_id_, false);

  // Stop the second player.
  manager_->OnPause(media_player_id2_, true);

  // There is exactly one player again (the second one). Media session should
  // use its updated video visibility.
  EXPECT_TRUE(observer.WaitForMeetsVisibilityThreshold(true));

  // Stop the first player.
  manager_->OnPause(media_player_id_, true);

  // There are no remaining players. Media session should report there are no
  // visible videos.
  EXPECT_FALSE(observer.WaitForMeetsVisibilityThreshold(false));
}

// First bool is to indicate whether InternalMediaSession is enabled.
// Second bool is to indicate whether AudioFocus is enabled.
INSTANTIATE_TEST_SUITE_P(MediaSessionEnabledTestInstances,
                         MediaSessionControllersManagerTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));
}  // namespace content
