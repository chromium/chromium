// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_controllers_manager.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_controller.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace content {

namespace {

class MockMediaSessionController : public MediaSessionController {
 public:
  MockMediaSessionController(
      const MediaPlayerId& id,
      MediaWebContentsObserver* media_web_contents_observer)
      : MediaSessionController(id, media_web_contents_observer) {}

  MOCK_METHOD0(OnPlaybackPaused, void());
};

}  // namespace

class MediaSessionControllersManagerTest
    : public RenderViewHostImplTestHarness,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  // Indices of the tuple parameters.
  static const int kIsInternalMediaSessionEnabled = 0;
  static const int kIsAudioFocusEnabled = 1;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

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

    service_manager_context_ = std::make_unique<TestServiceManagerContext>();

    media_player_id_ = MediaPlayerId(contents()->GetMainFrame(), 1);
    mock_media_session_controller_ =
        std::make_unique<StrictMock<MockMediaSessionController>>(
            media_player_id_, contents()->media_web_contents_observer());
    mock_media_session_controller_ptr_ = mock_media_session_controller_.get();
    manager_ = std::make_unique<MediaSessionControllersManager>(
        contents()->media_web_contents_observer());
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

  MediaSessionControllersManager::ControllersMap* GetControllersMap() {
    return &manager_->controllers_map_;
  }

  base::Optional<media_session::MediaPosition> GetPosition(
      const MediaPlayerId& id) {
    auto it = manager_->controllers_map_.find(id);
    DCHECK(it != manager_->controllers_map_.end());

    auto* controller = it->second.get();
    return controller->GetPosition(controller->get_player_id_for_testing());
  }

  void TearDown() override {
    mock_media_session_controller_.reset();
    mock_media_session_controller_ptr_ = nullptr;
    manager_.reset();
    service_manager_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  MediaPlayerId media_player_id_ = MediaPlayerId::CreateMediaPlayerIdForTests();
  std::unique_ptr<StrictMock<MockMediaSessionController>>
      mock_media_session_controller_;
  StrictMock<MockMediaSessionController>* mock_media_session_controller_ptr_ =
      nullptr;
  std::unique_ptr<MediaSessionControllersManager> manager_;
  std::unique_ptr<TestServiceManagerContext> service_manager_context_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(MediaSessionControllersManagerTest, RequestPlayAddsSessionsToMap) {
  EXPECT_TRUE(GetControllersMap()->empty());

  EXPECT_TRUE(manager_->RequestPlay(media_player_id_, true, false,
                                    media::MediaContentType::Transient));
  if (!IsMediaSessionEnabled()) {
    EXPECT_TRUE(GetControllersMap()->empty());
  } else {
    EXPECT_EQ(1U, GetControllersMap()->size());
    EXPECT_TRUE(
        manager_->RequestPlay(MediaPlayerId(contents()->GetMainFrame(), 2),
                              true, false, media::MediaContentType::Transient));
    EXPECT_EQ(2U, GetControllersMap()->size());
  }
}

TEST_P(MediaSessionControllersManagerTest, RepeatAddsOfInitializablePlayer) {
  // If not enabled, no adds will occur, as RequestPlay returns early.
  if (!IsMediaSessionEnabled())
    return;

  EXPECT_TRUE(GetControllersMap()->empty());

  EXPECT_TRUE(manager_->RequestPlay(media_player_id_, true, false,
                                    media::MediaContentType::Transient));
  EXPECT_EQ(1U, GetControllersMap()->size());

  EXPECT_TRUE(manager_->RequestPlay(media_player_id_, true, false,
                                    media::MediaContentType::Transient));
  EXPECT_EQ(1U, GetControllersMap()->size());
}

TEST_P(MediaSessionControllersManagerTest, RenderFrameDeletedRemovesHost) {
  EXPECT_TRUE(GetControllersMap()->empty());

  // Nothing should be removed if not enabled.
  if (!IsMediaSessionEnabled()) {
    // Artifically add controller to show early return.
    GetControllersMap()->insert(std::make_pair(
        media_player_id_, std::move(mock_media_session_controller_)));
    EXPECT_EQ(1U, GetControllersMap()->size());

    manager_->RenderFrameDeleted(contents()->GetMainFrame());
    EXPECT_EQ(1U, GetControllersMap()->size());
  } else {
    EXPECT_TRUE(manager_->RequestPlay(media_player_id_, true, false,
                                      media::MediaContentType::Transient));
    EXPECT_EQ(1U, GetControllersMap()->size());

    manager_->RenderFrameDeleted(contents()->GetMainFrame());
    EXPECT_TRUE(GetControllersMap()->empty());
  }
}

TEST_P(MediaSessionControllersManagerTest, OnPauseCallsPlaybackPaused) {
  // Artifically add controller to show early return.
  GetControllersMap()->insert(std::make_pair(
      media_player_id_, std::move(mock_media_session_controller_)));
  if (IsMediaSessionEnabled())
    EXPECT_CALL(*mock_media_session_controller_ptr_, OnPlaybackPaused());

  manager_->OnPause(media_player_id_);
}

TEST_P(MediaSessionControllersManagerTest, OnPauseIdNotFound) {
  // If MediaSession is not enabled, we don't remove anything, nothing to check.
  if (!IsMediaSessionEnabled())
    return;

  // Artifically add controller to show early return.
  GetControllersMap()->insert(std::make_pair(
      media_player_id_, std::move(mock_media_session_controller_)));

  MediaPlayerId id2 = MediaPlayerId(contents()->GetMainFrame(), 2);
  manager_->OnPause(id2);
}

TEST_P(MediaSessionControllersManagerTest, OnEndRemovesMediaPlayerId) {
  EXPECT_TRUE(GetControllersMap()->empty());

  // No op if not enabled.
  if (!IsMediaSessionEnabled()) {
    // Artifically add controller to show early return.
    GetControllersMap()->insert(std::make_pair(
        media_player_id_, std::move(mock_media_session_controller_)));
    EXPECT_EQ(1U, GetControllersMap()->size());

    manager_->OnEnd(media_player_id_);
    EXPECT_EQ(1U, GetControllersMap()->size());
  } else {
    EXPECT_TRUE(manager_->RequestPlay(media_player_id_, true, false,
                                      media::MediaContentType::Transient));
    EXPECT_EQ(1U, GetControllersMap()->size());

    manager_->OnEnd(media_player_id_);
    EXPECT_TRUE(GetControllersMap()->empty());
  }
}

TEST_P(MediaSessionControllersManagerTest, PositionState) {
  // If not enabled, no adds will occur, as RequestPlay returns early.
  if (!IsMediaSessionEnabled())
    return;

  {
    media_session::MediaPosition expected_position(1.0, base::TimeDelta(),
                                                   base::TimeDelta());

    manager_->OnMediaPositionStateChanged(media_player_id_, expected_position);

    EXPECT_TRUE(manager_->RequestPlay(media_player_id_, true, false,
                                      media::MediaContentType::Transient));
    EXPECT_EQ(1U, GetControllersMap()->size());

    // The controller should be created with the last received position for
    // that player.
    EXPECT_EQ(expected_position, GetPosition(media_player_id_));
  }

  {
    media_session::MediaPosition expected_position(
        0.0, base::TimeDelta::FromSeconds(10), base::TimeDelta());

    manager_->OnMediaPositionStateChanged(media_player_id_, expected_position);

    // The controller should be updated with the new position.
    EXPECT_EQ(expected_position, GetPosition(media_player_id_));

    // Destroy the current controller.
    manager_->OnEnd(media_player_id_);
    EXPECT_TRUE(GetControllersMap()->empty());

    // Recreate the current controller.
    EXPECT_TRUE(manager_->RequestPlay(media_player_id_, true, false,
                                      media::MediaContentType::Transient));
    EXPECT_EQ(1U, GetControllersMap()->size());

    // The controller should be created with the last received position for
    // that player.
    EXPECT_EQ(expected_position, GetPosition(media_player_id_));
  }
}

TEST_P(MediaSessionControllersManagerTest, MultiplePlayersWithPositionState) {
  // If not enabled, no adds will occur, as RequestPlay returns early.
  if (!IsMediaSessionEnabled())
    return;

  MediaPlayerId media_player_id_2 =
      MediaPlayerId(contents()->GetMainFrame(), 2);

  media_session::MediaPosition expected_position1(1.0, base::TimeDelta(),
                                                  base::TimeDelta());
  media_session::MediaPosition expected_position2(
      0.0, base::TimeDelta::FromSeconds(10), base::TimeDelta());

  manager_->OnMediaPositionStateChanged(media_player_id_, expected_position1);
  manager_->OnMediaPositionStateChanged(media_player_id_2, expected_position2);

  EXPECT_TRUE(manager_->RequestPlay(media_player_id_, true, false,
                                    media::MediaContentType::Transient));
  EXPECT_TRUE(manager_->RequestPlay(media_player_id_2, true, false,
                                    media::MediaContentType::Transient));

  EXPECT_EQ(2U, GetControllersMap()->size());

  // The controllers should have been created with the correct positions.
  EXPECT_EQ(expected_position1, GetPosition(media_player_id_));
  EXPECT_EQ(expected_position2, GetPosition(media_player_id_2));

  media_session::MediaPosition new_position(
      0.0, base::TimeDelta::FromSeconds(20), base::TimeDelta());

  manager_->OnMediaPositionStateChanged(media_player_id_, new_position);

  // The controller should be updated with the new position.
  EXPECT_EQ(new_position, GetPosition(media_player_id_));
  EXPECT_EQ(expected_position2, GetPosition(media_player_id_2));
}

// First bool is to indicate whether InternalMediaSession is enabled.
// Second bool is to indicate whether AudioFocus is enabled.
INSTANTIATE_TEST_SUITE_P(MediaSessionEnabledTestInstances,
                         MediaSessionControllersManagerTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));
}  // namespace content
