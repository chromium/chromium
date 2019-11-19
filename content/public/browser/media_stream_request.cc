// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_stream_request.h"

namespace content {

MediaStreamRequest::MediaStreamRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    bool user_gesture,
    blink::MediaStreamRequestType request_type,
    const std::string& requested_audio_device_id,
    const std::string& requested_video_device_id,
    blink::mojom::MediaStreamType audio_type,
    blink::mojom::MediaStreamType video_type,
    bool disable_local_echo)
    : render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      page_request_id(page_request_id),
      security_origin(security_origin),
      user_gesture(user_gesture),
      request_type(request_type),
      requested_audio_device_id(requested_audio_device_id),
      requested_video_device_id(requested_video_device_id),
      audio_type(audio_type),
      video_type(video_type),
      disable_local_echo(disable_local_echo),
      all_ancestors_have_same_origin(false) {}

MediaStreamRequest::MediaStreamRequest(const MediaStreamRequest& other) =
    default;

MediaStreamRequest::~MediaStreamRequest() {}

}  // namespace content
