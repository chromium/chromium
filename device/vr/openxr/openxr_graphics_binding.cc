// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_graphics_binding.h"

#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/openxr_composition_layer.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/openxr_view_configuration.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gl/gl_bindings.h"

namespace device {

// static
std::vector<std::string> OpenXrGraphicsBinding::GetOptionalExtensions() {
  return {XR_FB_COMPOSITION_LAYER_IMAGE_LAYOUT_EXTENSION_NAME};
}

OpenXrGraphicsBinding::OpenXrGraphicsBinding(
    const OpenXrExtensionEnumeration* extension_enum)
    : fb_composition_layer_ext_enabled_(extension_enum->ExtensionSupported(
          XR_FB_COMPOSITION_LAYER_IMAGE_LAYOUT_EXTENSION_NAME)) {
  if (fb_composition_layer_ext_enabled_) {
    y_flip_layer_layout_.type = XR_TYPE_COMPOSITION_LAYER_IMAGE_LAYOUT_FB;
    y_flip_layer_layout_.flags =
        XR_COMPOSITION_LAYER_IMAGE_LAYOUT_VERTICAL_FLIP_BIT_FB;
  }
}

OpenXrGraphicsBinding::~OpenXrGraphicsBinding() {
  DCHECK(!base_layer_);
  OnSessionDestroyed(nullptr);
}

void OpenXrGraphicsBinding::OnSessionCreated(XrSpace local_space,
                                             bool is_webgpu) {
  webgpu_session_ = is_webgpu;
  base_layer_ = CreateProjectionLayer(local_space);
}

void OpenXrGraphicsBinding::OnSessionDestroyed(gpu::SharedImageInterface* sii) {
  if (base_layer_) {
    base_layer_->DestroySwapchain(sii);
    base_layer_.reset();
  }
}

void OpenXrGraphicsBinding::PrepareViewConfigForRender(
    OpenXrViewConfiguration& view_config) {
  DCHECK(view_config.Active());
  CHECK(base_layer_);

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
    projection_view.subImage.swapchain = base_layer_->color_swapchain();
    // Since we're in double wide mode, the texture array only has one texture
    // and is always index 0. If secondary views are enabled, those views are
    // also in this same texture array.
    projection_view.subImage.imageArrayIndex = 0;
    projection_view.subImage.imageRect.extent.width = properties.Width();
    projection_view.subImage.imageRect.extent.height = properties.Height();
    projection_view.subImage.imageRect.offset.x = x_offset;
    x_offset += properties.Width();

    projection_view.subImage.imageRect.offset.y =
        base_layer_->GetSwapchainImageSize().height() - properties.Height();
    projection_view.fov.angleUp = view.fov.angleUp;
    projection_view.fov.angleDown = view.fov.angleDown;

    // WebGL layers may give us flipped content. We need to instruct OpenXR
    // to flip the content before showing it to the user. Some XR runtimes
    // are able to efficiently do this as part of existing post processing
    // steps. However, if we have the composition layer extension enabled, we
    // will instruct the runtime to invert the image in a different manner.
    if (ShouldFlipSubmittedImage(*base_layer_) &&
        !fb_composition_layer_ext_enabled_) {
      projection_view.subImage.imageRect.offset.y = 0;
      projection_view.fov.angleUp = -view.fov.angleUp;
      projection_view.fov.angleDown = -view.fov.angleDown;
    }
  }
}

void OpenXrGraphicsBinding::MaybeFlipLayer(
    XrCompositionLayerProjection& layer) const {
  // If we don't need to flip the image, then we have nothing to do here.
  // If we do need to flip the image and `fb_composition_layer_ext_enabled_`
  // is false, we have already flipped the image during
  // `PrepareViewConfigForRender`.
  if (!ShouldFlipSubmittedImage(*base_layer_) ||
      !fb_composition_layer_ext_enabled_) {
    return;
  }

  CHECK(layer.next == nullptr);

  layer.next = &y_flip_layer_layout_;
}

bool OpenXrGraphicsBinding::IsUsingSharedImages() const {
  return base_layer_ && base_layer_->IsUsingSharedImages();
}

void OpenXrGraphicsBinding::OnContextProviderLost() {
  if (base_layer_) {
    // Mark the shared mailboxes as invalid since the underlying GPU process
    // associated with them has gone down.
    for (OpenXrSwapchainInfo& info : base_layer_->GetSwapchainImages()) {
      info.Clear();
    }
  }
}

std::unique_ptr<OpenXrLayers> OpenXrGraphicsBinding::GetLayersForViewConfig(
    XrSpace local_space,
    XrEnvironmentBlendMode blend_mode,
    const std::vector<XrCompositionLayerProjectionView>& projection_views)
    const {
  auto layers = std::make_unique<OpenXrLayers>(base_layer_->space(), blend_mode,
                                               *this, projection_views);
  return layers;
}

XrResult OpenXrGraphicsBinding::CreateBaseLayerSwapchain(
    XrSession session,
    uint32_t sample_count) {
  CHECK(base_layer_);
  return base_layer_->CreateSwapchain(session, sample_count);
}

void OpenXrGraphicsBinding::DestroyBaseLayerSwapchain(
    gpu::SharedImageInterface* sii) {
  CHECK(base_layer_);
  base_layer_->DestroySwapchain(sii);
}

void OpenXrGraphicsBinding::CreateBaseLayerSharedImages(
    gpu::SharedImageInterface* sii) {
  CHECK(base_layer_);
  CreateSharedImages(*base_layer_, sii);
}

gfx::Size OpenXrGraphicsBinding::GetBaseLayerSwapchainImageSize() {
  return base_layer_->GetSwapchainImageSize();
}

void OpenXrGraphicsBinding::SetBaseLayerSwapchainImageSize(
    const gfx::Size& swapchain_image_size) {
  base_layer_->SetSwapchainImageSize(swapchain_image_size);
}

bool OpenXrGraphicsBinding::HasBaseLayerColorSwapchain() const {
  return base_layer_ && base_layer_->HasColorSwapchain() &&
         base_layer_->GetSwapchainImages().size() > 0;
}

void OpenXrGraphicsBinding::SetBaseLayerTransferSize(
    const gfx::Size& transfer_size) {
  CHECK(base_layer_);
  base_layer_->SetTransferSize(transfer_size);
}

bool OpenXrGraphicsBinding::WaitOnBaseLayerFence(gfx::GpuFence& gpu_fence) {
  CHECK(base_layer_);
  return WaitOnFence(*base_layer_, gpu_fence);
}

void OpenXrGraphicsBinding::UpdateBaseLayerActiveSwapchainImageSize(
    gpu::SharedImageInterface* sii) {
  CHECK(base_layer_);
  base_layer_->UpdateActiveSwapchainImageSize(sii);
}

XrResult OpenXrGraphicsBinding::ActivateSwapchainImages(
    gpu::SharedImageInterface* sii) {
  return base_layer_->ActivateSwapchainImage(sii);
}

XrResult OpenXrGraphicsBinding::ReleaseActiveSwapchainImages() {
  return base_layer_->ReleaseActiveSwapchainImage();
}

void OpenXrGraphicsBinding::PopulateSharedImageData(
    mojom::XRFrameData& frame_data) {
  DCHECK(base_layer_);
  const auto* swapchain_info = base_layer_->GetActiveSwapchainImage();
  if (swapchain_info && swapchain_info->shared_image) {
    frame_data.buffer_shared_image = swapchain_info->shared_image->Export();
    frame_data.buffer_sync_token = swapchain_info->sync_token;
  }
}

bool OpenXrGraphicsBinding::Render(
    const scoped_refptr<viz::ContextProvider>& context_provider) {
  CHECK(base_layer_);
  return RenderLayer(*base_layer_, context_provider);
}

}  // namespace device
