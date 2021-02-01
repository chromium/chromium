// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"

#include <memory>

namespace content {

ReadyBuffer::ReadyBuffer(int buffer_id,
                         media::mojom::VideoFrameInfoPtr frame_info)
    : buffer_id(buffer_id), frame_info(std::move(frame_info)) {}

ReadyBuffer::~ReadyBuffer() = default;

}  // namespace content
