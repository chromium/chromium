// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dcomp_image_backing_factory.h"

#include <d3d11_1.h>

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/dcomp_surface_image_backing.h"
#include "gpu/command_buffer/service/shared_image/dxgi_swap_chain_image_backing.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gl/direct_composition_child_surface_win.h"

namespace gpu {

namespace {

// Check if a format is supported by DXGI for DComp surfaces or swap chains.
// https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/converting-data-color-space
bool IsFormatSupportedForScanout(viz::SharedImageFormat format) {
  return ((format == viz::SinglePlaneFormat::kRGBA_8888) ||
          (format == viz::SinglePlaneFormat::kBGRA_8888) ||
          (format == viz::SinglePlaneFormat::kRGBX_8888) ||
          (format == viz::SinglePlaneFormat::kBGRX_8888) ||
          (format == viz::SinglePlaneFormat::kRGBA_F16) ||
          (format == viz::SinglePlaneFormat::kRGBA_1010102));
}

constexpr uint32_t kDXGISwapChainUsage = SHARED_IMAGE_USAGE_DISPLAY_READ |
                                         SHARED_IMAGE_USAGE_DISPLAY_WRITE |
                                         SHARED_IMAGE_USAGE_SCANOUT;
constexpr uint32_t kDCompSurfaceUsage =
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_SCANOUT |
    SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE;
constexpr uint32_t kSupportedUsage = kDXGISwapChainUsage | kDCompSurfaceUsage;

}  // namespace

DCompImageBackingFactory::DCompImageBackingFactory()
    : SharedImageBackingFactory(kSupportedUsage) {}

DCompImageBackingFactory::~DCompImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking> DCompImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);

  // DXGI only supports a handful of formats for scan-out, so we map the
  // requested format to a supported compatible DXGI format.
  DXGI_FORMAT internal_format = gfx::ColorSpaceWin::GetDXGIFormat(color_space);

  if (usage & SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE) {
    DCHECK_NE(internal_format, DXGI_FORMAT_R10G10B10A2_UNORM);
    return DCompSurfaceImageBacking::Create(mailbox, format, internal_format,
                                            size, color_space, surface_origin,
                                            alpha_type, usage);
  } else {
    return DXGISwapChainImageBacking::Create(mailbox, format, internal_format,
                                             size, color_space, surface_origin,
                                             alpha_type, usage);
  }
}

std::unique_ptr<SharedImageBacking> DCompImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    base::span<const uint8_t> pixel_data) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking> DCompImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label) {
  NOTREACHED();
  return nullptr;
}

bool DCompImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane()) {
    return false;
  }

  // TODO(tangm): Allow write-only swap chain usage?
  bool is_usage_valid =
      usage == kDXGISwapChainUsage || usage == kDCompSurfaceUsage;
  if (!is_usage_valid) {
    return false;
  }

  if (!IsFormatSupportedForScanout(format)) {
    return false;
  }

  // IDCompositionDevice2::CreateSurface does not support rgb10. In cases where
  // dc overlays are to be used for rgb10, the caller should use swap chains
  // instead.
  if (usage == kDCompSurfaceUsage &&
      format == viz::SinglePlaneFormat::kRGBA_1010102) {
    return false;
  }

  if (thread_safe) {
    return false;
  }

  if (gmb_type != gfx::GpuMemoryBufferType::EMPTY_BUFFER) {
    return false;
  }

  if (!pixel_data.empty()) {
    return false;
  }

  return true;
}

}  // namespace gpu
