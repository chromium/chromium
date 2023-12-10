// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/media_player_impl.h"

#include <fidl/fuchsia.media.sessions2/cpp/fidl.h>
#include <lib/async/default.h>

#include "base/fuchsia/fidl_event_handler.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
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

bool HasFlag(const fuchsia_media_sessions2::PlayerCapabilityFlags bits,
             const fuchsia_media_sessions2::PlayerCapabilityFlags flag) {
  return (bits & flag) == flag;
}

}  // namespace

class MediaPlayerImplTest : public testing::Test {
 public:
  MediaPlayerImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        player_error_handler_(
            base::BindRepeating(&MediaPlayerImplTest::OnPlayerFidlError,
                                base::Unretained(this))) {
    auto player_endpoints =
        fidl::CreateEndpoints<fuchsia_media_sessions2::Player>();
    ZX_CHECK(player_endpoints.is_ok(), player_endpoints.status_value());
    player_.Bind(std::move(player_endpoints->client),
                 async_get_default_dispatcher(), &player_error_handler_);
    player_server_end_ = std::move(player_endpoints->server);
  }

  MediaPlayerImplTest(const MediaPlayerImplTest&) = delete;
  MediaPlayerImplTest& operator=(const MediaPlayerImplTest&) = delete;

  ~MediaPlayerImplTest() override = default;

  void SetPlayerFidlErrorCallback(
      base::RepeatingCallback<void(fidl::UnbindInfo)>
          player_fidl_error_callback) {
    player_fidl_error_callback_ = std::move(player_fidl_error_callback);
  }

 protected:
  void OnPlayerFidlError(fidl::UnbindInfo error) {
    if (player_fidl_error_callback_) {
      player_fidl_error_callback_.Run(std::move(error));
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  testing::StrictMock<FakeMediaSession> fake_session_;
  fidl::Client<fuchsia_media_sessions2::Player> player_;
  base::FidlErrorEventHandler<fuchsia_media_sessions2::Player>
      player_error_handler_;
  base::RepeatingCallback<void(fidl::UnbindInfo)> player_fidl_error_callback_;

  fidl::ServerEnd<fuchsia_media_sessions2::Player> player_server_end_;

  std::unique_ptr<MediaPlayerImpl> player_impl_;
};

// Verify that the `on_disconnect` closure is invoked if the client disconnects.
TEST_F(MediaPlayerImplTest, OnDisconnectCalledOnDisconnect) {
  base::RunLoop run_loop;
  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, std::move(player_server_end_), run_loop.QuitClosure());
  player_ = {};
  run_loop.Run();
}

// Verify that the `on_disconnect` closure is invoked if the client calls the
// WatchInfoChange() API incorrectly.
TEST_F(MediaPlayerImplTest, ClientDisconnectedOnBadApiUsage) {
  base::RunLoop on_disconnected_loop;
  base::RunLoop player_error_loop;

  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, std::move(player_server_end_),
      on_disconnected_loop.QuitClosure());
  SetPlayerFidlErrorCallback(
      base::BindLambdaForTesting([&player_error_loop](fidl::UnbindInfo error) {
        EXPECT_EQ(error.status(), ZX_ERR_BAD_STATE);
        player_error_loop.Quit();
      }));

  // Call WatchInfoChange() three times in succession. The first call may
  // immediately invoke the callback, with initial state, but since there will
  // be no state-change between that and the second, it will hold the callback,
  // and the third call will therefore be a protocol violation.
  player_->WatchInfoChange().Then([](auto& result) {
    ASSERT_TRUE(result.is_ok()) << result.error_value().status_string();
  });
  player_->WatchInfoChange().Then(
      [](auto& result) { ASSERT_TRUE(result.is_error()); });
  player_->WatchInfoChange().Then(
      [](auto& result) { ASSERT_TRUE(result.is_error()); });

  // Wait for both on-disconnected and player error handler to be invoked.
  on_disconnected_loop.Run();
  player_error_loop.Run();
}

// Verify that the completer is Closed on destruction of `MediaPlayerImpl`.
// Otherwise it will trigger a CHECK for failing to reply.
TEST_F(MediaPlayerImplTest, WatchInfoChangeAsyncCompleterClosedOnDestruction) {
  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, std::move(player_server_end_),
      MakeExpectedNotRunClosure(FROM_HERE));
  player_->WatchInfoChange().Then([](auto result) {});
  // The first call always replies immediately, so call a second call to hold a
  // pending completer.
  player_->WatchInfoChange().Then([](auto result) {});

  // Pump the message loop to process the WatchInfoChange() call.
  base::RunLoop().RunUntilIdle();
}

// Verify that the first WatchInfoChange() registers the observer.
TEST_F(MediaPlayerImplTest, WatchInfoChangeRegistersObserver) {
  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, std::move(player_server_end_),
      MakeExpectedNotRunClosure(FROM_HERE));
  player_->WatchInfoChange().Then([](auto result) {});

  ASSERT_FALSE(fake_session_.observer());

  // Pump the message loop to process the WatchInfoChange() call.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_session_.observer());
}

// Verify that the initial session state is returned via WatchInfoChange(),
// potentially via several calls to it.
TEST_F(MediaPlayerImplTest, WatchInfoChangeReturnsInitialState) {
  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, std::move(player_server_end_),
      MakeExpectedNotRunClosure(FROM_HERE));

  base::RunLoop return_info_loop;
  fuchsia_media_sessions2::PlayerInfoDelta initial_info;
  std::function<void(
      fidl::Result<fuchsia_media_sessions2::Player::WatchInfoChange>&)>
      on_info_change =
          [this, &initial_info, &on_info_change, &return_info_loop](
              fidl::Result<fuchsia_media_sessions2::Player::WatchInfoChange>&
                  result) {
            ASSERT_TRUE(result.is_ok()) << result.error_value().status_string();
            auto delta = result->player_info_delta();
            if (delta.player_status().has_value()) {
              initial_info.player_status(
                  std::move(delta.player_status().value()));
            }
            if (delta.metadata().has_value()) {
              initial_info.metadata(std::move(delta.metadata().value()));
            }
            if (delta.player_capabilities().has_value()) {
              initial_info.player_capabilities(
                  std::move(delta.player_capabilities().value()));
            }

            // Only quit the loop once all of the expected fields are present.
            if (initial_info.player_status().has_value() &&
                initial_info.metadata().has_value() &&
                initial_info.player_capabilities().has_value()) {
              return_info_loop.Quit();
            } else {
              player_->WatchInfoChange().Then(on_info_change);
            }
          };
  player_->WatchInfoChange().Then(on_info_change);

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
  ASSERT_TRUE(initial_info.player_status().has_value());
  EXPECT_EQ(initial_info.player_status()->player_state(),
            fuchsia_media_sessions2::PlayerState::kPaused);
  ASSERT_TRUE(initial_info.metadata().has_value());
  std::map<std::string, std::string> received_metadata;
  for (auto& property : initial_info.metadata()->properties()) {
    received_metadata[property.label()] = property.value();
  }
  EXPECT_EQ(received_metadata[fuchsia_media::kMetadataLabelTitle],
            kExpectedTitle);
  EXPECT_EQ(received_metadata[fuchsia_media::kMetadataLabelArtist],
            kExpectedArtist);
  EXPECT_EQ(received_metadata[fuchsia_media::kMetadataLabelAlbum],
            kExpectedAlbum);
  EXPECT_EQ(received_metadata[fuchsia_media::kMetadataSourceTitle],
            kExpectedSourceTitle);
  ASSERT_TRUE(initial_info.player_capabilities().has_value());
  ASSERT_TRUE(initial_info.player_capabilities()->flags().has_value());
  const fuchsia_media_sessions2::PlayerCapabilityFlags received_flags =
      initial_info.player_capabilities()->flags().value();
  EXPECT_TRUE(HasFlag(received_flags,
                      fuchsia_media_sessions2::PlayerCapabilityFlags::kPlay));
  EXPECT_TRUE(HasFlag(
      received_flags,
      fuchsia_media_sessions2::PlayerCapabilityFlags::kChangeToNextItem));
  EXPECT_FALSE(HasFlag(received_flags,
                       fuchsia_media_sessions2::PlayerCapabilityFlags::kSeek));
  EXPECT_FALSE(HasFlag(received_flags,
                       fuchsia_media_sessions2::PlayerCapabilityFlags::kPause));
}

// Verify that WatchInfoChange() waits for the next change to the session state
// before returning.
TEST_F(MediaPlayerImplTest, WatchInfoChangeWaitsForNextChange) {
  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, std::move(player_server_end_),
      MakeExpectedNotRunClosure(FROM_HERE));

  // Start watching, which will connect the observer, and send some initial
  // state so that WatchInfoChange() will return.
  base::RunLoop player_state_loop;
  std::function<void(
      fidl::Result<fuchsia_media_sessions2::Player::WatchInfoChange>&)>
      on_info_change =
          [this, &on_info_change, &player_state_loop](
              fidl::Result<fuchsia_media_sessions2::Player::WatchInfoChange>&
                  result) {
            ASSERT_TRUE(result.is_ok()) << result.error_value().status_string();
            auto delta = result->player_info_delta();
            if (!delta.player_status().has_value() ||
                !delta.player_status()->player_state().has_value()) {
              player_->WatchInfoChange().Then(on_info_change);
              return;
            }
            EXPECT_EQ(delta.player_status()->player_state().value(),
                      fuchsia_media_sessions2::PlayerState::kPaused);
            player_state_loop.Quit();
          };
  player_->WatchInfoChange().Then(on_info_change);

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
  std::optional<fuchsia_media_sessions2::PlayerState> state_after_change;

  player_->WatchInfoChange().Then(
      [&change_loop, &state_after_change](
          fidl::Result<fuchsia_media_sessions2::Player::WatchInfoChange>&
              result) {
        ASSERT_TRUE(result.is_ok()) << result.error_value().status_string();
        auto delta = result->player_info_delta();
        ASSERT_TRUE(delta.player_status().has_value());
        ASSERT_TRUE(delta.player_status()->player_state().has_value());
        state_after_change.emplace(
            delta.player_status()->player_state().value());
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
            fuchsia_media_sessions2::PlayerState::kPlaying);
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

  player_impl_ = std::make_unique<MediaPlayerImpl>(
      &fake_session_, std::move(player_server_end_),
      MakeExpectedNotRunClosure(FROM_HERE));

  EXPECT_TRUE(player_->Play().is_ok());
  EXPECT_TRUE(player_->Pause().is_ok());
  EXPECT_TRUE(player_->Stop().is_ok());
  EXPECT_TRUE(player_->Seek(0).is_ok());
  EXPECT_TRUE(player_->SkipForward().is_ok());
  EXPECT_TRUE(player_->SkipReverse().is_ok());
  EXPECT_TRUE(player_->NextItem().is_ok());
  EXPECT_TRUE(player_->PrevItem().is_ok());

  // Pump the message loop to process each of the calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(skip_forward_delta_, -skip_reverse_delta_);
}
