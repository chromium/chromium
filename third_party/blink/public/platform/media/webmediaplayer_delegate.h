// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_player.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace media {
enum class MediaContentType;
}  // namespace media

namespace media_session {
struct MediaPosition;
}  // namespace media_session

namespace blink {

enum class WebFullscreenVideoStatus;
class WebMediaPlayer;

// An interface to collect WebMediaPlayer state changes and to fan out commands
// from the browser.
class BLINK_PLATFORM_EXPORT WebMediaPlayerDelegate {
 public:
  // Note: WebMediaPlayerDelegate implementations should not call an Observer
  // method on a stack that includes a call from the player.
  // Note: It is likely that players will call WebMediaPlayerDelegate methods
  // from within Observer callbacks.
  class Observer {
   public:
    // Called when the host frame is hidden (usually by tab switching).
    // Note: OnFrameHidden() is not called when the frame is closed, even though
    // IsFrameHidden() will start returning true.
    virtual void OnFrameHidden() = 0;

    // Called when the host frame is closed.
    // Note: It is possible for a closed frame to be shown again. (Android only;
    // other platforms tear down players when the host frame is closed.) There
    // is no callback for frame opening, observers are expected to wait until
    // OnFrameShown().
    // TODO(sandersd): Experiment to verify exactly what gets called when
    // restoring a closed tab on Android.
    virtual void OnFrameClosed() = 0;

    // Called when the host frame is shown (usually by tab switching).
    virtual void OnFrameShown() = 0;

    // Called when an idle player has become stale, usually interpreted to mean
    // that it is unlikely to be interacted with in the near future.
    //
    // Players should typically respond by releasing resources, for example by
    // discarding their decoders.
    virtual void OnIdleTimeout() = 0;

    // Called when external controls are activated.
    virtual void OnPlay() = 0;
    virtual void OnPause() = 0;
    virtual void OnMuted(bool muted) = 0;
    virtual void OnSeekForward(double seconds) = 0;
    virtual void OnSeekBackward(double seconds) = 0;
    virtual void OnEnterPictureInPicture() = 0;
    virtual void OnExitPictureInPicture() = 0;
    virtual void OnSetAudioSink(const std::string& sink_id) = 0;

    // Called to control audio ducking. Output volume should be set to
    // |player_volume| * |multiplier|. The range of |multiplier| is [0, 1],
    // where 1 indicates normal (non-ducked) playback.
    virtual void OnVolumeMultiplierUpdate(double multiplier) = 0;

    // Called to set as the persistent video. A persistent video should hide its
    // controls and go fullscreen.
    virtual void OnBecamePersistentVideo(bool value) = 0;

    // Notify the player that it is now eligible to start recording power
    // measurements if |state| is true, else it is no longer eligible.
    virtual void OnPowerExperimentState(bool state) {}
  };

  // Returns true if the host frame is hidden or closed.
  virtual bool IsFrameHidden() = 0;

  // Returns true if the host frame is closed.
  virtual bool IsFrameClosed() = 0;

  // Subscribe to observer callbacks. A player must use the returned |player_id|
  // for the rest of the calls below.
  virtual int AddObserver(Observer* observer) = 0;

  // Unsubscribe from observer callbacks.
  virtual void RemoveObserver(int player_id) = 0;

  // Notify about the kind of tracks the media player has, and the type of
  // content.
  virtual void DidMediaMetadataChange(
      int player_id,
      bool has_audio,
      bool has_video,
      media::MediaContentType media_content_type) = 0;

  // Notify playback started. This will request appropriate wake locks and, if
  // applicable, show a pause button in external controls.
  //
  // DidPlay() should not be called for remote playback.
  virtual void DidPlay(int player_id) = 0;

  // Notify that playback is paused. This will drop wake locks and, if
  // applicable, show a play button in external controls.
  // TODO(sandersd): It may be helpful to get |has_audio| and |has_video| here,
  // so that we can do the right thing with media that starts paused.
  virtual void DidPause(int player_id, bool reached_end_of_stream) = 0;

  // Notify that the size of the media player is changed.
  virtual void DidPlayerSizeChange(int delegate_id, const gfx::Size& size) = 0;

  // Notify that the muted status of the media player has changed.
  virtual void DidPlayerMutedStatusChange(int delegate_id, bool muted) = 0;

  // Notify that the media position state of the media player has changed.
  virtual void DidPlayerMediaPositionStateChange(
      int delegate_id,
      const media_session::MediaPosition& position) = 0;

  // Notify that picture-in-picture availability has changed.
  virtual void DidPictureInPictureAvailabilityChange(int delegate_id,
                                                     bool available) = 0;

  // Notify that the audio output sink has changed
  virtual void DidAudioOutputSinkChange(
      int delegate_id,
      const std::string& hashed_device_id) = 0;

  // Notify that the audio sink cannot be changed
  virtual void DidDisableAudioOutputSinkChanges(int delegate_id) = 0;

  // Notify that a buffer underflow event happened for the media player.
  virtual void DidBufferUnderflow(int player_id) = 0;

  // Notify that playback is stopped. This will drop wake locks and remove any
  // external controls.
  //
  // Clients must still call RemoveObserver() to unsubscribe from observer
  // callbacks.
  virtual void PlayerGone(int player_id) = 0;

  // Set the player's idle state. While idle, a player may recieve an
  // OnIdleTimeout() callback.
  // TODO(sandersd): Merge this into DidPlay()/DidPause()/PlayerGone().
  virtual void SetIdle(int player_id, bool is_idle) = 0;

  // Get the player's idle state. A stale player is considered idle.
  // TODO(sandersd): Remove this. It is only used in tests and in one special
  // case in WMPI.
  virtual bool IsIdle(int player_id) = 0;

  // Returns a stale player to an idle state, and resumes OnIdleTimeout() calls
  // without an additional idle timeout.
  // TODO(sandersd): This exists only to support WMPI's didLoadingProgress()
  // workaround. A better option may be to take a 'minimum idle' duration in
  // SetIdle().
  virtual void ClearStaleFlag(int player_id) = 0;

  // Returns |true| if the player is stale; that is that OnIdleTimeout() was
  // called and returned |true|.
  virtual bool IsStale(int player_id) = 0;

  // Notifies the delegate that the player has entered fullscreen. This does not
  // differentiate native controls fullscreen and custom controls fullscreen.
  // |fullscreen_video_status| is used by MediaWebContentsObserver to
  // trigger automatically Picture-in-Picture for fullscreen videos.
  virtual void SetIsEffectivelyFullscreen(
      int player_id,
      blink::WebFullscreenVideoStatus fullscreen_video_status) = 0;

 protected:
  WebMediaPlayerDelegate() = default;
  virtual ~WebMediaPlayerDelegate() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_
