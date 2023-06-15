// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_
#define DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11_4.h>
#include <wrl.h>
#endif

#if BUILDFLAG(IS_ANDROID)
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gl/scoped_egl_image.h"
#endif

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gpu {
class SharedImageInterface;
}  // namespace gpu

namespace device {

// TODO(https://crbug.com/1441072): Refactor this class.
struct SwapChainInfo {
 public:
#if BUILDFLAG(IS_WIN)
  explicit SwapChainInfo(ID3D11Texture2D*);
#elif BUILDFLAG(IS_ANDROID)
  explicit SwapChainInfo(uint32_t texture);
#endif
  SwapChainInfo();
  virtual ~SwapChainInfo();
  SwapChainInfo(SwapChainInfo&&);
  SwapChainInfo& operator=(SwapChainInfo&&);

  void Clear();

  gpu::MailboxHolder mailbox_holder;

#if BUILDFLAG(IS_WIN)
  // When shared images are being used, there is a corresponding MailboxHolder
  // and D3D11Fence for each D3D11 texture in the vector.
  raw_ptr<ID3D11Texture2D> d3d11_texture = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
#elif BUILDFLAG(IS_ANDROID)
  // Ideally this would be a gluint, but there are conflicting headers for GL
  // depending on *how* you want to use it; so we can't use it at the moment.
  uint32_t openxr_texture;

  uint32_t shared_buffer_texture;

  // Shared GpuMemoryBuffer
  std::unique_ptr<gpu::GpuMemoryBufferImplAndroidHardwareBuffer> gmb;

  // This object keeps the image alive while processing a frame. That's
  // required because it owns underlying resources, and must still be
  // alive when the mailbox texture backed by this image is used.
  gl::ScopedEGLImage local_eglimage;
#endif
};

// This class exists to provide an abstraction for the different rendering
// paths that can be taken by OpenXR (e.g. DirectX vs. GLES). Any OpenXr methods
// that need types specific for a given renderer type should go through this
// interface.
class OpenXrGraphicsBinding {
 public:
  // Gets the set of RequiredExtensions that need to be present on the platform.
  static void GetRequiredExtensions(std::vector<const char*>& extensions);

  virtual ~OpenXrGraphicsBinding() = default;

  // Ensures that the GraphicsBinding is ready for use.
  virtual bool Initialize(XrInstance instance, XrSystemId system) = 0;

  // Gets a pointer to a platform-specific XrGraphicsBindingFoo. The pointer is
  // guaranteed to live as long as this class does.
  virtual const void* GetSessionCreateInfo() const = 0;

  // Gets the format that we expect from the platform swapchain.
  virtual int64_t GetSwapchainFormat(XrSession session) const = 0;

  // Calls xrEnumerateSwapChain and updates the stored SwapChainInfo available
  // via `GetSwapChainImages`.
  virtual XrResult EnumerateSwapchainImages(
      const XrSwapchain& color_swapchain) = 0;

  // Clears the list of images allocated during `EnumerateSwapchainImages`.
  virtual void ClearSwapChainImages() = 0;

  // Returns a list of mutable SwapChainInfo objects. While the items themselves
  // are mutable, the list is not.
  // TODO(https://crbug.com/1441072): Make SwapChainInfo internal to the child
  // classes.
  virtual base::span<SwapChainInfo> GetSwapChainImages() = 0;

  // Returns whether or not the platform believes it can support using Shared
  // buffers/images.
  virtual bool CanUseSharedImages() const = 0;

  // Creates SharedImages for (and thus populates the mailbox holders of) all
  // currently held SwapChainInfo objects.
  virtual void CreateSharedImages(gpu::SharedImageInterface* sii) = 0;

  // Returns the currently active swapchain image. This is only valid between
  // calls to ActivateSwapchainImage and ReleaseSwapchainImage, which happens
  // after BeginFrame and before EndFrame.
  // TODO(https://crbug.com/1441072): Make SwapChainInfo internal to the child
  // classes.
  virtual const SwapChainInfo& GetActiveSwapchainImage() = 0;

  // Performs a server wait on the provided gpu_fence. Returns true if it was
  // able to successfully schedule and perform the wait, and false otherwise.
  virtual bool WaitOnFence(gfx::GpuFence& gpu_fence) = 0;

  // Causes the GraphicsBinding to render the currently active swapchain image.
  // TODO(https://crbug.com/1454943): Make pure virtual
  virtual bool Render();

  // Sets the size of the frame being used by the system. Does *not* cause a
  // corresponding re-creation of the Swapchain or Shared Images; which should
  // be driven by the caller.
  void SetFrameSize(gfx::Size frame_size);

  // Returns the previously set frame size, or a size of 0,0 if one has not been
  // set.
  gfx::Size GetFrameSize();

  // Acquire and activate a Swapchain image from the OpenXr system. This is the
  // swapchain image that will be in use for the next render.
  XrResult ActivateSwapchainImage(XrSwapchain color_swapchain);

  // Release the active swapchain image from the OpenXr system. This is called
  // before calling EndFrame and will enable acquiring a new swapchain image for
  // the next frame.
  XrResult ReleaseActiveSwapchainImage(XrSwapchain color_swapchain);

 protected:
  // Will be called when SetFrameSize is called, even if a change is not made,
  // to allow child classes/concrete implementations to override any state that
  // they may need to override as a result of the frame size changing.
  virtual void OnFrameSizeChanged() {}

  // Called at the end of ActivateSwapchainImage. Allows Children to setup the
  // appropriate image to be rendered to by, e.g. Render calls, if that needs
  // to happen ahead of time.
  // TODO(https://crbug.com/1454938): Currently only used on Windows. Is it
  // actually needed prior to the draw calls happening? Could the logic be
  // condensed into Render along with the other calls needed to render to the
  // texture?
  virtual void OnSwapchainImageActivated() {}

  // Used to access the active swapchain index as returned by the system. This
  // class does not attempt to use the index in conjunction with
  // `GetSwapChainImages` as the children may do their own mapping. However,
  // this corresponds to the position of the corresponding texture in the array
  // as was returned by the OpenXr system when querying for the swapchain info.
  uint32_t active_swapchain_index() { return active_swapchain_index_; }

  // Indicates whether or not we actually have an active swapchain image (e.g.
  // ActivateSwapchainImage has been called, but ReleaseSwapchainImage has not).
  bool has_active_swapchain_image() { return has_active_swapchain_image_; }

 private:
  gfx::Size frame_size_{0, 0};
  uint32_t active_swapchain_index_;
  bool has_active_swapchain_image_ = false;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_
