// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_internals_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "gpu/config/gpu_extra_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_lists_version.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "skia/ext/skia_commit_hash.h"
#include "third_party/angle/src/common/version.h"
#include "third_party/skia/include/core/SkMilestone.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gl/gpu_switching_manager.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#include "ui/gfx/win/physical_size.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"       // nogncheck
#include "ui/gfx/x/x11_atom_cache.h"  // nogncheck
#endif

namespace content {
namespace {

WebUIDataSource* CreateGpuHTMLSource() {
  WebUIDataSource* source = WebUIDataSource::Create(kChromeUIGpuHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  source->UseStringsJs();
  source->AddResourcePath("gpu_internals.js", IDR_GPU_INTERNALS_JS);
  source->AddResourcePath("vulkan_info.mojom.js", IDR_VULKAN_INFO_MOJO_JS);
  source->AddResourcePath("vulkan_types.mojom.js", IDR_VULKAN_TYPES_MOJO_JS);
  source->SetDefaultResource(IDR_GPU_INTERNALS_HTML);
  return source;
}

std::unique_ptr<base::DictionaryValue> NewDescriptionValuePair(
    base::StringPiece desc,
    base::StringPiece value) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("description", desc);
  dict->SetString("value", value);
  return dict;
}

std::unique_ptr<base::DictionaryValue> NewDescriptionValuePair(
    base::StringPiece desc,
    std::unique_ptr<base::Value> value) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("description", desc);
  dict->Set("value", std::move(value));
  return dict;
}

#if defined(OS_WIN)
// Output DxDiagNode tree as nested array of {description,value} pairs
std::unique_ptr<base::ListValue> DxDiagNodeToList(const gpu::DxDiagNode& node) {
  auto list = std::make_unique<base::ListValue>();
  for (std::map<std::string, std::string>::const_iterator it =
      node.values.begin();
      it != node.values.end();
      ++it) {
    list->Append(NewDescriptionValuePair(it->first, it->second));
  }

  for (std::map<std::string, gpu::DxDiagNode>::const_iterator it =
      node.children.begin();
      it != node.children.end();
      ++it) {
    std::unique_ptr<base::ListValue> sublist = DxDiagNodeToList(it->second);
    list->Append(NewDescriptionValuePair(it->first, std::move(sublist)));
  }
  return list;
}
#endif  // OS_WIN

std::string GPUDeviceToString(const gpu::GPUInfo::GPUDevice& gpu) {
  std::string vendor = base::StringPrintf("0x%04x", gpu.vendor_id);
  if (!gpu.vendor_string.empty())
    vendor += " [" + gpu.vendor_string + "]";
  std::string device = base::StringPrintf("0x%04x", gpu.device_id);
  if (!gpu.device_string.empty())
    device += " [" + gpu.device_string + "]";
  std::string rt = base::StringPrintf("VENDOR= %s, DEVICE=%s", vendor.c_str(),
                                      device.c_str());
#if defined(OS_WIN)
  if (gpu.sub_sys_id || gpu.revision) {
    rt += base::StringPrintf(", SUBSYS=0x%08x, REV=%u", gpu.sub_sys_id,
                             gpu.revision);
  }
#endif
  if (gpu.active)
    rt += " *ACTIVE*";
  return rt;
}

std::unique_ptr<base::ListValue> BasicGpuInfoAsListValue(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
  auto basic_info = std::make_unique<base::ListValue>();
  basic_info->Append(NewDescriptionValuePair(
      "Initialization time",
      base::NumberToString(gpu_info.initialization_time.InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair(
      "In-process GPU",
      std::make_unique<base::Value>(gpu_info.in_process_gpu)));
  basic_info->Append(NewDescriptionValuePair(
      "Passthrough Command Decoder",
      std::make_unique<base::Value>(gpu_info.passthrough_cmd_decoder)));
  basic_info->Append(NewDescriptionValuePair(
      "Sandboxed", std::make_unique<base::Value>(gpu_info.sandboxed)));
  basic_info->Append(
      NewDescriptionValuePair("GPU0", GPUDeviceToString(gpu_info.gpu)));
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    basic_info->Append(NewDescriptionValuePair(
        base::StringPrintf("GPU%d", static_cast<int>(i + 1)),
        GPUDeviceToString(gpu_info.secondary_gpus[i])));
  }
  basic_info->Append(NewDescriptionValuePair(
      "Optimus", std::make_unique<base::Value>(gpu_info.optimus)));
  basic_info->Append(NewDescriptionValuePair(
      "AMD switchable",
      std::make_unique<base::Value>(gpu_info.amd_switchable)));
#if defined(OS_WIN)
  std::string compositor =
      ui::win::IsAeroGlassEnabled() ? "Aero Glass" : "none";
  basic_info->Append(
      NewDescriptionValuePair("Desktop compositing", compositor));

  basic_info->Append(NewDescriptionValuePair(
      "Direct composition",
      std::make_unique<base::Value>(gpu_info.direct_composition)));
  basic_info->Append(NewDescriptionValuePair(
      "Supports overlays",
      std::make_unique<base::Value>(gpu_info.supports_overlays)));
  basic_info->Append(NewDescriptionValuePair(
      "YUY2 overlay support",
      gpu::OverlaySupportToString(gpu_info.yuy2_overlay_support)));
  basic_info->Append(NewDescriptionValuePair(
      "NV12 overlay support",
      gpu::OverlaySupportToString(gpu_info.nv12_overlay_support)));

  std::vector<gfx::PhysicalDisplaySize> display_sizes =
      gfx::GetPhysicalSizeForDisplays();
  for (const auto& display_size : display_sizes) {
    const int w = display_size.width_mm;
    const int h = display_size.height_mm;
    const double size_mm = sqrt(w * w + h * h);
    const double size_inches = 0.0393701 * size_mm;
    const double rounded_size_inches = floor(10.0 * size_inches) / 10.0;
    std::string size_string = base::StringPrintf("%.1f\"", rounded_size_inches);
    std::string description_string = base::StringPrintf(
        "Diagonal Monitor Size of %s", display_size.display_name.c_str());
    basic_info->Append(
        NewDescriptionValuePair(description_string, size_string));
  }

  basic_info->Append(NewDescriptionValuePair(
      "Driver D3D12 feature level",
      gpu::D3DFeatureLevelToString(
          gpu_info.dx12_vulkan_version_info.d3d12_feature_level)));

  basic_info->Append(NewDescriptionValuePair(
      "Driver Vulkan API version",
      gpu::VulkanVersionToString(
          gpu_info.dx12_vulkan_version_info.vulkan_version)));
#endif

  basic_info->Append(
      NewDescriptionValuePair("Driver vendor", active_gpu.driver_vendor));
  basic_info->Append(
      NewDescriptionValuePair("Driver version", active_gpu.driver_version));
  basic_info->Append(NewDescriptionValuePair(
      "GPU CUDA compute capability major version",
      std::make_unique<base::Value>(active_gpu.cuda_compute_capability_major)));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
                                             gpu_info.pixel_shader_version));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
                                             gpu_info.vertex_shader_version));
  basic_info->Append(
      NewDescriptionValuePair("Max. MSAA samples", gpu_info.max_msaa_samples));
  basic_info->Append(NewDescriptionValuePair("Machine model name",
                                             gpu_info.machine_model_name));
  basic_info->Append(NewDescriptionValuePair("Machine model version",
                                             gpu_info.machine_model_version));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR", gpu_info.gl_vendor));
  basic_info->Append(
      NewDescriptionValuePair("GL_RENDERER", gpu_info.gl_renderer));
  basic_info->Append(
      NewDescriptionValuePair("GL_VERSION", gpu_info.gl_version));
  basic_info->Append(
      NewDescriptionValuePair("GL_EXTENSIONS", gpu_info.gl_extensions));
  basic_info->Append(NewDescriptionValuePair(
      "Disabled Extensions", gpu_feature_info.disabled_extensions));
  basic_info->Append(NewDescriptionValuePair(
      "Disabled WebGL Extensions", gpu_feature_info.disabled_webgl_extensions));
  basic_info->Append(NewDescriptionValuePair("Window system binding vendor",
                                             gpu_info.gl_ws_vendor));
  basic_info->Append(NewDescriptionValuePair("Window system binding version",
                                             gpu_info.gl_ws_version));
  basic_info->Append(NewDescriptionValuePair("Window system binding extensions",
                                             gpu_info.gl_ws_extensions));
#if defined(USE_X11)
  basic_info->Append(
      NewDescriptionValuePair("Window manager", ui::GuessWindowManagerName()));
  {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    std::string value;
    const char kXDGCurrentDesktop[] = "XDG_CURRENT_DESKTOP";
    if (env->GetVar(kXDGCurrentDesktop, &value))
      basic_info->Append(NewDescriptionValuePair(kXDGCurrentDesktop, value));
    const char kGDMSession[] = "GDMSESSION";
    if (env->GetVar(kGDMSession, &value))
      basic_info->Append(NewDescriptionValuePair(kGDMSession, value));
    basic_info->Append(NewDescriptionValuePair(
        "Compositing manager",
        ui::IsCompositingManagerPresent() ? "Yes" : "No"));
  }
#endif
  std::string direct_rendering_version;
  if (gpu_info.direct_rendering_version == "1") {
    direct_rendering_version = "indirect";
  } else if (gpu_info.direct_rendering_version == "2") {
    direct_rendering_version = "direct but version unknown";
  } else if (base::StartsWith(gpu_info.direct_rendering_version, "2.",
                              base::CompareCase::INSENSITIVE_ASCII)) {
    direct_rendering_version = gpu_info.direct_rendering_version;
    base::ReplaceFirstSubstringAfterOffset(&direct_rendering_version, 0, "2.",
                                           "DRI");
  } else {
    direct_rendering_version = "unknown";
  }
  basic_info->Append(NewDescriptionValuePair("Direct rendering version",
                                             direct_rendering_version));

  std::string reset_strategy =
      base::StringPrintf("0x%04x", gpu_info.gl_reset_notification_strategy);
  basic_info->Append(
      NewDescriptionValuePair("Reset notification strategy", reset_strategy));

  basic_info->Append(NewDescriptionValuePair(
      "GPU process crash count",
      std::make_unique<base::Value>(GpuProcessHost::GetGpuCrashCount())));

#if defined(USE_X11)
  basic_info->Append(NewDescriptionValuePair(
      "System visual ID", base::NumberToString(gpu_info.system_visual)));
  basic_info->Append(NewDescriptionValuePair(
      "RGBA visual ID", base::NumberToString(gpu_info.rgba_visual)));
#endif

  std::string buffer_formats;
  for (int i = 0; i <= static_cast<int>(gfx::BufferFormat::LAST); ++i) {
    const gfx::BufferFormat buffer_format = static_cast<gfx::BufferFormat>(i);
    if (i > 0)
      buffer_formats += ",  ";
    buffer_formats += gfx::BufferFormatToString(buffer_format);
    const bool supported = base::Contains(
        gpu_feature_info.supported_buffer_formats_for_allocation_and_texturing,
        buffer_format);
    buffer_formats += supported ? ": supported" : ": not supported";
  }
  basic_info->Append(NewDescriptionValuePair(
      "gfx::BufferFormats supported for allocation and texturing",
      buffer_formats));

  return basic_info;
}

std::unique_ptr<base::DictionaryValue> GpuInfoAsDictionaryValue() {
  auto info = std::make_unique<base::DictionaryValue>();

  const gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  const gpu::GpuFeatureInfo gpu_feature_info =
      GpuDataManagerImpl::GetInstance()->GetGpuFeatureInfo();
  auto basic_info = BasicGpuInfoAsListValue(gpu_info, gpu_feature_info);
  info->Set("basicInfo", std::move(basic_info));

#if defined(OS_WIN)
  auto dx_info = std::make_unique<base::Value>();
  if (gpu_info.dx_diagnostics.children.size())
    dx_info = DxDiagNodeToList(gpu_info.dx_diagnostics);
  info->Set("diagnostics", std::move(dx_info));
#endif

#if BUILDFLAG(ENABLE_VULKAN)
  if (gpu_info.vulkan_info) {
    auto blob = gpu_info.vulkan_info->Serialize();
    info->SetString("vulkanInfo", base::Base64Encode(blob));
  }
#endif

  return info;
}

std::unique_ptr<base::ListValue> CompositorInfo() {
  auto compositor_info = std::make_unique<base::ListValue>();

  compositor_info->Append(NewDescriptionValuePair(
      "Tile Update Mode",
      IsZeroCopyUploadEnabled() ? "Zero-copy" : "One-copy"));

  compositor_info->Append(NewDescriptionValuePair(
      "Partial Raster", IsPartialRasterEnabled() ? "Enabled" : "Disabled"));
  return compositor_info;
}

std::unique_ptr<base::ListValue> GpuMemoryBufferInfo() {
  auto gpu_memory_buffer_info = std::make_unique<base::ListValue>();

  gpu::GpuMemoryBufferSupport gpu_memory_buffer_support;

  const auto native_configurations =
      gpu::GetNativeGpuMemoryBufferConfigurations(&gpu_memory_buffer_support);
  for (size_t format = 0;
       format < static_cast<size_t>(gfx::BufferFormat::LAST) + 1; format++) {
    std::string native_usage_support;
    for (size_t usage = 0;
         usage < static_cast<size_t>(gfx::BufferUsage::LAST) + 1; usage++) {
      if (base::Contains(
              native_configurations,
              std::make_pair(static_cast<gfx::BufferFormat>(format),
                             static_cast<gfx::BufferUsage>(usage)))) {
        native_usage_support = base::StringPrintf(
            "%s%s %s", native_usage_support.c_str(),
            native_usage_support.empty() ? "" : ",",
            gfx::BufferUsageToString(static_cast<gfx::BufferUsage>(usage)));
      }
    }
    if (native_usage_support.empty())
      native_usage_support = base::StringPrintf("Software only");

    gpu_memory_buffer_info->Append(NewDescriptionValuePair(
        gfx::BufferFormatToString(static_cast<gfx::BufferFormat>(format)),
        native_usage_support));
  }
  return gpu_memory_buffer_info;
}

std::unique_ptr<base::ListValue> getDisplayInfo() {
  auto display_info = std::make_unique<base::ListValue>();
  const std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const auto& display : displays) {
    display_info->Append(NewDescriptionValuePair("Info ", display.ToString()));
    display_info->Append(NewDescriptionValuePair(
        "Color space information", display.color_space().ToString()));
    display_info->Append(NewDescriptionValuePair(
        "SDR white level in nits",
        base::NumberToString(display.sdr_white_level())));
    display_info->Append(NewDescriptionValuePair(
        "Bits per color component",
        base::NumberToString(display.depth_per_component())));
    display_info->Append(NewDescriptionValuePair(
        "Bits per pixel", base::NumberToString(display.color_depth())));
    if (display.display_frequency()) {
      display_info->Append(NewDescriptionValuePair(
          "Refresh Rate in Hz",
          base::NumberToString(display.display_frequency())));
    }
  }
  return display_info;
}

const char* GetProfileName(gpu::VideoCodecProfile profile) {
  switch (profile) {
    case gpu::VIDEO_CODEC_PROFILE_UNKNOWN:
      return "unknown";
    case gpu::H264PROFILE_BASELINE:
      return "h264 baseline";
    case gpu::H264PROFILE_MAIN:
      return "h264 main";
    case gpu::H264PROFILE_EXTENDED:
      return "h264 extended";
    case gpu::H264PROFILE_HIGH:
      return "h264 high";
    case gpu::H264PROFILE_HIGH10PROFILE:
      return "h264 high 10";
    case gpu::H264PROFILE_HIGH422PROFILE:
      return "h264 high 4:2:2";
    case gpu::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return "h264 high 4:4:4 predictive";
    case gpu::H264PROFILE_SCALABLEBASELINE:
      return "h264 scalable baseline";
    case gpu::H264PROFILE_SCALABLEHIGH:
      return "h264 scalable high";
    case gpu::H264PROFILE_STEREOHIGH:
      return "h264 stereo high";
    case gpu::H264PROFILE_MULTIVIEWHIGH:
      return "h264 multiview high";
    case gpu::HEVCPROFILE_MAIN:
      return "hevc main";
    case gpu::HEVCPROFILE_MAIN10:
      return "hevc main 10";
    case gpu::HEVCPROFILE_MAIN_STILL_PICTURE:
      return "hevc main still-picture";
    case gpu::VP8PROFILE_ANY:
      return "vp8";
    case gpu::VP9PROFILE_PROFILE0:
      return "vp9 profile0";
    case gpu::VP9PROFILE_PROFILE1:
      return "vp9 profile1";
    case gpu::VP9PROFILE_PROFILE2:
      return "vp9 profile2";
    case gpu::VP9PROFILE_PROFILE3:
      return "vp9 profile3";
    case gpu::DOLBYVISION_PROFILE0:
      return "dolby vision profile 0";
    case gpu::DOLBYVISION_PROFILE4:
      return "dolby vision profile 4";
    case gpu::DOLBYVISION_PROFILE5:
      return "dolby vision profile 5";
    case gpu::DOLBYVISION_PROFILE7:
      return "dolby vision profile 7";
    case gpu::DOLBYVISION_PROFILE8:
      return "dolby vision profile 8";
    case gpu::DOLBYVISION_PROFILE9:
      return "dolby vision profile 9";
    case gpu::THEORAPROFILE_ANY:
      return "theora";
    case gpu::AV1PROFILE_PROFILE_MAIN:
      return "av1 profile main";
    case gpu::AV1PROFILE_PROFILE_HIGH:
      return "av1 profile high";
    case gpu::AV1PROFILE_PROFILE_PRO:
      return "av1 profile pro";
  }
  NOTREACHED();
  return "";
}

std::unique_ptr<base::ListValue> GetVideoAcceleratorsInfo() {
  gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  auto info = std::make_unique<base::ListValue>();

  for (const auto& profile :
       gpu_info.video_decode_accelerator_capabilities.supported_profiles) {
    std::string codec_string =
        base::StringPrintf("Decode %s", GetProfileName(profile.profile));
    std::string resolution_string = base::StringPrintf(
        "%s to %s pixels%s", profile.min_resolution.ToString().c_str(),
        profile.max_resolution.ToString().c_str(),
        profile.encrypted_only ? " (encrypted)" : "");
    info->Append(NewDescriptionValuePair(codec_string, resolution_string));
  }

  for (const auto& profile :
       gpu_info.video_encode_accelerator_supported_profiles) {
    std::string codec_string =
        base::StringPrintf("Encode %s", GetProfileName(profile.profile));
    std::string resolution_string = base::StringPrintf(
        "%s to %s pixels, and/or %.3f fps",
        profile.min_resolution.ToString().c_str(),
        profile.max_resolution.ToString().c_str(),
        static_cast<double>(profile.max_framerate_numerator) /
            profile.max_framerate_denominator);
    info->Append(NewDescriptionValuePair(codec_string, resolution_string));
  }
  return info;
}

std::unique_ptr<base::ListValue> GetANGLEFeatures() {
  gpu::GpuExtraInfo gpu_extra_info =
      GpuDataManagerImpl::GetInstance()->GetGpuExtraInfo();
  auto angle_features_list = std::make_unique<base::ListValue>();
  for (const auto& feature : gpu_extra_info.angle_features) {
    auto angle_feature = std::make_unique<base::DictionaryValue>();
    angle_feature->SetString("name", feature.name);
    angle_feature->SetString("category", feature.category);
    angle_feature->SetString("description", feature.description);
    angle_feature->SetString("bug", feature.bug);
    angle_feature->SetString("status", feature.status);
    angle_feature->SetString("condition", feature.condition);
    angle_features_list->Append(std::move(angle_feature));
  }

  return angle_features_list;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class GpuMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<GpuMessageHandler>,
      public GpuDataManagerObserver,
      public ui::GpuSwitchingObserver {
 public:
  GpuMessageHandler();
  ~GpuMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // GpuDataManagerObserver implementation.
  void OnGpuInfoUpdate() override;

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched(gl::GpuPreference) override;

  // Messages
  void OnBrowserBridgeInitialized(const base::ListValue* list);
  void OnCallAsync(const base::ListValue* list);

  // Submessages dispatched from OnCallAsync
  std::unique_ptr<base::DictionaryValue> OnRequestClientInfo(
      const base::ListValue* list);
  std::unique_ptr<base::ListValue> OnRequestLogMessages(
      const base::ListValue* list);

 private:
  // True if observing the GpuDataManager (re-attaching as observer would
  // DCHECK).
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// GpuMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

GpuMessageHandler::GpuMessageHandler()
    : observing_(false) {
}

GpuMessageHandler::~GpuMessageHandler() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  GpuDataManagerImpl::GetInstance()->RemoveObserver(this);
}

/* BrowserBridge.callAsync prepends a requestID to these messages. */
void GpuMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "browserBridgeInitialized",
      base::BindRepeating(&GpuMessageHandler::OnBrowserBridgeInitialized,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "callAsync", base::BindRepeating(&GpuMessageHandler::OnCallAsync,
                                       base::Unretained(this)));
}

void GpuMessageHandler::OnCallAsync(const base::ListValue* args) {
  DCHECK_GE(args->GetSize(), static_cast<size_t>(2));
  // unpack args into requestId, submessage and submessageArgs
  bool ok;
  const base::Value* requestId;
  ok = args->Get(0, &requestId);
  DCHECK(ok);

  std::string submessage;
  ok = args->GetString(1, &submessage);
  DCHECK(ok);

  auto submessageArgs = std::make_unique<base::ListValue>();
  for (size_t i = 2; i < args->GetSize(); ++i) {
    const base::Value* arg;
    ok = args->Get(i, &arg);
    DCHECK(ok);

    submessageArgs->Append(arg->CreateDeepCopy());
  }

  // call the submessage handler
  std::unique_ptr<base::Value> ret;
  if (submessage == "requestClientInfo") {
    ret = OnRequestClientInfo(submessageArgs.get());
  } else if (submessage == "requestLogMessages") {
    ret = OnRequestLogMessages(submessageArgs.get());
  } else {  // unrecognized submessage
    NOTREACHED();
    return;
  }

  // call BrowserBridge.onCallAsyncReply with result
  if (ret) {
    web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onCallAsyncReply",
                                           *requestId, *ret);
  } else {
    web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onCallAsyncReply",
                                           *requestId);
  }
}

void GpuMessageHandler::OnBrowserBridgeInitialized(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Watch for changes in GPUInfo
  if (!observing_) {
    GpuDataManagerImpl::GetInstance()->AddObserver(this);
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }
  observing_ = true;

  // Tell GpuDataManager it should have full GpuInfo. If the
  // Gpu process has not run yet, this will trigger its launch.
  GpuDataManagerImpl::GetInstance()->RequestDxdiagDx12VulkanGpuInfoIfNeeded(
      kGpuInfoRequestAll, /*delayed*/ false);

  // Run callback immediately in case the info is ready and no update in the
  // future.
  OnGpuInfoUpdate();
}

std::unique_ptr<base::DictionaryValue> GpuMessageHandler::OnRequestClientInfo(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto dict = std::make_unique<base::DictionaryValue>();

  dict->SetString("version", GetContentClient()->browser()->GetProduct());
  dict->SetString("command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
  dict->SetString("operating_system",
                  base::SysInfo::OperatingSystemName() + " " +
                  base::SysInfo::OperatingSystemVersion());
  dict->SetString("angle_commit_id", ANGLE_COMMIT_HASH);
  dict->SetString("graphics_backend",
                  std::string("Skia/" STRINGIZE(SK_MILESTONE)
                              " " SKIA_COMMIT_HASH));
  dict->SetString("revision_identifier", GPU_LISTS_VERSION);

  return dict;
}

std::unique_ptr<base::ListValue> GpuMessageHandler::OnRequestLogMessages(
    const base::ListValue*) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return GpuDataManagerImpl::GetInstance()->GetLogMessages();
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  // Get GPU Info.
  const gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  auto gpu_info_val = GpuInfoAsDictionaryValue();

  // Add in blacklisting features
  auto feature_status = std::make_unique<base::DictionaryValue>();
  feature_status->Set("featureStatus", GetFeatureStatus());
  feature_status->Set("problems", GetProblems());
  auto workarounds = std::make_unique<base::ListValue>();
  for (const auto& workaround : GetDriverBugWorkarounds())
    workarounds->AppendString(workaround);
  feature_status->Set("workarounds", std::move(workarounds));
  gpu_info_val->Set("featureStatus", std::move(feature_status));
  if (!GpuDataManagerImpl::GetInstance()->IsGpuProcessUsingHardwareGpu()) {
    auto feature_status_for_hardware_gpu =
        std::make_unique<base::DictionaryValue>();
    feature_status_for_hardware_gpu->Set("featureStatus",
                                         GetFeatureStatusForHardwareGpu());
    feature_status_for_hardware_gpu->Set("problems",
                                         GetProblemsForHardwareGpu());
    auto workarounds_for_hardware_gpu = std::make_unique<base::ListValue>();
    for (const auto& workaround : GetDriverBugWorkaroundsForHardwareGpu())
      workarounds_for_hardware_gpu->AppendString(workaround);
    feature_status_for_hardware_gpu->Set(
        "workarounds", std::move(workarounds_for_hardware_gpu));
    gpu_info_val->Set("featureStatusForHardwareGpu",
                      std::move(feature_status_for_hardware_gpu));
    const gpu::GPUInfo gpu_info_for_hardware_gpu =
        GpuDataManagerImpl::GetInstance()->GetGPUInfoForHardwareGpu();
    const gpu::GpuFeatureInfo gpu_feature_info_for_hardware_gpu =
        GpuDataManagerImpl::GetInstance()->GetGpuFeatureInfoForHardwareGpu();
    auto gpu_info_for_hardware_gpu_val = BasicGpuInfoAsListValue(
        gpu_info_for_hardware_gpu, gpu_feature_info_for_hardware_gpu);
    gpu_info_val->Set("basicInfoForHardwareGpu",
                      std::move(gpu_info_for_hardware_gpu_val));
  }
  gpu_info_val->Set("compositorInfo", CompositorInfo());
  gpu_info_val->Set("gpuMemoryBufferInfo", GpuMemoryBufferInfo());
  gpu_info_val->Set("displayInfo", getDisplayInfo());
  gpu_info_val->Set("videoAcceleratorsInfo", GetVideoAcceleratorsInfo());
  gpu_info_val->Set("ANGLEFeatures", GetANGLEFeatures());

  // Send GPU Info to javascript.
  web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onGpuInfoUpdate",
                                         *(gpu_info_val.get()));
}

void GpuMessageHandler::OnGpuSwitched(gl::GpuPreference active_gpu_heuristic_) {
  // Currently, about:gpu page does not update GPU info after the GPU switch.
  // If there is something to be updated, the code should be added here.
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// GpuInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

GpuInternalsUI::GpuInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<GpuMessageHandler>());

  // Set up the chrome://gpu/ source.
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, CreateGpuHTMLSource());
}

}  // namespace content
