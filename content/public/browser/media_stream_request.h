// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_STREAM_REQUEST_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_STREAM_REQUEST_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"
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
                     const url::Origin& url_origin,
                     bool user_gesture,
                     blink::MediaStreamRequestType request_type,
                     const std::vector<std::string>& requested_audio_device_ids,
                     const std::vector<std::string>& requested_video_device_ids,
                     blink::mojom::MediaStreamType audio_type,
                     blink::mojom::MediaStreamType video_type,
                     bool disable_local_echo,
                     bool request_pan_tilt_zoom_permission,
                     bool captured_surface_control_active);

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

  // TODO(crbug.com/40944449): Remove security_origin.
  // The WebKit security origin for the current request (e.g. "html5rocks.com").
  GURL security_origin;

  // The Origin of the current request.
  url::Origin url_origin;

  // Set to true if the call was made in the context of a user gesture.
  bool user_gesture;

  // Stores the type of request that was made to the media controller. Right now
  // this is only used to distinguish between WebRTC and Pepper requests, as the
  // latter should not be subject to user approval but only to policy check.
  // Pepper requests are signified by the |MEDIA_OPEN_DEVICE| value.
  blink::MediaStreamRequestType request_type;

  // Stores the requested raw device id for physical audio or video devices.
  std::vector<std::string> requested_audio_device_ids;
  std::vector<std::string> requested_video_device_ids;

  // Flag to indicate if the request contains audio.
  blink::mojom::MediaStreamType audio_type;

  // Flag to indicate if the request contains video.
  blink::mojom::MediaStreamType video_type;

  // Flag for desktop or tab share to indicate whether to prevent the captured
  // audio being played out locally.
  bool disable_local_echo;

  // Flag for desktop or tab share to indicate whether to prevent the captured
  // audio being played out locally.
  // This flag is distinct from |disable_local_echo|, because the former
  // hooks into an old non-standard constraint that should be deprecated,
  // whereas this flag hooks into a standardized option.
  bool suppress_local_audio_playback = false;

  // If audio is requested, |exclude_system_audio| can indicate that
  // system-audio should nevertheless not be offered to the user.
  bool exclude_system_audio = false;

  // Flag to indicate that the current tab should be excluded from the list of
  // tabs offered to the user.
  bool exclude_self_browser_surface = false;

  // Flag to indicate whether monitor type surfaces (screens) should be offered
  // to the user.
  bool exclude_monitor_type_surfaces = false;

  // Flag to indicate a preference for which display surface type (screen,
  // windows, or tabs) should be most prominently offered to the user.
  blink::mojom::PreferredDisplaySurface preferred_display_surface =
      blink::mojom::PreferredDisplaySurface::NO_PREFERENCE;

  // Flag to indicate whether the request is for PTZ use.
  bool request_pan_tilt_zoom_permission;

  // Indicates whether Captured Surface Control APIs (sendWheel, setZoomLevel)
  // have previously been used on the capture-session associated with this
  // request. This is only relevant for tab-sharing sessions.
  bool captured_surface_control_active;
};

// Interface used by the content layer to notify chrome about changes in the
// state of a media stream. Instances of this class are passed to content layer
// when MediaStream access is approved using MediaResponseCallback.
class MediaStreamUI {
 public:
  using SourceCallback =
      base::RepeatingCallback<void(const DesktopMediaID& media_id,
                                   bool captured_surface_control_active)>;
  using StateChangeCallback = base::RepeatingCallback<void(
      const DesktopMediaID& media_id,
      blink::mojom::MediaStreamStateChange new_state)>;

  virtual ~MediaStreamUI() = default;

  // Called when MediaStream capturing is started. Chrome layer can call |stop|
  // to stop the stream, or |source| to change the source of the stream, or
  // |state_change| to pause/unpause the stream.
  // |stop| is a callback that, once invoked, will stop the stream.
  // Stopping a stream is irreversible, so only the first invocation
  // will have an effect. |stop| is defined as RepeatingClosure so as
  // to allow its duplication upstream, thereby enabling multiple
  // potential sources for the stop invocation. (For example, allow
  // multiple UX elements that would stop the capture.)
  // Returns the platform-dependent window ID for the UI, or 0
  // if not applicable.
  virtual gfx::NativeViewId OnStarted(
      base::RepeatingClosure stop,
      SourceCallback source,
      const std::string& label,
      std::vector<DesktopMediaID> screen_capture_ids,
      StateChangeCallback state_change) = 0;

  // Called when the device is stopped because desktop capture identified by
  // |label| source is about to be changed from |old_media_id| to
  // |new_media_id|. Note that the switch is not necessarily completed.
  virtual void OnDeviceStoppedForSourceChange(
      const std::string& label,
      const DesktopMediaID& old_media_id,
      const DesktopMediaID& new_media_id,
      bool captured_surface_control_active) = 0;

  virtual void OnDeviceStopped(const std::string& label,
                               const DesktopMediaID& media_id) = 0;

  // Called when Region Capture starts/stops, or when the cropped area changes.
  // * A non-empty rect indicates the region which was cropped-to.
  // * An empty rect indicates that Region Capture was employed,
  //   but the cropped-to region ended up having zero pixels.
  // * Nullopt indicates that cropping stopped.
  virtual void OnRegionCaptureRectChanged(
      const std::optional<gfx::Rect>& region_capture_rect) {}

#if !BUILDFLAG(IS_ANDROID)
  // Focuses the display surface represented by |media_id|.
  //
  // |is_from_microtask| and |is_from_timer| are used to distinguish:
  // a. Explicit calls from the Web-application.
  // b. Implicit calls resulting from the focusability-window-closing microtask.
  // c. The browser-side timer.
  // This distinction is reflected by UMA.
  virtual void SetFocus(const DesktopMediaID& media_id,
                        bool focus,
                        bool is_from_microtask,
                        bool is_from_timer) {}
#endif
};

// Callback used return results of media access requests.
using MediaResponseCallback = base::OnceCallback<void(
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<MediaStreamUI> ui)>;
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_STREAM_REQUEST_H_
