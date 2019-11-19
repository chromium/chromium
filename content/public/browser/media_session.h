// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {

class BrowserContext;
class WebContents;

// MediaSession manages the media session and audio focus for a given
// WebContents. There is only one MediaSession per WebContents.
//
// MediaSession allows clients to observe its changes via MediaSessionObserver,
// and allows clients to resume/suspend/stop the managed players.
class MediaSession : public media_session::mojom::MediaSession {
 public:
  ~MediaSession() override = default;

  // Returns the MediaSession associated to this WebContents. Creates one if
  // none is currently available.
  CONTENT_EXPORT static MediaSession* Get(WebContents* contents);

  // Returns the source identity for the given BrowserContext.
  CONTENT_EXPORT static const base::UnguessableToken& GetSourceId(
      BrowserContext* browser_context);

  CONTENT_EXPORT static WebContents* GetWebContentsFromRequestId(
      const base::UnguessableToken& request_id);

  // Tell the media session a user action has performed.
  virtual void DidReceiveAction(
      media_session::mojom::MediaSessionAction action) = 0;

  // Set the volume multiplier applied during ducking.
  virtual void SetDuckingVolumeMultiplier(double multiplier) = 0;

  // Set the audio focus group id for this media session. Sessions in the same
  // group can share audio focus. Setting this to null will use the browser
  // default value. This will only have any effect if audio focus grouping is
  // supported.
  virtual void SetAudioFocusGroupId(const base::UnguessableToken& group_id) = 0;

  // media_session.mojom.MediaSession overrides -------------------------------

  // Suspend the media session.
  // |type| represents the origin of the request.
  void Suspend(SuspendType suspend_type) override = 0;

  // Resume the media session.
  // |type| represents the origin of the request.
  void Resume(SuspendType suspend_type) override = 0;

  // Let the media session start ducking such that the volume multiplier is
  // reduced.
  void StartDucking() override = 0;

  // Let the media session stop ducking such that the volume multiplier is
  // recovered.
  void StopDucking() override = 0;

  // Returns information about the MediaSession.
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override = 0;

  // Returns debug information about the MediaSession.
  void GetDebugInfo(GetDebugInfoCallback callback) override = 0;

  // Adds an observer to listen to events related to this MediaSession.
  void AddObserver(
      mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer)
      override = 0;

  // Skip to the previous track. If there is no previous track then this will be
  // a no-op.
  void PreviousTrack() override = 0;

  // Skip to the next track. If there is no next track then this will be a
  // no-op.
  void NextTrack() override = 0;

  // Skip ad.
  void SkipAd() override = 0;

  // Seek the media session from the current position. If the media cannot
  // seek then this will be a no-op. The |seek_time| is the time delta that
  // the media will seek by and supports both positive and negative values.
  // This value cannot be zero. The |kDefaultSeekTimeSeconds| provides a
  // default value for seeking by a few seconds.
  void Seek(base::TimeDelta seek_time) override = 0;

  // Stop the media session.
  // |type| represents the origin of the request.
  void Stop(SuspendType suspend_type) override = 0;

  // Downloads the bitmap version of a MediaImage at least |minimum_size_px|
  // and closest to |desired_size_px|. If the download failed, was too small or
  // the image did not come from the media session then returns a null image.
  void GetMediaImageBitmap(const media_session::MediaImage& image,
                           int minimum_size_px,
                           int desired_size_px,
                           GetMediaImageBitmapCallback callback) override = 0;

  // Seek the media session to a non-negative |seek_time| from the beginning of
  // the current playing media. If the media cannot seek then this will be a
  // no-op.
  void SeekTo(base::TimeDelta seek_time) override = 0;

  // Scrub ("fast seek") the media session to a non-negative |seek_time| from
  // the beginning of the current playing media. If the media cannot scrub then
  // this will be a no-op. The client should call |SeekTo| to finish the
  // scrubbing operation.
  void ScrubTo(base::TimeDelta seek_time) override = 0;

 protected:
  MediaSession() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_H_
