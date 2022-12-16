// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dcomp_surface_image_backing.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/EGL/eglext_angle.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {

// An EGL surface that can temporarily encapsulate a D3D texture (and ANGLE
// draw offset). This is used so Skia can draw into the draw texture returned
// by IDCompositionSurface::BeginDraw.
class DCompSurfaceImageBacking::D3DTextureGLSurfaceEGL
    : public gl::GLSurfaceEGL {
 public:
  D3DTextureGLSurfaceEGL(gl::GLDisplayEGL* display, const gfx::Size& size)
      : GLSurfaceEGL(display), size_(size) {}

  D3DTextureGLSurfaceEGL(const D3DTextureGLSurfaceEGL&) = delete;
  D3DTextureGLSurfaceEGL& operator=(const D3DTextureGLSurfaceEGL&) = delete;

  // Implement GLSurface.
  bool Initialize(gl::GLSurfaceFormat format) override {
    if (display_->GetDisplay() == EGL_NO_DISPLAY) {
      LOG(ERROR)
          << "Trying to create D3DTextureGLSurfaceEGL with invalid display.";
      return false;
    }

    if (size_.IsEmpty()) {
      LOG(ERROR) << "Trying to create D3DTextureGLSurfaceEGL with empty size.";
      return false;
    }

    return true;
  }

  void Destroy() override {
    if (surface_) {
      if (!eglDestroySurface(display_->GetDisplay(), surface_)) {
        LOG(ERROR) << "eglDestroySurface failed with error "
                   << ui::GetLastEGLErrorString();
      }
      surface_ = nullptr;
    }
  }

  bool IsOffscreen() override { return true; }

  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              gfx::FrameData data) override {
    NOTREACHED()
        << "Attempted to call SwapBuffers on a D3DTextureGLSurfaceEGL.";
    return gfx::SwapResult::SWAP_FAILED;
  }

  gfx::Size GetSize() override { return size_; }

  EGLSurface GetHandle() override { return surface_; }

  // Bind a texture to a pbuffer and use the resulting surface as this EGL
  // surface. The offset will be set as the EGL_TEXTURE_OFFSET_{X,Y}_ANGLE
  // pbuffer attributes. Call |Destroy| to destroy and un-set the pbuffer
  // surface.
  bool BindTextureToSurface(ID3D11Texture2D* texture,
                            const gfx::Vector2d& draw_offset) {
    DCHECK(!surface_);
    DCHECK(texture);

    std::vector<EGLint> pbuffer_attribs;
    pbuffer_attribs.push_back(EGL_WIDTH);
    pbuffer_attribs.push_back(size_.width());
    pbuffer_attribs.push_back(EGL_HEIGHT);
    pbuffer_attribs.push_back(size_.height());
    pbuffer_attribs.push_back(EGL_TEXTURE_OFFSET_X_ANGLE);
    pbuffer_attribs.push_back(draw_offset.x());
    pbuffer_attribs.push_back(EGL_TEXTURE_OFFSET_Y_ANGLE);
    pbuffer_attribs.push_back(draw_offset.y());
    pbuffer_attribs.push_back(EGL_NONE);

    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(texture);

    surface_ = eglCreatePbufferFromClientBuffer(
        display_->GetDisplay(), EGL_D3D_TEXTURE_ANGLE, buffer, GetConfig(),
        pbuffer_attribs.data());
    if (!surface_) {
      LOG(ERROR) << "eglCreatePbufferFromClientBuffer failed with error "
                 << ui::GetLastEGLErrorString();
      return false;
    }

    return true;
  }

 protected:
  ~D3DTextureGLSurfaceEGL() override { Destroy(); }

 private:
  gfx::Size size_;
  EGLSurface surface_ = nullptr;
};

// static
std::unique_ptr<DCompSurfaceImageBacking> DCompSurfaceImageBacking::Create(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    DXGI_FORMAT internal_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  // IDCompositionSurface only supports the following formats:
  // https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondevice2-createsurface#remarks
  DCHECK(internal_format == DXGI_FORMAT_B8G8R8A8_UNORM ||
         internal_format == DXGI_FORMAT_R8G8B8A8_UNORM ||
         internal_format == DXGI_FORMAT_R16G16B16A16_FLOAT)
      << "Incompatible DXGI_FORMAT = " << internal_format;

  TRACE_EVENT2("gpu", "DCompSurfaceImageBacking::Create", "width", size.width(),
               "height", size.height());
  // Always treat as premultiplied, because an underlay could cause it to
  // become transparent.
  Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface;
  HRESULT hr = gl::GetDirectCompositionDevice()->CreateSurface(
      size.width(), size.height(), internal_format,
      SkAlphaTypeIsOpaque(alpha_type) ? DXGI_ALPHA_MODE_IGNORE
                                      : DXGI_ALPHA_MODE_PREMULTIPLIED,
      &dcomp_surface);
  base::UmaHistogramSparse("GPU.DirectComposition.DcompDeviceCreateSurface",
                           hr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateSurface failed: "
                << logging::SystemErrorCodeToString(hr);

    // Disable direct composition because CreateSurface might fail again next
    // time.
    gl::SetDirectCompositionSwapChainFailed();
    return nullptr;
  }

  return base::WrapUnique(new DCompSurfaceImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(dcomp_surface)));
}

DCompSurfaceImageBacking::DCompSurfaceImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface)
    : ClearTrackingSharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          gfx::BufferSizeForBufferFormat(size, ToBufferFormat(format)),
          false /* is_thread_safe */),
      gl_surface_(scoped_refptr(
          new D3DTextureGLSurfaceEGL(gl::GLSurfaceEGL::GetGLDisplayEGL(),
                                     size))),
      dcomp_surface_(std::move(dcomp_surface)) {
  const bool has_scanout = !!(usage & SHARED_IMAGE_USAGE_SCANOUT);
  const bool write_only = !(usage & SHARED_IMAGE_USAGE_DISPLAY_READ) &&
                          !!(usage & SHARED_IMAGE_USAGE_DISPLAY_WRITE);
  DCHECK(has_scanout);
  DCHECK(write_only);
  DCHECK(dcomp_surface_);

  bool success = gl_surface_->Initialize(gl::GLSurfaceFormat());
  DCHECK(success);
}

DCompSurfaceImageBacking::~DCompSurfaceImageBacking() = default;

SharedImageBackingType DCompSurfaceImageBacking::GetType() const {
  return SharedImageBackingType::kDCompSurface;
}

void DCompSurfaceImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

std::unique_ptr<OverlayImageRepresentation>
DCompSurfaceImageBacking::ProduceOverlay(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  return std::make_unique<DCompSurfaceOverlayImageRepresentation>(manager, this,
                                                                  tracker);
}

std::unique_ptr<SkiaImageRepresentation> DCompSurfaceImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return std::make_unique<DCompSurfaceSkiaImageRepresentation>(
      std::move(context_state), manager, this, tracker);
}

sk_sp<SkSurface> DCompSurfaceImageBacking::BeginDraw(
    SharedContextState* context_state,
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect) {
  if (update_rect.IsEmpty()) {
    DLOG(ERROR) << "Draw rectangle must be non-empty";
    return nullptr;
  }

  if (!gfx::Rect(size()).Contains(update_rect)) {
    DLOG(ERROR) << "Draw rectangle must be contained within size of surface";
    return nullptr;
  }

  if (!IsCleared() && gfx::Rect(size()) != update_rect) {
    LOG(ERROR) << "First draw to surface must draw to everything";
    return nullptr;
  }

  TRACE_EVENT0("gpu", "DCompSurfaceImageBacking::BeginDraw");
  const RECT rect = update_rect.ToRECT();
  Microsoft::WRL::ComPtr<ID3D11Texture2D> draw_texture;
  POINT update_offset = {};
  HRESULT hr = dcomp_surface_->BeginDraw(&rect, IID_PPV_ARGS(&draw_texture),
                                         &update_offset);
  hr = dcomp_surface_->BeginDraw(&rect, IID_PPV_ARGS(&draw_texture),
                                 &update_offset);
  if (FAILED(hr)) {
    DCHECK_NE(hr, DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED)
        << "Concurrent writes to multiple DCompSurfaceImageBacking "
           "not allowed.";

    LOG(ERROR) << "BeginDraw failed: " << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  // Offset the BeginDraw offset by the update rect's origin so we don't have to
  // change the coordinate space of our DDL.
  gfx::Vector2d draw_offset = gfx::Point(update_offset) - update_rect.origin();

  if (!gl_surface_->BindTextureToSurface(draw_texture.Get(), draw_offset)) {
    DLOG(ERROR) << "Could not bind texture to GLSurface";
    return nullptr;
  }

  // Bind the default framebuffer to a SkSurface

  scoped_make_current_.emplace(context_state->context(), gl_surface_.get());
  if (!scoped_make_current_->IsContextCurrent()) {
    DLOG(ERROR) << "Failed to make current in BeginDraw";
    return nullptr;
  }

  GrGLFramebufferInfo framebuffer_info = {0};
  DCHECK_EQ(gl_surface_->GetBackingFramebufferObject(), 0u);

  SkColorType color_type = viz::ToClosestSkColorType(
      /*gpu_compositing=*/true, format());
  switch (color_type) {
    case kRGBA_8888_SkColorType:
      framebuffer_info.fFormat = GL_RGBA8;
      break;
    case kRGB_888x_SkColorType:
      framebuffer_info.fFormat = GL_RGB8;
      break;
    case kRGB_565_SkColorType:
      framebuffer_info.fFormat = GL_RGB565;
      break;
    case kRGBA_1010102_SkColorType:
      framebuffer_info.fFormat = GL_RGB10_A2_EXT;
      break;
    case kRGBA_F16_SkColorType:
      framebuffer_info.fFormat = GL_RGBA16F;
      break;
    case kBGRA_8888_SkColorType:
      framebuffer_info.fFormat = GL_BGRA8_EXT;
      break;
    default:
      NOTREACHED() << "color_type: " << color_type;
  }

  GrBackendRenderTarget render_target(size().width(), size().height(),
                                      final_msaa_count, 0, framebuffer_info);
  auto surface = SkSurface::MakeFromBackendRenderTarget(
      context_state->gr_context(), render_target, surface_origin(), color_type,
      color_space().ToSkColorSpace(
          // TODO(crbug/1385874): Read SDR white level from current frame
          gfx::ColorSpace::kDefaultSDRWhiteLevel),
      &surface_props);
  DCHECK(surface);

  return surface;
}

bool DCompSurfaceImageBacking::EndDraw() {
  TRACE_EVENT0("gpu", "DCompSurfaceImageBacking::EndDraw");

  DCHECK_EQ(gl::GLSurface::GetCurrent(), gl_surface_);
  gl_surface_->Destroy();

  scoped_make_current_.reset();

  HRESULT hr = dcomp_surface_->EndDraw();
  if (FAILED(hr)) {
    DLOG(ERROR) << "EndDraw failed: " << logging::SystemErrorCodeToString(hr);
    return false;
  }

  dcomp_surface_serial_++;

  return true;
}

}  // namespace gpu
