// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_graphics_binding.h"

#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
namespace device {

#if BUILDFLAG(IS_WIN)
SwapChainInfo::SwapChainInfo(ID3D11Texture2D* d3d11_texture)
    : d3d11_texture(d3d11_texture) {}
#elif BUILDFLAG(IS_ANDROID)
SwapChainInfo::SwapChainInfo(uint32_t texture) : openxr_texture(texture) {}
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

bool OpenXrGraphicsBinding::Render() {
  return true;
}

gfx::Size OpenXrGraphicsBinding::GetFrameSize() {
  return frame_size_;
}

void OpenXrGraphicsBinding::SetFrameSize(gfx::Size frame_size) {
  frame_size_ = frame_size;
  OnFrameSizeChanged();
}

XrResult OpenXrGraphicsBinding::ActivateSwapchainImage(
    XrSwapchain color_swapchain) {
  CHECK(!has_active_swapchain_image_);
  XrSwapchainImageAcquireInfo acquire_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  RETURN_IF_XR_FAILED(xrAcquireSwapchainImage(color_swapchain, &acquire_info,
                                              &active_swapchain_index_));

  XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wait_info.timeout = XR_INFINITE_DURATION;
  RETURN_IF_XR_FAILED(xrWaitSwapchainImage(color_swapchain, &wait_info));

  has_active_swapchain_image_ = true;
  OnSwapchainImageActivated();
  return XR_SUCCESS;
}

XrResult OpenXrGraphicsBinding::ReleaseActiveSwapchainImage(
    XrSwapchain color_swapchain) {
  CHECK(has_active_swapchain_image_);
  has_active_swapchain_image_ = false;

  // Since `active_swapchain_index_` is a unit32_t there's not a good "invalid"
  // number to set; so just leave it alone after clearing it.
  XrSwapchainImageReleaseInfo release_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  return xrReleaseSwapchainImage(color_swapchain, &release_info);
}

}  // namespace device
