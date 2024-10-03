// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/chrome_ml.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"

#if !BUILDFLAG(IS_IOS)
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#endif

namespace ml {

namespace {

constexpr std::string_view kChromeMLLibraryName = "optimization_guide_internal";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuErrorReason {
  kOther = 0,
  kDxgiErrorDeviceHung = 1,
  kDxgiErrorDeviceRemoved = 2,
  kDeviceCreationFailed = 3,
  kMaxValue = kDeviceCreationFailed,
};

void FatalGpuErrorFn(const char* msg) {
  SCOPED_CRASH_KEY_STRING1024("ChromeML(GPU)", "error_msg", msg);
  std::string msg_str(msg);
  GpuErrorReason error_reason = GpuErrorReason::kOther;
  if (msg_str.find("DXGI_ERROR_DEVICE_HUNG") != std::string::npos) {
    error_reason = GpuErrorReason::kDxgiErrorDeviceHung;
  } else if (msg_str.find("DXGI_ERROR_DEVICE_REMOVED") != std::string::npos) {
    error_reason = GpuErrorReason::kDxgiErrorDeviceRemoved;
  } else if (msg_str.find("Failed to create device") != std::string::npos) {
    error_reason = GpuErrorReason::kDeviceCreationFailed;
  }
  base::UmaHistogramEnumeration("OnDeviceModel.GpuErrorReason", error_reason);
  if (error_reason == GpuErrorReason::kOther) {
    // Collect crash reports on unknown errors.
    CHECK(false) << "ChromeML(GPU) Error: " << msg;
  } else {
    base::Process::TerminateCurrentProcessImmediately(0);
  }
}

void FatalErrorFn(const char* msg) {
  SCOPED_CRASH_KEY_STRING1024("ChromeML", "error_msg", msg);
  CHECK(false) << "ChromeML Error: " << msg;
}

// Helpers to disabiguate overloads in base.
void RecordExactLinearHistogram(const char* name,
                                int sample,
                                int exclusive_max) {
  base::UmaHistogramExactLinear(name, sample, exclusive_max);
}

void RecordCustomCountsHistogram(const char* name,
                                 int sample,
                                 int min,
                                 int exclusive_max,
                                 size_t buckets) {
  base::UmaHistogramCustomCounts(name, sample, min, exclusive_max, buckets);
}

}  // namespace

base::FilePath GetChromeMLPath(const std::optional<std::string>& library_name) {
  // TODO(https://crbug.com/366498630): Clean up the API to introduce dedicated
  // ForTesting() methods for loading the library / querying its path.
  if (library_name.has_value()) {
    // Library name override should only be used in test code.
    CHECK_IS_TEST();
  }

  base::FilePath base_dir;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_MAC)
  if (base::apple::AmIBundled()) {
    base_dir = base::apple::FrameworkBundlePath().Append("Libraries");
  } else {
#endif  // BUILDFLAG(IS_MAC)
    CHECK(base::PathService::Get(base::DIR_MODULE, &base_dir));
#if BUILDFLAG(IS_MAC)
  }
#endif  // BUILDFLAG(IS_MAC)
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_FUCHSIA)

  return base_dir.AppendASCII(base::GetNativeLibraryName(
      library_name.value_or(std::string(kChromeMLLibraryName))));
}

ChromeMLHolder::ChromeMLHolder(base::PassKey<ChromeMLHolder>,
                               base::ScopedNativeLibrary library,
                               const ChromeMLAPI* api)
    : library_(std::move(library)), api_(api) {
  CHECK(api_);
}

ChromeMLHolder::~ChromeMLHolder() = default;

// static
DISABLE_CFI_DLSYM
std::unique_ptr<ChromeMLHolder> ChromeMLHolder::Create(
    const std::optional<std::string>& library_name) {
  base::NativeLibraryLoadError error;
  base::NativeLibrary library =
      base::LoadNativeLibrary(GetChromeMLPath(library_name), &error);
  if (!library) {
    LOG(ERROR) << "Error loading native library: " << error.ToString();
    return {};
  }

  base::ScopedNativeLibrary scoped_library(library);
  auto get_api = reinterpret_cast<ChromeMLAPIGetter>(
      scoped_library.GetFunctionPointer("GetChromeMLAPI"));
  if (!get_api) {
    LOG(ERROR) << "Unable to resolve GetChromeMLAPI() symbol.";
    return {};
  }

  const ChromeMLAPI* api = get_api();
  if (!api) {
    LOG(ERROR) << "GetChromeMLAPI() returned null.";
    return {};
  }

  return std::make_unique<ChromeMLHolder>(base::PassKey<ChromeMLHolder>(),
                                          std::move(scoped_library), api);
}

ChromeML::ChromeML(const ChromeMLAPI* api) : api_(api) {}
ChromeML::~ChromeML() = default;

// static
ChromeML* ChromeML::Get(const std::optional<std::string>& library_name) {
  static base::NoDestructor<std::unique_ptr<ChromeML>> chrome_ml{
      Create(library_name)};
  return chrome_ml->get();
}

// static
DISABLE_CFI_DLSYM
std::unique_ptr<ChromeML> ChromeML::Create(
    const std::optional<std::string>& library_name) {
#if !BUILDFLAG(IS_IOS)
  // Log GPU info for crash reports.
  gpu::GPUInfo gpu_info;
  gpu::CollectBasicGraphicsInfo(&gpu_info);
  gpu::SetKeysForCrashLogging(gpu_info);
#endif

  static base::NoDestructor<std::unique_ptr<ChromeMLHolder>> holder{
      ChromeMLHolder::Create(library_name)};
  if (!holder.get()) {
    return {};
  }

  auto& api = (*holder)->api();

  dawnProcSetProcs(&dawn::native::GetProcs());
  api.InitDawnProcs(dawn::native::GetProcs());
  if (api.SetFatalErrorFn) {
    api.SetFatalErrorFn(&FatalGpuErrorFn);
  }
  if (api.SetMetricsFns) {
    const ChromeMLMetricsFns metrics_fns{
        .RecordExactLinearHistogram = &RecordExactLinearHistogram,
        .RecordCustomCountsHistogram = &RecordCustomCountsHistogram,
    };
    api.SetMetricsFns(&metrics_fns);
  }
  if (api.SetFatalErrorNonGpuFn) {
    api.SetFatalErrorNonGpuFn(&FatalErrorFn);
  }
  return base::WrapUnique(new ChromeML(&api));
}

}  // namespace ml
