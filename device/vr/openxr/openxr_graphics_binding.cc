// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_graphics_binding.h"

namespace device {

#if BUILDFLAG(IS_WIN)
SwapChainInfo::SwapChainInfo(ID3D11Texture2D* d3d11_texture)
    : d3d11_texture(d3d11_texture) {}
#elif BUILDFLAG(IS_ANDROID)
SwapChainInfo::SwapChainInfo(uint32_t texture) : texture(texture) {}
#endif

SwapChainInfo::~SwapChainInfo() {
  // If shared images are being used, the mailbox holder should have been
  // cleared before destruction, either due to the context provider being lost
  // or from normal session ending. If shared images are not being used, these
  // should not have been initialized in the first place.
  DCHECK(mailbox_holder.mailbox.IsZero());
  DCHECK(!mailbox_holder.sync_token.HasData());
}
SwapChainInfo::SwapChainInfo(SwapChainInfo&&) = default;
SwapChainInfo& SwapChainInfo::operator=(SwapChainInfo&&) = default;

void SwapChainInfo::Clear() {
  mailbox_holder.mailbox.SetZero();
  mailbox_holder.sync_token.Clear();
}

}  // namespace device
