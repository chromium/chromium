// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

#if BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/shared_image/dawn_fallback_image_representation.h"
#endif

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image/dawn_gl_texture_representation.h"
#endif

namespace gpu {

namespace {

// Representation of a GLTextureImageBacking as a GL Texture.
class GLTextureImageRepresentationImpl : public GLTextureImageRepresentation {
 public:
  GLTextureImageRepresentationImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<raw_ptr<gles2::Texture>> textures)
      : GLTextureImageRepresentation(manager, backing, tracker),
        textures_(std::move(textures)) {
    DCHECK_EQ(textures_.size(), NumPlanesExpected());
  }

  ~GLTextureImageRepresentationImpl() override = default;

 private:
  // GLTextureImageRepresentation:
  gles2::Texture* GetTexture(int plane_index) override {
    DCHECK(format().IsValidPlaneIndex(plane_index));
    return textures_[plane_index];
  }
  bool BeginAccess(GLenum mode) override { return true; }
  void EndAccess() override {}

  std::vector<raw_ptr<gles2::Texture>> textures_;
};

// Representation of a GLTextureImageBacking as a GLTexturePassthrough.
class GLTexturePassthroughImageRepresentationImpl
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughImageRepresentationImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<scoped_refptr<gles2::TexturePassthrough>> textures)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        textures_(std::move(textures)) {
    DCHECK_EQ(textures_.size(), NumPlanesExpected());
  }

  ~GLTexturePassthroughImageRepresentationImpl() override = default;

 private:
  // GLTexturePassthroughImageRepresentation:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK(format().IsValidPlaneIndex(plane_index));
    return textures_[plane_index];
  }
  bool BeginAccess(GLenum mode) override { return true; }
  void EndAccess() override {}

  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_;
};

// Skia representation.
class SkiaGaneshImageRepresentationImpl : public SkiaGaneshImageRepresentation {
 public:
  SkiaGaneshImageRepresentationImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      scoped_refptr<SharedContextState> context_state,
      std::vector<sk_sp<GrPromiseImageTexture>> promise_textures,
      MemoryTypeTracker* tracker)
      : SkiaGaneshImageRepresentation(context_state->gr_context(),
                                      manager,
                                      backing,
                                      tracker),
        context_state_(std::move(context_state)),
        promise_textures_(std::move(promise_textures)) {
    DCHECK_EQ(promise_textures_.size(), NumPlanesExpected());
#if DCHECK_IS_ON()
    if (context_state_->GrContextIsGL()) {
      context_ = gl::GLContext::GetCurrent();
    }
#endif
  }
  ~SkiaGaneshImageRepresentationImpl() override {
    DCHECK(write_surfaces_.empty());
  }

 private:
  // SkiaImageRepresentation:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    CheckContext();

    if (!write_surfaces_.empty()) {
      // Write access is already in progress.
      return {};
    }

    for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
      SkColorType sk_color_type = viz::ToClosestSkColorType(
          /*gpu_compositing=*/true, format(), plane);
      // Gray is not a renderable single channel format, but alpha is.
      if (sk_color_type == kGray_8_SkColorType) {
        sk_color_type = kAlpha_8_SkColorType;
      }
      auto surface = SkSurfaces::WrapBackendTexture(
          context_state_->gr_context(),
          promise_textures_[plane]->backendTexture(), surface_origin(),
          final_msaa_count, sk_color_type,
          backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
          &surface_props);
      if (!surface) {
        write_surfaces_.clear();
        return {};
      }
      write_surfaces_.push_back(std::move(surface));
    }

    return write_surfaces_;
  }

  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphore,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    CheckContext();

    return promise_textures_;
  }

  void EndWriteAccess() override {
    if (!write_surfaces_.empty()) {
#if DCHECK_IS_ON()
      for (auto& write_surface : write_surfaces_) {
        DCHECK(write_surface->unique());
      }
      CheckContext();
#endif
      write_surfaces_.clear();
    }
  }
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    CheckContext();
    return promise_textures_;
  }

  void EndReadAccess() override {}

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

  void CheckContext() {
#if DCHECK_IS_ON()
    if (!context_state_->context_lost() && context_) {
      DCHECK(gl::GLContext::GetCurrent() == context_);
    }
#endif
  }

  scoped_refptr<SharedContextState> context_state_;
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures_;
  std::vector<sk_sp<SkSurface>> write_surfaces_;
#if DCHECK_IS_ON()
  raw_ptr<gl::GLContext> context_ = nullptr;
#endif
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageBacking

bool GLTextureImageBacking::SupportsPixelReadbackWithFormat(
    viz::SharedImageFormat format) {
  // NOTE: Using MultiPlaneFormats is okay here are this is only used with
  // SharedMemory GMBs which correspond to specific multiplanar formats.
  return (format == viz::MultiPlaneFormat::kNV12 ||
          format == viz::MultiPlaneFormat::kYV12 ||
          format == viz::MultiPlaneFormat::kI420 ||
          format == viz::SinglePlaneFormat::kRGBA_8888 ||
          format == viz::SinglePlaneFormat::kBGRA_8888 ||
          format == viz::SinglePlaneFormat::kR_8 ||
          format == viz::SinglePlaneFormat::kRG_88 ||
          format == viz::SinglePlaneFormat::kRGBX_8888 ||
          format == viz::SinglePlaneFormat::kBGRX_8888);
}

bool GLTextureImageBacking::SupportsPixelUploadWithFormat(
    viz::SharedImageFormat format) {
  // NOTE: Using MultiPlaneFormats is okay here are this is only used with
  // SharedMemory GMBs which correspond to specific multiplanar formats.
  return (format == viz::MultiPlaneFormat::kNV12 ||
          format == viz::MultiPlaneFormat::kYV12 ||
          format == viz::MultiPlaneFormat::kI420 ||
          format == viz::SinglePlaneFormat::kRGBA_8888 ||
          format == viz::SinglePlaneFormat::kRGBA_4444 ||
          format == viz::SinglePlaneFormat::kBGRA_8888 ||
          format == viz::SinglePlaneFormat::kR_8 ||
          format == viz::SinglePlaneFormat::kRG_88 ||
          format == viz::SinglePlaneFormat::kRGBA_F16 ||
          format == viz::SinglePlaneFormat::kR_16 ||
          format == viz::SinglePlaneFormat::kRG_1616 ||
          format == viz::SinglePlaneFormat::kRGBX_8888 ||
          format == viz::SinglePlaneFormat::kBGRX_8888 ||
          format == viz::SinglePlaneFormat::kRGBA_1010102 ||
          format == viz::SinglePlaneFormat::kBGRA_1010102);
}

GLTextureImageBacking::GLTextureImageBacking(const Mailbox& mailbox,
                                             viz::SharedImageFormat format,
                                             const gfx::Size& size,
                                             const gfx::ColorSpace& color_space,
                                             GrSurfaceOrigin surface_origin,
                                             SkAlphaType alpha_type,
                                             SharedImageUsageSet usage,
                                             std::string debug_label,
                                             bool is_passthrough)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      std::move(debug_label),
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

  return std::make_unique<GLTextureImageRepresentationImpl>(
      manager, this, tracker, std::move(gl_textures));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
GLTextureImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures;
  for (auto& texture : textures_) {
    DCHECK(texture.passthrough_texture());
    gl_textures.push_back(texture.passthrough_texture());
  }

  return std::make_unique<GLTexturePassthroughImageRepresentationImpl>(
      manager, this, tracker, std::move(gl_textures));
}

std::unique_ptr<DawnImageRepresentation> GLTextureImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == wgpu::BackendType::OpenGLES) {
    std::unique_ptr<GLTextureImageRepresentationBase> image;
    if (IsPassthrough()) {
      image = ProduceGLTexturePassthrough(manager, tracker);
    } else {
      image = ProduceGLTexture(manager, tracker);
    }
    auto result = std::make_unique<DawnGLTextureRepresentation>(
        std::move(image), manager, this, tracker, device);
    return result;
  }
#endif

  // TODO (crbug.com/1434885) - Delete this code path if it's not used.
  // Otherwise optimize this path with a GPU copy.
  SCOPED_CRASH_KEY_STRING256("", "GLSharedImage_DebugLabel", debug_label());
  SCOPED_CRASH_KEY_STRING32("", "GLSharedImage_Usage",
                            base::NumberToString(uint32_t(usage())));
  base::debug::DumpWithoutCrashing();

#if BUILDFLAG(USE_DAWN)
  // This is a slow path with a GPU<=>CPU<=>GPU copy.
  return std::make_unique<DawnFallbackImageRepresentation>(
      manager, this, tracker, device, ToDawnFormat(format()),
      std::move(view_formats));
#else
  return nullptr;
#endif
}

std::unique_ptr<SkiaGaneshImageRepresentation>
GLTextureImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (cached_promise_textures_.empty()) {
    for (auto& texture : textures_) {
      cached_promise_textures_.push_back(
          texture.GetPromiseImage(context_state.get()));
    }
  }

  return std::make_unique<SkiaGaneshImageRepresentationImpl>(
      manager, this, std::move(context_state), cached_promise_textures_,
      tracker);
}

void GLTextureImageBacking::InitializeGLTexture(
    const std::vector<GLCommonImageBackingFactory::FormatInfo>& format_info,
    base::span<const uint8_t> pixel_data,
    gl::ProgressReporter* progress_reporter,
    bool framebuffer_attachment_angle) {
  const std::string debug_label =
      "GLSharedImage_" + SharedImageBacking::debug_label();
  int num_planes = format().NumberOfPlanes();
  textures_.reserve(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    auto plane_format = GLTextureHolder::GetPlaneFormat(format(), plane);
    textures_.emplace_back(plane_format, format().GetPlaneSize(plane, size()),
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
