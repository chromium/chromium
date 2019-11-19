// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_STREAM_REQUEST_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_STREAM_REQUEST_H_

#include <memory>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {

// Represents a request for media streams (audio/video).
// TODO(vrk,justinlin,wjia): Figure out a way to share this code cleanly between
// vanilla WebRTC, Tab Capture, and Pepper Video Capture. Right now there is
// Tab-only stuff and Pepper-only stuff being passed around to all clients,
// which is icky.
struct CONTENT_EXPORT MediaStreamRequest {
  MediaStreamRequest(int render_process_id,
                     int render_frame_id,
                     int page_request_id,
                     const GURL& security_origin,
                     bool user_gesture,
                     blink::MediaStreamRequestType request_type,
                     const std::string& requested_audio_device_id,
                     const std::string& requested_video_device_id,
                     blink::mojom::MediaStreamType audio_type,
                     blink::mojom::MediaStreamType video_type,
                     bool disable_local_echo);

  MediaStreamRequest(const MediaStreamRequest& other);

  ~MediaStreamRequest();

  // This is the render process id for the renderer associated with generating
  // frames for a MediaStream. Any indicators associated with a capture will be
  // displayed for this renderer.
  int render_process_id;

  // This is the render frame id for the renderer associated with generating
  // frames for a MediaStream. Any indicators associated with a capture will be
  // displayed for this renderer.
  int render_frame_id;

  // The unique id combined with render_process_id and render_frame_id for
  // identifying this request. This is used for cancelling request.
  int page_request_id;

  // The WebKit security origin for the current request (e.g. "html5rocks.com").
  GURL security_origin;

  // Set to true if the call was made in the context of a user gesture.
  bool user_gesture;

  // Stores the type of request that was made to the media controller. Right now
  // this is only used to distinguish between WebRTC and Pepper requests, as the
  // latter should not be subject to user approval but only to policy check.
  // Pepper requests are signified by the |MEDIA_OPEN_DEVICE| value.
  blink::MediaStreamRequestType request_type;

  // Stores the requested raw device id for physical audio or video devices.
  std::string requested_audio_device_id;
  std::string requested_video_device_id;

  // Flag to indicate if the request contains audio.
  blink::mojom::MediaStreamType audio_type;

  // Flag to indicate if the request contains video.
  blink::mojom::MediaStreamType video_type;

  // Flag for desktop or tab share to indicate whether to prevent the captured
  // audio being played out locally.
  bool disable_local_echo;

  // True if all ancestors of the requesting frame have the same origin.
  bool all_ancestors_have_same_origin;
};

// Interface used by the content layer to notify chrome about changes in the
// state of a media stream. Instances of this class are passed to content layer
// when MediaStream access is approved using MediaResponseCallback.
class MediaStreamUI {
 public:
  using SourceCallback =
      base::RepeatingCallback<void(const DesktopMediaID& media_id)>;

  virtual ~MediaStreamUI() {}

  // Called when MediaStream capturing is started. Chrome layer can call |stop|
  // to stop the stream, or |source| to change the source of the stream.
  // Returns the platform-dependent window ID for the UI, or 0 if not
  // applicable.
  virtual gfx::NativeViewId OnStarted(base::OnceClosure stop,
                                      SourceCallback source) = 0;
};

// Callback used return results of media access requests.
using MediaResponseCallback =
    base::OnceCallback<void(const blink::MediaStreamDevices& devices,
                            blink::mojom::MediaStreamRequestResult result,
                            std::unique_ptr<MediaStreamUI> ui)>;
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_STREAM_REQUEST_H_
