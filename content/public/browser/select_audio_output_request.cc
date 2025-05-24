// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/select_audio_output_request.h"

#include <utility>

namespace content {

SelectAudioOutputRequest::SelectAudioOutputRequest(
    GlobalRenderFrameHostId render_frame_host_id,
    const std::vector<AudioOutputDeviceInfo>& audio_output_devices)
    : render_frame_host_id_(render_frame_host_id),
      audio_output_devices_(audio_output_devices) {}

SelectAudioOutputRequest::~SelectAudioOutputRequest() = default;

}  // namespace content
