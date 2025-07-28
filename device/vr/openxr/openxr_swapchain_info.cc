

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_swapchain_info.h"

#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

#if BUILDFLAG(IS_WIN)
OpenXrSwapchainInfo::OpenXrSwapchainInfo(ID3D11Texture2D* d3d11_texture)
    : d3d11_texture(d3d11_texture) {}
#elif BUILDFLAG(IS_ANDROID)
OpenXrSwapchainInfo::OpenXrSwapchainInfo(uint32_t texture)
    : openxr_texture(texture) {}
#endif

OpenXrSwapchainInfo::~OpenXrSwapchainInfo() {
  // If shared images are being used, the mailbox holder should have been
  // cleared before destruction, either due to the context provider being lost
  // or from normal session ending. If shared images are not being used, these
  // should not have been initialized in the first place.
  DCHECK(!shared_image);
  DCHECK(!sync_token.HasData());
}
OpenXrSwapchainInfo::OpenXrSwapchainInfo(OpenXrSwapchainInfo&&) = default;
OpenXrSwapchainInfo& OpenXrSwapchainInfo::operator=(OpenXrSwapchainInfo&&) =
    default;

void OpenXrSwapchainInfo::Clear() {
  shared_image.reset();
  sync_token.Clear();
#if BUILDFLAG(IS_ANDROID)
  // Resetting the SharedBufferSize ensures that we will re-create the Shared
  // Buffer if it is needed.
  shared_buffer_size = {0, 0};
#endif
}

}  // namespace device
