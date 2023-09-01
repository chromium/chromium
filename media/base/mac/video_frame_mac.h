// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_
#define MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_

#include <CoreVideo/CVPixelBuffer.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"
#include "ui/gfx/mac/io_surface.h"

namespace media {

class VideoFrame;

// Wrap a VideoFrame's data in a CVPixelBuffer object. The frame's lifetime is
// extended for the duration of the pixel buffer's lifetime. If the frame's data
// is already managed by a CVPixelBuffer (the frame was created using
// |WrapCVPixelBuffer()|, then the underlying CVPixelBuffer is returned.
//
// The only supported formats are I420 and NV12. Frames with extended pixels
// (the visible rect's size does not match the coded size) are not supported.
// If an unsupported frame is specified, null is returned.
MEDIA_EXPORT base::apple::ScopedCFTypeRef<CVPixelBufferRef>
WrapVideoFrameInCVPixelBuffer(scoped_refptr<VideoFrame> frame);

// Return true if IOSurface Pixel Format is supported by WebGPU and
// can be imported in WebGPU.
MEDIA_EXPORT bool IOSurfaceIsWebGPUCompatible(IOSurfaceRef io_surface);

}  // namespace media

#endif  // MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_
