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

#if BUILDFLAG(IS_WIN)
#include <d3d11_4.h>
#include <wrl.h>
#endif

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
  uint32_t texture;
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
  // via `GetSwapChainInfo`.
  virtual XrResult EnumerateSwapchainImages(
      const XrSwapchain& color_swapchain) = 0;
  virtual void ClearSwapChainInfo() = 0;

  virtual base::span<SwapChainInfo> GetSwapChainInfo() = 0;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_
