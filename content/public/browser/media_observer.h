// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_

#include "content/public/browser/media_request_state.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

class GURL;

namespace content {

// An embedder may implement MediaObserver and return it from
// ContentBrowserClient to receive callbacks as media events occur.
class MediaObserver {
 public:
  // Called when a audio capture device is plugged in or unplugged.
  virtual void OnAudioCaptureDevicesChanged() = 0;

  // Called when a video capture device is plugged in or unplugged.
  virtual void OnVideoCaptureDevicesChanged() = 0;

  // Called when a media request changes state.
  virtual void OnMediaRequestStateChanged(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      const GURL& security_origin,
      blink::mojom::MediaStreamType stream_type,
      MediaRequestState state) = 0;

  // Called when an audio stream is being created.
  virtual void OnCreatingAudioStream(int render_process_id,
                                     int render_frame_id) = 0;

  // Called when the secure display link status of one or more consumers of this
  // media stream has changed.
  virtual void OnSetCapturingLinkSecured(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      blink::mojom::MediaStreamType stream_type,
      bool is_secure) = 0;

 protected:
  virtual ~MediaObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
