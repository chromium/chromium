// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_
#define MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_

#include <d3d11_1.h>
#include <wrl/client.h>
#include <memory>
#include <utility>
#include <vector>

#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "ui/gfx/geometry/rect.h"


namespace media {

using ResolutionPair = std::pair<gfx::Size, gfx::Size>;

bool IsLegacyGPU(ID3D11Device* device);

// Returns true if a ID3D11VideoDecoder can be created for |resolution_to_test|
// on the given |video_device|.
bool IsResolutionSupportedForDevice(const gfx::Size& resolution_to_test,
                                    const GUID& decoder_guid,
                                    ID3D11VideoDevice* video_device,
                                    DXGI_FORMAT format);

ResolutionPair GetMaxResolutionsForGUIDs(
    const gfx::Size& default_max,
    ID3D11VideoDevice* video_device,
    const std::vector<GUID>& valid_guids,
    const std::vector<gfx::Size>& resolutions_to_test,
    DXGI_FORMAT format = DXGI_FORMAT_NV12);

MEDIA_GPU_EXPORT
void GetResolutionsForDecoders(std::vector<GUID> h264_guids,
                               ComD3D11Device device,
                               const gpu::GpuDriverBugWorkarounds& workarounds,
                               ResolutionPair* h264_resolutions,
                               ResolutionPair* vp9_0_resolutions,
                               ResolutionPair* vp9_2_resolutions);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_
