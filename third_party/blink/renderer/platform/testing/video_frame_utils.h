// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIDEO_FRAME_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIDEO_FRAME_UTILS_H_

#include "media/base/video_frame.h"
#include "ui/gfx/color_space.h"

namespace gpu {
class TestSharedImageInterface;
}  // namespace gpu

namespace blink {

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type,
    gpu::TestSharedImageInterface* test_sii);

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type,
    media::VideoPixelFormat pixel_format,
    base::TimeDelta timestamp,
    gpu::TestSharedImageInterface* test_sii,
    const gfx::ColorSpace& color_space = gfx::ColorSpace::CreateSRGB());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_VIDEO_FRAME_UTILS_H_
