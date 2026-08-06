// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_
#define MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_

#include <CoreVideo/CVPixelBuffer.h>
#include <IOSurface/IOSurfaceRef.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"

namespace media {

class VideoFrame;

// Wrap a VideoFrame's data in a CVPixelBuffer object. The frame's lifetime is
// extended for the duration of the pixel buffer's lifetime.
//
// The only supported formats are I420, NV12, NV12A, and P010LE. A visible rect
// smaller than the coded size is represented with a clean-aperture attachment.
// If an unsupported frame is specified, null is returned.
MEDIA_EXPORT base::apple::ScopedCFTypeRef<CVPixelBufferRef>
WrapVideoFrameInCVPixelBuffer(scoped_refptr<VideoFrame> frame);

// Wraps IOSurface in a CVPixelBuffer, validates its format, and applies
// attachments from frame. Returns null if the IOSurface cannot be wrapped or
// its pixel format does not match frame.
MEDIA_EXPORT base::apple::ScopedCFTypeRef<CVPixelBufferRef>
WrapIOSurfaceInCVPixelBuffer(const VideoFrame& frame, IOSurfaceRef io_surface);

}  // namespace media

#endif  // MEDIA_BASE_MAC_VIDEO_FRAME_MAC_H_
