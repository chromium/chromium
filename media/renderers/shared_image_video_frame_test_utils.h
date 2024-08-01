// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_SHARED_IMAGE_VIDEO_FRAME_TEST_UTILS_H_
#define MEDIA_RENDERERS_SHARED_IMAGE_VIDEO_FRAME_TEST_UTILS_H_

#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/functional/bind.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/video_frame.h"

namespace media {

// Creates a shared image backed frame in RGBA format, with colors on the shared
// image mapped as follow.
// Bk | R | G | Y
// ---+---+---+---
// Bl | M | C | W
scoped_refptr<VideoFrame> CreateSharedImageRGBAFrame(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback);

// Creates a shared image backed frame in I420 format, with colors mapped
// exactly like CreateSharedImageRGBAFrame above, noting that subsamples may get
// interpolated leading to inconsistent colors around the "seams".
scoped_refptr<VideoFrame> CreateSharedImageI420Frame(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback);

// Creates a shared image backed frame in NV12 format, with colors mapped
// exactly like CreateSharedImageRGBAFrame above.
// This will return nullptr if the necessary extension is not available for NV12
// support.
scoped_refptr<VideoFrame> CreateSharedImageNV12Frame(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback);

}  // namespace media

#endif  // MEDIA_RENDERERS_SHARED_IMAGE_VIDEO_FRAME_TEST_UTILS_H_
