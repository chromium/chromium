// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_graphics_binding.h"

#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/openxr_view_configuration.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gl/gl_bindings.h"

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
  DCHECK(!shared_image);
  DCHECK(!sync_token.HasData());
}
SwapChainInfo::SwapChainInfo(SwapChainInfo&&) = default;
SwapChainInfo& SwapChainInfo::operator=(SwapChainInfo&&) = default;

void SwapChainInfo::Clear() {
  shared_image.reset();
  sync_token.Clear();
#if BUILDFLAG(IS_ANDROID)
  // Resetting the SharedBufferSize ensures that we will re-create the Shared
  // Buffer if it is needed.
  shared_buffer_size = {0, 0};
#endif
}

gpu::MailboxHolder SwapChainInfo::GetMailboxHolder() const {
  CHECK(shared_image);
  return gpu::MailboxHolder(shared_image->mailbox(), sync_token, GL_TEXTURE_2D);
}

void OpenXrGraphicsBinding::PrepareViewConfigForRender(
    const XrSwapchain& color_swapchain,
    OpenXrViewConfiguration& view_config) {
  DCHECK(view_config.Active());

  uint32_t x_offset = view_config.Viewport().x();
  for (uint32_t view_index = 0; view_index < view_config.Views().size();
       view_index++) {
    const XrView& view = view_config.Views()[view_index];

    XrCompositionLayerProjectionView& projection_view =
        view_config.GetProjectionView(view_index);
    const OpenXrViewProperties& properties =
        view_config.Properties()[view_index];
    projection_view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    projection_view.pose = view.pose;
    projection_view.fov.angleLeft = view.fov.angleLeft;
    projection_view.fov.angleRight = view.fov.angleRight;
    projection_view.subImage.swapchain = color_swapchain;
    // Since we're in double wide mode, the texture array only has one texture
    // and is always index 0. If secondary views are enabled, those views are
    // also in this same texture array.
    projection_view.subImage.imageArrayIndex = 0;
    projection_view.subImage.imageRect.extent.width = properties.Width();
    projection_view.subImage.imageRect.extent.height = properties.Height();
    projection_view.subImage.imageRect.offset.x = x_offset;
    x_offset += properties.Width();

    projection_view.subImage.imageRect.offset.y =
        GetSwapchainImageSize().height() - properties.Height();
    projection_view.fov.angleUp = view.fov.angleUp;
    projection_view.fov.angleDown = view.fov.angleDown;

    // WebGL layers may give us flipped content. We need to instruct OpenXR
    // to flip the content before showing it to the user. Some XR runtimes
    // are able to efficiently do this as part of existing post processing
    // steps.
    if (ShouldFlipSubmittedImage()) {
      projection_view.subImage.imageRect.offset.y = 0;
      projection_view.fov.angleUp = view.fov.angleDown;
      projection_view.fov.angleDown = view.fov.angleUp;
    }
  }
}

bool OpenXrGraphicsBinding::IsUsingSharedImages() {
  const auto swapchain_info = GetSwapChainImages();
  return ((swapchain_info.size() > 1) && swapchain_info[0].shared_image);
}

gfx::Size OpenXrGraphicsBinding::GetSwapchainImageSize() {
  return swapchain_image_size_;
}

void OpenXrGraphicsBinding::SetSwapchainImageSize(
    const gfx::Size& swapchain_image_size) {
  swapchain_image_size_ = swapchain_image_size;
  OnSwapchainImageSizeChanged();

  // By default assume that we're transfering something the same size as the
  // swapchain image. However, if it's already been set, we don't want to
  // override that.
  if (transfer_size_.IsZero()) {
    SetTransferSize(swapchain_image_size);
  }
}

gfx::Size OpenXrGraphicsBinding::GetTransferSize() {
  return transfer_size_;
}

void OpenXrGraphicsBinding::SetTransferSize(const gfx::Size& transfer_size) {
  transfer_size_ = transfer_size;
}

void OpenXrGraphicsBinding::DestroySwapchainImages(
    viz::ContextProvider* context_provider) {
  // As long as we have a context provider we need to destroy any SharedImages
  // that may exist.
  if (context_provider) {
    gpu::SharedImageInterface* shared_image_interface =
        context_provider->SharedImageInterface();
    for (SwapChainInfo& info : GetSwapChainImages()) {
      if (shared_image_interface && info.shared_image &&
          info.sync_token.HasData()) {
        shared_image_interface->DestroySharedImage(
            info.sync_token, std::move(info.shared_image));
      }
      info.Clear();
    }
  }

  // Regardless of if we had a context provider or any shared images, we need to
  // clear the list of SwapchainImages.
  ClearSwapchainImages();
}

XrResult OpenXrGraphicsBinding::ActivateSwapchainImage(
    XrSwapchain color_swapchain,
    gpu::SharedImageInterface* sii) {
  CHECK(!has_active_swapchain_image_);
  XrSwapchainImageAcquireInfo acquire_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  RETURN_IF_XR_FAILED(xrAcquireSwapchainImage(color_swapchain, &acquire_info,
                                              &active_swapchain_index_));

  XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wait_info.timeout = XR_INFINITE_DURATION;
  RETURN_IF_XR_FAILED(xrWaitSwapchainImage(color_swapchain, &wait_info));

  has_active_swapchain_image_ = true;
  OnSwapchainImageActivated(sii);
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
