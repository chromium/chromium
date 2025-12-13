// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_composition_layer.h"

#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_util.h"
#include "gpu/command_buffer/client/shared_image_interface.h"

namespace device {

// static
OpenXrCompositionLayer::Type OpenXrCompositionLayer::GetTypeFromMojomData(
    const mojom::XRLayerSpecificData& layer_specific_data) {
  switch (layer_specific_data.which()) {
    case mojom::XRLayerSpecificData::Tag::kProjection:
      return Type::kProjection;
    case mojom::XRLayerSpecificData::Tag::kQuad:
      return Type::kQuad;
    case mojom::XRLayerSpecificData::Tag::kCylinder:
      return Type::kCylinder;
    case mojom::XRLayerSpecificData::Tag::kEquirect:
      return Type::kEquirect;
    case mojom::XRLayerSpecificData::Tag::kCube:
      return Type::kCube;
  }
}

OpenXrCompositionLayer::OpenXrCompositionLayer(
    mojom::XRCompositionLayerDataPtr layer_data,
    OpenXrGraphicsBinding* graphics_binding,
    std::unique_ptr<GraphicsBindingData> graphics_binding_data)
    : graphics_binding_(graphics_binding),
      graphics_binding_data_(std::move(graphics_binding_data)),
      creation_data_(std::move(layer_data)) {
  type_ = GetTypeFromMojomData(*creation_data_->mutable_data->layer_data);

  // Projection layers will have same size as the base layer, and will
  // be set later.
  if (type_ != Type::kProjection) {
    SetSwapchainImageSize(gfx::Size(read_only_data().texture_width,
                                    read_only_data().texture_height));
  }
}

OpenXrCompositionLayer::~OpenXrCompositionLayer() = default;

gfx::Size OpenXrCompositionLayer::GetSwapchainImageSize() {
  return swapchain_image_size_;
}

void OpenXrCompositionLayer::SetSwapchainImageSize(
    const gfx::Size& swapchain_image_size) {
  swapchain_image_size_ = swapchain_image_size;
  graphics_binding_->OnSwapchainImageSizeChanged(*this);

  // By default assume that we're transferring something the same size as the
  // swapchain image. However, if it's already been set, we don't want to
  // override that.
  if (transfer_size_.IsZero()) {
    SetTransferSize(swapchain_image_size);
  }
}

gfx::Size OpenXrCompositionLayer::GetTransferSize() {
  return transfer_size_;
}

void OpenXrCompositionLayer::SetTransferSize(const gfx::Size& transfer_size) {
  transfer_size_ = transfer_size;
}

void OpenXrCompositionLayer::SetSwapchainImages(
    std::vector<OpenXrSwapchainInfo> images) {
  CHECK(color_swapchain_images_.empty());
  color_swapchain_images_ = std::move(images);
}

base::span<OpenXrSwapchainInfo> OpenXrCompositionLayer::GetSwapchainImages() {
  return color_swapchain_images_;
}

base::span<const OpenXrSwapchainInfo>
OpenXrCompositionLayer::GetSwapchainImages() const {
  return color_swapchain_images_;
}

XrResult OpenXrCompositionLayer::CreateSwapchain(XrSession session,
                                                 uint32_t sample_count) {
  DCHECK(!HasColorSwapchain());
  DCHECK(GetSwapchainImages().empty());

  XrSwapchainCreateInfo swapchain_create_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
  swapchain_create_info.arraySize = 1;
  swapchain_create_info.format = graphics_binding_->GetSwapchainFormat(session);

  swapchain_create_info.createFlags = creation_data_->read_only_data->is_static
                                          ? XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT
                                          : 0;
  swapchain_create_info.width = swapchain_image_size_.width();
  swapchain_create_info.height = swapchain_image_size_.height();
  swapchain_create_info.mipCount = 1;
  swapchain_create_info.faceCount = type_ == Type::kCube ? 6 : 1;
  swapchain_create_info.sampleCount = sample_count;
  swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

  XrSwapchain color_swapchain;
  RETURN_IF_XR_FAILED(
      xrCreateSwapchain(session, &swapchain_create_info, &color_swapchain));

  color_swapchain_ = color_swapchain;
  needs_redraw_ = true;

  RETURN_IF_XR_FAILED(graphics_binding_->EnumerateSwapchainImages(*this));

  return XR_SUCCESS;
}

void OpenXrCompositionLayer::DestroySwapchain(gpu::SharedImageInterface* sii) {
  // In case we still hold an active swapchain image.
  ReleaseActiveSwapchainImage();

  // Reset rendered state.
  needs_redraw_ = false;
  is_rendered_ = false;

  // As long as we have a context provider we need to destroy any SharedImages
  // that may exist.
  for (OpenXrSwapchainInfo& info : GetSwapchainImages()) {
    if (sii && info.shared_image && info.sync_token.HasData()) {
      sii->DestroySharedImage(info.sync_token, std::move(info.shared_image));
    }
    info.Clear();
  }

  if (color_swapchain_) {
    xrDestroySwapchain(color_swapchain_);
    color_swapchain_ = XR_NULL_HANDLE;
  }

  // Regardless of if we had a context provider or any shared images, we need to
  // clear the list of SwapchainImages.
  color_swapchain_images_.clear();
}

XrResult OpenXrCompositionLayer::ActivateSwapchainImage(
    gpu::SharedImageInterface* sii) {
  CHECK(!has_active_swapchain_image_);
  XrSwapchainImageAcquireInfo acquire_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  RETURN_IF_XR_FAILED(xrAcquireSwapchainImage(color_swapchain_, &acquire_info,
                                              &active_swapchain_index_));

  XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wait_info.timeout = XR_INFINITE_DURATION;
  RETURN_IF_XR_FAILED(xrWaitSwapchainImage(color_swapchain_, &wait_info));

  has_active_swapchain_image_ = true;
  // The current active swapchain image has not yet been rendered.
  is_rendered_ = false;
  graphics_binding_->OnSwapchainImageActivated(*this, sii);
  return XR_SUCCESS;
}

XrResult OpenXrCompositionLayer::ReleaseActiveSwapchainImage() {
  if (!has_active_swapchain_image_) {
    return XR_SUCCESS;
  }

  has_active_swapchain_image_ = false;

  // Since `active_swapchain_index_` is a unit32_t there's not a good "invalid"
  // number to set; so just leave it alone after clearing it.
  XrSwapchainImageReleaseInfo release_info = {
      XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  return xrReleaseSwapchainImage(color_swapchain_, &release_info);
}

OpenXrSwapchainInfo* OpenXrCompositionLayer::GetActiveSwapchainImage() {
  if (!has_active_swapchain_image_ ||
      active_swapchain_index_ >= color_swapchain_images_.size()) {
    return nullptr;
  }

  // We don't do any index translation on the images returned from the system;
  // so whatever the system says is the active swapchain image, it is in the
  // same spot in our vector.
  return &color_swapchain_images_[active_swapchain_index_];
}

bool OpenXrCompositionLayer::IsUsingSharedImages() const {
  const auto swapchain_info = GetSwapchainImages();
  return ((swapchain_info.size() > 1) && swapchain_info[0].shared_image);
}

LayerId OpenXrCompositionLayer::GetLayerId() const {
  return creation_data_->read_only_data->layer_id;
}

void OpenXrCompositionLayer::UpdateMutableLayerData(
    mojom::XRLayerMutableDataPtr data) {
  CHECK_EQ(type_, GetTypeFromMojomData(*data->layer_data));
  creation_data_->mutable_data = std::move(data);
}

void OpenXrCompositionLayer::UpdateActiveSwapchainImageSize(
    gpu::SharedImageInterface* sii) {
  if (has_active_swapchain_image_) {
    graphics_binding_->ResizeSharedBuffer(*this, *GetActiveSwapchainImage(),
                                          sii);
  }
}

const gfx::Rect OpenXrCompositionLayer::GetSubImageViewport(
    XrEyeVisibility eye) const {
  gfx::Rect info{0, 0, static_cast<int>(read_only_data().texture_width),
                 static_cast<int>(read_only_data().texture_height)};
  if (read_only_data().layout ==
      device::mojom::XRLayerLayout::kStereoLeftRight) {
    info.set_width(info.width() / 2);
    if (eye == XR_EYE_VISIBILITY_RIGHT) {
      info.set_x(info.width());
    }
  } else if (read_only_data().layout ==
             device::mojom::XRLayerLayout::kStereoTopBottom) {
    info.set_height(info.height() / 2);
    if (eye == XR_EYE_VISIBILITY_RIGHT) {
      info.set_y(info.height());
    }
  }
  return info;
}

std::vector<XrEyeVisibility> OpenXrCompositionLayer::GetXrEyesForComposition()
    const {
  if (read_only_data().layout ==
          device::mojom::XRLayerLayout::kStereoTopBottom ||
      read_only_data().layout ==
          device::mojom::XRLayerLayout::kStereoLeftRight) {
    return {XR_EYE_VISIBILITY_LEFT, XR_EYE_VISIBILITY_RIGHT};
  }
  return {XR_EYE_VISIBILITY_BOTH};
}

}  // namespace device
