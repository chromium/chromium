// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_stream_request.h"

namespace content {

MediaStreamRequest::MediaStreamRequest(
    int render_process_id,
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
    bool captured_surface_control_active)
    : render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      page_request_id(page_request_id),
      security_origin(url_origin.GetURL()),
      url_origin(url_origin),
      user_gesture(user_gesture),
      request_type(request_type),
      requested_audio_device_ids(requested_audio_device_ids),
      requested_video_device_ids(requested_video_device_ids),
      audio_type(audio_type),
      video_type(video_type),
      disable_local_echo(disable_local_echo),
      request_pan_tilt_zoom_permission(request_pan_tilt_zoom_permission),
      captured_surface_control_active(captured_surface_control_active) {}

MediaStreamRequest::MediaStreamRequest(const MediaStreamRequest& other) =
    default;

MediaStreamRequest::~MediaStreamRequest() = default;

}  // namespace content
