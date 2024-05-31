// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_FORMAT_UTILS_H_
#define MEDIA_GPU_WINDOWS_FORMAT_UTILS_H_

#include <dxgi.h>

#include "media/gpu/media_gpu_export.h"

namespace media {

// Get the number of planes that a D3D12Resource of |format| has.
MEDIA_GPU_EXPORT size_t GetFormatPlaneCount(DXGI_FORMAT format);

MEDIA_GPU_EXPORT const char* DxgiFormatToString(DXGI_FORMAT format);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_FORMAT_UTILS_H_
