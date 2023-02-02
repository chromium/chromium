// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace {

viz::ResourceFormat GetPlaneFormat(viz::SharedImageFormat format,
                                   int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  if (format.is_single_plane()) {
    return format.resource_format();
  }

  if (format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR) {
    return plane_index == 0 ? viz::ResourceFormat::RED_8
                            : viz::ResourceFormat::RG_88;
  } else if (format == viz::MultiPlaneFormat::kYVU_420) {
    return viz::ResourceFormat::RED_8;
  }

  NOTREACHED();
  return viz::ResourceFormat::RGBA_8888;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageBacking

bool GLTextureImageBacking::SupportsPixelReadbackWithFormat(
    viz::SharedImageFormat format) {
  if (format.is_multi_plane()) {
    if (format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR ||
        format == viz::MultiPlaneFormat::kYVU_420) {
      return true;
    }
    return false;
  }

  switch (format.resource_format()) {
    case viz::ResourceFormat::RGBA_8888:
    case viz::ResourceFormat::BGRA_8888:
    case viz::ResourceFormat::RED_8:
    case viz::ResourceFormat::RG_88:
    case viz::ResourceFormat::RGBX_8888:
    case viz::ResourceFormat::BGRX_8888:
      return true;
    default:
      return false;
  }
}

bool GLTextureImageBacking::SupportsPixelUploadWithFormat(
    viz::SharedImageFormat format) {
  if (format.is_multi_plane()) {
    if (format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR ||
        format == viz::MultiPlaneFormat::kYVU_420) {
      return true;
    }
    return false;
  }

  switch (format.resource_format()) {
    case viz::ResourceFormat::RGBA_8888:
    case viz::ResourceFormat::RGBA_4444:
    case viz::ResourceFormat::BGRA_8888:
    case viz::ResourceFormat::RED_8:
    case viz::ResourceFormat::RG_88:
    case viz::ResourceFormat::RGBA_F16:
    case viz::ResourceFormat::R16_EXT:
    case viz::ResourceFormat::RG16_EXT:
    case viz::ResourceFormat::RGBX_8888:
    case viz::ResourceFormat::BGRX_8888:
    case viz::ResourceFormat::RGBA_1010102:
    case viz::ResourceFormat::BGRA_1010102:
      return true;
    case viz::ResourceFormat::ALPHA_8:
    case viz::ResourceFormat::LUMINANCE_8:
    case viz::ResourceFormat::RGB_565:
    case viz::ResourceFormat::BGR_565:
    case viz::ResourceFormat::ETC1:
    case viz::ResourceFormat::LUMINANCE_F16:
    case viz::ResourceFormat::YVU_420:
    case viz::ResourceFormat::YUV_420_BIPLANAR:
    case viz::ResourceFormat::YUVA_420_TRIPLANAR:
    case viz::ResourceFormat::P010:
      return false;
  }
}

GLTextureImageBacking::GLTextureImageBacking(const Mailbox& mailbox,
                                             viz::SharedImageFormat format,
                                             const gfx::Size& size,
                                             const gfx::ColorSpace& color_space,
                                             GrSurfaceOrigin surface_origin,
                                             SkAlphaType alpha_type,
                                             uint32_t usage,
                                             bool is_passthrough)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      format.EstimatedSizeInBytes(size),
                                      /*is_thread_safe=*/false),
      is_passthrough_(is_passthrough) {
  // With validating command decoder the clear rect tracking doesn't work with
  // multi-planar textures.
  DCHECK(is_passthrough_ || format.is_single_plane());
}

GLTextureImageBacking::~GLTextureImageBacking() {
  if (!have_context()) {
    for (auto& texture : textures_) {
      texture.SetContextLost();
    }
  }
}

SharedImageBackingType GLTextureImageBacking::GetType() const {
  return SharedImageBackingType::kGLTexture;
}

gfx::Rect GLTextureImageBacking::ClearedRect() const {
  if (!IsPassthrough()) {
    return textures_[0].GetClearedRect();
  }

  // Use shared image based tracking for passthrough, because we don't always
  // use angle robust initialization.
  return ClearTrackingSharedImageBacking::ClearedRect();
}

void GLTextureImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  if (!IsPassthrough()) {
    textures_[0].SetClearedRect(cleared_rect);
    return;
  }

  // Use shared image based tracking for passthrough, because we don't always
  // use angle robust initialization.
  ClearTrackingSharedImageBacking::SetClearedRect(cleared_rect);
}

void GLTextureImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {}

bool GLTextureImageBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(pixmaps.size(), textures_.size());
  DCHECK(SupportsPixelUploadWithFormat(format()));
  DCHECK(gl::GLContext::GetCurrent());

  for (size_t i = 0; i < textures_.size(); ++i) {
    if (!textures_[i].UploadFromMemory(pixmaps[i])) {
      return false;
    }
  }
  return true;
}

bool GLTextureImageBacking::ReadbackToMemory(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(pixmaps.size(), textures_.size());
  DCHECK(gl::GLContext::GetCurrent());

  // TODO(kylechar): Ideally there would be a usage that stated readback was
  // required so support could be verified at creation time and then asserted
  // here instead.
  if (!SupportsPixelReadbackWithFormat(format())) {
    return false;
  }

  for (size_t i = 0; i < textures_.size(); ++i) {
    if (!textures_[i].ReadbackToMemory(pixmaps[i])) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<GLTextureImageRepresentation>
GLTextureImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  std::vector<raw_ptr<gles2::Texture>> gl_textures;
  for (auto& texture : textures_) {
    DCHECK(texture.texture());
    gl_textures.push_back(texture.texture());
  }

  return std::make_unique<GLTextureGLCommonRepresentation>(
      manager, this, nullptr, tracker, std::move(gl_textures));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
GLTextureImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures;
  for (auto& texture : textures_) {
    DCHECK(texture.passthrough_texture());
    gl_textures.push_back(texture.passthrough_texture());
  }

  return std::make_unique<GLTexturePassthroughGLCommonRepresentation>(
      manager, this, nullptr, tracker, std::move(gl_textures));
}

std::unique_ptr<DawnImageRepresentation> GLTextureImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return GLTextureImageBackingHelper::ProduceDawnCommon(
      factory(), manager, tracker, device, backend_type,
      std::move(view_formats), this, IsPassthrough());
}

std::unique_ptr<SkiaImageRepresentation> GLTextureImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (cached_promise_textures_.empty()) {
    for (auto& texture : textures_) {
      cached_promise_textures_.push_back(
          texture.GetPromiseImage(context_state.get()));
    }
  }

  return std::make_unique<SkiaGLCommonRepresentation>(
      manager, this, nullptr, std::move(context_state),
      cached_promise_textures_, tracker);
}

void GLTextureImageBacking::InitializeGLTexture(
    const std::vector<GLCommonImageBackingFactory::FormatInfo>& format_info,
    base::span<const uint8_t> pixel_data,
    gl::ProgressReporter* progress_reporter,
    bool framebuffer_attachment_angle) {
  std::string debug_label;
  if (gl::g_current_gl_driver->ext.b_GL_KHR_debug) {
    debug_label =
        "SharedImage_GLTexture" + CreateLabelForSharedImageUsage(usage());
  }

  int num_planes = format().NumberOfPlanes();
  textures_.reserve(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    textures_.emplace_back(GetPlaneFormat(format(), plane),
                           format().GetPlaneSize(plane, size()),
                           is_passthrough_, progress_reporter);
    textures_[plane].Initialize(format_info[plane],
                                framebuffer_attachment_angle, pixel_data,
                                debug_label);
  }

  if (!pixel_data.empty()) {
    SetCleared();
  }
}

}  // namespace gpu
