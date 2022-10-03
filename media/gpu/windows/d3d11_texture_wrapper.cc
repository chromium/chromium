// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_wrapper.h"

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/win/mf_helpers.h"
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

size_t NumPlanes(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_Y416:
      return 3;
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
      return 2;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return 1;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // anonymous namespace

Texture2DWrapper::Texture2DWrapper() = default;

Texture2DWrapper::~Texture2DWrapper() = default;

DefaultTexture2DWrapper::DefaultTexture2DWrapper(const gfx::Size& size,
                                                 DXGI_FORMAT dxgi_format)
    : size_(size), dxgi_format_(dxgi_format) {}

DefaultTexture2DWrapper::~DefaultTexture2DWrapper() = default;

D3D11Status DefaultTexture2DWrapper::AcquireKeyedMutexIfNeeded() {
  // keyed_mutex_acquired_ should be false when calling this API.
  // For non-shareable resource, the keyed_mutex_acquired_ should
  // never be reset.
  // For shareable resource, it lives behind use_single_texture flag
  // and decoder should always follow acquire-release operation pairs.
  DCHECK(!keyed_mutex_acquired_);

  // No need to acquire key mutex for non-shared resource.
  if (!keyed_mutex_) {
    return D3D11Status::Codes::kOk;
  }

  // Handled shared resource with no key mutex acquired.
  HRESULT hr =
      keyed_mutex_->AcquireSync(gpu::kDXGIKeyedMutexAcquireKey, INFINITE);

  if (FAILED(hr)) {
    keyed_mutex_acquired_ = false;
    DPLOG(ERROR) << "Unable to acquire the key mutex, error: " << hr;
    return {D3D11Status::Codes::kAcquireKeyedMutexFailed, hr};
  }

  // Key mutex has been acquired for shared resource.
  keyed_mutex_acquired_ = true;

  return D3D11Status::Codes::kOk;
}

D3D11Status DefaultTexture2DWrapper::ProcessTexture(
    const gfx::ColorSpace& input_color_space,
    MailboxHolderArray* mailbox_dest,
    gfx::ColorSpace* output_color_space) {
  // If the decoder acquired the key mutex before, it should be released now.
  if (keyed_mutex_) {
    DCHECK(keyed_mutex_acquired_);
    HRESULT hr = keyed_mutex_->ReleaseSync(gpu::kDXGIKeyedMutexAcquireKey);
    if (FAILED(hr)) {
      DPLOG(ERROR) << "Unable to release the keyed mutex, error: " << hr;
      return {D3D11Status::Codes::kReleaseKeyedMutexFailed, hr};
    }

    keyed_mutex_acquired_ = false;
  }

  // If we've received an error, then return it to our caller.  This is probably
  // from some previous operation.
  // TODO(liberato): Return the error.
  if (received_error_)
    return D3D11Status::Codes::kProcessTextureFailed;

  // TODO(liberato): make sure that |mailbox_holders_| is zero-initialized in
  // case we don't use all the planes.
  for (size_t i = 0; i < VideoFrame::kMaxPlanes; i++)
    (*mailbox_dest)[i] = mailbox_holders_[i];

  // We're just binding, so the output and output color spaces are the same.
  *output_color_space = input_color_space;

  return D3D11Status::Codes::kOk;
}

D3D11Status DefaultTexture2DWrapper::Init(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferHelperCB get_helper_cb,
    ComD3D11Texture2D texture,
    size_t array_slice) {
  if (!SupportsFormat(dxgi_format_))
    return D3D11Status::Codes::kUnsupportedTextureFormatForBind;

  // Init IDXGIKeyedMutex when using shared handle.
  if (texture) {
    // Cannot use shared handle for swap chain output texture.
    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    if (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
      DCHECK(!keyed_mutex_acquired_);
      HRESULT hr = texture.As(&keyed_mutex_);
      if (FAILED(hr)) {
        DPLOG(ERROR) << "Failed to get key_mutex from output resource, error "
                     << std::hex << hr;
        return {D3D11Status::Codes::kGetKeyedMutexFailed, hr};
      }
    }
  }

  // Generate mailboxes and holders.
  // TODO(liberato): Verify that this is really okay off the GPU main thread.
  // The current implementation is.
  std::vector<gpu::Mailbox> mailboxes;
  for (size_t plane = 0; plane < NumPlanes(dxgi_format_); plane++) {
    mailboxes.push_back(gpu::Mailbox::GenerateForSharedImage());
    mailbox_holders_[plane] = gpu::MailboxHolder(
        mailboxes[plane], gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);
  }

  // Start construction of the GpuResources.
  // We send the texture itself, since we assume that we're using the angle
  // device for decoding.  Sharing seems not to work very well.  Otherwise, we
  // would create the texture with KEYED_MUTEX and NTHANDLE, then send along
  // a handle that we get from |texture| as an IDXGIResource1.
  auto on_error_cb = BindToCurrentLoop(base::BindOnce(
      &DefaultTexture2DWrapper::OnError, weak_factory_.GetWeakPtr()));
  gpu_resources_ = base::SequenceBound<GpuResources>(
      std::move(gpu_task_runner), std::move(on_error_cb),
      std::move(get_helper_cb), std::move(mailboxes), size_, dxgi_format_,
      texture, array_slice);
  return D3D11Status::Codes::kOk;
}

void DefaultTexture2DWrapper::OnError(D3D11Status status) {
  if (!received_error_)
    received_error_ = status;
}

void DefaultTexture2DWrapper::SetStreamHDRMetadata(
    const gfx::HDRMetadata& stream_metadata) {}

void DefaultTexture2DWrapper::SetDisplayHDRMetadata(
    const DXGI_HDR_METADATA_HDR10& dxgi_display_metadata) {}

DefaultTexture2DWrapper::GpuResources::GpuResources(
    OnErrorCB on_error_cb,
    GetCommandBufferHelperCB get_helper_cb,
    const std::vector<gpu::Mailbox>& mailboxes,
    const gfx::Size& size,
    DXGI_FORMAT dxgi_format,
    ComD3D11Texture2D texture,
    size_t array_slice) {
  helper_ = get_helper_cb.Run();

  if (!helper_ || !helper_->MakeContextCurrent()) {
    std::move(on_error_cb)
        .Run(std::move(D3D11Status::Codes::kMakeContextCurrentFailed));
    return;
  }

  // Usage flags to allow the display compositor to draw from it, video to
  // decode, and allow webgl/canvas access.
  constexpr uint32_t usage =
      gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE | gpu::SHARED_IMAGE_USAGE_GLES2 |
      gpu::SHARED_IMAGE_USAGE_RASTER | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_SCANOUT;

  scoped_refptr<gpu::DXGISharedHandleState> dxgi_shared_handle_state;
  if (texture) {
    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    // Create shared handle for shareable output texture.
    if (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) {
      Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
      HRESULT hr = texture.As(&dxgi_resource);
      if (FAILED(hr)) {
        DLOG(ERROR) << "QueryInterface for IDXGIResource failed with error "
                    << std::hex << hr;
        std::move(on_error_cb)
            .Run(std::move(D3D11Status::Codes::kCreateSharedHandleFailed));
        return;
      }

      HANDLE shared_handle = nullptr;
      hr = dxgi_resource->CreateSharedHandle(
          nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
          nullptr, &shared_handle);
      if (FAILED(hr)) {
        DLOG(ERROR) << "CreateSharedHandle failed with error " << std::hex
                    << hr;
        std::move(on_error_cb)
            .Run(std::move(D3D11Status::Codes::kCreateSharedHandleFailed));
        return;
      }

      dxgi_shared_handle_state =
          helper_->GetDXGISharedHandleManager()
              ->CreateAnonymousSharedHandleState(
                  base::win::ScopedHandle(shared_handle), texture);
    }
  }

  auto shared_image_backings = gpu::D3DImageBacking::CreateFromVideoTexture(
      mailboxes, dxgi_format, size, usage, texture, array_slice,
      std::move(dxgi_shared_handle_state));
  if (shared_image_backings.empty()) {
    std::move(on_error_cb)
        .Run(std::move(D3D11Status::Codes::kCreateSharedImageFailed));
    return;
  }
  DCHECK_EQ(shared_image_backings.size(), NumPlanes(dxgi_format));

  for (auto& backing : shared_image_backings)
    shared_images_.push_back(helper_->Register(std::move(backing)));
}

DefaultTexture2DWrapper::GpuResources::~GpuResources() {
  // Destroy shared images with a current context.
  if (!helper_ || !helper_->MakeContextCurrent())
    return;
  shared_images_.clear();
}

}  // namespace media
