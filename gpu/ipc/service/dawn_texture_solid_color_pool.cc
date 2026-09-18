// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/dawn_texture_solid_color_pool.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/types/expected_macros.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "third_party/microsoft_dxheaders/src/include/experimental-composition/experimental-dcomp.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/win/d3d_shared_fence.h"
#include "ui/gl/solid_color_pool_base.h"

namespace gpu {

namespace {

constexpr DXGI_FORMAT kDxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

base::expected<Microsoft::WRL::ComPtr<ID3D12Resource>, gl::CommitError>
CreateD3D12Resource(ID3D12Device* d3d12_device) {
  D3D12_RESOURCE_DESC tex_desc = {};
  tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  tex_desc.Alignment = 0;
  tex_desc.Width = gl::kSolidColorSurfaceSize.width();
  tex_desc.Height = gl::kSolidColorSurfaceSize.height();
  tex_desc.DepthOrArraySize = 1;
  tex_desc.MipLevels = 1;
  tex_desc.Format = kDxgiFormat;
  tex_desc.SampleDesc.Count = 1;
  tex_desc.SampleDesc.Quality = 0;
  tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  // `ALLOW_RENDER_TARGET` so Dawn can render into it. `ALLOW_SIMULTANEOUS_
  // ACCESS` is required by Dawn's `SharedTextureMemoryD3D12Resource` import
  // path (see dawn/native/d3d12/SharedTextureMemoryD3D12.cpp). The flag also
  // implies the resource is created in COMMON state, which is what Dawn
  // expects for its internal state tracking.
  tex_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
                   D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

  D3D12_HEAP_PROPERTIES heap_props = {};
  heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
  heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heap_props.CreationNodeMask = 0;
  heap_props.VisibleNodeMask = 0;

  // The resource must be shared (`D3D12_HEAP_FLAG_SHARED`) so that DComp's
  // compositor and Dawn's `SharedTextureMemoryD3D12Resource` import path can
  // both reference the same underlying texture across device/process
  // boundaries.
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  HRESULT hr = d3d12_device->CreateCommittedResource(
      &heap_props, D3D12_HEAP_FLAG_SHARED, &tex_desc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource));
  if (FAILED(hr)) {
    return base::unexpected(gl::CommitError{
        gl::CommitError::Reason::kSolidColorTexturePoolCreateD3D12Resource,
        hr});
  }
  return resource;
}

Microsoft::WRL::ComPtr<IDCompositionTexture> CreateCompositionTexture(
    IDCompositionDevice4* dcomp_device4,
    ID3D12Resource* d3d12_resource) {
  Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture;
  HRESULT hr =
      dcomp_device4->CreateCompositionTexture(d3d12_resource, &dcomp_texture);
  CHECK_EQ(hr, S_OK);
  hr = dcomp_texture->SetAlphaMode(DXGI_ALPHA_MODE_IGNORE);
  CHECK_EQ(hr, S_OK);
  hr = dcomp_texture->SetColorSpace(
      gfx::ColorSpaceWin::GetDXGIColorSpace(gfx::ColorSpace::CreateSRGB()));
  CHECK_EQ(hr, S_OK);
  return dcomp_texture;
}

base::expected<wgpu::SharedTextureMemory, gl::CommitError>
CreateSharedTextureMemory(
    const wgpu::Device& device,
    Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource) {
  wgpu::SharedTextureMemory shared_texture_memory =
      CreateDawnSharedTextureMemory(device, std::move(d3d12_resource));
  if (!shared_texture_memory) {
    LOG(ERROR) << "CreateDawnSharedTextureMemory failed for solid color "
                  "texture";
    return base::unexpected(
        gl::CommitError{gl::CommitError::Reason::
                            kSolidColorTexturePoolCreateSharedTextureMemory});
  }
  return shared_texture_memory;
}

}  // namespace

DawnTextureSolidColorPool::TextureEntry::TextureEntry(
    Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource,
    Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture,
    wgpu::SharedTextureMemory shared_texture_memory)
    : d3d12_resource_(std::move(d3d12_resource)),
      dcomp_texture_(std::move(dcomp_texture)),
      shared_texture_memory_(std::move(shared_texture_memory)) {
  CHECK(d3d12_resource_);
  CHECK(dcomp_texture_);
  // TODO(crbug.com/526545475) CHECK the shared_texture_memory when
  // tests support passing in a non-null STM.
}

DawnTextureSolidColorPool::TextureEntry::~TextureEntry() = default;

IUnknown* DawnTextureSolidColorPool::TextureEntry::GetContent() const {
  return dcomp_texture_.Get();
}

// static
base::expected<std::unique_ptr<DawnTextureSolidColorPool::TextureEntry>,
               gl::CommitError>
DawnTextureSolidColorPool::TextureEntry::Create(
    ID3D12Device* d3d12_device,
    IDCompositionDevice4* dcomp_device4,
    const wgpu::Device& device) {
  Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource;
  ASSIGN_OR_RETURN(d3d12_resource, CreateD3D12Resource(d3d12_device));
  Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture =
      CreateCompositionTexture(dcomp_device4, d3d12_resource.Get());
  wgpu::SharedTextureMemory shared_texture_memory;
  ASSIGN_OR_RETURN(shared_texture_memory,
                   CreateSharedTextureMemory(device, d3d12_resource));
  return base::WrapUnique(new TextureEntry(std::move(d3d12_resource),
                                           std::move(dcomp_texture),
                                           std::move(shared_texture_memory)));
}

base::expected<std::unique_ptr<DawnTextureSolidColorPool::TextureEntry>,
               gl::CommitError>
DawnTextureSolidColorPool::CreateEntry() {
  return TextureEntry::Create(d3d12_device_.Get(), dcomp_device4_.Get(),
                              device_);
}

DawnTextureSolidColorPool::FenceCheckResult
DawnTextureSolidColorPool::CheckAvailableFence(
    IDCompositionTexture* dcomp_texture) {
  Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_fence;
  uint64_t fence_value = 0;
  HRESULT hr = dcomp_texture->GetAvailableFence(&fence_value,
                                                IID_PPV_ARGS(&d3d12_fence));
  CHECK_EQ(hr, S_OK) << ", GetAvailableFence failed: "
                     << logging::SystemErrorCodeToString(hr);
  if (!d3d12_fence) {
    return {.needs_rebuild = true};
  }
  if (d3d12_fence->GetCompletedValue() >= fence_value) {
    return {};
  }
  return {.wait_fence = gfx::D3DSharedFence::CreateFromD3D12Fence(
              std::move(d3d12_fence), fence_value)};
}

base::expected<void, gl::CommitError> DawnTextureSolidColorPool::EncodeFill(
    const wgpu::SharedTextureMemory& shared_texture_memory,
    const SkColor4f& color,
    scoped_refptr<gfx::D3DSharedFence> wait_fence,
    bool already_initialized) {
  wgpu::Texture texture = CreateDawnSharedTexture(
      shared_texture_memory, wgpu::TextureUsage::RenderAttachment,
      wgpu::TextureUsage::RenderAttachment, /*view_formats=*/{});
  if (!texture) {
    LOG(ERROR) << "CreateDawnSharedTexture failed for solid color fill";
    return base::unexpected(
        gl::CommitError{gl::CommitError::Reason::
                            kSolidColorTexturePoolCreateDawnSharedTexture});
  }

  wgpu::SharedTextureMemoryBeginAccessDescriptor begin_desc = {};
  begin_desc.initialized = already_initialized;
  wgpu::SharedFence shared_fence;
  uint64_t signaled_value = 0;
  if (wait_fence) {
    shared_fence = CreateDawnSharedFence(device_, wait_fence);
    signaled_value = wait_fence->GetFenceValue();
    begin_desc.fenceCount = 1;
    begin_desc.fences = &shared_fence;
    begin_desc.signaledValues = &signaled_value;
  }
  if (shared_texture_memory.BeginAccess(texture, &begin_desc) !=
      wgpu::Status::Success) {
    LOG(ERROR) << "SharedTextureMemory::BeginAccess failed for solid color "
                  "fill";
    return base::unexpected(gl::CommitError{
        gl::CommitError::Reason::kSolidColorTexturePoolBeginAccess});
  }

  // Record the GPU work to clear the texture to `color`.
  const SkColor4f opaque = color.makeOpaque();
  wgpu::RenderPassColorAttachment color_attachment = {};
  color_attachment.view = texture.CreateView();
  color_attachment.loadOp = wgpu::LoadOp::Clear;
  color_attachment.storeOp = wgpu::StoreOp::Store;
  color_attachment.clearValue = {opaque.fR, opaque.fG, opaque.fB, opaque.fA};
  color_attachment.depthSlice = wgpu::kDepthSliceUndefined;

  wgpu::RenderPassDescriptor pass_desc = {};
  pass_desc.label = "DawnTextureSolidColorPool clear";
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &color_attachment;

  wgpu::CommandEncoder encoder = EnsureEncoder();
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&pass_desc);
  pass.End();

  // The exported end-access fence is unused: DComp's
  // `PresentCompositionTextures` runs on the same `wgpu::Queue` after our
  // `FlushPendingFillsBeforeCommit` submit, so single-queue ordering alone
  // guarantees the fill executes before any consumer reads the texture.
  wgpu::SharedTextureMemoryEndAccessState end_state = {};
  shared_texture_memory.EndAccess(texture, &end_state);
  return base::ok();
}

DawnTextureSolidColorPool::DawnTextureSolidColorPool(
    wgpu::Device device,
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_command_queue,
    Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device)
    : device_(std::move(device)) {
  CHECK(device_);
  CHECK(d3d12_command_queue);
  CHECK(dcomp_device);
  queue_ = device_.GetQueue();
  CHECK(queue_);

  HRESULT hr = dcomp_device.As(&dcomp_device4_);
  CHECK_EQ(hr, S_OK);

  d3d12_command_queue->GetDevice(IID_PPV_ARGS(&d3d12_device_));
  CHECK(d3d12_device_);
}

DawnTextureSolidColorPool::DawnTextureSolidColorPool() = default;

DawnTextureSolidColorPool::~DawnTextureSolidColorPool() = default;

wgpu::CommandEncoder DawnTextureSolidColorPool::EnsureEncoder() {
  if (encoder_) {
    return encoder_;
  }
  wgpu::CommandEncoderDescriptor desc = {};
  desc.label = "DawnTextureSolidColorPool";
  encoder_ = device_.CreateCommandEncoder(&desc);
  return encoder_;
}

base::expected<void, gl::CommitError> DawnTextureSolidColorPool::FillEntry(
    std::unique_ptr<gl::SolidColorPoolBase::Entry>& base_entry,
    const SkColor4f& color) {
  // If an entry was previously used, check whether DWM is done scanning out
  // its texture. If so, reuse it in place; otherwise build a fresh entry so we
  // don't race the read. There may be another entry in the pool immediately
  // available that we're ignoring in favor of reusing the provided one, but
  // this keeps pool management in `SolidColorPoolBase` without exposing the
  // resource details it manages. For ~12 1x1 DComp textures this is an
  // acceptable tradeoff.
  if (auto* existing = static_cast<TextureEntry*>(base_entry.get())) {
    FenceCheckResult fence_state =
        CheckAvailableFence(existing->dcomp_texture());
    if (!fence_state.needs_rebuild) {
      // Fill the existing entry in place. Its D3D12 resource still carries the
      // contents of its prior fill, so Dawn can skip the lazy clear.
      return EncodeFill(existing->shared_texture_memory(), color,
                        std::move(fence_state.wait_fence),
                        /*already_initialized=*/true);
    }
  }

  // We either have no entry to reuse, or the provided entry's texture is still
  // in use by DWM. Build the fresh entry and only publish it into `base_entry`
  // on success.
  ASSIGN_OR_RETURN(std::unique_ptr<TextureEntry> new_entry, CreateEntry());
  RETURN_IF_ERROR(EncodeFill(new_entry->shared_texture_memory(), color,
                             /*wait_fence=*/nullptr,
                             /*already_initialized=*/false));
  base_entry = std::move(new_entry);
  return base::ok();
}

base::expected<void, gl::CommitError>
DawnTextureSolidColorPool::FlushPendingFillsBeforeCommit() {
  if (!encoder_) {
    return base::ok();
  }
  wgpu::CommandBuffer cmd = encoder_.Finish();
  encoder_ = nullptr;
  queue_.Submit(1, &cmd);
  return base::ok();
}

gl::SolidColorPoolFactory CreateDawnTextureSolidColorPoolFactory(
    wgpu::Device device,
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_command_queue) {
  return base::BindRepeating(
      [](wgpu::Device device,
         Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12_command_queue,
         IDCompositionDevice3* dcomp_device)
          -> std::unique_ptr<gl::SolidColorPoolBase> {
        CHECK(dcomp_device);
        return std::make_unique<DawnTextureSolidColorPool>(
            std::move(device), std::move(d3d12_command_queue),
            Microsoft::WRL::ComPtr<IDCompositionDevice3>(dcomp_device));
      },
      std::move(device), std::move(d3d12_command_queue));
}

}  // namespace gpu
