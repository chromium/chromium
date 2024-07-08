// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/windows/openxr_graphics_binding_d3d11.h"

#include <d3d11_4.h>
#include <wrl.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/openxr_view_configuration.h"
#include "device/vr/openxr/windows/openxr_platform_helper_windows.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/gpu_fence.h"

namespace device {

// static
void OpenXrGraphicsBinding::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
  extensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
}

OpenXrGraphicsBindingD3D11::OpenXrGraphicsBindingD3D11(
    base::WeakPtr<OpenXrPlatformHelperWindows> weak_platform_helper)
    : texture_helper_(std::make_unique<D3D11TextureHelper>()),
      weak_platform_helper_(weak_platform_helper) {}

OpenXrGraphicsBindingD3D11::~OpenXrGraphicsBindingD3D11() = default;

bool OpenXrGraphicsBindingD3D11::Initialize(XrInstance instance,
                                            XrSystemId system) {
  if (initialized_) {
    return true;
  }

  if (!texture_helper_) {
    DVLOG(1) << __func__ << " No TextureHelper";
    return false;
  }

  if (!weak_platform_helper_) {
    DVLOG(1) << __func__ << " WeakPtr failed to resolve";
    return false;
  }

  LUID luid;
  if (!weak_platform_helper_->TryGetLuid(&luid, system)) {
    DVLOG(1) << __func__ << " Did not get a luid";
    return false;
  }

  texture_helper_->SetUseBGRA(true);
  if (!texture_helper_->SetAdapterLUID(luid) ||
      !texture_helper_->EnsureInitialized()) {
    DVLOG(1) << __func__ << " Texture helper initialization failed";
    return false;
  }

  binding_.device = texture_helper_->GetDevice().Get();
  initialized_ = true;
  return true;
}

const void* OpenXrGraphicsBindingD3D11::GetSessionCreateInfo() const {
  CHECK(initialized_);
  return &binding_;
}

int64_t OpenXrGraphicsBindingD3D11::GetSwapchainFormat(
    XrSession session) const {
  // OpenXR's swapchain format expects to describe the texture content.
  // The result of a swapchain image created from OpenXR API always contains a
  // typeless texture. On the other hand, WebGL API uses CSS color convention
  // that's sRGB. The RGBA typelss texture from OpenXR swapchain image leads to
  // a linear format render target view (reference to function
  // D3D11TextureHelper::EnsureRenderTargetView in d3d11_texture_helper.cc).
  // Therefore, the content in this openxr swapchain image is in sRGB format.
  return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

XrResult OpenXrGraphicsBindingD3D11::EnumerateSwapchainImages(
    const XrSwapchain& color_swapchain) {
  CHECK(color_swapchain != XR_NULL_HANDLE);
  CHECK(color_swapchain_images_.empty());

  uint32_t chain_length;
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainImages(color_swapchain, 0, &chain_length, nullptr));
  std::vector<XrSwapchainImageD3D11KHR> xr_color_swapchain_images(
      chain_length, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});

  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(
      color_swapchain, xr_color_swapchain_images.size(), &chain_length,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(
          xr_color_swapchain_images.data())));

  color_swapchain_images_.reserve(xr_color_swapchain_images.size());
  for (const auto& swapchain_image : xr_color_swapchain_images) {
    color_swapchain_images_.emplace_back(swapchain_image.texture);
  }

  return XR_SUCCESS;
}

void OpenXrGraphicsBindingD3D11::ClearSwapchainImages() {
  color_swapchain_images_.clear();
}

base::span<SwapChainInfo> OpenXrGraphicsBindingD3D11::GetSwapChainImages() {
  return color_swapchain_images_;
}

bool OpenXrGraphicsBindingD3D11::CanUseSharedImages() const {
  // Put shared image feature behind a flag until remaining issues with overlays
  // are resolved.
  return !base::FeatureList::IsEnabled(device::features::kOpenXRSharedImages);
}

void OpenXrGraphicsBindingD3D11::CreateSharedImages(
    gpu::SharedImageInterface* sii) {
  CHECK(sii);

  for (auto& swap_chain_info : color_swapchain_images_) {
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    HRESULT hr = swap_chain_info.d3d11_texture->QueryInterface(
        IID_PPV_ARGS(&dxgi_resource));
    if (FAILED(hr)) {
      DLOG(ERROR) << "QueryInterface for IDXGIResource failed with error "
                  << std::hex << hr;
      return;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
    hr = dxgi_resource.As(&d3d11_texture);
    if (FAILED(hr)) {
      DLOG(ERROR) << "QueryInterface for ID3D11Texture2D failed with error "
                  << std::hex << hr;
      return;
    }

    D3D11_TEXTURE2D_DESC texture2d_desc;
    d3d11_texture->GetDesc(&texture2d_desc);

    // Shared handle creation can fail on platforms where the texture, for
    // whatever reason, cannot be shared. We need to fallback gracefully to
    // texture copies.
    HANDLE shared_handle;
    hr = dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &shared_handle);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unable to create shared handle for DXGIResource "
                  << std::hex << hr;
      return;
    }

    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
    gpu_memory_buffer_handle.dxgi_handle.Set(shared_handle);
    gpu_memory_buffer_handle.dxgi_token = gfx::DXGIHandleToken();
    gpu_memory_buffer_handle.type = gfx::DXGI_SHARED_HANDLE;

    // TODO(crbug.com/40918787): This size is the size of the texture
    // from the OpenXr runtime, which is fine but does not work properly if the
    // page requests any kind of framebuffer scaling, because then the image
    // size that the page uses would be different than this size, which can
    // cause errors in rendering.
    gfx::Size buffer_size =
        gfx::Size(texture2d_desc.Width, texture2d_desc.Height);

    // The SharedImages created here will eventually be transferred to other
    // processes to have their contents written by WebGL and read via GL by
    // OpenXR.
    const gpu::SharedImageUsageSet shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
        gpu::SHARED_IMAGE_USAGE_GLES2_READ |
        gpu::SHARED_IMAGE_USAGE_GLES2_WRITE;

    swap_chain_info.shared_image = sii->CreateSharedImage(
        {viz::SinglePlaneFormat::kRGBA_8888, buffer_size,
         gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                         gfx::ColorSpace::TransferID::LINEAR),
         shared_image_usage, "OpenXrSwapChain"},
        std::move(gpu_memory_buffer_handle));
    CHECK(swap_chain_info.shared_image);
    swap_chain_info.sync_token = sii->GenVerifiedSyncToken();
  }
}

const SwapChainInfo& OpenXrGraphicsBindingD3D11::GetActiveSwapchainImage() {
  CHECK(has_active_swapchain_image());
  CHECK(active_swapchain_index() < color_swapchain_images_.size());

  // We don't do any index translation on the images returned from the system;
  // so whatever the system says is the active swapchain image, it is in the
  // same spot in our vector.
  return color_swapchain_images_[active_swapchain_index()];
}

bool OpenXrGraphicsBindingD3D11::WaitOnFence(gfx::GpuFence& gpu_fence) {
  if (!has_active_swapchain_image() ||
      active_swapchain_index() >= color_swapchain_images_.size()) {
    return false;
  }

  // We don't do any index translation on the images returned from the system;
  // so whatever the system says is the active swapchain image, it is in the
  // same spot in our vector.
  auto& swap_chain_info = color_swapchain_images_[active_swapchain_index()];

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      texture_helper_->GetDevice();
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device.As(&d3d11_device5);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11Device5 interface " << std::hex
                << hr;
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  hr = d3d11_device5->OpenSharedFence(gpu_fence.GetGpuFenceHandle().Peek(),
                                      IID_PPV_ARGS(&d3d11_fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to open a shared fence " << std::hex << hr;
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device5->GetImmediateContext(&d3d11_device_context);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> d3d11_device_context4;
  hr = d3d11_device_context.As(&d3d11_device_context4);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11DeviceContext4 interface "
                << std::hex << hr;
    return false;
  }

  hr = d3d11_device_context4->Wait(d3d11_fence.Get(), 1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to Wait on D3D11 fence " << std::hex << hr;
    return false;
  }

  // In order for the fence to be respected by the system, it needs to stick
  // around until the next time the texture comes up for use.
  swap_chain_info.d3d11_fence = std::move(d3d11_fence);

  return true;
}

bool OpenXrGraphicsBindingD3D11::Render(
    const scoped_refptr<viz::ContextProvider>& context_provider) {
  return texture_helper_->UpdateBackbufferSizes() &&
         texture_helper_->CompositeToBackBuffer(context_provider);
}

void OpenXrGraphicsBindingD3D11::CleanupWithoutSubmit() {
  texture_helper_->CleanupNoSubmit();
}

bool OpenXrGraphicsBindingD3D11::ShouldFlipSubmittedImage() {
  return IsUsingSharedImages();
}

void OpenXrGraphicsBindingD3D11::OnSwapchainImageSizeChanged() {
  texture_helper_->SetDefaultSize(GetSwapchainImageSize());
}

void OpenXrGraphicsBindingD3D11::OnSwapchainImageActivated(
    gpu::SharedImageInterface* sii) {
  CHECK(has_active_swapchain_image());
  CHECK(active_swapchain_index() < color_swapchain_images_.size());

  texture_helper_->SetBackbuffer(
      color_swapchain_images_[active_swapchain_index()].d3d11_texture.get());
}

void OpenXrGraphicsBindingD3D11::SetOverlayAndWebXrVisibility(
    bool overlay_visible,
    bool webxr_visible) {
  texture_helper_->SetSourceAndOverlayVisible(webxr_visible, overlay_visible);
}

void OpenXrGraphicsBindingD3D11::SetWebXrTexture(
    mojo::PlatformHandle texture_handle,
    const gpu::SyncToken& sync_token,
    const gfx::RectF& left,
    const gfx::RectF& right) {
  base::win::ScopedHandle scoped_handle = texture_handle.is_valid()
                                              ? texture_handle.TakeHandle()
                                              : base::win::ScopedHandle();
  texture_helper_->SetSourceTexture(std::move(scoped_handle), sync_token, left,
                                    right);
}

bool OpenXrGraphicsBindingD3D11::SetOverlayTexture(
    gfx::GpuMemoryBufferHandle texture,
    const gpu::SyncToken& sync_token,
    const gfx::RectF& left,
    const gfx::RectF& right) {
  if (texture.is_null()) {
    return false;
  }

  CHECK(texture.type == gfx::DXGI_SHARED_HANDLE);
  return texture_helper_->SetOverlayTexture(std::move(texture.dxgi_handle),
                                            sync_token, left, right);
}

}  // namespace device
