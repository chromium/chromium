// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include <memory>

#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "media/mojo/mojom/media_player.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
namespace {

constexpr auto kContentType = media::MediaContentType::kPersistent;

class TestMediaPlayer final : public media::mojom::MediaPlayer {
 public:
  mojo::AssociatedReceiver<media::mojom::MediaPlayer>& receiver() {
    return receiver_;
  }

  // media::mojom::MediaPlayer implementation.
  void RequestPlay() override {}
  void RequestPause(bool triggered_by_user) override {}
  void RequestSeekForward(base::TimeDelta seek_time) override {}
  void RequestSeekBackward(base::TimeDelta seek_time) override {}
  void RequestSeekTo(base::TimeDelta seek_time) override {}
  void RequestEnterPictureInPicture() override {}
  void RequestMute(bool mute) override {}
  void SetVolumeMultiplier(double multiplier) override {}
  void SetPersistentState(bool persistent) override {}
  void SetPowerExperimentState(bool enabled) override {}
  void SetAudioSinkId(const std::string& sink_id) override {}
  void SuspendForFrameClosed() override {}
  void RequestMediaRemoting() override {}
  void RequestVisibility(
      RequestVisibilityCallback request_visibility_callback) override {}
  void RecordAutoPictureInPictureInfo(
      const media::PictureInPictureEventsInfo::AutoPipInfo&
          auto_picture_in_picture_info) override {}

 private:
  mojo::AssociatedReceiver<media::mojom::MediaPlayer> receiver_{this};
};

class MediaWebContentsObserverTest : public RenderViewHostImplTestHarness {
 public:
  struct PlayerSetup {
    std::unique_ptr<TestMediaPlayer> player;
    mojo::AssociatedRemote<media::mojom::MediaPlayerObserver> observer;
    int32_t player_id;
  };

 protected:
  mojo::AssociatedRemote<media::mojom::MediaPlayerHost> SetupPlayerHost() {
    mojo::AssociatedRemote<media::mojom::MediaPlayerHost> player_host;
    contents()->media_web_contents_observer()->BindMediaPlayerHost(
        contents()->GetPrimaryMainFrame()->GetGlobalId(),
        player_host.BindNewEndpointAndPassDedicatedReceiver());
    return player_host;
  }

  auto CreateAndAddPlayer(
      mojo::AssociatedRemote<media::mojom::MediaPlayerHost>& player_host,
      int32_t player_id) -> PlayerSetup {
    PlayerSetup setup{.player = std::make_unique<TestMediaPlayer>(),
                      .player_id = player_id};
    player_host->OnMediaPlayerAdded(
        setup.player->receiver().BindNewEndpointAndPassRemote(),
        setup.observer.BindNewEndpointAndPassReceiver(), player_id);
    player_host.FlushForTesting();
    return setup;
  }

  void SetMediaMetadata(
      mojo::AssociatedRemote<media::mojom::MediaPlayerObserver>& observer,
      bool has_audio,
      bool has_video) {
    observer->OnMediaMetadataChanged(has_audio, has_video, kContentType);
    observer.FlushForTesting();
  }

  void PlayMedia(
      mojo::AssociatedRemote<media::mojom::MediaPlayerObserver>& observer) {
    observer->OnMediaPlaying();
    observer.FlushForTesting();
  }

  void PauseMedia(
      mojo::AssociatedRemote<media::mojom::MediaPlayerObserver>& observer,
      bool stream_ended = false) {
    observer->OnMediaPaused(stream_ended);
    observer.FlushForTesting();
  }

  void SetFullscreenStatus(
      mojo::AssociatedRemote<media::mojom::MediaPlayerObserver>& observer,
      blink::WebFullscreenVideoStatus status) {
    observer->OnMediaEffectivelyFullscreenChanged(status);
    observer.FlushForTesting();
  }

  MediaPlayerId CreatePlayerId(int32_t player_id) {
    return MediaPlayerId(contents()->GetPrimaryMainFrame()->GetGlobalId(),
                         player_id);
  }

  MediaWebContentsObserver& media_web_contents_observer() {
    return *contents()->media_web_contents_observer();
  }
};

TEST_F(MediaWebContentsObserverTest, GetCurrentlyPlayingVideoCount) {
  auto player_host = SetupPlayerHost();
  constexpr int32_t kAudioVideoPlayerId = 0;
  constexpr int32_t kVideoPlayerId = 1;
  auto audio_video_player =
      CreateAndAddPlayer(player_host, kAudioVideoPlayerId);
  auto video_player = CreateAndAddPlayer(player_host, kVideoPlayerId);

  EXPECT_EQ(media_web_contents_observer().GetCurrentlyPlayingVideoCount(), 0);

  SetMediaMetadata(audio_video_player.observer, /*has_audio=*/true,
                   /*has_video=*/false);
  EXPECT_EQ(media_web_contents_observer().GetCurrentlyPlayingVideoCount(), 0)
      << "Nothing is playing";

  PlayMedia(audio_video_player.observer);
  EXPECT_EQ(media_web_contents_observer().GetCurrentlyPlayingVideoCount(), 0)
      << "An audio-only player is playing";

  SetMediaMetadata(video_player.observer, /*has_audio=*/false,
                   /*has_video=*/true);
  EXPECT_EQ(media_web_contents_observer().GetCurrentlyPlayingVideoCount(), 0)
      << "An audio-only player is playing";

  PlayMedia(video_player.observer);
  EXPECT_EQ(media_web_contents_observer().GetCurrentlyPlayingVideoCount(), 1);

  SetMediaMetadata(audio_video_player.observer, /*has_audio=*/true,
                   /*has_video=*/true);
  EXPECT_EQ(media_web_contents_observer().GetCurrentlyPlayingVideoCount(), 2)
      << "A video track was added to an initially audio-only player";

  PauseMedia(video_player.observer);
  EXPECT_EQ(media_web_contents_observer().GetCurrentlyPlayingVideoCount(), 1);

  SetMediaMetadata(audio_video_player.observer, /*has_audio=*/true,
                   /*has_video=*/false);
  EXPECT_EQ(media_web_contents_observer().GetCurrentlyPlayingVideoCount(), 0)
      << "The video track was removed again";
}

TEST_F(MediaWebContentsObserverTest,
       HasActiveEffectivelyFullscreenVideoEarlyReturns) {
  // Test early return when no fullscreen player exists.
  EXPECT_FALSE(
      media_web_contents_observer().HasActiveEffectivelyFullscreenVideo());

  auto player_host = SetupPlayerHost();
  constexpr int32_t kVideoPlayerId = 0;
  auto video_player = CreateAndAddPlayer(player_host, kVideoPlayerId);

  SetFullscreenStatus(
      video_player.observer,
      blink::WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled);

  // Should return false when player is fullscreen but web contents is not.
  EXPECT_FALSE(
      media_web_contents_observer().HasActiveEffectivelyFullscreenVideo());
}

TEST_F(MediaWebContentsObserverTest, FullscreenVideoPlayerIdAndPipPermission) {
  // Initially no fullscreen player.
  EXPECT_FALSE(media_web_contents_observer()
                   .GetFullscreenVideoMediaPlayerId()
                   .has_value());

  auto player_host = SetupPlayerHost();
  constexpr int32_t kVideoPlayerId = 42;
  auto video_player = CreateAndAddPlayer(player_host, kVideoPlayerId);

  // Enter fullscreen with PiP enabled.
  SetFullscreenStatus(
      video_player.observer,
      blink::WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled);

  EXPECT_TRUE(media_web_contents_observer()
                  .GetFullscreenVideoMediaPlayerId()
                  .has_value());
  EXPECT_EQ(media_web_contents_observer()
                .GetFullscreenVideoMediaPlayerId()
                ->player_id,
            kVideoPlayerId);
  EXPECT_TRUE(media_web_contents_observer()
                  .IsPictureInPictureAllowedForFullscreenVideo());

  // Change to PiP disabled.
  SetFullscreenStatus(
      video_player.observer,
      blink::WebFullscreenVideoStatus::kFullscreenAndPictureInPictureDisabled);

  EXPECT_TRUE(media_web_contents_observer()
                  .GetFullscreenVideoMediaPlayerId()
                  .has_value());
  EXPECT_FALSE(media_web_contents_observer()
                   .IsPictureInPictureAllowedForFullscreenVideo());

  // Exit fullscreen.
  SetFullscreenStatus(
      video_player.observer,
      blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen);

  EXPECT_FALSE(media_web_contents_observer()
                   .GetFullscreenVideoMediaPlayerId()
                   .has_value());
}

TEST_F(MediaWebContentsObserverTest, RequestPersistentVideoStateInvariant) {
  // Early return when no fullscreen player exists.
  EXPECT_FALSE(media_web_contents_observer()
                   .GetFullscreenVideoMediaPlayerId()
                   .has_value());
  media_web_contents_observer().RequestPersistentVideo(true);
  EXPECT_FALSE(media_web_contents_observer()
                   .GetFullscreenVideoMediaPlayerId()
                   .has_value());

  auto player_host = SetupPlayerHost();
  constexpr int32_t kVideoPlayerId = 0;
  auto video_player = CreateAndAddPlayer(player_host, kVideoPlayerId);

  SetFullscreenStatus(
      video_player.observer,
      blink::WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled);

  EXPECT_TRUE(media_web_contents_observer()
                  .GetFullscreenVideoMediaPlayerId()
                  .has_value());
  EXPECT_EQ(media_web_contents_observer()
                .GetFullscreenVideoMediaPlayerId()
                ->player_id,
            kVideoPlayerId);

  // Send persistent state change.
  media_web_contents_observer().RequestPersistentVideo(false);

  EXPECT_TRUE(media_web_contents_observer()
                  .GetFullscreenVideoMediaPlayerId()
                  .has_value());
  EXPECT_EQ(media_web_contents_observer()
                .GetFullscreenVideoMediaPlayerId()
                ->player_id,
            kVideoPlayerId);

  SetFullscreenStatus(
      video_player.observer,
      blink::WebFullscreenVideoStatus::kNotEffectivelyFullscreen);

  EXPECT_FALSE(media_web_contents_observer()
                   .GetFullscreenVideoMediaPlayerId()
                   .has_value());

  // Early return again when no fullscreen player exists.
  media_web_contents_observer().RequestPersistentVideo(true);
  EXPECT_FALSE(media_web_contents_observer()
                   .GetFullscreenVideoMediaPlayerId()
                   .has_value());
}

TEST_F(MediaWebContentsObserverTest, PlayerStartStopNotifications) {
  auto player_host = SetupPlayerHost();
  constexpr int32_t kTestPlayerId = 123;
  auto test_player = CreateAndAddPlayer(player_host, kTestPlayerId);
  auto player_id = CreatePlayerId(test_player.player_id);

  // Audio player setup to trigger audio stream monitor registration.
  SetMediaMetadata(test_player.observer, /*has_audio=*/true,
                   /*has_video=*/false);

  PlayMedia(test_player.observer);

  EXPECT_TRUE(media_web_contents_observer().IsPlayerActive(player_id));

  PauseMedia(test_player.observer);
  EXPECT_FALSE(media_web_contents_observer().IsPlayerActive(player_id));
}

TEST_F(MediaWebContentsObserverTest, PlayerPausedWithStreamEnded) {
  auto player_host = SetupPlayerHost();
  constexpr int32_t kTestPlayerId = 456;
  auto test_player = CreateAndAddPlayer(player_host, kTestPlayerId);
  auto player_id = CreatePlayerId(test_player.player_id);

  SetMediaMetadata(test_player.observer, /*has_audio=*/true,
                   /*has_video=*/false);

  PlayMedia(test_player.observer);
  EXPECT_TRUE(media_web_contents_observer().IsPlayerActive(player_id));

  // Test pause with stream ended.
  PauseMedia(test_player.observer, /*stream_ended=*/true);
  EXPECT_FALSE(media_web_contents_observer().IsPlayerActive(player_id));
}

}  // namespace
}  // namespace content
