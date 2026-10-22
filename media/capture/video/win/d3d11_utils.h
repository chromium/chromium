// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_D3D11_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_WIN_D3D11_UTILS_H_

#include <d3d11.h>

#include "base/win/scoped_handle.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/capture/capture_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Creates an NV12 D3D11 texture on |d3d11_device| with the given |size|,
// shareable across processes via an NT handle. Returns a shared handle to the
// texture, or an invalid handle on failure. The handle keeps the texture alive
// even after the local texture reference is released.
CAPTURE_EXPORT base::win::ScopedHandle CreateNV12Texture(
    ID3D11Device* d3d11_device,
    const gfx::Size& size,
    const char* debug_name);

// Returns true if |dxgi_device_manager|'s current device is no longer
// |expected_device|, which means buffers created against |expected_device| can
// no longer be used. Recreates the manager's device if it was removed, and
// logs plus records a metric when device loss is detected. Note that it does
// not recreate any resources built on the old device - that is the caller's
// job, typically by dropping them.
CAPTURE_EXPORT bool HasD3DDeviceChanged(DXGIDeviceManager* dxgi_device_manager,
                                        ID3D11Device* expected_device);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_D3D11_UTILS_H_
