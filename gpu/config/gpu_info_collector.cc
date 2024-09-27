// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/webgpu_blocklist.h"
#include "skia/buildflags.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"  // nogncheck
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"           // nogncheck
#include "ui/ozone/public/platform_gl_egl_utility.h"  // nogncheck
#endif

#if !BUILDFLAG(USE_DAWN) && BUILDFLAG(SKIA_USE_DAWN)
#error "SKIA_USE_DAWN used without USE_DAWN is not supposed to work."
#endif

#if BUILDFLAG(USE_DAWN)
#include "third_party/dawn/include/dawn/dawn_proc.h"             // nogncheck
#include "third_party/dawn/include/dawn/native/DawnNative.h"     // nogncheck
#include "third_party/dawn/include/dawn/native/OpenGLBackend.h"  // nogncheck
#include "third_party/dawn/include/dawn/webgpu.h"                // nogncheck
#include "third_party/dawn/include/dawn/webgpu_cpp.h"            // nogncheck
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

scoped_refptr<gl::GLSurface> InitializeGLSurface(gl::GLDisplay* display) {
  scoped_refptr<gl::GLSurface> surface(
      gl::init::CreateOffscreenGLSurface(display, gfx::Size()));
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
  const char* gl_string = reinterpret_cast<const char*>(glGetString(pname));
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
int StringContainsName(const std::string& str,
                       base::span<const std::string> names) {
  std::vector<std::string> tokens = base::SplitString(
      str, " .,()-_", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t ii = 0; ii < tokens.size(); ++ii) {
    for (size_t name_index = 0; name_index < names.size(); ++name_index) {
      if (tokens[ii] == names[name_index]) {
        return base::checked_cast<int>(name_index);
      }
    }
  }
  return -1;
}

std::string GetDisplayTypeString(gl::DisplayType type) {
  switch (type) {
    case gl::DEFAULT:
      return "DEFAULT";
    case gl::SWIFT_SHADER:
      return "SWIFT_SHADER";
    case gl::ANGLE_WARP:
      return "ANGLE_WARP";
    case gl::ANGLE_D3D9:
      return "ANGLE_D3D9";
    case gl::ANGLE_D3D11:
      return "ANGLE_D3D11";
    case gl::ANGLE_OPENGL:
      return "ANGLE_OPENGL";
    case gl::ANGLE_OPENGLES:
      return "ANGLE_OPENGLES";
    case gl::ANGLE_NULL:
      return "ANGLE_NULL";
    case gl::ANGLE_D3D11_NULL:
      return "ANGLE_D3D11_NULL";
    case gl::ANGLE_OPENGL_NULL:
      return "ANGLE_OPENGL_NULL";
    case gl::ANGLE_OPENGLES_NULL:
      return "ANGLE_OPENGLES_NULL";
    case gl::ANGLE_VULKAN:
      return "ANGLE_VULKAN";
    case gl::ANGLE_VULKAN_NULL:
      return "ANGLE_VULKAN_NULL";
    case gl::ANGLE_D3D11on12:
      return "ANGLE_D3D11on12";
    case gl::ANGLE_SWIFTSHADER:
      return "ANGLE_SWIFTSHADER";
    case gl::ANGLE_OPENGL_EGL:
      return "ANGLE_OPENGL_EGL";
    case gl::ANGLE_OPENGLES_EGL:
      return "ANGLE_OPENGLES_EGL";
    case gl::ANGLE_METAL:
      return "ANGLE_METAL";
    case gl::ANGLE_METAL_NULL:
      return "ANGLE_METAL_NULL";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

#if BUILDFLAG(USE_DAWN)
std::string GetDawnAdapterTypeString(wgpu::AdapterType type) {
  switch (type) {
    case wgpu::AdapterType::IntegratedGPU:
      return "<Integrated GPU> ";
    case wgpu::AdapterType::DiscreteGPU:
      return "<Discrete GPU> ";
    case wgpu::AdapterType::CPU:
      return "<CPU> ";
    default:
      return "<Unknown GPU> ";
  }
}

std::string GetDawnBackendTypeString(wgpu::BackendType type) {
  switch (type) {
    case wgpu::BackendType::D3D11:
      return "D3D11 backend";
    case wgpu::BackendType::D3D12:
      return "D3D12 backend";
    case wgpu::BackendType::Metal:
      return "Metal backend";
    case wgpu::BackendType::Vulkan:
      return "Vulkan backend";
    case wgpu::BackendType::OpenGL:
      return "OpenGL backend";
    case wgpu::BackendType::OpenGLES:
      return "OpenGLES backend";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

void AddTogglesToDawnInfoList(dawn::native::Instance* instance,
                              const std::vector<const char*>& toggle_names,
                              std::vector<std::string>* dawn_info_list) {
  for (auto* name : toggle_names) {
    const dawn::native::ToggleInfo* info = instance->GetToggleInfo(name);
    if (info) {
      dawn_info_list->push_back(info->name);
      dawn_info_list->push_back(info->url);
      dawn_info_list->push_back(info->description);
    }
  }
}

void GetDawnTogglesForWebGPU(
    bool enable_unsafe_webgpu,
    bool enable_webgpu_developer_features,
    const std::vector<std::string>& enabled_preference,
    const std::vector<std::string>& disabled_preference,
    std::vector<const char*>* force_enabled_toggles,
    std::vector<const char*>* force_disabled_toggles) {
  // Disallows usage of SPIR-V by default for security (we only ensure that WGSL
  // is secure).
  force_enabled_toggles->push_back("disallow_spirv");
  // Enable timestamp quantization by default for privacy, unless
  // --enable-webgpu-developer-features is used.
  if (!enable_webgpu_developer_features) {
    force_enabled_toggles->push_back("timestamp_quantization");
  } else {
    force_disabled_toggles->push_back("timestamp_quantization");
  }

  for (const std::string& toggle : enabled_preference) {
    force_enabled_toggles->push_back(toggle.c_str());
  }

  for (const std::string& toggle : disabled_preference) {
    force_disabled_toggles->push_back(toggle.c_str());
  }
}

#if BUILDFLAG(SKIA_USE_DAWN)
void GetDawnTogglesForSkiaGraphite(
    std::vector<const char*>* force_enabled_toggles,
    std::vector<const char*>* force_disabled_toggles) {
#if DCHECK_IS_ON()
  force_enabled_toggles->push_back("use_user_defined_labels_in_backend");
#else
  force_enabled_toggles->push_back("disable_robustness");
  force_enabled_toggles->push_back("skip_validation");
  force_disabled_toggles->push_back("lazy_clear_resource_on_first_use");
#endif
}
#endif  // BUILDFLAG(SKIA_USE_DAWN)

void ReportWebGPUAdapterMetrics(dawn::native::Instance* instance) {
  static BASE_FEATURE(kCollectDawnGpuMetrics, "CollectDawnGpuMetrics",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  if (!base::FeatureList::IsEnabled(kCollectDawnGpuMetrics)) {
    return;
  }
  WGPULimits max_limits{};
  wgpu::AdapterType adapter_type = wgpu::AdapterType::Unknown;

  wgpu::RequestAdapterOptions adapter_options = {};
  // Search for the backend used for core WebGPU.
#if BUILDFLAG(IS_WIN)
  adapter_options.backendType = wgpu::BackendType::D3D12;
#elif BUILDFLAG(IS_MAC)
  adapter_options.backendType = wgpu::BackendType::Metal;
#else
  adapter_options.backendType = wgpu::BackendType::Vulkan;
#endif

  bool supports_shader_f16 = false;
  for (dawn::native::Adapter& adapter :
       instance->EnumerateAdapters(&adapter_options)) {
    adapter.SetUseTieredLimits(false);
    wgpu::AdapterInfo info;
    adapter.GetInfo(&info);
    if (info.adapterType != wgpu::AdapterType::DiscreteGPU &&
        info.adapterType != wgpu::AdapterType::IntegratedGPU) {
      // We only care about GPU adapters and not CPU adapters.
      continue;
    }

    WGPUSupportedLimits limits;
    limits.nextInChain = nullptr;
    if (adapter.GetLimits(&limits) != wgpu::Status::Success) {
      continue;
    }

    // Prefer the adapter with larger buffer binding size.
    if (limits.limits.maxStorageBufferBindingSize >
        max_limits.maxStorageBufferBindingSize) {
      max_limits = limits.limits;
      adapter_type = info.adapterType;
    }

    supports_shader_f16 |=
        wgpu::Adapter(adapter.Get()).HasFeature(wgpu::FeatureName::ShaderF16);
  }

  bool has_gpu_adapter = adapter_type != wgpu::AdapterType::Unknown;
  base::UmaHistogramBoolean("GPU.WebGPU.HasGpuAdapter", has_gpu_adapter);
  if (has_gpu_adapter) {
    std::string adapter_string = adapter_type == wgpu::AdapterType::DiscreteGPU
                                     ? "Discrete"
                                     : "Integrated";
    base::UmaHistogramMemoryLargeMB(
        "GPU.WebGPU.MaxStorageBufferBindingSize." + adapter_string,
        max_limits.maxStorageBufferBindingSize / (1024 * 1024));
    base::UmaHistogramCounts100000(
        "GPU.WebGPU.MaxTextureDimension2D." + adapter_string,
        max_limits.maxTextureDimension2D);

    base::UmaHistogramBoolean("GPU.WebGPU.Support.ShaderF16",
                              supports_shader_f16);
  }
}

void ReportWebGPUSupportMetrics(dawn::native::Instance* instance) {
  static BASE_FEATURE(kCollectWebGPUSupportMetrics,
                      "CollectWebGPUSupportMetrics",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
                      base::FEATURE_DISABLED_BY_DEFAULT);
#else
                      base::FEATURE_ENABLED_BY_DEFAULT);
#endif
  if (!base::FeatureList::IsEnabled(kCollectWebGPUSupportMetrics)) {
    return;
  }
  // Note: These enum values should not change and should match those in
  // //tools/metrics/histograms/enums.xml
  enum class WebGPUSupport {
    kNone = 0,
    kCoreNone_CompatBlocklisted = 1,
    kCoreNone_CompatSupported = 2,
    kCoreBlocklisted_CompatNone = 3,
    kCoreBlocklisted_CompatBlocklisted = 4,
    kCoreBlocklisted_CompatSupported = 5,
    kCoreSupported = 6,
    kMaxValue = kCoreSupported,
  };

  bool has_core_blocklisted_adapter = false;
  bool has_core_adapter = false;
  bool has_compat_blocklisted_adapter = false;
  bool has_compat_adapter = false;

  wgpu::RequestAdapterOptions adapter_options = {};
  // Search for the backend used for core WebGPU.
#if BUILDFLAG(IS_WIN)
  adapter_options.backendType = wgpu::BackendType::D3D12;
#elif BUILDFLAG(IS_MAC)
  adapter_options.backendType = wgpu::BackendType::Metal;
#else
  adapter_options.backendType = wgpu::BackendType::Vulkan;
#endif
  // Check core adapters.
  for (const dawn::native::Adapter& native_adapter :
       instance->EnumerateAdapters(&adapter_options)) {
    wgpu::Adapter adapter(native_adapter.Get());
    wgpu::AdapterInfo info = {};
    adapter.GetInfo(&info);

    switch (info.adapterType) {
      case wgpu::AdapterType::CPU:
        // Skip CPU adapters.
        break;
      default:
        if (gpu::IsWebGPUAdapterBlocklisted(adapter).blocked) {
          has_core_blocklisted_adapter = true;
        } else {
          has_core_adapter = true;
        }
    }
  }

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  // Check for compat adapters on GLES.
  adapter_options.backendType = wgpu::BackendType::OpenGLES;
  adapter_options.compatibilityMode = true;

  dawn::native::opengl::RequestAdapterOptionsGetGLProc
      adapter_options_get_gl_proc = {};
  adapter_options_get_gl_proc.getProc = gl::GetGLProcAddress;
  gl::GLDisplayEGL* gl_display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  if (gl_display) {
    adapter_options_get_gl_proc.display = gl_display->GetDisplay();
  } else {
    adapter_options_get_gl_proc.display = EGL_NO_DISPLAY;
  }
  adapter_options_get_gl_proc.nextInChain = adapter_options.nextInChain;
  adapter_options.nextInChain = &adapter_options_get_gl_proc;

  for (const dawn::native::Adapter& native_adapter :
       instance->EnumerateAdapters(&adapter_options)) {
    wgpu::Adapter adapter(native_adapter.Get());
    wgpu::AdapterInfo info = {};
    adapter.GetInfo(&info);

    switch (info.adapterType) {
      case wgpu::AdapterType::CPU:
        // Skip CPU adapters.
        break;
      default:
        if (gpu::IsWebGPUAdapterBlocklisted(adapter).blocked) {
          has_compat_blocklisted_adapter = true;
        } else {
          has_compat_adapter = true;
        }
    }
  }
#endif

  WebGPUSupport tier;
  if (has_core_adapter) {
    tier = WebGPUSupport::kCoreSupported;
  } else if (has_core_blocklisted_adapter) {
    if (has_compat_adapter) {
      tier = WebGPUSupport::kCoreBlocklisted_CompatSupported;
    } else if (has_compat_blocklisted_adapter) {
      tier = WebGPUSupport::kCoreBlocklisted_CompatBlocklisted;
    } else {
      tier = WebGPUSupport::kCoreBlocklisted_CompatNone;
    }
  } else {
    if (has_compat_adapter) {
      tier = WebGPUSupport::kCoreNone_CompatSupported;
    } else if (has_compat_blocklisted_adapter) {
      tier = WebGPUSupport::kCoreNone_CompatBlocklisted;
    } else {
      tier = WebGPUSupport::kNone;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.WebGPU.Support", tier);
  ReportWebGPUAdapterMetrics(instance);
}
#endif  // BUILDFLAG(USE_DAWN)

}  // namespace

namespace gpu {

bool CollectGraphicsDeviceInfoFromCommandLine(
    const base::CommandLine* command_line,
    GPUInfo* gpu_info) {
  GPUInfo::GPUDevice& gpu = gpu_info->gpu;

  if (command_line->HasSwitch(switches::kGpuVendorId)) {
    const std::string vendor_id_str =
        command_line->GetSwitchValueASCII(switches::kGpuVendorId);
    base::StringToUint(vendor_id_str, &gpu.vendor_id);
  }

  if (command_line->HasSwitch(switches::kGpuDeviceId)) {
    const std::string device_id_str =
        command_line->GetSwitchValueASCII(switches::kGpuDeviceId);
    base::StringToUint(device_id_str, &gpu.device_id);
  }

#if BUILDFLAG(IS_WIN)
  if (command_line->HasSwitch(switches::kGpuSubSystemId)) {
    const std::string syb_system_id_str =
        command_line->GetSwitchValueASCII(switches::kGpuSubSystemId);
    base::StringToUint(syb_system_id_str, &gpu.sub_sys_id);
  }

  if (command_line->HasSwitch(switches::kGpuRevision)) {
    const std::string revision_str =
        command_line->GetSwitchValueASCII(switches::kGpuRevision);
    base::StringToUint(revision_str, &gpu.revision);
  }
#endif

  if (command_line->HasSwitch(switches::kGpuDriverVersion)) {
    gpu.driver_version =
        command_line->GetSwitchValueASCII(switches::kGpuDriverVersion);
  }

  bool info_updated = gpu.vendor_id || gpu.device_id ||
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
                      gpu.revision ||
#endif
#if BUILDFLAG(IS_WIN)
                      gpu.sub_sys_id ||
#endif
                      !gpu.driver_version.empty();

  return info_updated;
}

bool CollectBasicGraphicsInfo(const base::CommandLine* command_line,
                              GPUInfo* gpu_info) {
  // In the info-collection GPU process on Windows, we get the device info from
  // the browser.
  if (CollectGraphicsDeviceInfoFromCommandLine(command_line, gpu_info)) {
    return true;
  }

  // We can't check if passthrough is supported yet because GL may not be
  // initialized.
  gpu_info->passthrough_cmd_decoder =
      gl::UsePassthroughCommandDecoder(command_line);

  std::optional<gl::GLImplementationParts> implementation =
      gl::GetRequestedGLImplementationFromCommandLine(command_line);

  if (implementation == gl::kGLImplementationDisabled) {
    // If GL is disabled then we don't need GPUInfo.
    gpu_info->gl_vendor = "Disabled";
    gpu_info->gl_renderer = "Disabled";
    gpu_info->gl_version = "Disabled";
    return true;
  } else if (implementation == gl::GetSoftwareGLImplementation()) {
    // If using the software GL implementation, use fake vendor and
    // device ids to make sure it never gets blocklisted. It allows us
    // to proceed with loading the blocklist which may have non-device
    // specific entries we want to apply anyways (e.g., OS version
    // blocklisting).
    gpu_info->gpu.vendor_id = 0xffff;
    gpu_info->gpu.device_id = 0xffff;

    // Also declare the driver_vendor to be <SwANGLE> to be able to
    // specify exceptions based on driver_vendor==<SwANGLE> for some
    // blocklist rules.
    gpu_info->gpu.driver_vendor = "SwANGLE";

    GPUInfo swangle_gpu_info(*gpu_info);
    if (CollectBasicGraphicsInfo(&swangle_gpu_info)) {
      // Also store the machine model and version
      gpu_info->machine_model_name = swangle_gpu_info.machine_model_name;
      gpu_info->machine_model_version = swangle_gpu_info.machine_model_version;
    }

    return true;
  }

  return CollectBasicGraphicsInfo(gpu_info);
}

bool CollectGraphicsInfoGL(GPUInfo* gpu_info, gl::GLDisplay* display) {
  TRACE_EVENT0("startup", "gpu_info_collector::CollectGraphicsInfoGL");
  DCHECK_NE(gl::GetGLImplementationParts(), gl::kGLImplementationNone);
  gl::GLDisplayEGL* egl_display = display->GetAs<gl::GLDisplayEGL>();

  // Now that we can check GL extensions, update passthrough support info.
  if (!gl::PassthroughCommandDecoderSupported()) {
    gpu_info->passthrough_cmd_decoder = false;
  }

  scoped_refptr<gl::GLSurface> surface(InitializeGLSurface(display));
  if (!surface.get()) {
    LOG(ERROR) << "Could not create surface for info collection.";
    return false;
  }

  scoped_refptr<gl::GLContext> context(InitializeGLContext(surface.get()));
  if (!context.get()) {
    LOG(ERROR) << "Could not create context for info collection.";
    return false;
  }

  if (egl_display) {
    gpu_info->display_type =
        GetDisplayTypeString(egl_display->GetDisplayType());
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
  if (!gl_info.driver_vendor.empty() && active_gpu.driver_vendor.empty()) {
    active_gpu.driver_vendor = gl_info.driver_vendor;
  }
  if (!gl_info.driver_version.empty() && active_gpu.driver_version.empty()) {
    active_gpu.driver_version = gl_info.driver_version;
  }

  GLint max_samples = 0;
  if (gl_info.IsAtLeastGLES(3, 0) ||
      gfx::HasExtension(extension_set, "GL_ANGLE_framebuffer_multisample") ||
      gfx::HasExtension(extension_set, "GL_APPLE_framebuffer_multisample") ||
      gfx::HasExtension(extension_set, "GL_EXT_framebuffer_multisample") ||
      gfx::HasExtension(extension_set,
                        "GL_EXT_multisampled_render_to_texture") ||
      gfx::HasExtension(extension_set, "GL_NV_framebuffer_multisample")) {
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
  }
  gpu_info->max_msaa_samples = base::NumberToString(max_samples);

#if BUILDFLAG(IS_ANDROID)
  gpu_info->can_support_threaded_texture_mailbox =
      egl_display->ext->b_EGL_KHR_fence_sync &&
      egl_display->ext->b_EGL_KHR_image_base &&
      egl_display->ext->b_EGL_KHR_gl_texture_2D_image &&
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
#endif  // BUILDFLAG(IS_ANDROID)

  bool supports_robustness =
      gfx::HasExtension(extension_set, "GL_EXT_robustness") ||
      gfx::HasExtension(extension_set, "GL_KHR_robustness") ||
      gfx::HasExtension(extension_set, "GL_ARB_robustness");
  if (supports_robustness) {
    glGetIntegerv(
        GL_RESET_NOTIFICATION_STRATEGY_ARB,
        reinterpret_cast<GLint*>(&gpu_info->gl_reset_notification_strategy));
  }

  // TODO(kbr): remove once the destruction of a current context automatically
  // clears the current context.
  context->ReleaseCurrent(surface.get());

  std::string glsl_version = GetVersionFromString(glsl_version_string);
  gpu_info->pixel_shader_version = glsl_version;
  gpu_info->vertex_shader_version = glsl_version;

  bool active_gpu_identified = false;
#if BUILDFLAG(IS_WIN)
  active_gpu_identified = IdentifyActiveGPUWithLuid(gpu_info);
#endif  // BUILDFLAG(IS_WIN)

  if (!active_gpu_identified)
    IdentifyActiveGPU(gpu_info);

  return true;
}

void IdentifyActiveGPU(GPUInfo* gpu_info) {
  const std::string kNVidiaName = "nvidia";
  const std::string kNouveauName = "nouveau";
  const std::string kIntelName = "intel";
  const std::string kAMDName = "amd";
  const std::string kATIName = "ati";
  const std::array<std::string, 5> kVendorNames = {
      {kNVidiaName, kNouveauName, kIntelName, kAMDName, kATIName}};

  const uint32_t kNVidiaID = 0x10de;
  const uint32_t kIntelID = 0x8086;
  const uint32_t kAMDID = 0x1002;
  const uint32_t kATIID = 0x1002;
  const std::array<uint32_t, 5> kVendorIDs = {
      {kNVidiaID, kNVidiaID, kIntelID, kAMDID, kATIID}};

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
    int index = StringContainsName(gl_vendor_lower, kVendorNames);
    if (index >= 0) {
      active_vendor_id = kVendorIDs[index];
    }
  }
  if (active_vendor_id == 0 && !gpu_info->gl_renderer.empty()) {
    std::string gl_renderer_lower = base::ToLowerASCII(gpu_info->gl_renderer);
    int index = StringContainsName(gl_renderer_lower, kVendorNames);
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
#if BUILDFLAG(IS_CHROMEOS)
  gpu_info->gpu.revision = active->revisionId;
#endif  // BUILDFLAG(IS_CHROMEOS)
  gpu_info->gpu.system_device_id = active->systemDeviceId;
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
#if BUILDFLAG(IS_CHROMEOS)
    device.revision = system_info->gpus[i].revisionId;
#endif  // BUILDFLAG(IS_CHROMEOS)
    device.system_device_id = system_info->gpus[i].systemDeviceId;
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
#if BUILDFLAG(IS_ANDROID)
  CollectContextGraphicsInfo(gpu_info);
#else
  CollectBasicGraphicsInfo(gpu_info);
#endif  // BUILDFLAG(IS_ANDROID)
}

bool CollectGpuExtraInfo(gfx::GpuExtraInfo* gpu_extra_info,
                         const GpuPreferences& prefs) {
  // Populate the list of ANGLE features by querying the functions exposed by
  // EGL_ANGLE_feature_control if it's available.
  if (gl::g_driver_egl.client_ext.b_EGL_ANGLE_feature_control) {
    EGLDisplay display = gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay();
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

#if BUILDFLAG(IS_OZONE)
  if (const auto* const egl_utility =
          ui::OzonePlatform::GetInstance()->GetPlatformGLEGLUtility()) {
    egl_utility->CollectGpuExtraInfo(prefs.enable_native_gpu_memory_buffers,
                                     *gpu_extra_info);
  }
#endif
  return true;
}

void CollectDawnInfo(const gpu::GpuPreferences& gpu_preferences,
                     bool collect_metrics,
                     std::vector<std::string>* dawn_info_list) {
#if BUILDFLAG(USE_DAWN)
  DawnProcTable procs = dawn::native::GetProcs();
  dawnProcSetProcs(&procs);

  std::string dawn_search_path;
  base::FilePath module_path;
#if BUILDFLAG(IS_MAC)
  if (base::apple::AmIBundled()) {
    dawn_search_path = base::apple::FrameworkBundlePath()
                           .Append("Libraries")
                           .AsEndingWithSeparator()
                           .MaybeAsASCII();
  }
  if (dawn_search_path.empty())
#endif
  {
#if BUILDFLAG(IS_IOS)
    if (base::PathService::Get(base::DIR_ASSETS, &module_path)) {
#else
    if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
#endif
      dawn_search_path = module_path.AsEndingWithSeparator().MaybeAsASCII();
    }
  }
  const char* dawn_search_path_c_str = dawn_search_path.c_str();

  // Get the list of required toggles for WebGPU.
  std::vector<const char*> required_enabled_toggles_webgpu;
  std::vector<const char*> required_disabled_toggles_webgpu;

  GetDawnTogglesForWebGPU(gpu_preferences.enable_unsafe_webgpu,
                          gpu_preferences.enable_webgpu_developer_features,
                          gpu_preferences.enabled_dawn_features_list,
                          gpu_preferences.disabled_dawn_features_list,
                          &required_enabled_toggles_webgpu,
                          &required_disabled_toggles_webgpu);

  // Build toggles descriptor for instance, adapters and devices.
  wgpu::DawnTogglesDescriptor dawn_toggles;

  dawn_toggles.enabledToggleCount = required_enabled_toggles_webgpu.size();
  dawn_toggles.enabledToggles = required_enabled_toggles_webgpu.data();
  dawn_toggles.disabledToggleCount = required_disabled_toggles_webgpu.size();
  dawn_toggles.disabledToggles = required_disabled_toggles_webgpu.data();

  dawn::native::DawnInstanceDescriptor dawn_instance_desc = {};
  dawn_instance_desc.additionalRuntimeSearchPathsCount =
      dawn_search_path.empty() ? 0u : 1u;
  dawn_instance_desc.additionalRuntimeSearchPaths = &dawn_search_path_c_str;

  wgpu::InstanceDescriptor instance_desc = {};
  instance_desc.nextInChain = &dawn_instance_desc;
  // Create instance with Dawn toggles.
  dawn_instance_desc.nextInChain = &dawn_toggles;

  auto instance = std::make_unique<dawn::native::Instance>(
      reinterpret_cast<const WGPUInstanceDescriptor*>(&instance_desc));
  if (collect_metrics) {
    ReportWebGPUSupportMetrics(instance.get());
    return;
  }

  // Enumerate adapters with required toggles.
  wgpu::RequestAdapterOptions adapter_options = {};
  adapter_options.nextInChain = &dawn_toggles;

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  dawn::native::opengl::RequestAdapterOptionsGetGLProc
      adapter_options_get_gl_proc = {};
  adapter_options_get_gl_proc.getProc = gl::GetGLProcAddress;
  gl::GLDisplayEGL* gl_display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  EGLDisplay display = gl_display ? gl_display->GetDisplay() : EGL_NO_DISPLAY;
  adapter_options_get_gl_proc.display = display;
  adapter_options_get_gl_proc.nextInChain = adapter_options.nextInChain;
  adapter_options.nextInChain = &adapter_options_get_gl_proc;
  EGLSurface drawSurface = eglGetCurrentSurface(EGL_DRAW);
  EGLSurface readSurface = eglGetCurrentSurface(EGL_READ);
  EGLContext context = eglGetCurrentContext();

  // Dawn WebGPU API calls, such as adapter.CreateDevice(), may change the
  // EGLContext. Restore the context on return from this function.
  absl::Cleanup on_return = [display, drawSurface, readSurface, context] {
    eglMakeCurrent(display, drawSurface, readSurface, context);
  };
#endif

  for (bool compatibilityMode : {false, true}) {
    adapter_options.compatibilityMode = compatibilityMode;
    std::vector<dawn::native::Adapter> adapters = instance->EnumerateAdapters(
        reinterpret_cast<const WGPURequestAdapterOptions*>(&adapter_options));
    for (dawn::native::Adapter& native_adapter : adapters) {
      wgpu::Adapter adapter(native_adapter.Get());
      wgpu::AdapterInfo info = {};
      adapter.GetInfo(&info);
      if (compatibilityMode &&
          info.backendType != wgpu::BackendType::OpenGLES) {
        continue;
      }

      // Both Integrated-GPU and Discrete-GPU backend types will be displayed.
      if (info.backendType != wgpu::BackendType::Null) {
        // Get the adapter and the device name.
        std::string gpu_str = GetDawnAdapterTypeString(info.adapterType);
        gpu_str += " " + GetDawnBackendTypeString(info.backendType);
        gpu_str += " - " + std::string(info.device);
        if (compatibilityMode) {
          gpu_str += " (Compatibility Mode)";
        }
        dawn_info_list->push_back(gpu_str);

        dawn_info_list->push_back("[WebGPU Status]");
        auto blocklist_result = IsWebGPUAdapterBlocklisted(adapter);
        if (blocklist_result.blocked) {
          dawn_info_list->push_back("Blocklisted - " + blocklist_result.reason);
        } else {
          dawn_info_list->push_back("Available");
        }

        // Get supported features under required adapter toggles if Dawn
        // available, or default toggles otherwise.
        dawn_info_list->push_back("[Adapter Supported Features]");
        std::vector<wgpu::FeatureName> features(
            adapter.EnumerateFeatures(nullptr));
        adapter.EnumerateFeatures(features.data());
        for (wgpu::FeatureName f : features) {
          dawn_info_list->push_back(dawn::native::GetFeatureInfo(f)->name);
        }

        // Scope the lifetime of |device| to avoid accidental use after release.
        {
          // If Dawn is available, create the device with Dawn toggles.
          wgpu::DeviceDescriptor device_descriptor = {};
          device_descriptor.nextInChain = &dawn_toggles;
          wgpu::Device device = adapter.CreateDevice(&device_descriptor);
          // CreateDevice can return null if the device has been removed or
          // we've run out of memory. Ensure we don't crash in these instances.
          if (device) {
            // Get the list of enabled toggles on the device
            dawn_info_list->push_back("[Enabled Toggle Names]");
            std::vector<const char*> toggle_names =
                dawn::native::GetTogglesUsed(device.Get());
            AddTogglesToDawnInfoList(instance.get(), toggle_names,
                                     dawn_info_list);
          }
        }

        if (!required_enabled_toggles_webgpu.empty()) {
          dawn_info_list->push_back("[WebGPU Required Toggles - enabled]");
          AddTogglesToDawnInfoList(
              instance.get(), required_enabled_toggles_webgpu, dawn_info_list);
        }

        if (!required_disabled_toggles_webgpu.empty()) {
          dawn_info_list->push_back("[WebGPU Required Toggles - disabled]");
          AddTogglesToDawnInfoList(
              instance.get(), required_disabled_toggles_webgpu, dawn_info_list);
        }

#if BUILDFLAG(SKIA_USE_DAWN)
        if (gpu_preferences.gr_context_type == GrContextType::kGraphiteDawn) {
          // Get the list of required toggles for Skia.
          // TODO(sunnyps): Ideally these should come from a single source of
          // truth e.g. from DawnContextProvider or a common helper, instead of
          // just assuming some values here.
          std::vector<const char*> force_enabled_toggles_skia;
          std::vector<const char*> force_disabled_toggles_skia;
          GetDawnTogglesForSkiaGraphite(&force_enabled_toggles_skia,
                                        &force_disabled_toggles_skia);

          if (!force_enabled_toggles_skia.empty()) {
            dawn_info_list->push_back("[Skia Required Toggles - enabled]");
            AddTogglesToDawnInfoList(instance.get(), force_enabled_toggles_skia,
                                     dawn_info_list);
          }

          if (!force_disabled_toggles_skia.empty()) {
            dawn_info_list->push_back("[Skia Required Toggles - disabled]");
            AddTogglesToDawnInfoList(
                instance.get(), force_disabled_toggles_skia, dawn_info_list);
          }
        }
#endif  // BUILDFLAG(SKIA_USE_DAWN)
      }
    }
  }
#endif  // BUILDFLAG(USE_DAWN)
}

}  // namespace gpu
