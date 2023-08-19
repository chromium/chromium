// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/android/openxr_graphics_binding_open_gles.h"

#include <vector>

#include "base/containers/contains.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "device/vr/android/xr_image_transport_base.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_util.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/scoped_egl_image.h"

namespace device {

namespace {
int next_memory_buffer_id = 0;
}  // namespace

// static
void OpenXrGraphicsBinding::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
  extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
}

OpenXrGraphicsBindingOpenGLES::OpenXrGraphicsBindingOpenGLES() = default;
OpenXrGraphicsBindingOpenGLES::~OpenXrGraphicsBindingOpenGLES() = default;

bool OpenXrGraphicsBindingOpenGLES::Initialize(XrInstance instance,
                                               XrSystemId system) {
  if (initialized_) {
    return true;
  }

  PFN_xrGetOpenGLESGraphicsRequirementsKHR get_graphics_requirements_fn{
      nullptr};
  if (XR_FAILED(xrGetInstanceProcAddr(
          instance, "xrGetOpenGLESGraphicsRequirementsKHR",
          (PFN_xrVoidFunction*)(&get_graphics_requirements_fn)))) {
    return false;
  }

  // TODO(alcooper): Validate/set version based on the output here.
  XrGraphicsRequirementsOpenGLESKHR graphics_requirements = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
  if (XR_FAILED(get_graphics_requirements_fn(instance, system,
                                             &graphics_requirements))) {
    return false;
  }

  // None of the other runtimes support ANGLE, so we disable it too for now.
  // TODO(alcooper): Investigate if we can support ANGLE or if we'll run into
  // similar problems as cardboard.
  gl::init::DisableANGLE();

  // Everything below is a hacky first pass at making a session and likely needs
  // to be re-written with proper context/surfaces/types.
  gl::GLDisplay* display = nullptr;
  if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
    display = gl::init::InitializeGLOneOff(
        /*gpu_preference=*/gl::GpuPreference::kDefault);
    if (!display) {
      DLOG(ERROR) << "gl::init::InitializeGLOneOff failed";
      return false;
    }
  }
  display = gl::GetDefaultDisplayEGL();

  DCHECK(gl::GetGLImplementation() != gl::kGLImplementationEGLANGLE);

  scoped_refptr<gl::GLSurface> surface;
  surface = gl::init::CreateOffscreenGLSurfaceWithFormat(display, {0, 0},
                                                         gl::GLSurfaceFormat());

  DVLOG(3) << "surface=" << surface.get();
  if (!surface.get()) {
    DLOG(ERROR) << "gl::init::CreateViewGLSurface failed";
    return false;
  }

  gl::GLContextAttribs context_attribs;
  // OpenXr's shared EGL context needs to be compatible with ours.
  // Any mismatches result in a EGL_BAD_MATCH error, including different reset
  // notification behavior according to
  // https://www.khronos.org/registry/EGL/specs/eglspec.1.5.pdf page 56.
  // Chromium defaults to lose context on reset when the robustness extension is
  // present, even if robustness features are not requested specifically.
  context_attribs.lose_context_on_reset = false;

  scoped_refptr<gl::GLContextEGL> egl_context = new gl::GLContextEGL(nullptr);
  scoped_refptr<gl::GLContext> context = gl::InitializeGLContext(
      egl_context.get(), surface.get(), context_attribs);
  if (!context.get()) {
    DLOG(ERROR) << "gl::init::CreateGLContext failed";
    return false;
  }
  if (!context->MakeCurrent(surface.get())) {
    DLOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }

  // Assign the surface and context members now that initialization has
  // succeeded.
  surface_ = std::move(surface);
  context_ = std::move(context);
  egl_context_ = std::move(egl_context);

  binding_.display = display->GetDisplay();
  binding_.config = (EGLConfig)0;
  binding_.context = egl_context_->GetHandle();

  renderer_ = std::make_unique<XrRenderer>();

  initialized_ = true;
  return true;
}

const void* OpenXrGraphicsBindingOpenGLES::GetSessionCreateInfo() const {
  CHECK(initialized_);
  return &binding_;
}

int64_t OpenXrGraphicsBindingOpenGLES::GetSwapchainFormat(
    XrSession session) const {
  uint32_t format_length = 0;
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainFormats(session, 0, &format_length, nullptr));
  std::vector<int64_t> swapchain_formats(format_length);
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainFormats(session, (uint32_t)swapchain_formats.size(),
                                  &format_length, swapchain_formats.data()));
  DCHECK(!swapchain_formats.empty());

  // If the runtime doesn't support the swapchain format we need, log it. We'll
  // still return it anyway as that will cause an error with creating the
  // swapchain, which is better than arbitrarily returning a type that the
  // runtime supports, but we don't.
  if (!base::Contains(swapchain_formats, GL_RGBA8)) {
    LOG(ERROR) << "No matching supported swapchain formats with OpenXr Runtime";
  }

  return GL_RGBA8;
}

XrResult OpenXrGraphicsBindingOpenGLES::EnumerateSwapchainImages(
    const XrSwapchain& color_swapchain) {
  CHECK(color_swapchain != XR_NULL_HANDLE);
  CHECK(color_swapchain_images_.empty());

  uint32_t chain_length;
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainImages(color_swapchain, 0, &chain_length, nullptr));
  std::vector<XrSwapchainImageOpenGLESKHR> xr_color_swapchain_images(
      chain_length, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});

  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(
      color_swapchain, xr_color_swapchain_images.size(), &chain_length,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(
          xr_color_swapchain_images.data())));

  color_swapchain_images_.reserve(xr_color_swapchain_images.size());
  for (const auto& swapchain_image : xr_color_swapchain_images) {
    color_swapchain_images_.emplace_back(swapchain_image.image);
  }

  return XR_SUCCESS;
}

void OpenXrGraphicsBindingOpenGLES::ClearSwapChainImages() {
  color_swapchain_images_.clear();
}

base::span<SwapChainInfo> OpenXrGraphicsBindingOpenGLES::GetSwapChainImages() {
  return color_swapchain_images_;
}

bool OpenXrGraphicsBindingOpenGLES::CanUseSharedImages() const {
  return XrImageTransportBase::UseSharedBuffer();
}

// This is more or less copied from XrImageTransportBase::ResizeSharedBuffer,
// with just the types changed as needed, and logic extracted out of the
// mailbox_to_surface_bridge.
void OpenXrGraphicsBindingOpenGLES::ResizeSharedBuffer(
    SwapChainInfo& swap_chain_info,
    gpu::SharedImageInterface* sii) {
  CHECK(sii);
  auto transfer_size = GetTransferSize();
  if (!using_shared_images_ ||
      swap_chain_info.shared_buffer_size == transfer_size) {
    return;
  }

  // Unbind previous image (if any).
  if (!swap_chain_info.mailbox_holder.mailbox.IsZero()) {
    DVLOG(2) << ": DestroySharedImage, mailbox="
             << swap_chain_info.mailbox_holder.mailbox.ToDebugString();
    // Note: the sync token in mailbox_holder may not be accurate. See comment
    // in TransferFrame below.
    sii->DestroySharedImage(swap_chain_info.mailbox_holder.sync_token,
                            swap_chain_info.mailbox_holder.mailbox);
  }

  // Remove reference to previous image (if any).
  swap_chain_info.local_eglimage.reset();

  static constexpr gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  static constexpr gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  glGenTextures(1, &swap_chain_info.shared_buffer_texture);

  gfx::GpuMemoryBufferId kBufferId(next_memory_buffer_id++);
  swap_chain_info.gmb = gpu::GpuMemoryBufferImplAndroidHardwareBuffer::Create(
      kBufferId, transfer_size, format, usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback());
  swap_chain_info.shared_buffer_size = transfer_size;

  uint32_t shared_image_usage = gpu::SHARED_IMAGE_USAGE_SCANOUT |
                                gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                gpu::SHARED_IMAGE_USAGE_GLES2;

  swap_chain_info.mailbox_holder.mailbox = sii->CreateSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, swap_chain_info.gmb->GetSize(),
      gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      shared_image_usage, "OpenXrGraphicsBinding",
      swap_chain_info.gmb->CloneHandle());
  swap_chain_info.mailbox_holder.sync_token = sii->GenVerifiedSyncToken();
  DCHECK(!gpu::NativeBufferNeedsPlatformSpecificTextureTarget(
      swap_chain_info.gmb->GetFormat()));
  swap_chain_info.mailbox_holder.texture_target = GL_TEXTURE_2D;

  DVLOG(2) << ": CreateSharedImage, mailbox="
           << swap_chain_info.mailbox_holder.mailbox.ToDebugString()
           << ", SyncToken="
           << swap_chain_info.mailbox_holder.sync_token.ToDebugString();

  base::android::ScopedHardwareBufferHandle ahb =
      swap_chain_info.gmb->CloneHandle().android_hardware_buffer;

  // Create an EGLImage for the buffer.
  auto egl_image = gpu::CreateEGLImageFromAHardwareBuffer(ahb.get());
  if (!egl_image.is_valid()) {
    DLOG(WARNING) << __func__ << ": ERROR: failed to initialize image!";
    return;
  }

  glBindTexture(GL_TEXTURE_EXTERNAL_OES, swap_chain_info.shared_buffer_texture);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image.get());
  swap_chain_info.local_eglimage = std::move(egl_image);
}

void OpenXrGraphicsBindingOpenGLES::CreateSharedImages(
    gpu::SharedImageInterface* sii) {
  CHECK(sii);
  using_shared_images_ = true;

  for (auto& swap_chain_info : color_swapchain_images_) {
    // ResizeSharedBuffer will also cause the shared buffer to be recreated.
    ResizeSharedBuffer(swap_chain_info, sii);
  }
}

const SwapChainInfo& OpenXrGraphicsBindingOpenGLES::GetActiveSwapchainImage() {
  CHECK(has_active_swapchain_image());
  CHECK(active_swapchain_index() < color_swapchain_images_.size());

  // We don't do any index translation on the images returned from the system;
  // so whatever the system says is the active swapchain image, it is in the
  // same spot in our vector.
  return color_swapchain_images_[active_swapchain_index()];
}

bool OpenXrGraphicsBindingOpenGLES::Render() {
  if (!has_active_swapchain_image() ||
      active_swapchain_index() >= color_swapchain_images_.size()) {
    return false;
  }

  // We don't do any index translation on the images returned from the system;
  // so whatever the system says is the active swapchain image, it is in the
  // same spot in our vector.
  auto& swap_chain_info = color_swapchain_images_[active_swapchain_index()];

  gfx::Size swapchain_image_size = GetSwapchainImageSize();

  if (!back_buffer_fbo_) {
    glGenFramebuffersEXT(1, &back_buffer_fbo_);
  }
  glBindFramebufferEXT(GL_FRAMEBUFFER, back_buffer_fbo_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            swap_chain_info.openxr_texture, 0);

  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glViewport(0, 0, swapchain_image_size.width(), swapchain_image_size.height());

  gfx::Transform transform;
  float transform_floats[16];
  transform.GetColMajorF(transform_floats);
  renderer_->Draw(swap_chain_info.shared_buffer_texture, transform_floats);

  return true;
}

bool OpenXrGraphicsBindingOpenGLES::WaitOnFence(gfx::GpuFence& gpu_fence) {
  std::unique_ptr<gl::GLFence> local_fence =
      gl::GLFence::CreateFromGpuFence(gpu_fence);
  local_fence->ServerWait();

  return true;
}

void OpenXrGraphicsBindingOpenGLES::OnSwapchainImageActivated(
    gpu::SharedImageInterface* sii) {
  CHECK(has_active_swapchain_image());
  CHECK(active_swapchain_index() < color_swapchain_images_.size());
  ResizeSharedBuffer(color_swapchain_images_[active_swapchain_index()], sii);
}

}  // namespace device
