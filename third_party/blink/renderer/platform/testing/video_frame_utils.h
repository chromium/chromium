// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIDEO_FRAME_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIDEO_FRAME_UTILS_H_

#include "media/base/video_frame.h"

namespace blink {

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type);

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type,
    media::VideoPixelFormat pixel_format,
    base::TimeDelta timestamp,
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb = nullptr);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIDEO_FRAME_UTILS_H_
