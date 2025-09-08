// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_COMPOSITION_LAYER_H_
#define DEVICE_VR_OPENXR_OPENXR_COMPOSITION_LAYER_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/openxr_swapchain_info.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrGraphicsBinding;

class OpenXrCompositionLayer {
 public:
  // Base class for graphics binding specific layer data.
  struct GraphicsBindingData {
    enum GraphicsBindingDataType {
      kInvalid,
      kOpenGLES,
      kD3D,
    };

    virtual ~GraphicsBindingData() = default;

    GraphicsBindingDataType type = kInvalid;
  };

  // Construct a projection layer
  OpenXrCompositionLayer(
      XrSpace space,
      OpenXrGraphicsBinding* graphics_binding,
      std::unique_ptr<GraphicsBindingData> graphics_binding_data);

  ~OpenXrCompositionLayer();

  // Returns the previously set swapchain image size, or 0,0 if one is not set.
  gfx::Size GetSwapchainImageSize();

  // Gets the size of the texture that is shared between our process and the
  // renderer process.
  gfx::Size GetTransferSize();

  // Sets the size of the texture being used between the renderer process and
  // our process. This is largely driven by the page and any framebuffer scaling
  // it may apply. When rendering to the SwapchainImage scaling will be
  // performed as necessary.
  void SetTransferSize(const gfx::Size& transfer_size);

  // Sets the size of the swapchain images being used by the system. Does *not*
  // cause a corresponding re-creation of the Swapchain or Shared Images; which
  // should be driven by the caller. If a transfer size has not been specified
  // yet, will also set the transfer size as well.
  void SetSwapchainImageSize(const gfx::Size& swapchain_image_size);

  // Called by graphics binding to set the swapchain images.
  void SetSwapchainImages(std::vector<OpenXrSwapchainInfo>);

  // Updates the active swapchain image size if the transfer size has changed.
  // No-ops if there is currently no active swapchain image.
  void UpdateActiveSwapchainImageSize(gpu::SharedImageInterface* sii);

  base::span<OpenXrSwapchainInfo> GetSwapchainImages();
  base::span<const OpenXrSwapchainInfo> GetSwapchainImages() const;

  // Create the XrSwapchain and swapchain images.
  XrResult CreateSwapchain(XrSession session, uint32_t sample_count);

  // Clears the list of images allocated during `CreateSwapchain` and
  // if a context_provider is provided and the Swapchain entries have had
  // corresponding SharedImages created via `CreateSharedImages` will also clean
  // up those SharedImages.
  void DestroySwapchain(gpu::SharedImageInterface* sii);

  // Acquire and activate a Swapchain image from the OpenXr system. This is the
  // swapchain image that will be in use for the next render.
  XrResult ActivateSwapchainImage(gpu::SharedImageInterface* sii);

  // Release the active swapchain image from the OpenXr system. This is called
  // before calling EndFrame and will enable acquiring a new swapchain image for
  // the next frame.
  XrResult ReleaseActiveSwapchainImage();

  // Get the currently active swapchain image.
  OpenXrSwapchainInfo* GetActiveSwapchainImage();

  // Return if the XrSwapchain is available.
  bool HasColorSwapchain() const { return color_swapchain_ != XR_NULL_HANDLE; }

  // Returns whether or not the current Swapchain is actually using SharedImages
  // or not.
  bool IsUsingSharedImages() const;

  // A group of simple getters.
  XrSwapchain color_swapchain() const { return color_swapchain_; }
  bool has_active_swapchain_image() const {
    return has_active_swapchain_image_;
  }
  GraphicsBindingData* graphics_binding_data() {
    return graphics_binding_data_.get();
  }
  XrSpace space() const { return space_; }

 private:
  XrSpace space_ = XR_NULL_HANDLE;
  raw_ptr<OpenXrGraphicsBinding> graphics_binding_;
  std::vector<OpenXrSwapchainInfo> color_swapchain_images_;
  gfx::Size swapchain_image_size_{0, 0};
  gfx::Size transfer_size_{0, 0};

  // Used to access the active swapchain index as returned by the system. This
  // class does not attempt to use the index in conjunction with
  // `GetSwapchainImages` as the children may do their own mapping. However,
  // this corresponds to the position of the corresponding texture in the array
  // as was returned by the OpenXr system when querying for the swapchain info.
  uint32_t active_swapchain_index_ = 0;

  // Indicates whether or not we actually have an active swapchain image (e.g.
  // ActivateSwapchainImage has been called, but ReleaseSwapchainImage has not).
  bool has_active_swapchain_image_ = false;

  // The swapchain is initializd when a session begins and is re-created when
  // the state of a secondary view configuration changes.
  XrSwapchain color_swapchain_ = XR_NULL_HANDLE;

  // Store graphics binding specific data.
  std::unique_ptr<GraphicsBindingData> graphics_binding_data_;
};  // OpenXrCompositionLayer

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_COMPOSITION_LAYER_H_
