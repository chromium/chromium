// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/android/openxr_graphics_binding_open_gles.h"

#include <vector>

#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_util.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace device {

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
  // TODO(alcooper): Care about the swapchain format that we pick.
  return swapchain_formats[0];
}

XrResult OpenXrGraphicsBindingOpenGLES::EnumerateSwapchainImages(
    const XrSwapchain& color_swapchain,
    std::vector<SwapChainInfo>& color_swapchain_images) const {
  CHECK(color_swapchain != XR_NULL_HANDLE);
  CHECK(color_swapchain_images.empty());

  uint32_t chain_length;
  RETURN_IF_XR_FAILED(
      xrEnumerateSwapchainImages(color_swapchain, 0, &chain_length, nullptr));
  std::vector<XrSwapchainImageOpenGLESKHR> xr_color_swapchain_images(
      chain_length, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});

  RETURN_IF_XR_FAILED(xrEnumerateSwapchainImages(
      color_swapchain, xr_color_swapchain_images.size(), &chain_length,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(
          xr_color_swapchain_images.data())));

  color_swapchain_images.reserve(xr_color_swapchain_images.size());
  for (size_t i = 0; i < xr_color_swapchain_images.size(); i++) {
    color_swapchain_images.emplace_back();
  }

  return XR_SUCCESS;
}

}  // namespace device
