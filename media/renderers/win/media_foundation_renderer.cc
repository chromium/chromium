// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer.h"

#include <Audioclient.h>
#include <mferror.h>
#include <winuser.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/profiler/frame.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/wrapped_window_proc.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_context.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/base/win/hresults.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

ATOM g_video_window_class = 0;

// GPU vendor IDs
constexpr uint32_t kGpuVendorIdIntel = 0x8086;
constexpr uint32_t kGpuVendorIdNvidia = 0x10de;
constexpr uint32_t kGpuVendorIdAmd = 0x1002;
constexpr uint32_t kGpuVendorIdNone = 0x0000;
constexpr int kGpuVendorIdUnknown = -1;

constexpr uint32_t kGpuBitmaskIntel = 0x001;
constexpr uint32_t kGpuBitmaskNvidia = 0x001 << 1;
constexpr uint32_t kGpuBitmaskAmd = 0x001 << 2;
constexpr uint32_t kGpuBitmaskOther = 0x001 << 3;

constexpr uint32_t kMakeGpuNonActive = 4;

// Reported to UMA. Do NOT change or reuse existing values.
enum class GpuOrDisplayCount {
  kUnknown = 0,
  kOne = 1,
  kTwoOrMore = 2,  // We don't care if more than 2 gpus are present. So single
                   // and two or more is enough.
  kMaxValue = kTwoOrMore
};

// Reported to UMA. Do NOT change or reuse existing values.
enum class ActiveGpuInfo : uint32_t {
  kNone = 0,
  kIntel = kGpuBitmaskIntel,
  kNvidia = kGpuBitmaskNvidia,
  kAmd = kGpuBitmaskAmd,
  kOther = kGpuBitmaskOther,
  kIntelIntel = kIntel | (kIntel << kMakeGpuNonActive),
  kNvidiaIntel = kNvidia | (kIntel << kMakeGpuNonActive),
  kAmdIntel = kAmd | (kIntel << kMakeGpuNonActive),
  kOtherIntel = kOther | (kIntel << kMakeGpuNonActive),
  kIntelNvidia = kIntel | (kNvidia << kMakeGpuNonActive),
  kNvidiaNvidia = kNvidia | (kNvidia << kMakeGpuNonActive),
  kAmdNvidia = kAmd | (kNvidia << kMakeGpuNonActive),
  kOtherNvidia = kOther | (kNvidia << kMakeGpuNonActive),
  kIntelAmd = kIntel | (kAmd << kMakeGpuNonActive),
  kNvidiaAmd = kNvidia | (kAmd << kMakeGpuNonActive),
  kAmdAmd = kAmd | (kAmd << kMakeGpuNonActive),
  kOtherAmd = kOther | (kAmd << kMakeGpuNonActive),
  kIntelOther = kIntel | (kOther << kMakeGpuNonActive),
  kNvidiaOther = kNvidia | (kOther << kMakeGpuNonActive),
  kAmdOther = kAmd | (kOther << kMakeGpuNonActive),
  kOtherOther = kOther | (kOther << kMakeGpuNonActive),
  kMaxValue = kOtherOther
};

// Reported to UMA. Do NOT change or reuse existing values.
enum class ActiveGpuDisplayInfo {
  kUnknown = 0,
  kLikelyBuiltIn = 1,   // Likely built-in display
  kLikelyExternal = 2,  // Likely external display
  kMaxValue = kLikelyExternal
};

// The |g_video_window_class| atom obtained is used as the |lpClassName|
// parameter in CreateWindowEx().
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa
//
// To enable OPM
// (https://docs.microsoft.com/en-us/windows/win32/medfound/output-protection-manager)
// protection for video playback, We call CreateWindowEx() to get a window
// and pass it to MFMediaEngine as an attribute.
bool InitializeVideoWindowClass() {
  if (g_video_window_class)
    return true;

  WNDCLASSEX intermediate_class;
  base::win::InitializeWindowClass(
      L"VirtualMediaFoundationCdmVideoWindow",
      &base::win::WrappedWindowProc<::DefWindowProc>, CS_OWNDC, 0, 0, nullptr,
      reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr, nullptr,
      nullptr, &intermediate_class);
  g_video_window_class = RegisterClassEx(&intermediate_class);
  if (!g_video_window_class) {
    HRESULT register_class_error = HRESULT_FROM_WIN32(GetLastError());
    DLOG(ERROR) << "RegisterClass failed: " << PrintHr(register_class_error);
    return false;
  }

  return true;
}

const std::string GetErrorReasonString(
    const MediaFoundationRenderer::ErrorReason& reason) {
#define STRINGIFY(value)                            \
  case MediaFoundationRenderer::ErrorReason::value: \
    return #value
  switch (reason) {
    STRINGIFY(kUnknown);
    STRINGIFY(kCdmProxyReceivedInInvalidState);
    STRINGIFY(kFailedToSetSourceOnMediaEngine);
    STRINGIFY(kFailedToSetCurrentTime);
    STRINGIFY(kFailedToPlay);
    STRINGIFY(kOnPlaybackError);
    STRINGIFY(kOnDCompSurfaceHandleSetError);
    STRINGIFY(kOnConnectionError);
    STRINGIFY(kFailedToSetDCompMode);
    STRINGIFY(kFailedToGetDCompSurface);
    STRINGIFY(kFailedToDuplicateHandle);
    STRINGIFY(kFailedToCreateMediaEngine);
    STRINGIFY(kFailedToCreateDCompTextureWrapper);
    STRINGIFY(kFailedToInitDCompTextureWrapper);
    STRINGIFY(kFailedToSetPlaybackRate);
    STRINGIFY(kFailedToGetMediaEngineEx);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // "This return value is no longer used, but may occur in older versions of
    // windows."
    STRINGIFY(kOnDCompSurfaceReceivedError);
#pragma clang diagnostic pop
  }
#undef STRINGIFY
}

// INVALID_HANDLE_VALUE is the official invalid handle value. Historically, 0 is
// not used as a handle value too.
bool IsInvalidHandle(const HANDLE& handle) {
  return handle == INVALID_HANDLE_VALUE || handle == nullptr;
}

std::tuple<uint32_t, LUID> GetVendorIdAndLUIDFromD3D11Device(
    IMFDXGIDeviceManager* dxgi_device_manager) {
  DCHECK(dxgi_device_manager);

  DXGIDeviceScopedHandle dxgi_device_handle(dxgi_device_manager);
  ComPtr<ID3D11Device> d3d11_device = dxgi_device_handle.GetDevice();
  if (!d3d11_device) {
    return {kGpuVendorIdNone, {}};
  }

  ComPtr<IDXGIDevice> dxgi_device;
  HRESULT hr = d3d11_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
  if (FAILED(hr)) {
    return {kGpuVendorIdNone, {}};
  }

  ComPtr<IDXGIAdapter> adapter;
  hr = dxgi_device->GetAdapter(&adapter);
  if (FAILED(hr)) {
    return {kGpuVendorIdNone, {}};
  }

  DXGI_ADAPTER_DESC desc = {};
  hr = adapter->GetDesc(&desc);
  if (FAILED(hr)) {
    return {kGpuVendorIdNone, {}};
  }

  return {desc.VendorId, desc.AdapterLuid};
}

uint32_t GpuVendorIdToBitmask(const uint32_t vendor_id) {
  if (vendor_id == kGpuVendorIdIntel) {
    return kGpuBitmaskIntel;
  }
  if (vendor_id == kGpuVendorIdNvidia) {
    return kGpuBitmaskNvidia;
  }
  if (vendor_id == kGpuVendorIdAmd) {
    return kGpuBitmaskAmd;
  }
  if (vendor_id == kGpuVendorIdNone) {
    return 0;
  }
  return kGpuBitmaskOther;
}

// Get non-active GPU vendor IDs.
std::vector<uint32_t> GetNonActiveGpuVendorIds(const LUID& active_gpu_luid) {
  std::vector<uint32_t> vendor_ids;
  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory)))) {
    return vendor_ids;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  for (UINT i = 0; SUCCEEDED(dxgi_factory->EnumAdapters(i, &adapter)); ++i) {
    DXGI_ADAPTER_DESC adapter_desc;
    if (SUCCEEDED(adapter->GetDesc(&adapter_desc))) {
      if (adapter_desc.AdapterLuid.HighPart == active_gpu_luid.HighPart &&
          adapter_desc.AdapterLuid.LowPart == active_gpu_luid.LowPart) {
        continue;
      }
      // Ignore software renderer based GPUs. See gpu/config/gpu_info.cc
      if (adapter_desc.VendorId == 0x0000 || adapter_desc.VendorId == 0xFFFF ||
          adapter_desc.VendorId == 0x15ad ||
          (adapter_desc.VendorId == 0x1414 &&
           adapter_desc.DeviceId == 0x008c)) {
        DVLOG(3) << __func__ << ": Adapter " << i << " Vendor ID: 0x"
                 << std::hex << adapter_desc.VendorId << ", Device ID: 0x"
                 << std::hex << adapter_desc.DeviceId
                 << " is a software renderer!";
        continue;
      }
      vendor_ids.push_back(adapter_desc.VendorId);
      DVLOG(3) << __func__ << ": Adapter " << i << " Vendor ID: 0x" << std::hex
               << adapter_desc.VendorId;
    }
    adapter.Reset();
  }
  return vendor_ids;
}

// Callback function that EnumDisplayMonitors calls for each monitor.
BOOL CALLBACK MyMonitorEnumProc(
    HMONITOR hMonitor,   // Handle to display monitor
    HDC hdcMonitor,      // Handle to monitor DC
    LPRECT lprcMonitor,  // Monitor intersection rectangle
    LPARAM dwData        // Data passed from EnumDisplayMonitors
) {
  if (!dwData) {
    return FALSE;
  }
  // Cast dwData back to the integer pointer we passed in.
  int* monitorCount = reinterpret_cast<int*>(dwData);
  // Increment the count for each monitor found.
  (*monitorCount)++;
  return TRUE;  // Return TRUE to continue the enumeration.
}

// Get the total number of attached displays.
int GetTotalDisplayCount() {
  int count = 0;
  BOOL result = EnumDisplayMonitors(nullptr, nullptr, MyMonitorEnumProc,
                                    reinterpret_cast<LPARAM>(&count));
  if (!result) {
    // This case is unlikely for standard usage but good to be aware of.
    DVLOG(1) << "EnumDisplayMonitors failed: " << GetLastError();
    return -1;  // Indicate an error
  }
  return count;
}

ActiveGpuDisplayInfo GetActiveGpuDisplayInfo(const LUID& active_gpu_luid) {
  UINT32 num_paths = 0;
  UINT32 num_modes = 0;

  // Get required buffer sizes for active paths
  LONG status = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &num_paths,
                                            &num_modes);

  if (status != ERROR_SUCCESS) {
    DVLOG(1) << __func__ << ": GetDisplayConfigBufferSizes failed: " << status;
    return ActiveGpuDisplayInfo::kUnknown;
  }

  std::vector<DISPLAYCONFIG_PATH_INFO> paths(num_paths);
  std::vector<DISPLAYCONFIG_MODE_INFO> modes(num_modes);

  // Query the display configuration
  status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &num_paths, paths.data(),
                              &num_modes, modes.data(), nullptr);

  if (status != ERROR_SUCCESS) {
    DVLOG(1) << __func__ << ": QueryDisplayConfig failed: " << status;
    return ActiveGpuDisplayInfo::kUnknown;
  }

  // Iterate through paths and retrieve target device info
  for (UINT32 i = 0; i < num_paths; ++i) {
    DISPLAYCONFIG_PATH_INFO& current_path = paths[i];
    DISPLAYCONFIG_TARGET_DEVICE_NAME target_name = {};
    target_name.header.size = sizeof(target_name);
    target_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
    target_name.header.adapterId = current_path.targetInfo.adapterId;
    target_name.header.id = current_path.targetInfo.id;

    status = DisplayConfigGetDeviceInfo(&target_name.header);
    if (status != ERROR_SUCCESS) {
      continue;
    }

    if (active_gpu_luid.HighPart == target_name.header.adapterId.HighPart &&
        active_gpu_luid.LowPart == target_name.header.adapterId.LowPart) {
      // Check specifically for embedded types to infer "built-in"
      if (target_name.outputTechnology ==
              DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS ||
          target_name.outputTechnology ==
              DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED ||
          target_name.outputTechnology ==
              DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL) {
        DVLOG(3) << __func__ << ": This is likely a BUILT-IN display.";
        return ActiveGpuDisplayInfo::kLikelyBuiltIn;
      } else {
        DVLOG(3) << __func__ << ": This is likely an EXTERNAL display.";
        return ActiveGpuDisplayInfo::kLikelyExternal;
      }
    }
  }
  return ActiveGpuDisplayInfo::kUnknown;
}

// Get GPU LUID from display device name.
LUID GetDisplayGpuLuid(const std::wstring& display_device_name) {
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
    LOG(ERROR) << "Failed to create DXGIFactory1.";
    return {};
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  for (UINT i = 0; factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND;
       ++i) {
    DXGI_ADAPTER_DESC adapter_desc;
    if (SUCCEEDED(adapter->GetDesc(&adapter_desc))) {
      LUID adapter_luid = adapter_desc.AdapterLuid;

      Microsoft::WRL::ComPtr<IDXGIOutput> output;
      for (UINT j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND;
           ++j) {
        DXGI_OUTPUT_DESC output_desc;
        if (SUCCEEDED(output->GetDesc(&output_desc))) {
          if (display_device_name == output_desc.DeviceName) {
            return adapter_luid;
          }
        }
      }
    }
  }
  return {};
}

// Get the display device name for the nearest display to the specified window.
std::wstring GetNearestDisplayDeviceNameFromWindow(HWND virtual_video_window) {
  DCHECK(virtual_video_window);

  // Get the monitor handle for the window
  HMONITOR monitor =
      MonitorFromWindow(virtual_video_window, MONITOR_DEFAULTTONEAREST);
  if (!monitor) {
    LOG(ERROR) << "Could not get monitor from window.";
    return std::wstring();
  }

  MONITORINFOEXW monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfoW(monitor, &monitor_info)) {
    LOG(ERROR) << "Could not get monitor info.";
    return std::wstring();
  }

  // display_device_name is the display identifier, e.g., L"\\\\.\\DISPLAY1"
  std::wstring display_device_name = monitor_info.szDevice;
  DVLOG(3) << __func__ << ": Window is on display: " << display_device_name;
  return display_device_name;
}

bool DoesGpuMatchWithDisplayOnMultiGpu(HWND virtual_video_window,
                                       const LUID& active_gpu_luid) {
  std::wstring display_device_name =
      GetNearestDisplayDeviceNameFromWindow(virtual_video_window);
  if (display_device_name.empty()) {
    LOG(ERROR) << "Display device name is empty.";
    return false;
  }

  LUID display_gpu_luid = GetDisplayGpuLuid(display_device_name);
  DVLOG(3) << __func__
           << ": display_gpu_luid.HigPart=" << display_gpu_luid.HighPart
           << ", display_gpu_luid.LowPart=" << display_gpu_luid.LowPart
           << ", active_gpu_luid.HighPart=" << active_gpu_luid.HighPart
           << ", active_gpu_luid.LowPart=" << active_gpu_luid.LowPart;

  return (active_gpu_luid.LowPart == display_gpu_luid.LowPart &&
          active_gpu_luid.HighPart == display_gpu_luid.HighPart);
}

void ReportGpuInfoUma(const std::string& uma_prefix,
                      IMFDXGIDeviceManager* dxgi_device_manager,
                      HWND virtual_video_window) {
  // For some tests, this can be nullptr.
  if (!dxgi_device_manager) {
    return;
  }

  const auto [active_gpu_vendor_id, active_gpu_luid] =
      GetVendorIdAndLUIDFromD3D11Device(dxgi_device_manager);
  if (active_gpu_vendor_id == kGpuVendorIdNone &&
      active_gpu_luid.LowPart == 0 && active_gpu_luid.HighPart == 0) {
    DVLOG(1) << __func__ << ": Failed to get active GPU info.";
    base::UmaHistogramEnumeration(uma_prefix + ".GpuCount",
                                  GpuOrDisplayCount::kUnknown);
    base::UmaHistogramEnumeration(uma_prefix + ".ActiveGpuInfo",
                                  ActiveGpuInfo::kNone);
    base::UmaHistogramEnumeration(uma_prefix + ".ActiveGpuDisplayInfo",
                                  ActiveGpuDisplayInfo::kUnknown);
    base::UmaHistogramSparse(uma_prefix + ".ActiveGpuVendorId",
                             kGpuVendorIdUnknown);
    base::UmaHistogramSparse(uma_prefix + ".NonActiveGpuVendorId",
                             kGpuVendorIdUnknown);
  } else {
    const auto all_nonactive_gpus = GetNonActiveGpuVendorIds(active_gpu_luid);
    const auto nonactive_gpu_count = all_nonactive_gpus.size();
    const bool is_multi_gpu = nonactive_gpu_count > 0;
    const auto nonactive_gpu_id =
        is_multi_gpu ? all_nonactive_gpus[0] : kGpuVendorIdNone;
    const auto active_gpu_info = static_cast<ActiveGpuInfo>(
        GpuVendorIdToBitmask(active_gpu_vendor_id) |
        (GpuVendorIdToBitmask(nonactive_gpu_id) << kMakeGpuNonActive));
    const auto active_gpu_display_info =
        GetActiveGpuDisplayInfo(active_gpu_luid);

    DVLOG(3) << __func__ << ": nonactive_gpu_count=" << nonactive_gpu_count
             << ", is_multi_gpu=" << is_multi_gpu
             << ", active_gpu_vendor_id=" << active_gpu_vendor_id
             << ", nonactive_gpu_id=" << nonactive_gpu_id
             << ", active_gpu_info=" << static_cast<uint32_t>(active_gpu_info)
             << ", active_gpu_display_info="
             << static_cast<uint32_t>(active_gpu_display_info);

    base::UmaHistogramEnumeration(
        uma_prefix + ".GpuCount",
        is_multi_gpu ? GpuOrDisplayCount::kTwoOrMore : GpuOrDisplayCount::kOne);
    base::UmaHistogramEnumeration(uma_prefix + ".ActiveGpuInfo",
                                  active_gpu_info);
    base::UmaHistogramEnumeration(uma_prefix + ".ActiveGpuDisplayInfo",
                                  active_gpu_display_info);
    base::UmaHistogramSparse(uma_prefix + ".ActiveGpuVendorId",
                             active_gpu_vendor_id);
    if (nonactive_gpu_id != kGpuVendorIdNone) {
      base::UmaHistogramSparse(uma_prefix + ".NonActiveGpuVendorId",
                               nonactive_gpu_id);
    }

    // On multi-gpu devices with NVIDIA active gpu associated with an external
    // display
    const auto multigpu_nvidia_active_with_external =
        is_multi_gpu && active_gpu_vendor_id == kGpuVendorIdNvidia &&
        active_gpu_display_info == ActiveGpuDisplayInfo::kLikelyExternal;
    DVLOG(3) << __func__ << ": multigpu_nvidia_active_with_external="
             << multigpu_nvidia_active_with_external;
    base::UmaHistogramBoolean(
        uma_prefix + ".MultiGpuNvidiaActiveWithExternalDisplay",
        multigpu_nvidia_active_with_external);

    // On multi-gpu devices where the active gpu associated with an external
    // display
    const auto multigpu_with_external =
        is_multi_gpu &&
        active_gpu_display_info == ActiveGpuDisplayInfo::kLikelyExternal;
    DVLOG(3) << __func__
             << ": multigpu_with_external=" << multigpu_with_external;
    base::UmaHistogramBoolean(uma_prefix + ".MultiGpuWithExternalDisplay",
                              multigpu_with_external);

    // Check if the active gpu matches with the display gpu on multi-gpu. For
    // example, with NVIDIA-Intel gpu setup, the browser picked Intel gpu as
    // Active gpu but the browser window's nearest display is associated with
    // NVIDIA gpu. In this case, the playback will fail all the time.
    if (is_multi_gpu && virtual_video_window) {
      const auto does_gpu_match_with_display_on_multigpu =
          DoesGpuMatchWithDisplayOnMultiGpu(virtual_video_window,
                                            active_gpu_luid);
      DVLOG(3) << __func__ << ": does_gpu_match_with_display_on_multigpu="
               << does_gpu_match_with_display_on_multigpu;
      base::UmaHistogramBoolean(
          uma_prefix + ".DoesGpuMatchWithDisplayOnMultiGpu",
          does_gpu_match_with_display_on_multigpu);
    }
  }

  const auto display_count = GetTotalDisplayCount();
  DVLOG(3) << __func__ << ": display_count=" << display_count;
  base::UmaHistogramEnumeration(
      uma_prefix + ".DisplayCount",
      display_count == -1 ? GpuOrDisplayCount::kUnknown
                          : (display_count > 1 ? GpuOrDisplayCount::kTwoOrMore
                                               : GpuOrDisplayCount::kOne));
}

std::string RenderedVideoFrameDetectionResultToString(
    MediaFoundationRenderer::RenderedVideoFrameDetectionResult reasult) {
  switch (reasult) {
    case MediaFoundationRenderer::RenderedVideoFrameDetectionResult::kDetected:
      return "Detected";
    case MediaFoundationRenderer::RenderedVideoFrameDetectionResult::
        kNotDetected:
      return "NotDetected";
    case MediaFoundationRenderer::RenderedVideoFrameDetectionResult::
        kUnknownByPlaybackError:
      return "UnknownByPlaybackError";
    case MediaFoundationRenderer::RenderedVideoFrameDetectionResult::
        kUnknownByPlaybackEnd:
      return "UnknownByPlaybackEnd";
    case MediaFoundationRenderer::RenderedVideoFrameDetectionResult::
        kUnknownByShutdown:
      return "UnknownByShutdown";
  }
}

}  // namespace

// static
void MediaFoundationRenderer::ReportErrorReason(ErrorReason reason) {
  base::UmaHistogramEnumeration("Media.MediaFoundationRenderer.ErrorReason",
                                reason);
}

MediaFoundationRenderer::MediaFoundationRenderer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log,
    LUID gpu_process_adapter_luid,
    bool is_testing)
    : task_runner_(std::move(task_runner)),
      media_log_(std::move(media_log)),
      gpu_process_adapter_luid_(gpu_process_adapter_luid),
      is_testing_(is_testing) {
  DVLOG_FUNC(1);
}

MediaFoundationRenderer::~MediaFoundationRenderer() {
  DVLOG_FUNC(1);

  // Perform shutdown/cleanup in the order (shutdown/detach/destroy) we wanted
  // without depending on the order of destructors being invoked. We also need
  // to invoke MFShutdown() after shutdown/cleanup of MF related objects.

  StopSendingStatistics(StopSendingStatisticsReason::kShutdown);

  // 'mf_media_engine_notify_' should be shutdown first as errors are possible
  // if source is being created while shutdown is called (causing
  // ERROR_FILE_NOT_FOUND from Media Foundations). These errors should be
  // ignored by 'mf_media_engine_notify_' instead of being propagated up.
  if (mf_media_engine_notify_) {
    mf_media_engine_notify_->Shutdown();
  }
  if (mf_media_engine_extension_) {
    mf_media_engine_extension_->Shutdown();
  }
  if (mf_media_engine_) {
    mf_media_engine_->Shutdown();
  }

  if (mf_source_) {
    mf_source_->DetachResource();
  }

  if (dxgi_device_manager_) {
    dxgi_device_manager_.Reset();
    MFUnlockDXGIDeviceManager();
  }
  if (virtual_video_window_) {
    DestroyWindow(virtual_video_window_);
  }
}

void MediaFoundationRenderer::Initialize(MediaResource* media_resource,
                                         RendererClient* client,
                                         PipelineStatusCallback init_cb) {
  DVLOG_FUNC(1);

  renderer_client_ = client;

  // MediaFoundationRenderer now only support DirectComposition mode.
  MEDIA_LOG(INFO, media_log_)
      << "Starting MediaFoundationRenderer: DirectComposition";

  HRESULT hr = CreateMediaEngine(media_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create media engine: " << PrintHr(hr);
    base::UmaHistogramSparse(
        "Media.MediaFoundationRenderer.CreateMediaEngineError", hr);
    OnError(PIPELINE_ERROR_INITIALIZATION_FAILED,
            ErrorReason::kFailedToCreateMediaEngine, hr, std::move(init_cb));
    return;
  }

  SetVolume(volume_);
  std::move(init_cb).Run(PIPELINE_OK);
}

HRESULT MediaFoundationRenderer::CreateMediaEngine(
    MediaResource* media_resource) {
  DVLOG_FUNC(1);

  if (!InitializeMediaFoundation())
    return kErrorInitializeMediaFoundation;

  // Set `cdm_proxy_` early on so errors can be reported via the CDM for better
  // error aggregation. See `CdmDocumentServiceImpl::OnCdmEvent()`.
  if (cdm_context_) {
    cdm_proxy_ = cdm_context_->GetMediaFoundationCdmProxy();
    if (!cdm_proxy_) {
      DLOG(ERROR) << __func__ << ": CDM does not support MF CDM interface";
      return kErrorInvalidCdmProxy;
    }
  }

  std::optional<media::VideoDecoderConfig> video_decoder_config = std::nullopt;
  std::optional<media::AudioDecoderConfig> audio_decoder_config = std::nullopt;

  // Only call the following when there is a video stream.
  for (media::DemuxerStream* stream : media_resource->GetAllStreams()) {
    if (stream->type() == media::DemuxerStream::VIDEO) {
      video_decoder_config = stream->video_decoder_config();
      RETURN_IF_FAILED(InitializeDXGIDeviceManager());
      RETURN_IF_FAILED(InitializeVirtualVideoWindow());
      break;
    } else if (stream->type() == media::DemuxerStream::AUDIO) {
      audio_decoder_config = stream->audio_decoder_config();
    }
  }

  // The OnXxx() callbacks are invoked by MF threadpool thread, we would like
  // to bind the callbacks to |task_runner_| MessageLoop.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto weak_this = weak_factory_.GetWeakPtr();
  RETURN_IF_FAILED(MakeAndInitialize<MediaEngineNotifyImpl>(
      &mf_media_engine_notify_,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnPlaybackError, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnPlaybackEnded, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnFormatChange, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnLoadedData, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnCanPlayThrough, weak_this)),
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&MediaFoundationRenderer::OnPlaying, weak_this)),
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&MediaFoundationRenderer::OnWaiting, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnFrameStepCompleted, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnTimeUpdate, weak_this)),
      video_decoder_config, audio_decoder_config));

  ComPtr<IMFAttributes> creation_attributes;
  RETURN_IF_FAILED(MFCreateAttributes(&creation_attributes, 6));
  RETURN_IF_FAILED(creation_attributes->SetUnknown(
      MF_MEDIA_ENGINE_CALLBACK, mf_media_engine_notify_.Get()));
  RETURN_IF_FAILED(
      creation_attributes->SetUINT32(MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS,
                                     MF_MEDIA_ENGINE_ENABLE_PROTECTED_CONTENT));
  RETURN_IF_FAILED(creation_attributes->SetUINT32(
      MF_MEDIA_ENGINE_AUDIO_CATEGORY, AudioCategory_Media));
  if (virtual_video_window_) {
    RETURN_IF_FAILED(creation_attributes->SetUINT64(
        MF_MEDIA_ENGINE_OPM_HWND,
        reinterpret_cast<uint64_t>(virtual_video_window_)));
  }

  if (dxgi_device_manager_) {
    RETURN_IF_FAILED(creation_attributes->SetUnknown(
        MF_MEDIA_ENGINE_DXGI_MANAGER, dxgi_device_manager_.Get()));

  }

  RETURN_IF_FAILED(
      MakeAndInitialize<MediaEngineExtension>(&mf_media_engine_extension_));
  RETURN_IF_FAILED(creation_attributes->SetUnknown(
      MF_MEDIA_ENGINE_EXTENSION, mf_media_engine_extension_.Get()));

  ComPtr<IMFMediaEngineClassFactory> class_factory;
  RETURN_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&class_factory)));

  DWORD creation_flags = 0;
  // Enable low-latency mode if latency hint is low.
  if (latency_hint_.has_value() && (*latency_hint_ <= base::Milliseconds(50))) {
    creation_flags |= MF_MEDIA_ENGINE_REAL_TIME_MODE;
  }
  RETURN_IF_FAILED(class_factory->CreateInstance(
      creation_flags, creation_attributes.Get(), &mf_media_engine_));

  // The Media Foundation Media Engine has an initial playback rate of 1.0, but
  // chromium uses an initial playback rate of 0.0. The Media Engine's topology
  // may not be completely loaded at this point - so we use
  // SetDefaultPlaybackRate as using SetPlaybackRate may be overwritten while
  // the topology is loading.
  RETURN_IF_FAILED(mf_media_engine_->SetDefaultPlaybackRate(0.0));

  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationSourceWrapper>(
      &mf_source_, media_resource, media_log_.get(), task_runner_));

  std::ignore = SetDCompModeInternal();

  if (!mf_source_->HasEncryptedStream()) {
    // Supports clear stream for testing.
    return SetSourceOnMediaEngine();
  }

  // Has encrypted stream.
  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationProtectionManager>(
      &content_protection_manager_, task_runner_,
      base::BindRepeating(&MediaFoundationRenderer::OnProtectionManagerWaiting,
                          weak_factory_.GetWeakPtr())));
  ComPtr<IMFMediaEngineProtectedContent> protected_media_engine;
  RETURN_IF_FAILED(mf_media_engine_.As(&protected_media_engine));
  RETURN_IF_FAILED(protected_media_engine->SetContentProtectionManager(
      content_protection_manager_.Get()));

  waiting_for_mf_cdm_ = true;
  if (!cdm_context_) {
    DCHECK(!cdm_proxy_);
    return S_OK;
  }

  DCHECK(cdm_proxy_);
  OnCdmProxyReceived();
  return S_OK;
}

HRESULT MediaFoundationRenderer::SetSourceOnMediaEngine() {
  DVLOG_FUNC(1);

  if (!mf_source_) {
    LOG(ERROR) << "mf_source_ is null.";
    return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
  }

  ComPtr<IUnknown> source_unknown;
  RETURN_IF_FAILED(mf_source_.As(&source_unknown));
  RETURN_IF_FAILED(
      mf_media_engine_extension_->SetMediaSource(source_unknown.Get()));

  DVLOG(2) << "Set MFRendererSrc scheme as the source for MFMediaEngine.";
  base::win::ScopedBstr mf_renderer_source_scheme(
      base::ASCIIToWide("MFRendererSrc"));
  // We need to set our source scheme first in order for the MFMediaEngine to
  // load of our custom MFMediaSource.
  RETURN_IF_FAILED(
      mf_media_engine_->SetSource(mf_renderer_source_scheme.Get()));

  return S_OK;
}

HRESULT MediaFoundationRenderer::InitializeDXGIDeviceManager() {
  UINT device_reset_token;
  RETURN_IF_FAILED(
      MFLockDXGIDeviceManager(&device_reset_token, &dxgi_device_manager_));
  // `dxgi_device_manager_` returned is a singleton object, thus all
  // MediaFoundationRenderer instances will all receive the
  // `dxgi_device_manager_` pointing to the same object. Therefore we only need
  // to and can only call `ResetDevice()` once, If it's called more than once,
  // all open device handles become invalid, even when it is the same device as
  // before. This will cause an existing instance attempting to use the invalid
  // handle to error out.
  // https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfdxgidevicemanager-resetdevice
  DXGIDeviceScopedHandle dxgi_device_handle(dxgi_device_manager_.Get());
  if (dxgi_device_handle.GetDevice()) {
    return S_OK;
  }

  ComPtr<ID3D11Device> d3d11_device;
  UINT creation_flags =
      (D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
       D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
  static const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
      D3D_FEATURE_LEVEL_9_1};

  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  RETURN_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_to_use;
  // TODO(crbug.com/40899242): Need to handle the case when Adapter LUID is
  // specific per instance of the video playback. This will now allow all
  // instances to use the default DXGI device manager.
  if (gpu_process_adapter_luid_.LowPart || gpu_process_adapter_luid_.HighPart) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> temp_adapter;
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters(i, &temp_adapter)); i++) {
      DXGI_ADAPTER_DESC desc;
      RETURN_IF_FAILED(temp_adapter->GetDesc(&desc));
      if (desc.AdapterLuid.LowPart == gpu_process_adapter_luid_.LowPart &&
          desc.AdapterLuid.HighPart == gpu_process_adapter_luid_.HighPart) {
        adapter_to_use = std::move(temp_adapter);
        break;
      }
    }
  }

  HRESULT hr = D3D11CreateDevice(
      adapter_to_use.Get(),
      adapter_to_use ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, 0,
      creation_flags, feature_levels, std::size(feature_levels),
      D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr);
  if (FAILED(hr)) {
    base::UmaHistogramSparse(
        "Media.MediaFoundationRenderer.D3D11CreateDeviceFailed", hr);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
      // If hardware device creation fails, try creating a software device.
      // HWDRM cases require hardware security, which is not applicable for a
      // basic software GPU adapter without hardware-level security. Using 0 for
      // creation_flags is acceptable for basic video rendering, as warp devices
      // lack video support, and the warp adapter is a software GPU so
      // D3D11_CREATE_DEVICE_BGRA_SUPPORT and
      // D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS don't
      // apply.
      RETURN_IF_FAILED(D3D11CreateDevice(
          adapter_to_use.Get(),
          adapter_to_use ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
          0,
          /*creation_flags=*/0, feature_levels, std::size(feature_levels),
          D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr));
    } else {
      RETURN_IF_FAILED(hr);
    }
  }
  RETURN_IF_FAILED(media::SetDebugName(d3d11_device.Get(), "Media_MFRenderer"));

  ComPtr<ID3D10Multithread> multithreaded_device;
  RETURN_IF_FAILED(d3d11_device.As(&multithreaded_device));
  multithreaded_device->SetMultithreadProtected(TRUE);

  return dxgi_device_manager_->ResetDevice(d3d11_device.Get(),
                                           device_reset_token);
}

HRESULT MediaFoundationRenderer::InitializeVirtualVideoWindow() {
  if (!InitializeVideoWindowClass())
    return kErrorInitializeVideoWindowClass;

  virtual_video_window_ =
      CreateWindowEx(WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED | WS_EX_TRANSPARENT |
                         WS_EX_NOREDIRECTIONBITMAP,
                     reinterpret_cast<wchar_t*>(g_video_window_class), L"",
                     WS_POPUP | WS_DISABLED | WS_CLIPSIBLINGS, 0, 0, 1, 1,
                     nullptr, nullptr, nullptr, nullptr);
  if (!virtual_video_window_) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    DLOG(ERROR) << "Failed to create virtual window: " << PrintHr(hr);
    return hr;
  }

  return S_OK;
}

void MediaFoundationRenderer::SetCdm(CdmContext* cdm_context,
                                     CdmAttachedCB cdm_attached_cb) {
  DVLOG_FUNC(1);

  if (cdm_context_ || !cdm_context) {
    DLOG(ERROR) << "Failed in checking CdmContext.";
    std::move(cdm_attached_cb).Run(false);
    return;
  }

  cdm_context_ = cdm_context;

  if (waiting_for_mf_cdm_) {
    cdm_proxy_ = cdm_context_->GetMediaFoundationCdmProxy();
    if (!cdm_proxy_) {
      DLOG(ERROR) << "CDM does not support MediaFoundationCdmProxy";
      std::move(cdm_attached_cb).Run(false);
      return;
    }

    OnCdmProxyReceived();
  }

  std::move(cdm_attached_cb).Run(true);
}

void MediaFoundationRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  DVLOG_FUNC(1);

  if (latency_hint.has_value()) {
    DLOG_IF(WARNING, mf_media_engine_)
        << "Latency hint is not utilized after MF media engine creation.";
    CHECK(*latency_hint >= base::Milliseconds(0));
  }
  latency_hint_ = latency_hint;
}

void MediaFoundationRenderer::OnCdmProxyReceived() {
  DVLOG_FUNC(1);
  DCHECK(cdm_proxy_);

  if (!waiting_for_mf_cdm_ || !content_protection_manager_) {
    OnError(PIPELINE_ERROR_INVALID_STATE,
            ErrorReason::kCdmProxyReceivedInInvalidState,
            kErrorCdmProxyReceivedInInvalidState);
    return;
  }

  waiting_for_mf_cdm_ = false;
  content_protection_manager_->SetCdmProxy(cdm_proxy_);
  mf_source_->SetCdmProxy(cdm_proxy_);

  HRESULT hr = SetSourceOnMediaEngine();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetSourceOnMediaEngine, hr);
    return;
  }
}

void MediaFoundationRenderer::Flush(base::OnceClosure flush_cb) {
  DVLOG_FUNC(2);

  HRESULT hr = PauseInternal();
  // Ignore any Pause() error. We can continue to flush |mf_source_| instead of
  // stopping the playback with error.
  DVLOG_IF(1, FAILED(hr)) << "Failed to pause playback on flush: "
                          << PrintHr(hr);

  mf_source_->FlushStreams();
  std::move(flush_cb).Run();
}

void MediaFoundationRenderer::StartPlayingFrom(base::TimeDelta time) {
  double current_time = time.InSecondsF();
  DVLOG_FUNC(2) << "current_time=" << current_time;

  // Note: It is okay for |waiting_for_mf_cdm_| to be true here. The
  // MFMediaEngine supports calls to Play/SetCurrentTime before a source is set
  // (it will apply the relevant changes to the playback state once a source is
  // set on it).

  // SetCurrentTime() completes asynchronously. When the seek operation starts,
  // the MFMediaEngine sends an MF_MEDIA_ENGINE_EVENT_SEEKING event. When the
  // seek operation completes, the MFMediaEngine sends an
  // MF_MEDIA_ENGINE_EVENT_SEEKED event.
  HRESULT hr = mf_media_engine_->SetCurrentTime(current_time);
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetCurrentTime, hr);
    return;
  }

  hr = mf_media_engine_->Play();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER, ErrorReason::kFailedToPlay, hr);
    return;
  }
}

void MediaFoundationRenderer::SetPlaybackRate(double playback_rate) {
  DVLOG_FUNC(2) << "playback_rate=" << playback_rate;

  // If the Media Engine's topology has not finished loading then
  // the call to SetPlaybackRate may be overwritten. To work around this
  // we call SetDefaultPlaybackRate which would be picked up when transitioning
  // to the Play state.
  HRESULT hr = mf_media_engine_->SetDefaultPlaybackRate(playback_rate);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to set default playback rate: " << PrintHr(hr);
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetPlaybackRate, hr);
    return;
  }

  hr = mf_media_engine_->SetPlaybackRate(playback_rate);

  if (SUCCEEDED(hr)) {
    // Set the start time for the rendered video frame detection if playback
    // rate was set to 0 and changed to non-zero.
    if (playback_rate_ == 0.0 && playback_rate > 0.0) {
      RestartRenderedVideoFrameDetectionTimerInNotReported();
    }

    playback_rate_ = playback_rate;
  } else {
    DVLOG_IF(1, FAILED(hr)) << "Failed to set playback rate: " << PrintHr(hr);
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetPlaybackRate, hr);
  }
}

void MediaFoundationRenderer::GetDCompSurface(GetDCompSurfaceCB callback) {
  DVLOG_FUNC(1);

  HRESULT hr = SetDCompModeInternal();
  if (FAILED(hr)) {
    base::UmaHistogramSparse(
        "Media.MediaFoundationRenderer.FailedToSetDCompMode", hr);
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER, ErrorReason::kFailedToSetDCompMode,
            hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  HANDLE surface_handle = INVALID_HANDLE_VALUE;
  hr = GetDCompSurfaceInternal(&surface_handle);
  // The handle could still be invalid after a non failure (e.g. S_FALSE) is
  // returned. See https://crbug.com/1307065.
  if (FAILED(hr) || IsInvalidHandle(surface_handle)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToGetDCompSurface, hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  // Only need read & execute access right for the handle to be duplicated
  // without breaking in sandbox_win.cc!CheckDuplicateHandle().
  const base::ProcessHandle process = ::GetCurrentProcess();
  HANDLE duplicated_handle = INVALID_HANDLE_VALUE;
  const BOOL result = ::DuplicateHandle(
      process, surface_handle, process, &duplicated_handle,
      GENERIC_READ | GENERIC_EXECUTE, false, DUPLICATE_CLOSE_SOURCE);
  if (!result || IsInvalidHandle(surface_handle)) {
    hr = ::GetLastError();
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToDuplicateHandle, hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  std::move(callback).Run(base::win::ScopedHandle(duplicated_handle), "");
}

// TODO(crbug.com/40126181): Investigate if we need to add
// OnSelectedVideoTracksChanged() to media renderer.mojom.
void MediaFoundationRenderer::SetVideoStreamEnabled(bool enabled) {
  DVLOG_FUNC(1) << "enabled=" << enabled;
  if (!mf_source_)
    return;

  const bool needs_restart = mf_source_->SetVideoStreamEnabled(enabled);
  if (needs_restart) {
    // If the media source indicates that we need to restart playback (e.g due
    // to a newly enabled stream being EOS), queue a pause and play operation.
    PauseInternal();
    mf_media_engine_->Play();
  }
}

void MediaFoundationRenderer::SetOutputRect(const gfx::Rect& output_rect,
                                            SetOutputRectCB callback) {
  DVLOG_FUNC(2);

  // Call SetWindowPos to reposition the video from output_rect.
  if (virtual_video_window_ &&
      !::SetWindowPos(virtual_video_window_, HWND_BOTTOM, output_rect.x(),
                      output_rect.y(), output_rect.width(),
                      output_rect.height(), SWP_NOACTIVATE)) {
    DLOG(ERROR) << "Failed to SetWindowPos: "
                << PrintHr(HRESULT_FROM_WIN32(GetLastError()));
    std::move(callback).Run(false);
    return;
  }

  if (FAILED(UpdateVideoStream(output_rect.size()))) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

HRESULT MediaFoundationRenderer::UpdateVideoStream(const gfx::Size rect_size) {
  if (current_video_rect_size_ == rect_size) {
    return S_OK;
  }

  current_video_rect_size_ = rect_size;

  ComPtr<IMFMediaEngineEx> mf_media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&mf_media_engine_ex));

  RECT dest_rect = {0, 0, rect_size.width(), rect_size.height()};

  // https://learn.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengineex-updatevideostream
  // Updates the source rectangle, destination rectangle, and border color for
  // the video. Source is set to Null so the entire frame is displayed.
  // Position is not set because SetWindowPos sets the position already.
  // Destination rectangle relative to the top-left corner of the window
  // rect set in SetWindowPos.
  RETURN_IF_FAILED(mf_media_engine_ex->UpdateVideoStream(
      /*pSrc=*/nullptr, &dest_rect, /*pBorderClr=*/nullptr));

  // Set the start time for the rendered video frame detection.
  RestartRenderedVideoFrameDetectionTimerInNotReported();

  return S_OK;
}

HRESULT MediaFoundationRenderer::SetDCompModeInternal() {
  DVLOG_FUNC(1);

  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));
  RETURN_IF_FAILED(media_engine_ex->EnableWindowlessSwapchainMode(true));
  return S_OK;
}

HRESULT MediaFoundationRenderer::GetDCompSurfaceInternal(
    HANDLE* surface_handle) {
  DVLOG_FUNC(1);

  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));
  RETURN_IF_FAILED(media_engine_ex->GetVideoSwapchainHandle(surface_handle));
  return S_OK;
}

HRESULT MediaFoundationRenderer::PopulateStatistics(
    PipelineStatistics& statistics) {
  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));

  base::win::ScopedPropVariant frames_rendered;
  RETURN_IF_FAILED(media_engine_ex->GetStatistics(
      MF_MEDIA_ENGINE_STATISTIC_FRAMES_RENDERED, frames_rendered.Receive()));
  base::win::ScopedPropVariant frames_dropped;
  RETURN_IF_FAILED(media_engine_ex->GetStatistics(
      MF_MEDIA_ENGINE_STATISTIC_FRAMES_DROPPED, frames_dropped.Receive()));

  statistics.video_frames_decoded =
      frames_rendered.get().ulVal + frames_dropped.get().ulVal;
  statistics.video_frames_dropped = frames_dropped.get().ulVal;
  DVLOG_FUNC(3) << "video_frames_decoded=" << statistics.video_frames_decoded
                << ", video_frames_dropped=" << statistics.video_frames_dropped;
  return S_OK;
}

void MediaFoundationRenderer::SendStatistics() {
  PipelineStatistics new_stats = {};
  HRESULT hr = PopulateStatistics(new_stats);
  if (FAILED(hr)) {
    LIMITED_MEDIA_LOG(INFO, media_log_, populate_statistics_failure_count_, 3)
        << "MediaFoundationRenderer failed to populate stats: " + PrintHr(hr);
    return;
  }

  const int kSignificantPlaybackFrames = 5400;  // About 30 fps for 3 minutes.
  if (!has_reported_significant_playback_ && cdm_proxy_ &&
      new_stats.video_frames_decoded >= kSignificantPlaybackFrames) {
    has_reported_significant_playback_ = true;
    cdm_proxy_->OnSignificantPlayback();
  }

  CheckRenderedVideoFrame(new_stats);

  if (statistics_ != new_stats) {
    // OnStatisticsUpdate() expects delta values.
    PipelineStatistics delta;
    delta.video_frames_decoded = base::ClampSub(
        new_stats.video_frames_decoded, statistics_.video_frames_decoded);
    delta.video_frames_dropped = base::ClampSub(
        new_stats.video_frames_dropped, statistics_.video_frames_dropped);
    statistics_ = new_stats;
    renderer_client_->OnStatisticsUpdate(delta);
  }
}

void MediaFoundationRenderer::StartSendingStatistics() {
  DVLOG_FUNC(2);

  // Clear `statistics_` to reset the base for OnStatisticsUpdate(), this is
  // needed since flush will clear the internal stats in MediaFoundation.
  statistics_ = PipelineStatistics();

  const auto kPipelineStatsPollingPeriod = base::Milliseconds(500);
  statistics_timer_.Start(FROM_HERE, kPipelineStatsPollingPeriod, this,
                          &MediaFoundationRenderer::SendStatistics);

  // Set the start time for the rendered video frame detection.
  RestartRenderedVideoFrameDetectionTimerInNotReported();
}

void MediaFoundationRenderer::StopSendingStatistics(
    StopSendingStatisticsReason reason) {
  DVLOG_FUNC(2) << "reason=" << static_cast<int>(reason);

  statistics_timer_.Stop();

  // Conclude the rendered video frame detection only when the reason is not by
  // playback pause. Otherwise, just reset the start time.
  if (reason != StopSendingStatisticsReason::kPlaybackPauseInternal &&
      NeedRenderedVideoFrameDetection()) {
    DVLOG_FUNC(1) << "First rendered video frame check has not done yet. But "
                     "video is ended, failed or shutting down!";
    switch (reason) {
      case StopSendingStatisticsReason::kPlaybackEnded:
        ReportRenderedVideoFrameDetectionResult(
            RenderedVideoFrameDetectionResult::kUnknownByPlaybackEnd);
        break;
      case StopSendingStatisticsReason::kPlaybackError:
        ReportRenderedVideoFrameDetectionResult(
            RenderedVideoFrameDetectionResult::kUnknownByPlaybackError);
        break;
      case StopSendingStatisticsReason::kShutdown:
        ReportRenderedVideoFrameDetectionResult(
            RenderedVideoFrameDetectionResult::kUnknownByShutdown);
        break;
      case StopSendingStatisticsReason::kPlaybackPauseInternal:
        break;
    }
  }

  rendered_video_frame_detection_start_time_.reset();
}

bool MediaFoundationRenderer::NeedRenderedVideoFrameDetection() {
  // We need to check rendered video frame only if the detection check has never
  // done before and the start time is set.
  return !has_reported_rendered_video_frame_detection_ &&
         rendered_video_frame_detection_start_time_.has_value();
}

void MediaFoundationRenderer::CheckRenderedVideoFrame(
    const PipelineStatistics& stats) {
  if (!NeedRenderedVideoFrameDetection()) {
    return;
  }

  // Minimally required number of rendered video frames. 1 means any frame.
  const int kMinRenderedVideoFrames = 1;
  // Minimum 10 seconds to be considered something is rendered on the screen
  // regardless of the current playback rate.
  const base::TimeDelta kMinPlaybackTimeout = base::Seconds(10);
  DVLOG_FUNC(3) << "stats.video_frames_decoded=" << stats.video_frames_decoded
                << ", stats.video_frames_dropped="
                << stats.video_frames_dropped;

  // Use the number of rendered frames instead since frames dropped would
  // count towards "hanging". video_frames_decoded = rendered_frame +
  // video_frames_dropped.
  const uint32_t rendered_frame =
      stats.video_frames_decoded - stats.video_frames_dropped;

  if (rendered_frame >= kMinRenderedVideoFrames) {
    DVLOG_FUNC(1) << "First rendered video frame detected!";
    ReportRenderedVideoFrameDetectionResult(
        RenderedVideoFrameDetectionResult::kDetected);

    has_reported_rendered_video_frame_detection_ = true;
    rendered_video_frame_detection_start_time_.reset();
    return;
  }

  auto elapsed_time = base::TimeTicks::Now() -
                      rendered_video_frame_detection_start_time_.value();
  DVLOG_FUNC(3) << "elapsed_time=" << elapsed_time;

  // If the elapsed time is greater than or equal to `kMinPlaybackTimeout`,
  // consider it as no decode video frame detected.
  if (elapsed_time >= kMinPlaybackTimeout) {
    DVLOG_FUNC(1) << "Not enough rendered video frame detected (expected: "
                  << kMinRenderedVideoFrames << " vs actual: " << rendered_frame
                  << ") within the given time "
                  << kMinPlaybackTimeout.InSeconds() << " seconds!";
    ReportRenderedVideoFrameDetectionResult(
        RenderedVideoFrameDetectionResult::kNotDetected);

    has_reported_rendered_video_frame_detection_ = true;
    rendered_video_frame_detection_start_time_.reset();
  }
}

void MediaFoundationRenderer::
    RestartRenderedVideoFrameDetectionTimerInNotReported() {
  rendered_video_frame_detection_start_time_.reset();

  // Don't set the start time if the current playback rate is 0.0. For example,
  // format/size change can trigger this call while `playback_rate_` is still
  // zero.
  if (playback_rate_ == 0.0) {
    return;
  }

  if (!has_reported_rendered_video_frame_detection_) {
    rendered_video_frame_detection_start_time_ = base::TimeTicks::Now();
  }
}

void MediaFoundationRenderer::ReportRenderedVideoFrameDetectionResult(
    RenderedVideoFrameDetectionResult result) {
  DVLOG_FUNC(2) << "result=" << static_cast<int>(result);

  base::UmaHistogramSparse(
      "Media.MediaFoundationRenderer.RenderedVideoFrameDetectionResult",
      static_cast<int>(result));

  if (rendered_video_frame_detection_start_time_.has_value()) {
    const auto elapsed_time =
        base::TimeTicks::Now() -
        rendered_video_frame_detection_start_time_.value();
    DVLOG_FUNC(2) << "elapsed_time=" << elapsed_time.InMilliseconds() << " ms";
    base::UmaHistogramTimes(
        "Media.MediaFoundationRenderer.RenderedVideoFrameDetectionResult."
        "TimeTo." +
            RenderedVideoFrameDetectionResultToString(result),
        elapsed_time);
  }
}

void MediaFoundationRenderer::SetVolume(float volume) {
  DVLOG_FUNC(2) << "volume=" << volume;
  volume_ = volume;
  if (!mf_media_engine_)
    return;

  HRESULT hr = mf_media_engine_->SetVolume(volume_);
  DVLOG_IF(1, FAILED(hr)) << "Failed to set volume: " << PrintHr(hr);
}

void MediaFoundationRenderer::SetGpuProcessAdapterLuid(
    LUID gpu_process_adapter_luid) {
  // TODO(wicarr, crbug.com/1342621): When the GPU adapter changes or the GPU
  // process is restarted we need to recover our Frame Server or DComp
  // textures, otherwise we'll fail to present any video frames to the user.
  gpu_process_adapter_luid_ = gpu_process_adapter_luid;
}

base::TimeDelta MediaFoundationRenderer::GetMediaTime() {
// GetCurrentTime is expanded as GetTickCount in base/win/windows_types.h
#undef GetCurrentTime
  double current_time = mf_media_engine_->GetCurrentTime();
// Restore macro definition.
#define GetCurrentTime() GetTickCount()
  auto media_time = base::Seconds(current_time);
  DVLOG_FUNC(3) << "media_time=" << media_time;
  return media_time;
}

RendererType MediaFoundationRenderer::GetRendererType() {
  return RendererType::kMediaFoundation;
}

void MediaFoundationRenderer::OnPlaybackError(PipelineStatus status,
                                              HRESULT hr) {
  DVLOG_FUNC(1) << "status=" << status << ", hr=" << hr;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::UmaHistogramSparse("Media.MediaFoundationRenderer.PlaybackError", hr);
  ReportGpuInfoUma("Media.MediaFoundationRenderer.PlaybackError",
                   dxgi_device_manager_.Get(), virtual_video_window_);

  StopSendingStatistics(StopSendingStatisticsReason::kPlaybackError);
  OnError(status, ErrorReason::kOnPlaybackError, hr);
}

void MediaFoundationRenderer::OnPlaybackEnded() {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  StopSendingStatistics(StopSendingStatisticsReason::kPlaybackEnded);
  renderer_client_->OnEnded();
}

void MediaFoundationRenderer::OnFormatChange() {
  DVLOG_FUNC(2);
  OnVideoNaturalSizeChange();

  // Set the start time for the rendered video frame detection.
  RestartRenderedVideoFrameDetectionTimerInNotReported();
}

void MediaFoundationRenderer::OnLoadedData() {
  DVLOG_FUNC(2);

  // According to HTML5 <video> spec, on "loadeddata", the first frame is
  // available for the first time, so we can report natural size change and
  // set up the dcomp frame.
  OnVideoNaturalSizeChange();
}

void MediaFoundationRenderer::OnCanPlayThrough() {
  DVLOG_FUNC(2);

  // If the playback rate in Media Foundations is 0, the video renderer would
  // not pre-roll and request frames. Use Frame Step function to force
  // pre-rolling
  if (playback_rate_ == 0) {
    ComPtr<IMFMediaEngineEx> mf_media_engine_ex;

    HRESULT hr = mf_media_engine_.As(&mf_media_engine_ex);
    if (SUCCEEDED(hr)) {
      mf_media_engine_ex->FrameStep(/*Forward=*/true);
    } else {
      OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
              ErrorReason::kFailedToGetMediaEngineEx, hr);
      return;
    }
  }

  // According to HTML5 <video> spec, on "canplaythrough", the video could be
  // rendered at the current playback rate all the way to its end, and it's
  // the time to report BUFFERING_HAVE_ENOUGH.
  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_ENOUGH,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void MediaFoundationRenderer::OnPlaying() {
  DVLOG_FUNC(2);

  has_reported_playing_ = true;

  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_ENOUGH,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);

  // The OnPlaying callback from MediaEngineNotifyImpl lets us know that an
  // MF_MEDIA_ENGINE_EVENT_PLAYING message has been received. At this point we
  // can safely start sending Statistics as any asynchronous Flush action in
  // media engine, which would have reset the engine's statistics, will have
  // been completed.
  StartSendingStatistics();
}

void MediaFoundationRenderer::OnWaiting() {
  DVLOG_FUNC(2);
  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_NOTHING,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void MediaFoundationRenderer::OnTimeUpdate() {
  DVLOG_FUNC(3);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void MediaFoundationRenderer::OnFrameStepCompleted() {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Frame-Stepping causes Media engine to be in a paused state after finishing.
  // Thus play and set playback rate is needed to change the state to be
  // playing.

  // Set playback rate is call again because on start, if SetPlaybackRate of 0
  // is called before pipeline topology is setup, the playback rate of Media
  // Engine will be defaulted to 1 as setting playback rate is ignored until
  // topology is set. Thus, when frame step is finished, setting the playback
  // rate again ensures consistency.
  HRESULT hr = mf_media_engine_->Play();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER, ErrorReason::kFailedToPlay, hr);
    return;
  }
  SetPlaybackRate(playback_rate_);
}

void MediaFoundationRenderer::OnProtectionManagerWaiting(WaitingReason reason) {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  renderer_client_->OnWaiting(reason);
}

void MediaFoundationRenderer::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (state == BufferingState::BUFFERING_HAVE_ENOUGH) {
    max_buffering_state_ = state;
  }

  if (state == BufferingState::BUFFERING_HAVE_NOTHING &&
      max_buffering_state_ != BufferingState::BUFFERING_HAVE_ENOUGH) {
    // Prevent sending BUFFERING_HAVE_NOTHING if we haven't previously sent a
    // BUFFERING_HAVE_ENOUGH state.
    return;
  }

  DVLOG_FUNC(2) << "state=" << state << ", reason=" << reason;
  renderer_client_->OnBufferingStateChange(state, reason);
}

HRESULT MediaFoundationRenderer::PauseInternal() {
  // Media Engine resets aggregate statistics when it flushes - such as a
  // transition to the Pause state & then back to Play state. To try and
  // avoid cases where we may get Media Engine's reset statistics call
  // StopSendingStatistics before transitioning to Pause.
  // Note that we should not conclude the rendered video frame detection since
  // PauseInternal() can be called by flush or restart.
  StopSendingStatistics(StopSendingStatisticsReason::kPlaybackPauseInternal);
  return mf_media_engine_->Pause();
}

void MediaFoundationRenderer::OnVideoNaturalSizeChange() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool has_video = mf_media_engine_->HasVideo();
  DVLOG_FUNC(2) << "has_video=" << has_video;

  // Skip if there are no video streams. This can happen because this is
  // originated from MF_MEDIA_ENGINE_EVENT_FORMATCHANGE.
  if (!has_video)
    return;

  DWORD native_width;
  DWORD native_height;
  HRESULT hr =
      mf_media_engine_->GetNativeVideoSize(&native_width, &native_height);
  if (FAILED(hr)) {
    // TODO(xhwang): Add UMA to probe if this can happen.
    DLOG(ERROR) << "Failed to get native video size from MediaEngine, using "
                   "default (640x320). hr="
                << hr;
    native_video_size_ = {640, 320};
  } else {
    native_video_size_ = {base::checked_cast<int>(native_width),
                          base::checked_cast<int>(native_height)};
  }

  // TODO(frankli): Let test code to call `UpdateVideoStream()`.
  if (is_testing_) {
    const gfx::Size test_size(/*width=*/640, /*height=*/320);
    // This invokes IMFMediaEngineEx::UpdateVideoStream() for video frames to
    // be presented. Otherwise, the Media Foundation video renderer will not
    // request video samples from our source.
    std::ignore = UpdateVideoStream(test_size);
  }

  renderer_client_->OnVideoNaturalSizeChange(native_video_size_);
}

void MediaFoundationRenderer::OnError(PipelineStatus status,
                                      ErrorReason reason,
                                      HRESULT hresult,
                                      PipelineStatusCallback status_cb) {
  const std::string error =
      "MediaFoundationRenderer error: " + GetErrorReasonString(reason) + " (" +
      PrintHr(hresult) + ")";

  DLOG(ERROR) << error;

  // Report to MediaLog so the error will show up in media internals and
  // MediaError.message.
  MEDIA_LOG(ERROR, media_log_) << error;

  // Report the error to UMA.
  ReportErrorReason(reason);

  // DRM_E_TEE_INVALID_HWDRM_STATE can happen during OS sleep/resume, or moving
  // video to different graphics adapters. This is not an error, so special case
  // it here.
  PipelineStatus new_status = status;
  // DRM_OEM_E_ASD_ACTIVE_DISPLAY_FAIL (0x8004DD2E) is an error code which
  // comes from old AMD drivers. This error is produced when entering S3/S4
  // sleep mode and during hotplug, but should be treated the same as
  // DRM_E_TEE_INVALID_HWDRM_STATE.
  if (hresult == DRM_OEM_E_ASD_ACTIVE_DISPLAY_FAIL) {
    // Attempt to get the vendor_id using the dxgi device.
    const auto [vendor_id, _] =
        GetVendorIdAndLUIDFromD3D11Device(dxgi_device_manager_.Get());
    if (vendor_id == kGpuVendorIdAmd) {
      hresult = DRM_E_TEE_INVALID_HWDRM_STATE;
    }
  }

  if (hresult == DRM_E_TEE_INVALID_HWDRM_STATE) {
    // TODO(crbug.com/40870069): Remove these after the investigation is done.
    base::UmaHistogramBoolean(
        "Media.MediaFoundationRenderer.InvalidHwdrmState.HasReportedPlaying",
        has_reported_playing_);
    base::UmaHistogramCounts10000(
        "Media.MediaFoundationRenderer.InvalidHwdrmState.VideoFrameDecoded",
        statistics_.video_frames_decoded);

    ReportGpuInfoUma("Media.EME.MediaFoundationService.HardwareContextReset",
                     dxgi_device_manager_.Get(), virtual_video_window_);

    new_status = PIPELINE_ERROR_HARDWARE_CONTEXT_RESET;
    if (cdm_proxy_)
      cdm_proxy_->OnHardwareContextReset();
  } else if (cdm_proxy_) {
    cdm_proxy_->OnPlaybackError(hresult);
  }

  // Attach hresult to `new_status` for logging and metrics reporting.
  new_status.WithData("hresult", static_cast<uint32_t>(hresult));

  if (status_cb)
    std::move(status_cb).Run(new_status);
  else
    renderer_client_->OnError(new_status);
}

}  // namespace media
