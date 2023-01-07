// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"

#include <memory>

namespace content {

ReadyBuffer::ReadyBuffer(int buffer_id,
                         media::mojom::VideoFrameInfoPtr frame_info)
    : buffer_id(buffer_id), frame_info(std::move(frame_info)) {}

ReadyBuffer::ReadyBuffer(ReadyBuffer&& other)
    : buffer_id(other.buffer_id), frame_info(std::move(other.frame_info)) {}

ReadyBuffer::~ReadyBuffer() = default;

ReadyBuffer& ReadyBuffer::operator=(ReadyBuffer&& other) {
  buffer_id = other.buffer_id;
  frame_info = std::move(other.frame_info);
  return *this;
}

}  // namespace content
