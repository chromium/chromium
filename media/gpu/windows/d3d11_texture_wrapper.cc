// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_wrapper.h"

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/format_utils.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media {

namespace {

bool SupportsFormat(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return true;
    default:
      return false;
  }
}

viz::SharedImageFormat DXGIFormatToMultiPlanarSharedImageFormat(
    DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
      return viz::MultiPlaneFormat::kNV12;
    case DXGI_FORMAT_P010:
      return viz::MultiPlaneFormat::kP010;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return viz::SinglePlaneFormat::kBGRA_8888;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return viz::SinglePlaneFormat::kRGBA_1010102;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return viz::SinglePlaneFormat::kRGBA_F16;
    default:
      NOTREACHED_IN_MIGRATION();
      return viz::SinglePlaneFormat::kBGRA_8888;
  }
}

}  // anonymous namespace

Texture2DWrapper::Texture2DWrapper() = default;

Texture2DWrapper::~Texture2DWrapper() = default;

DefaultTexture2DWrapper::DefaultTexture2DWrapper(
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    DXGI_FORMAT dxgi_format,
    ComD3D11Device device)
    : size_(size),
      color_space_(color_space),
      dxgi_format_(dxgi_format),
      video_device_(std::move(device)) {}

DefaultTexture2DWrapper::~DefaultTexture2DWrapper() = default;

D3D11Status DefaultTexture2DWrapper::BeginSharedImageAccess() {
  if (shared_image_access_) {
    return D3D11Status::Codes::kOk;
  }

  if (shared_image_rep_) {
    TRACE_EVENT0("gpu", "D3D11TextureWrapper::BeginScopedWriteAccess");
    shared_image_access_ = shared_image_rep_->BeginScopedWriteAccess();
    if (!shared_image_access_) {
      return {D3D11Status::Codes::
                  kVideoDecodeImageRepresentationBeginScopedWriteAccessFailed,
              "Failed to begin shared image access"};
    }
  }

  return D3D11Status::Codes::kOk;
}

D3D11Status DefaultTexture2DWrapper::ProcessTexture(
    const gfx::ColorSpace& input_color_space,
    scoped_refptr<gpu::ClientSharedImage>& shared_image_dest) {
  // If we've received an error, then return it to our caller.  This is probably
  // from some previous operation.
  // TODO(liberato): Return the error.
  if (shared_image_access_) {
    TRACE_EVENT0("gpu", "D3D11TextureWrapper::EndScopedWriteAccess");
    shared_image_access_.reset();
  }

  shared_image_dest = shared_image_;

  // TODO(hitawala): Possibly optimize this method as input and stored color
  // spaces should be same.
  CHECK_EQ(input_color_space, color_space_);

  return D3D11Status::Codes::kOk;
}

D3D11Status DefaultTexture2DWrapper::Init(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferHelperCB get_helper_cb,
    ComD3D11Texture2D texture,
    size_t array_slice,
    scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
    Texture2DWrapper::PictureBufferGPUResourceInitDoneCB
        picture_buffer_gpu_resource_init_done_cb) {
  if (!SupportsFormat(dxgi_format_))
    return D3D11Status::Codes::kUnsupportedTextureFormatForBind;

  picture_buffer_gpu_resource_init_done_cb_ =
      std::move(picture_buffer_gpu_resource_init_done_cb);

  // Start construction of the GpuResources.
  // We send the texture itself, since we assume that we're using the angle
  // device for decoding.  Sharing seems not to work very well.  Otherwise, we
  // would create the texture with KEYED_MUTEX and NTHANDLE, then send along
  // a handle that we get from |texture| as an IDXGIResource1.
  auto on_error_cb = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &DefaultTexture2DWrapper::OnError, weak_factory_.GetWeakPtr()));

  auto gpu_resource_init_cb = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&DefaultTexture2DWrapper::OnGPUResourceInitDone,
                     weak_factory_.GetWeakPtr()));
  gpu_resources_ = base::SequenceBound<GpuResources>(
      std::move(gpu_task_runner), std::move(on_error_cb),
      std::move(get_helper_cb), size_, color_space_, dxgi_format_,
      video_device_, texture, array_slice, std::move(picture_buffer),
      std::move(gpu_resource_init_cb));
  return D3D11Status::Codes::kOk;
}

void DefaultTexture2DWrapper::OnError(D3D11Status status) {
  if (!received_error_)
    received_error_ = status;
}

void DefaultTexture2DWrapper::OnGPUResourceInitDone(
    scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
    std::unique_ptr<gpu::VideoImageRepresentation> shared_image_rep,
    scoped_refptr<gpu::ClientSharedImage> client_shared_image) {
  DCHECK(shared_image_rep);
  shared_image_rep_ = std::move(shared_image_rep);
  if (client_shared_image) {
    shared_image_ = std::move(client_shared_image);
  }
  std::move(picture_buffer_gpu_resource_init_done_cb_)
      .Run(std::move(picture_buffer));
}

DefaultTexture2DWrapper::GpuResources::GpuResources(
    OnErrorCB on_error_cb,
    GetCommandBufferHelperCB get_helper_cb,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    DXGI_FORMAT dxgi_format,
    ComD3D11Device video_device,
    ComD3D11Texture2D texture,
    size_t array_slice,
    scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
    GPUResourceInitCB gpu_resource_init_cb) {
  CHECK(texture);

  helper_ = get_helper_cb.Run();
  if (!helper_) {
    std::move(on_error_cb)
        .Run(std::move(D3D11Status::Codes::kGetCommandBufferHelperFailed));
    return;
  }

  auto* shared_image_manager = helper_->GetSharedImageManager();

  // Usage flags to allow the display compositor to draw from it, video to
  // decode from it, and webgl/canvas to read from it.
  gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  scoped_refptr<gpu::DXGISharedHandleState> dxgi_shared_handle_state;
  D3D11_TEXTURE2D_DESC desc = {};
  texture->GetDesc(&desc);
  // Create shared handle for shareable output texture.
  if (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) {
    ComDXGIResource1 dxgi_resource;
    HRESULT hr = texture.As(&dxgi_resource);
    if (FAILED(hr)) {
      DLOG(ERROR) << "QueryInterface for IDXGIResource failed with error "
                  << std::hex << hr;
      std::move(on_error_cb)
          .Run(std::move(D3D11Status::Codes::kCreateSharedHandleFailed));
      return;
    }

    // WebGPU will potentially read directly from this texture.
    usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;

    HANDLE shared_handle = nullptr;
    hr = dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &shared_handle);
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateSharedHandle failed with error " << std::hex << hr;
      std::move(on_error_cb)
          .Run(std::move(D3D11Status::Codes::kCreateSharedHandleFailed));
      return;
    }

    dxgi_shared_handle_state =
        shared_image_manager->dxgi_shared_handle_manager()
            ->CreateAnonymousSharedHandleState(
                base::win::ScopedHandle(shared_handle), texture);
  }
  const bool is_thread_safe =
      IsDedicatedMediaServiceThreadEnabled(gl::ANGLEImplementation::kD3D11);

  gpu::SharedImageInfo si_info{
      DXGIFormatToMultiPlanarSharedImageFormat(dxgi_format),
      size,
      color_space,
      kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType,
      usage,
      "VideoTexture"};
  scoped_refptr<gpu::GpuChannelSharedImageInterface>
      gpu_channel_shared_image_interface =
          helper_->GetSharedImageStub()->shared_image_interface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu_channel_shared_image_interface->CreateSharedImageForD3D11Video(
          si_info, texture, std::move(dxgi_shared_handle_state), array_slice,
          is_thread_safe);
  if (!shared_image) {
    std::move(on_error_cb)
        .Run(std::move(D3D11Status::Codes::kCreateSharedImageFailed));
    return;
  }

  auto* memory_type_tracker = helper_->GetMemoryTypeTracker();
  std::unique_ptr<gpu::VideoImageRepresentation> shared_image_rep =
      shared_image_manager->ProduceVideo(
          video_device.Get(), shared_image->mailbox(), memory_type_tracker);
  if (!shared_image_rep) {
    std::move(on_error_cb)
        .Run(D3D11Status::Codes::kProduceVideoDecodeImageRepresentationFailed);
    shared_image_ = nullptr;
    return;
  }

  std::move(gpu_resource_init_cb)
      .Run(std::move(picture_buffer), std::move(shared_image_rep),
           std::move(shared_image));
}

DefaultTexture2DWrapper::GpuResources::~GpuResources() = default;

}  // namespace media
