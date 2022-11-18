// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_OBSERVER_H_
#define MEDIA_BASE_MEDIA_OBSERVER_H_

#include "media/base/pipeline_metadata.h"
#include "url/gurl.h"

namespace media {

class MEDIA_EXPORT MediaObserverClient {
 public:
  // Reasons to switch to local renderer from using remote renderer.
  enum class ReasonToSwitchToLocal {
    NORMAL,  // Remoting is disabled or media no longer occupies the viewport.
    POOR_PLAYBACK_QUALITY,  // Playback quality is poor.
    PIPELINE_ERROR,         // Error occurred.
    ROUTE_TERMINATED,       // No longer show the media on remote screen.
  };

  virtual ~MediaObserverClient() {}

  // Requests to restart the media pipeline and create a new renderer as soon as
  // possible. When switching to remote renderer, all the optimizations that
  // might suspend the media pipeline should be disabled.
  // |remote_device_friendly_name| can be empty if the remote device is unknown.
  virtual void SwitchToRemoteRenderer(
      const std::string& remote_device_friendly_name) = 0;

  // Requests to switch to local renderer. According to |reason|, a text message
  // may be displayed to explain why the switch occurred.
  virtual void SwitchToLocalRenderer(ReasonToSwitchToLocal reason) = 0;

  // Reports the latest compatibility state of the element's source for remote
  // playback.
  virtual void UpdateRemotePlaybackCompatibility(bool is_compatible) = 0;

  // Gets the number of video frames decoded so far from the media pipeline.
  // All the counts keep increasing and will not be reset during seek.
  virtual unsigned DecodedFrameCount() const = 0;

  // Gets the media duration in seconds. Returns
  // |std::numeric_limits<double>::infinity()| for an infinite stream duration.
  // TODO(xjz): Use base::TimeDelta for media duration (crbug.com/773911).
  virtual double Duration() const = 0;
};

// This class is an observer of media player events.
class MEDIA_EXPORT MediaObserver {
 public:
  MediaObserver();
  virtual ~MediaObserver();

  // Called when the media element starts/stops being the dominant visible
  // content.
  virtual void OnBecameDominantVisibleContent(bool is_dominant) {}

  // Called after demuxer is initialized.
  virtual void OnMetadataChanged(const PipelineMetadata& metadata) = 0;

  // Called to indicate whether the site requests that remote playback be
  // disabled. The "disabled" naming corresponds with the
  // "disableRemotePlayback" media element attribute, as described in the
  // Remote Playback API spec: https://w3c.github.io/remote-playback
  virtual void OnRemotePlaybackDisabled(bool disabled) = 0;

  // Called when the browser requests to start Media Remoting when the video is
  // not the dominant visible content.
  virtual void OnMediaRemotingRequested() = 0;

  // Called on Android, whenever we detect that we are playing back HLS.
  virtual void OnHlsManifestDetected() = 0;

  // Called when the media is playing/paused.
  virtual void OnPlaying() = 0;
  virtual void OnPaused() = 0;

  // Called when the media is frozen.
  virtual void OnFrozen() = 0;

  // Called when the data source is asynchronously initialized.
  virtual void OnDataSourceInitialized(const GURL& url_after_redirects) = 0;

  // Set the MediaObserverClient. May be called with nullptr to disconnect the
  // the client from the observer.
  virtual void SetClient(MediaObserverClient* client) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_OBSERVER_H_
