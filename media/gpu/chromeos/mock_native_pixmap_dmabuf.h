// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_MOCK_NATIVE_PIXMAP_DMABUF_H_
#define MEDIA_GPU_CHROMEOS_MOCK_NATIVE_PIXMAP_DMABUF_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace media {

// Creates a NativePixmapDmaBuf with valid strides and other metadata, but with
// FD's that reference a /dev/null instead of referencing a DMABuf.
scoped_refptr<const gfx::NativePixmapDmaBuf> CreateMockNativePixmapDmaBuf(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    uint64_t modifier = 0);

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_MOCK_NATIVE_PIXMAP_DMABUF_H_
