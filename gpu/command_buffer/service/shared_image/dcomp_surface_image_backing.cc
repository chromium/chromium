// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dcomp_surface_image_backing.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/EGL/eglext_angle.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gl/debug_utils.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"
#endif

namespace gpu {

namespace {

// Ensure that the full bounds of |surface| have been drawn to, so subsequent
// |BeginDraw| calls are not required to cover the entire surface.
bool InitializeDCompSurface(IDCompositionSurface* surface,
                            const gfx::Size& surface_size) {
  HRESULT hr = S_OK;

  RECT rect = gfx::Rect(surface_size).ToRECT();
  Microsoft::WRL::ComPtr<ID3D11Texture2D> draw_texture;
  POINT update_offset = {};
  hr = surface->BeginDraw(&rect, IID_PPV_ARGS(&draw_texture), &update_offset);
  if (FAILED(hr)) {
    LOG(ERROR) << "BeginDraw failed: " << logging::SystemErrorCodeToString(hr);
    return false;
  }

#if DCHECK_IS_ON()
  const SkColor4f initialize_color = SkColors::kBlue;
#else
  const SkColor4f initialize_color = SkColors::kTransparent;
#endif

  // DX11 protects the DComp surface atlas so this clear only affects pixels in
  // the update rect.
  if (!ClearD3D11TextureToColor(draw_texture, initialize_color)) {
    return false;
  }

  hr = surface->EndDraw();
  if (FAILED(hr)) {
    LOG(ERROR) << "EndDraw failed: " << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return true;
}

}  // namespace

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
    NOTREACHED_IN_MIGRATION()
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
  ~D3DTextureGLSurfaceEGL() override {
    InvalidateWeakPtrs();
    Destroy();
  }

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
    gpu::SharedImageUsageSet usage,
    std::string debug_label) {
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
      std::move(debug_label), std::move(dcomp_surface)));
}

DCompSurfaceImageBacking::DCompSurfaceImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface)
    : ClearTrackingSharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          std::move(debug_label),
          gfx::BufferSizeForBufferFormat(size, ToBufferFormat(format)),
          /*is_thread_safe=*/false),
      gl_surface_(scoped_refptr(
          new D3DTextureGLSurfaceEGL(gl::GLSurfaceEGL::GetGLDisplayEGL(),
                                     size))),
      dcomp_surface_(std::move(dcomp_surface)) {
  const bool has_scanout = usage.Has(SHARED_IMAGE_USAGE_SCANOUT);
  const bool write_only = !usage.Has(SHARED_IMAGE_USAGE_DISPLAY_READ) &&
                          usage.Has(SHARED_IMAGE_USAGE_DISPLAY_WRITE);
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

std::unique_ptr<SkiaGaneshImageRepresentation>
DCompSurfaceImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK_EQ(context_state->gr_context_type(), GrContextType::kGL);
  return std::make_unique<DCompSurfaceSkiaGaneshImageRepresentation>(
      std::move(context_state), manager, this, tracker);
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
DCompSurfaceImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
#if BUILDFLAG(SKIA_USE_DAWN)
  DCHECK_EQ(context_state->gr_context_type(), GrContextType::kGraphiteDawn);

  auto device = context_state->dawn_context_provider()->GetDevice();
  auto dawn_representation =
      std::make_unique<DCompSurfaceDawnImageRepresentation>(
          manager, this, tracker, device, wgpu::BackendType::D3D11);
  return SkiaGraphiteDawnImageRepresentation::Create(
      std::move(dawn_representation), context_state,
      context_state->gpu_main_graphite_recorder(), manager, this, tracker);
#else
  NOTREACHED();
#endif  // BUILDFLAG(SKIA_USE_DAWN)
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> DCompSurfaceImageBacking::BeginDraw(
    const gfx::Rect& update_rect,
    gfx::Point& update_offset_out) {
  if (update_rect.IsEmpty()) {
    DLOG(ERROR) << "Draw rectangle must be non-empty";
    return nullptr;
  }

  if (!gfx::Rect(size()).Contains(update_rect)) {
    DLOG(ERROR) << "Draw rectangle must be contained within size of surface";
    return nullptr;
  }

  // SharedImage allows an incomplete first draw so long as we only read from
  // the part that we've previously drawn to. However, IDCompositionSurface
  // requires a full draw on the first |BeginDraw|. To make an incomplete first
  // draw valid, we'll initialize all the pixels and expand the swap rect.
  if (!IsCleared() && gfx::Rect(size()) != update_rect) {
    if (!InitializeDCompSurface(dcomp_surface_.Get(), size())) {
      LOG(ERROR) << "Could not initialize DComp surface";
      return nullptr;
    }
  }

  TRACE_EVENT0("gpu", "DCompSurfaceImageBacking::BeginDraw");
  const RECT rect = update_rect.ToRECT();
  Microsoft::WRL::ComPtr<ID3D11Texture2D> draw_texture;
  POINT update_offset = {};
  HRESULT hr = dcomp_surface_->BeginDraw(&rect, IID_PPV_ARGS(&draw_texture),
                                         &update_offset);
  if (FAILED(hr)) {
    DCHECK_NE(hr, DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED)
        << "Concurrent writes to multiple DCompSurfaceImageBacking "
           "not allowed.";

    LOG(ERROR) << "BeginDraw failed: " << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  update_offset_out = gfx::Point(update_offset.x, update_offset.y);
  return draw_texture;
}

void DCompSurfaceImageBacking::EndDraw() {
  TRACE_EVENT0("gpu", "DCompSurfaceImageBacking::EndDraw");

  HRESULT hr = dcomp_surface_->EndDraw();
  if (FAILED(hr)) {
    DLOG(ERROR) << "EndDraw failed: " << logging::SystemErrorCodeToString(hr);
    return;
  }

  dcomp_surface_serial_++;
}

sk_sp<SkSurface> DCompSurfaceImageBacking::BeginDrawGanesh(
    SharedContextState* context_state,
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect) {
  gfx::Point update_offset;
  auto draw_texture = BeginDraw(update_rect, update_offset);
  if (!draw_texture) {
    return nullptr;
  }

  // Offset the BeginDraw offset by the update rect's origin so we don't have to
  // change the coordinate space of our DDL.
  gfx::Vector2d draw_offset = update_offset - update_rect.origin();

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
      NOTREACHED_IN_MIGRATION() << "color_type: " << color_type;
  }

  auto render_target = GrBackendRenderTargets::MakeGL(
      size().width(), size().height(), final_msaa_count, 0, framebuffer_info);
  auto surface = SkSurfaces::WrapBackendRenderTarget(
      context_state->gr_context(), render_target, surface_origin(), color_type,
      color_space().ToSkColorSpace(), &surface_props);
  DCHECK(surface);

  return surface;
}

void DCompSurfaceImageBacking::EndDrawGanesh() {
  DCHECK_EQ(gl::GLSurface::GetCurrent(), gl_surface_);
  gl_surface_->Destroy();

  scoped_make_current_.reset();

  EndDraw();
}

wgpu::Texture DCompSurfaceImageBacking::BeginDrawDawn(
    const wgpu::Device& device,
    const wgpu::TextureUsage usage,
    const wgpu::TextureUsage internal_usage,
    const gfx::Rect& update_rect) {
  auto draw_texture = BeginDraw(update_rect, dcomp_update_offset_);

  if (!draw_texture) {
    return nullptr;
  }

  if (!dcomp_surface_draw_texture_copy_) {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    draw_texture->GetDevice(&d3d11_device);

    D3D11_TEXTURE2D_DESC d3d11_texture_desc;
    draw_texture->GetDesc(&d3d11_texture_desc);

    // Textures from DComp are guarded and not textureable. Graphite cannot
    // render to it. We need to create an intermediate texture with
    // D3D11_BIND_SHADER_RESOURCE and without D3D11_RESOURCE_MISC_GUARDED.
    // The EndDraw() will copy the content from the intermediate texture back.

    d3d11_texture_desc.Width = size().width();
    d3d11_texture_desc.Height = size().height();
    d3d11_texture_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    d3d11_texture_desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GUARDED;

    HRESULT hr = d3d11_device->CreateTexture2D(
        &d3d11_texture_desc, nullptr, &dcomp_surface_draw_texture_copy_);

    if (FAILED(hr)) {
      LOG(ERROR) << "CreateTexture2D failed: "
                 << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }

    const char kDebugLabel[] = "SharedImage_DCompSurfaceDrawTextureCopy";
    gl::SetDebugName(dcomp_surface_draw_texture_copy_.Get(), kDebugLabel);
  }

  dcomp_surface_draw_texture_ = std::move(draw_texture);
  update_rect_ = update_rect;

  // Import the texture into dawn

  DCHECK(!shared_texture_memory_);
  shared_texture_memory_ =
      CreateDawnSharedTextureMemory(device, dcomp_surface_draw_texture_copy_);
  if (!shared_texture_memory_) {
    LOG(ERROR) << "Failed to create shared texture memory.";
    return nullptr;
  }

  wgpu::SharedTextureMemoryD3DSwapchainBeginState swapchain_begin_state = {};
  swapchain_begin_state.isSwapchain = true;

  wgpu::SharedTextureMemoryBeginAccessDescriptor desc = {};
  desc.initialized = true;
  desc.nextInChain = &swapchain_begin_state;

  wgpu::Texture texture =
      CreateDawnSharedTexture(shared_texture_memory_, usage, internal_usage,
                              /*view_formats=*/{});
  if (!texture || shared_texture_memory_.BeginAccess(texture, &desc) !=
                      wgpu::Status::Success) {
    LOG(ERROR) << "Failed to begin access and produce WGPUTexture";
    return nullptr;
  }
  return texture;
}

void DCompSurfaceImageBacking::EndDrawDawn(const wgpu::Device& device,
                                           wgpu::Texture texture) {
  // We don't need any synchronization here because dawn and dcomp are using the
  // same d3d11 device.
  wgpu::SharedTextureMemoryEndAccessState end_state = {};
  shared_texture_memory_.EndAccess(texture.Get(), &end_state);
  shared_texture_memory_ = nullptr;
  texture.Destroy();

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  dcomp_surface_draw_texture_->GetDevice(&d3d11_device);
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device->GetImmediateContext(&d3d11_context);
  D3D11_BOX src_box = {
      static_cast<UINT>(update_rect_.x()),
      static_cast<UINT>(update_rect_.y()),
      0u,  // front
      static_cast<UINT>(update_rect_.right()),
      static_cast<UINT>(update_rect_.bottom()),
      1u,  // back
  };
  d3d11_context->CopySubresourceRegion(
      dcomp_surface_draw_texture_.Get(), /*DstSubresource=*/0,
      dcomp_update_offset_.x(), dcomp_update_offset_.y(), /*DstZ=*/0,
      dcomp_surface_draw_texture_copy_.Get(), /*SrcSubresource=*/0, &src_box);
  dcomp_surface_draw_texture_.Reset();

  EndDraw();
}

}  // namespace gpu
