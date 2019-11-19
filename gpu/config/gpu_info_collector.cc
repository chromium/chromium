// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"  // nogncheck
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_X11)
#include "ui/gl/gl_visual_picker_glx.h"
#endif

namespace {

// From ANGLE's egl/eglext.h.
#ifndef EGL_ANGLE_feature_control
#define EGL_ANGLE_feature_control 1
#define EGL_FEATURE_NAME_ANGLE 0x3460
#define EGL_FEATURE_CATEGORY_ANGLE 0x3461
#define EGL_FEATURE_DESCRIPTION_ANGLE 0x3462
#define EGL_FEATURE_BUG_ANGLE 0x3463
#define EGL_FEATURE_STATUS_ANGLE 0x3464
#define EGL_FEATURE_COUNT_ANGLE 0x3465
#define EGL_FEATURE_OVERRIDES_ENABLED_ANGLE 0x3466
#define EGL_FEATURE_OVERRIDES_DISABLED_ANGLE 0x3467
#define EGL_FEATURE_CONDITION_ANGLE 0x3468
#endif /* EGL_ANGLE_feature_control */

scoped_refptr<gl::GLSurface> InitializeGLSurface() {
  scoped_refptr<gl::GLSurface> surface(
      gl::init::CreateOffscreenGLSurface(gfx::Size()));
  if (!surface.get()) {
    LOG(ERROR) << "gl::GLContext::CreateOffscreenGLSurface failed";
    return nullptr;
  }

  return surface;
}

scoped_refptr<gl::GLContext> InitializeGLContext(gl::GLSurface* surface) {
  gl::GLContextAttribs attribs;
  attribs.client_major_es_version = 2;
  scoped_refptr<gl::GLContext> context(
      gl::init::CreateGLContext(nullptr, surface, attribs));
  if (!context.get()) {
    LOG(ERROR) << "gl::init::CreateGLContext failed";
    return nullptr;
  }

  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return nullptr;
  }

  return context;
}

std::string GetGLString(unsigned int pname) {
  const char* gl_string =
      reinterpret_cast<const char*>(glGetString(pname));
  if (gl_string)
    return std::string(gl_string);
  return std::string();
}

std::string QueryEGLStringi(EGLDisplay display,
                            unsigned int name,
                            unsigned int index) {
  const char* egl_string =
      reinterpret_cast<const char*>(eglQueryStringiANGLE(display, name, index));
  if (egl_string)
    return std::string(egl_string);
  return std::string();
}

// Return a version string in the format of "major.minor".
std::string GetVersionFromString(const std::string& version_string) {
  size_t begin = version_string.find_first_of("0123456789");
  if (begin != std::string::npos) {
    size_t end = version_string.find_first_not_of("01234567890.", begin);
    std::string sub_string;
    if (end != std::string::npos)
      sub_string = version_string.substr(begin, end - begin);
    else
      sub_string = version_string.substr(begin);
    std::vector<std::string> pieces = base::SplitString(
        sub_string, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (pieces.size() >= 2)
      return pieces[0] + "." + pieces[1];
  }
  return std::string();
}

// Return the array index of the found name, or return -1.
int StringContainsName(
    const std::string& str, const std::string* names, size_t num_names) {
  std::vector<std::string> tokens = base::SplitString(
      str, " .,()-_", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t ii = 0; ii < tokens.size(); ++ii) {
    for (size_t name_index = 0; name_index < num_names; ++name_index) {
      if (tokens[ii] == names[name_index]) {
        return base::checked_cast<int>(name_index);
      }
    }
  }
  return -1;
}

bool SupportsOOPRaster(const gl::GLVersionInfo& gl_info) {
  const bool use_version_es2 = false;
  sk_sp<const GrGLInterface> gl_interface(
      gl::init::CreateGrGLInterface(gl_info, use_version_es2));
  if (!gl_interface) {
    return false;
  }

  sk_sp<GrContext> gr_context = GrContext::MakeGL(std::move(gl_interface));
  if (gr_context) {
    // TODO(backer): Stash this GrContext for future use. For now, destroy.
    return true;
  }

  return false;
}

}  // namespace

namespace gpu {

bool CollectBasicGraphicsInfo(const base::CommandLine* command_line,
                              GPUInfo* gpu_info) {
  std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);

  // If GL is disabled then we don't need GPUInfo.
  if (use_gl == gl::kGLImplementationDisabledName) {
    gpu_info->gl_vendor = "Disabled";
    gpu_info->gl_renderer = "Disabled";
    gpu_info->gl_version = "Disabled";

    return true;
  }

  base::StringPiece software_gl_impl_name =
      gl::GetGLImplementationName(gl::GetSoftwareGLImplementation());
  if (use_gl == software_gl_impl_name ||
      command_line->HasSwitch(switches::kOverrideUseSoftwareGLForTests)) {
    // If using the software GL implementation, use fake vendor and
    // device ids to make sure it never gets blacklisted. It allows us
    // to proceed with loading the blacklist which may have non-device
    // specific entries we want to apply anyways (e.g., OS version
    // blacklisting).
    gpu_info->gpu.vendor_id = 0xffff;
    gpu_info->gpu.device_id = 0xffff;

    // Also declare the driver_vendor to be <software GL> to be able to
    // specify exceptions based on driver_vendor==<software GL> for some
    // blacklist rules.
    gpu_info->gpu.driver_vendor = software_gl_impl_name.as_string();

    return true;
  }

  return CollectBasicGraphicsInfo(gpu_info);
}

bool CollectGraphicsInfoGL(GPUInfo* gpu_info) {
  TRACE_EVENT0("startup", "gpu_info_collector::CollectGraphicsInfoGL");
  DCHECK_NE(gl::GetGLImplementation(), gl::kGLImplementationNone);

  scoped_refptr<gl::GLSurface> surface(InitializeGLSurface());
  if (!surface.get()) {
    LOG(ERROR) << "Could not create surface for info collection.";
    return false;
  }

  scoped_refptr<gl::GLContext> context(InitializeGLContext(surface.get()));
  if (!context.get()) {
    LOG(ERROR) << "Could not create context for info collection.";
    return false;
  }

  gpu_info->gl_renderer = GetGLString(GL_RENDERER);
  gpu_info->gl_vendor = GetGLString(GL_VENDOR);
  gpu_info->gl_version = GetGLString(GL_VERSION);
  std::string glsl_version_string = GetGLString(GL_SHADING_LANGUAGE_VERSION);

  gpu_info->gl_extensions = gl::GetGLExtensionsFromCurrentContext();
  gfx::ExtensionSet extension_set =
      gfx::MakeExtensionSet(gpu_info->gl_extensions);

  gl::GLVersionInfo gl_info(gpu_info->gl_version.c_str(),
                            gpu_info->gl_renderer.c_str(), extension_set);
  GPUInfo::GPUDevice& active_gpu = gpu_info->active_gpu();
  if (!gl_info.driver_vendor.empty() && active_gpu.driver_vendor.empty())
    active_gpu.driver_vendor = gl_info.driver_vendor;
  if (!gl_info.driver_version.empty() && active_gpu.driver_version.empty())
    active_gpu.driver_version = gl_info.driver_version;

  GLint max_samples = 0;
  if (gl_info.IsAtLeastGL(3, 0) || gl_info.IsAtLeastGLES(3, 0) ||
      gfx::HasExtension(extension_set, "GL_ANGLE_framebuffer_multisample") ||
      gfx::HasExtension(extension_set, "GL_APPLE_framebuffer_multisample") ||
      gfx::HasExtension(extension_set, "GL_EXT_framebuffer_multisample") ||
      gfx::HasExtension(extension_set,
                        "GL_EXT_multisampled_render_to_texture") ||
      gfx::HasExtension(extension_set, "GL_NV_framebuffer_multisample")) {
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
  }
  gpu_info->max_msaa_samples = base::NumberToString(max_samples);
  base::UmaHistogramSparse("GPU.MaxMSAASampleCount", max_samples);

#if defined(OS_ANDROID)
  gpu_info->can_support_threaded_texture_mailbox =
      gl::GLSurfaceEGL::HasEGLExtension("EGL_KHR_fence_sync") &&
      gl::GLSurfaceEGL::HasEGLExtension("EGL_KHR_image_base") &&
      gl::GLSurfaceEGL::HasEGLExtension("EGL_KHR_gl_texture_2D_image") &&
      gfx::HasExtension(extension_set, "GL_OES_EGL_image");
#else
  gl::GLWindowSystemBindingInfo window_system_binding_info;
  if (gl::init::GetGLWindowSystemBindingInfo(gl_info,
                                             &window_system_binding_info)) {
    gpu_info->gl_ws_vendor = window_system_binding_info.vendor;
    gpu_info->gl_ws_version = window_system_binding_info.version;
    gpu_info->gl_ws_extensions = window_system_binding_info.extensions;
    gpu_info->direct_rendering_version =
        window_system_binding_info.direct_rendering_version;
  }
#endif  // OS_ANDROID

  bool supports_robustness =
      gfx::HasExtension(extension_set, "GL_EXT_robustness") ||
      gfx::HasExtension(extension_set, "GL_KHR_robustness") ||
      gfx::HasExtension(extension_set, "GL_ARB_robustness");
  if (supports_robustness) {
    glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB,
        reinterpret_cast<GLint*>(&gpu_info->gl_reset_notification_strategy));
  }

#if defined(USE_X11)
  if (gl::GetGLImplementation() == gl::kGLImplementationDesktopGL) {
    gl::GLVisualPickerGLX* visual_picker = gl::GLVisualPickerGLX::GetInstance();
    gpu_info->system_visual = visual_picker->system_visual().visualid;
    gpu_info->rgba_visual = visual_picker->rgba_visual().visualid;
  }
#endif

  // Unconditionally check oop raster status regardless of preferences
  // so that finch trials can turn it on.
  gpu_info->oop_rasterization_supported = SupportsOOPRaster(gl_info);

  // TODO(kbr): remove once the destruction of a current context automatically
  // clears the current context.
  context->ReleaseCurrent(surface.get());

  std::string glsl_version = GetVersionFromString(glsl_version_string);
  gpu_info->pixel_shader_version = glsl_version;
  gpu_info->vertex_shader_version = glsl_version;

  IdentifyActiveGPU(gpu_info);
  return true;
}

void IdentifyActiveGPU(GPUInfo* gpu_info) {
  const std::string kNVidiaName = "nvidia";
  const std::string kNouveauName = "nouveau";
  const std::string kIntelName = "intel";
  const std::string kAMDName = "amd";
  const std::string kATIName = "ati";
  const std::string kVendorNames[] = {kNVidiaName, kNouveauName, kIntelName,
                                      kAMDName, kATIName};

  const uint32_t kNVidiaID = 0x10de;
  const uint32_t kIntelID = 0x8086;
  const uint32_t kAMDID = 0x1002;
  const uint32_t kATIID = 0x1002;
  const uint32_t kVendorIDs[] = {kNVidiaID, kNVidiaID, kIntelID, kAMDID,
                                 kATIID};

  DCHECK(gpu_info);
  if (gpu_info->secondary_gpus.size() == 0) {
    // If there is only a single GPU, that GPU is active.
    gpu_info->gpu.active = true;
    gpu_info->gpu.vendor_string = gpu_info->gl_vendor;
    gpu_info->gpu.device_string = gpu_info->gl_renderer;
    return;
  }

  uint32_t active_vendor_id = 0;
  if (!gpu_info->gl_vendor.empty()) {
    std::string gl_vendor_lower = base::ToLowerASCII(gpu_info->gl_vendor);
    int index = StringContainsName(gl_vendor_lower, kVendorNames,
                                   base::size(kVendorNames));
    if (index >= 0) {
      active_vendor_id = kVendorIDs[index];
    }
  }
  if (active_vendor_id == 0 && !gpu_info->gl_renderer.empty()) {
    std::string gl_renderer_lower = base::ToLowerASCII(gpu_info->gl_renderer);
    int index = StringContainsName(gl_renderer_lower, kVendorNames,
                                   base::size(kVendorNames));
    if (index >= 0) {
      active_vendor_id = kVendorIDs[index];
    }
  }
  if (active_vendor_id == 0) {
    // We fail to identify the GPU vendor through GL_VENDOR/GL_RENDERER.
    return;
  }
  gpu_info->gpu.active = false;
  for (size_t ii = 0; ii < gpu_info->secondary_gpus.size(); ++ii)
    gpu_info->secondary_gpus[ii].active = false;

  // TODO(zmo): if two GPUs are from the same vendor, this code will always
  // set the first GPU as active, which could be wrong.
  if (active_vendor_id == gpu_info->gpu.vendor_id) {
    gpu_info->gpu.active = true;
    return;
  }
  for (size_t ii = 0; ii < gpu_info->secondary_gpus.size(); ++ii) {
    if (active_vendor_id == gpu_info->secondary_gpus[ii].vendor_id) {
      gpu_info->secondary_gpus[ii].active = true;
      return;
    }
  }
}

void FillGPUInfoFromSystemInfo(GPUInfo* gpu_info,
                               angle::SystemInfo* system_info) {
  // We fill gpu_info even when angle::GetSystemInfo failed so that we can see
  // partial information even when GPU info collection fails. Handle malformed
  // angle::SystemInfo first.
  if (system_info->gpus.empty()) {
    return;
  }
  if (system_info->activeGPUIndex < 0) {
    system_info->activeGPUIndex = 0;
  }

  angle::GPUDeviceInfo* active =
      &system_info->gpus[system_info->activeGPUIndex];

  gpu_info->gpu.vendor_id = active->vendorId;
  gpu_info->gpu.device_id = active->deviceId;
  gpu_info->gpu.driver_vendor = std::move(active->driverVendor);
  gpu_info->gpu.driver_version = std::move(active->driverVersion);
  gpu_info->gpu.active = true;

  for (size_t i = 0; i < system_info->gpus.size(); i++) {
    if (static_cast<int>(i) == system_info->activeGPUIndex) {
      continue;
    }

    GPUInfo::GPUDevice device;
    device.vendor_id = system_info->gpus[i].vendorId;
    device.device_id = system_info->gpus[i].deviceId;
    device.driver_vendor = std::move(system_info->gpus[i].driverVendor);
    device.driver_version = std::move(system_info->gpus[i].driverVersion);

    gpu_info->secondary_gpus.push_back(device);
  }

  gpu_info->optimus = system_info->isOptimus;
  gpu_info->amd_switchable = system_info->isAMDSwitchable;

  gpu_info->machine_model_name = system_info->machineModelName;
  gpu_info->machine_model_version = system_info->machineModelVersion;
}

void CollectGraphicsInfoForTesting(GPUInfo* gpu_info) {
  DCHECK(gpu_info);
#if defined(OS_ANDROID)
  CollectContextGraphicsInfo(gpu_info);
#else
  CollectBasicGraphicsInfo(gpu_info);
#endif  // OS_ANDROID
}

bool CollectGpuExtraInfo(GpuExtraInfo* gpu_extra_info) {
  // Populate the list of ANGLE features by querying the functions exposed by
  // EGL_ANGLE_feature_control if it's available.
  if (gl::GLSurfaceEGL::IsANGLEFeatureControlSupported()) {
    EGLDisplay display = gl::GLSurfaceEGL::GetHardwareDisplay();
    EGLAttrib feature_count = 0;
    eglQueryDisplayAttribANGLE(display, EGL_FEATURE_COUNT_ANGLE,
                               &feature_count);
    gpu_extra_info->angle_features.resize(static_cast<size_t>(feature_count));
    for (size_t i = 0; i < gpu_extra_info->angle_features.size(); i++) {
      gpu_extra_info->angle_features[i].name =
          QueryEGLStringi(display, EGL_FEATURE_NAME_ANGLE, i);
      gpu_extra_info->angle_features[i].category =
          QueryEGLStringi(display, EGL_FEATURE_CATEGORY_ANGLE, i);
      gpu_extra_info->angle_features[i].description =
          QueryEGLStringi(display, EGL_FEATURE_DESCRIPTION_ANGLE, i);
      gpu_extra_info->angle_features[i].bug =
          QueryEGLStringi(display, EGL_FEATURE_BUG_ANGLE, i);
      gpu_extra_info->angle_features[i].status =
          QueryEGLStringi(display, EGL_FEATURE_STATUS_ANGLE, i);
      gpu_extra_info->angle_features[i].condition =
          QueryEGLStringi(display, EGL_FEATURE_CONDITION_ANGLE, i);
    }
  }

  return true;
}

}  // namespace gpu
