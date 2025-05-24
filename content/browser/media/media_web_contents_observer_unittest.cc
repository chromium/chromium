// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

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

using MediaWebContentsObserverTest = RenderViewHostImplTestHarness;

TEST_F(MediaWebContentsObserverTest, GetCurrentlyPlayingVideoCount) {
  MediaWebContentsObserver& media_web_contents_observer =
      *contents()->media_web_contents_observer();

  mojo::AssociatedRemote<media::mojom::MediaPlayerHost> player_host;
  media_web_contents_observer.BindMediaPlayerHost(
      contents()->GetPrimaryMainFrame()->GetGlobalId(),
      player_host.BindNewEndpointAndPassDedicatedReceiver());

  TestMediaPlayer audio_video_player;
  mojo::AssociatedRemote<media::mojom::MediaPlayerObserver>
      audio_video_player_observer;
  player_host->OnMediaPlayerAdded(
      audio_video_player.receiver().BindNewEndpointAndPassRemote(),
      audio_video_player_observer.BindNewEndpointAndPassReceiver(),
      /*player_id=*/0);

  TestMediaPlayer video_player;
  mojo::AssociatedRemote<media::mojom::MediaPlayerObserver>
      video_player_observer;
  player_host->OnMediaPlayerAdded(
      video_player.receiver().BindNewEndpointAndPassRemote(),
      video_player_observer.BindNewEndpointAndPassReceiver(),
      /*player_id=*/1);

  player_host.FlushForTesting();

  EXPECT_EQ(media_web_contents_observer.GetCurrentlyPlayingVideoCount(), 0);

  audio_video_player_observer->OnMediaMetadataChanged(
      /*has_audio=*/true, /*has_video=*/false, kContentType);
  audio_video_player_observer.FlushForTesting();
  EXPECT_EQ(media_web_contents_observer.GetCurrentlyPlayingVideoCount(), 0)
      << "Nothing is playing";

  audio_video_player_observer->OnMediaPlaying();
  audio_video_player_observer.FlushForTesting();
  EXPECT_EQ(media_web_contents_observer.GetCurrentlyPlayingVideoCount(), 0)
      << "An audio-only player is playing";

  video_player_observer->OnMediaMetadataChanged(
      /*has_audio=*/false, /*has_video=*/true, kContentType);
  video_player_observer.FlushForTesting();
  EXPECT_EQ(media_web_contents_observer.GetCurrentlyPlayingVideoCount(), 0)
      << "An audio-only player is playing";

  video_player_observer->OnMediaPlaying();
  video_player_observer.FlushForTesting();
  EXPECT_EQ(media_web_contents_observer.GetCurrentlyPlayingVideoCount(), 1);

  audio_video_player_observer->OnMediaMetadataChanged(
      /*has_audio=*/true, /*has_video=*/true, kContentType);
  audio_video_player_observer.FlushForTesting();
  EXPECT_EQ(media_web_contents_observer.GetCurrentlyPlayingVideoCount(), 2)
      << "A video track was added to an initially audio-only player";

  video_player_observer->OnMediaPaused(/*stream_ended=*/false);
  video_player_observer.FlushForTesting();
  EXPECT_EQ(media_web_contents_observer.GetCurrentlyPlayingVideoCount(), 1);

  audio_video_player_observer->OnMediaMetadataChanged(
      /*has_audio=*/true, /*has_video=*/false, kContentType);
  audio_video_player_observer.FlushForTesting();
  EXPECT_EQ(media_web_contents_observer.GetCurrentlyPlayingVideoCount(), 0)
      << "The video track was removed again";
}

}  // namespace
}  // namespace content
