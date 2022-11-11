// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/media_player_impl.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/mock_media_session.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeMediaSession : public content::MockMediaSession {
 public:
  // content::MediaSession APIs mocked to observe if/when they are called.
  void SetDuckingVolumeMultiplier(double multiplier) override { ADD_FAILURE(); }
  void SetAudioFocusGroupId(const base::UnguessableToken& group_id) override {
    ADD_FAILURE();
  }

  // content::MediaSession APIs faked to implement testing behaviour.
  void AddObserver(
      mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer)
      override {
    if (observer_.is_bound()) {
      ADD_FAILURE();
    } else {
      observer_.Bind(std::move(observer));
    }
  }

  media_session::mojom::MediaSessionObserver* observer() const {
    return observer_.is_bound() ? observer_.get() : nullptr;
  }

 protected:
  mojo::Remote<media_session::mojom::MediaSessionObserver> observer_;
};

bool HasFlag(const fuchsia::media::sessions2::PlayerCapabilityFlags bits,
             const fuchsia::media::sessions2::PlayerCapabilityFlags flag) {
  return (bits & flag) == flag;
}

}  // namespace

class MediaPlayerImplTest : public testing::Test {
 public:
  MediaPlayerImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  MediaPlayerImplTest(const MediaPlayerImplTest&) = delete;
  MediaPlayerImplTest& operator=(const MediaPlayerImplTest&) = delete;

  ~MediaPlayerImplTest() override = default;

  void OnPlayerDisconnected() {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::StrictMock<FakeMediaSession> fake_session_;
  fuchsia::media::sessions2::PlayerPtr player_;

  std::unique_ptr<MediaPlayerImpl> player_impl_;
};

// Verify that the |on_disconnect| closure is invoked if the client disconnects.
TEST_F(MediaPlayerImplTest, OnDisconnectCalledOnDisconnect) {
  base::RunLoop run_loop;
  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, player_.NewRequest(), run_loop.QuitClosure());
  player_.Unbind();
  run_loop.Run();
}

// Verify that the |on_disconnect| closure is invoked if the client calls the
// WatchInfoChange() API incorrectly.
TEST_F(MediaPlayerImplTest, ClientDisconnectedOnBadApiUsage) {
  base::RunLoop on_disconnected_loop;
  base::RunLoop player_error_loop;

  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, player_.NewRequest(), on_disconnected_loop.QuitClosure());
  player_.set_error_handler([&player_error_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_BAD_STATE);
    player_error_loop.Quit();
  });

  // Call WatchInfoChange() three times in succession. The first call may
  // immediately invoke the callback, with initial state, but since there will
  // be no state-change between that and the second, it will hold the callback,
  // and the third call will therefore be a protocol violation.
  player_->WatchInfoChange([](fuchsia::media::sessions2::PlayerInfoDelta) {});
  player_->WatchInfoChange(
      [](fuchsia::media::sessions2::PlayerInfoDelta) { ADD_FAILURE(); });
  player_->WatchInfoChange(
      [](fuchsia::media::sessions2::PlayerInfoDelta) { ADD_FAILURE(); });

  // Wait for both on-disconnected and player error handler to be invoked.
  on_disconnected_loop.Run();
  player_error_loop.Run();
}

// Verify that the first WatchInfoChange() registers the observer.
TEST_F(MediaPlayerImplTest, WatchInfoChangeRegistersObserver) {
  player_impl_ =
      std::make_unique<MediaPlayerImpl>(&fake_session_, player_.NewRequest(),
                                        MakeExpectedNotRunClosure(FROM_HERE));
  player_->WatchInfoChange([](fuchsia::media::sessions2::PlayerInfoDelta) {});

  ASSERT_FALSE(fake_session_.observer());

  // Pump the message loop to process the WatchInfoChange() call.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_session_.observer());
}

// Verify that the initial session state is returned via WatchInfoChange(),
// potentially via several calls to it.
TEST_F(MediaPlayerImplTest, WatchInfoChangeReturnsInitialState) {
  player_impl_ =
      std::make_unique<MediaPlayerImpl>(&fake_session_, player_.NewRequest(),
                                        MakeExpectedNotRunClosure(FROM_HERE));

  base::RunLoop return_info_loop;
  fuchsia::media::sessions2::PlayerInfoDelta initial_info;
  std::function<void(fuchsia::media::sessions2::PlayerInfoDelta)> watch_info =
      [this, &initial_info, &watch_info,
       &return_info_loop](fuchsia::media::sessions2::PlayerInfoDelta delta) {
        if (delta.has_player_status())
          initial_info.set_player_status(
              std::move(*delta.mutable_player_status()));
        if (delta.has_metadata())
          initial_info.set_metadata(delta.metadata());
        if (delta.has_player_capabilities())
          initial_info.set_player_capabilities(
              std::move(*delta.mutable_player_capabilities()));

        // Only quit the loop once all of the expected fields are present.
        if (initial_info.has_player_status() && initial_info.has_metadata() &&
            initial_info.has_player_capabilities()) {
          return_info_loop.Quit();
        } else {
          player_->WatchInfoChange(watch_info);
        }
      };
  player_->WatchInfoChange(watch_info);

  // Pump the message loop to process the WatchInfoChange() call.
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fake_session_.observer());

  media_session::mojom::MediaSessionInfoPtr info(
      media_session::mojom::MediaSessionInfo::New());
  info->state =
      media_session::mojom::MediaSessionInfo::SessionState::kSuspended;
  fake_session_.observer()->MediaSessionInfoChanged(std::move(info));

  media_session::MediaMetadata metadata;
  constexpr char kExpectedTitle[] = "Love Like A Sunset, Pt.1";
  constexpr char16_t kExpectedTitle16[] = u"Love Like A Sunset, Pt.1";
  metadata.title = kExpectedTitle16;
  constexpr char kExpectedArtist[] = "Phoenix";
  constexpr char16_t kExpectedArtist16[] = u"Phoenix";
  metadata.artist = kExpectedArtist16;
  constexpr char kExpectedAlbum[] = "Wolfgang Amadeus Phoenix";
  constexpr char16_t kExpectedAlbum16[] = u"Wolfgang Amadeus Phoenix";
  metadata.album = kExpectedAlbum16;
  constexpr char kExpectedSourceTitle[] = "Unknown";
  constexpr char16_t kExpectedSourceTitle16[] = u"Unknown";
  metadata.source_title = kExpectedSourceTitle16;
  fake_session_.observer()->MediaSessionMetadataChanged(metadata);

  std::vector<media_session::mojom::MediaSessionAction> actions = {
      media_session::mojom::MediaSessionAction::kPlay,
      media_session::mojom::MediaSessionAction::kNextTrack,
      media_session::mojom::MediaSessionAction::kScrubTo};
  fake_session_.observer()->MediaSessionActionsChanged(actions);

  // These are sent by MediaSessionImpl, but currently ignored.
  fake_session_.observer()->MediaSessionImagesChanged({});
  fake_session_.observer()->MediaSessionPositionChanged({});

  return_info_loop.Run();

  // Verify that all of the expected fields are present, and correct.
  ASSERT_TRUE(initial_info.has_player_status());
  ASSERT_TRUE(initial_info.player_status().has_player_state());
  EXPECT_EQ(initial_info.player_status().player_state(),
            fuchsia::media::sessions2::PlayerState::PAUSED);
  ASSERT_TRUE(initial_info.has_metadata());
  std::map<std::string, std::string> received_metadata;
  for (auto& property : initial_info.metadata().properties)
    received_metadata[property.label] = property.value;
  EXPECT_EQ(received_metadata[fuchsia::media::METADATA_LABEL_TITLE],
            kExpectedTitle);
  EXPECT_EQ(received_metadata[fuchsia::media::METADATA_LABEL_ARTIST],
            kExpectedArtist);
  EXPECT_EQ(received_metadata[fuchsia::media::METADATA_LABEL_ALBUM],
            kExpectedAlbum);
  EXPECT_EQ(received_metadata[fuchsia::media::METADATA_SOURCE_TITLE],
            kExpectedSourceTitle);
  ASSERT_TRUE(initial_info.has_player_capabilities());
  ASSERT_TRUE(initial_info.player_capabilities().has_flags());
  const fuchsia::media::sessions2::PlayerCapabilityFlags received_flags =
      initial_info.player_capabilities().flags();
  EXPECT_TRUE(HasFlag(received_flags,
                      fuchsia::media::sessions2::PlayerCapabilityFlags::PLAY));
  EXPECT_TRUE(HasFlag(
      received_flags,
      fuchsia::media::sessions2::PlayerCapabilityFlags::CHANGE_TO_NEXT_ITEM));
  EXPECT_FALSE(HasFlag(received_flags,
                       fuchsia::media::sessions2::PlayerCapabilityFlags::SEEK));
  EXPECT_FALSE(HasFlag(
      received_flags, fuchsia::media::sessions2::PlayerCapabilityFlags::PAUSE));
}

// Verify that WatchInfoChange() waits for the next change to the session state
// before returning.
TEST_F(MediaPlayerImplTest, WatchInfoChangeWaitsForNextChange) {
  player_impl_ =
      std::make_unique<MediaPlayerImpl>(&fake_session_, player_.NewRequest(),
                                        MakeExpectedNotRunClosure(FROM_HERE));

  // Start watching, which will connect the observer, and send some initial
  // state so that WatchInfoChange() will return.
  base::RunLoop player_state_loop;
  std::function<void(fuchsia::media::sessions2::PlayerInfoDelta)>
      watch_for_player_state =
          [this, &watch_for_player_state, &player_state_loop](
              fuchsia::media::sessions2::PlayerInfoDelta delta) {
            if (!delta.has_player_status() ||
                !delta.player_status().has_player_state()) {
              player_->WatchInfoChange(watch_for_player_state);
              return;
            }
            EXPECT_EQ(delta.player_status().player_state(),
                      fuchsia::media::sessions2::PlayerState::PAUSED);
            player_state_loop.Quit();
          };
  player_->WatchInfoChange(watch_for_player_state);

  // Pump the message loop to process the first WatchInfoChange() call.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_session_.observer());

  // Set an initial player state for the session, and wait for it.
  media_session::mojom::MediaSessionInfoPtr info(
      media_session::mojom::MediaSessionInfo::New());
  info->state =
      media_session::mojom::MediaSessionInfo::SessionState::kSuspended;
  fake_session_.observer()->MediaSessionInfoChanged(std::move(info));
  player_state_loop.Run();

  // Calling WatchInfoChange() now should succeed, but not immediately return
  // any new data.
  base::RunLoop change_loop;
  absl::optional<fuchsia::media::sessions2::PlayerState> state_after_change;

  player_->WatchInfoChange(
      [&change_loop,
       &state_after_change](fuchsia::media::sessions2::PlayerInfoDelta delta) {
        ASSERT_TRUE(delta.has_player_status());
        ASSERT_TRUE(delta.player_status().has_player_state());
        state_after_change.emplace(delta.player_status().player_state());
        change_loop.Quit();
      });

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(state_after_change.has_value());

  // Generate a player status change, which should cause WatchInfoChange() to
  // return.
  info = media_session::mojom::MediaSessionInfo::New();
  info->state = media_session::mojom::MediaSessionInfo::SessionState::kActive;
  fake_session_.observer()->MediaSessionInfoChanged(std::move(info));
  change_loop.Run();
  ASSERT_TRUE(state_after_change.has_value());
  EXPECT_EQ(*state_after_change,
            fuchsia::media::sessions2::PlayerState::PLAYING);
}

// Verify that each of the fire-and-forget playback controls are routed to the
// expected MediaSession APIs.
TEST_F(MediaPlayerImplTest, PlaybackControls) {
  testing::InSequence sequence;
  EXPECT_CALL(fake_session_, Resume(content::MediaSession::SuspendType::kUI));
  EXPECT_CALL(fake_session_, Suspend(content::MediaSession::SuspendType::kUI));
  EXPECT_CALL(fake_session_, Suspend(content::MediaSession::SuspendType::kUI));
  EXPECT_CALL(fake_session_, SeekTo(base::TimeDelta()));
  base::TimeDelta skip_forward_delta_;
  EXPECT_CALL(fake_session_, Seek(testing::_))
      .WillOnce(testing::SaveArg<0>(&skip_forward_delta_));
  base::TimeDelta skip_reverse_delta_;
  EXPECT_CALL(fake_session_, Seek(testing::_))
      .WillOnce(testing::SaveArg<0>(&skip_reverse_delta_));
  EXPECT_CALL(fake_session_, NextTrack());
  EXPECT_CALL(fake_session_, PreviousTrack());

  player_impl_ =
      std::make_unique<MediaPlayerImpl>(&fake_session_, player_.NewRequest(),
                                        MakeExpectedNotRunClosure(FROM_HERE));

  player_->Play();
  player_->Pause();
  player_->Stop();
  player_->Seek(0);
  player_->SkipForward();
  player_->SkipReverse();
  player_->NextItem();
  player_->PrevItem();

  // Pump the message loop to process each of the calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(skip_forward_delta_, -skip_reverse_delta_);
}
