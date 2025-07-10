// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/chrome_ml.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "services/on_device_model/ml/chrome_ml_holder.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/rust/chromium_crates_io/vendor/llguidance-v1/llguidance.h"

#if !BUILDFLAG(IS_IOS)
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#endif

namespace ml {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuErrorReason {
  kOther = 0,
  kDxgiErrorDeviceHung = 1,
  kDeviceRemoved = 2,
  kDeviceCreationFailed = 3,
  kOutOfMemory = 4,
  kMaxValue = kOutOfMemory,
};

void FatalGpuErrorFn(const char* msg) {
  SCOPED_CRASH_KEY_STRING1024("ChromeML(GPU)", "error_msg", msg);
  std::string msg_str(msg);
  std::string msg_continued;
  constexpr size_t kCrashStringSize = 1024;
  // The error message may be long as it potentially includes the shader,
  // collect another 3k if needed.
  if (msg_str.size() > kCrashStringSize) {
    msg_continued = msg_str.substr(kCrashStringSize);
  }
  SCOPED_CRASH_KEY_STRING1024("ChromeML(GPU)", "error_msg2", msg_continued);
  msg_continued =
      msg_continued.substr(std::min(kCrashStringSize, msg_continued.size()));
  SCOPED_CRASH_KEY_STRING1024("ChromeML(GPU)", "error_msg3", msg_continued);
  msg_continued =
      msg_continued.substr(std::min(kCrashStringSize, msg_continued.size()));
  SCOPED_CRASH_KEY_STRING1024("ChromeML(GPU)", "error_msg4", msg_continued);
  msg_continued =
      msg_continued.substr(std::min(kCrashStringSize, msg_continued.size()));
  SCOPED_CRASH_KEY_STRING1024("ChromeML(GPU)", "error_msg5", msg_continued);
  msg_continued =
      msg_continued.substr(std::min(kCrashStringSize, msg_continued.size()));
  SCOPED_CRASH_KEY_STRING1024("ChromeML(GPU)", "error_msg6", msg_continued);

  GpuErrorReason error_reason = GpuErrorReason::kOther;
  if (msg_str.find("DXGI_ERROR_DEVICE_HUNG") != std::string::npos) {
    error_reason = GpuErrorReason::kDxgiErrorDeviceHung;
  } else if (msg_str.find("DXGI_ERROR_DEVICE_REMOVED") != std::string::npos ||
             msg_str.find("VK_ERROR_DEVICE_LOST") != std::string::npos) {
    error_reason = GpuErrorReason::kDeviceRemoved;
  } else if (msg_str.find("Failed to create device") != std::string::npos) {
    error_reason = GpuErrorReason::kDeviceCreationFailed;
  } else if (msg_str.find("VK_ERROR_OUT_OF_DEVICE_MEMORY") !=
             std::string::npos) {
    error_reason = GpuErrorReason::kOutOfMemory;
  }
  base::UmaHistogramEnumeration("OnDeviceModel.GpuErrorReason", error_reason);
  if (error_reason == GpuErrorReason::kOther) {
    // Collect crash reports on unknown errors.
    NOTREACHED() << "ChromeML(GPU) Error: " << msg;
  } else {
    base::Process::TerminateCurrentProcessImmediately(0);
  }
}

void FatalErrorFn(const char* msg) {
  SCOPED_CRASH_KEY_STRING1024("ChromeML", "error_msg", msg);
  NOTREACHED() << "ChromeML Error: " << msg;
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

void RecordMediumTimesHistogram(const char* name, int64_t milliseconds) {
  base::UmaHistogramMediumTimes(name, base::Milliseconds(milliseconds));
}

}  // namespace

ChromeML::ChromeML(const ChromeMLAPI* api) : api_(api) {}
ChromeML::~ChromeML() = default;

// static
ChromeML* ChromeML::Get(const std::optional<std::string>& library_name) {
  static base::NoDestructor<std::unique_ptr<ChromeML>> chrome_ml{
      Create(library_name)};
  return chrome_ml->get();
}

#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
void ConstraintDelete(ChromeMLConstraint constraint) {
  llg_free_constraint(reinterpret_cast<LlgConstraint*>(constraint));
}

bool ConstraintComputeMask(ChromeMLConstraint constraint,
                           ChromeMLConstraintMask& mask) {
  LlgMaskResult res;
  if (llg_compute_mask(reinterpret_cast<LlgConstraint*>(constraint), &res) !=
      0) {
    return false;
  }
  mask.sample_mask = res.sample_mask;
  mask.is_stop = res.is_stop;
  return true;
}

bool ConstraintCommitToken(ChromeMLConstraint constraint, uint32_t token) {
  LlgCommitResult result;
  return llg_commit_token(reinterpret_cast<LlgConstraint*>(constraint), token,
                          &result) >= 0;
}

bool ConstraintIsStopped(ChromeMLConstraint constraint) {
  return llg_is_stopped(reinterpret_cast<LlgConstraint*>(constraint));
}

const char* ConstraintGetError(ChromeMLConstraint constraint) {
  return llg_get_error(reinterpret_cast<LlgConstraint*>(constraint));
}

ChromeMLConstraint ConstraintClone(ChromeMLConstraint constraint) {
  return reinterpret_cast<ChromeMLConstraint>(
      llg_clone_constraint(reinterpret_cast<LlgConstraint*>(constraint)));
}
#endif

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
  ChromeMLHolder* holder_ptr = holder->get();
  if (!holder_ptr) {
    return {};
  }

  auto& api = holder_ptr->api();

  dawnProcSetProcs(&dawn::native::GetProcs());
  api.InitDawnProcs(dawn::native::GetProcs());
  if (api.SetFatalErrorFn) {
    api.SetFatalErrorFn(&FatalGpuErrorFn);
  }
  if (api.SetMetricsFns) {
    const ChromeMLMetricsFns metrics_fns{
        .RecordExactLinearHistogram = &RecordExactLinearHistogram,
        .RecordCustomCountsHistogram = &RecordCustomCountsHistogram,
        .RecordMediumTimesHistogram = &RecordMediumTimesHistogram,
    };
    api.SetMetricsFns(&metrics_fns);
  }
  if (api.SetConstraintFns) {
    api.SetConstraintFns(GetConstraintFns());
  }
  if (api.SetFatalErrorNonGpuFn) {
    api.SetFatalErrorNonGpuFn(&FatalErrorFn);
  }
  return base::WrapUnique(new ChromeML(&api));
}

const ChromeMLConstraintFns* GetConstraintFns() {
#if defined(ENABLE_ON_DEVICE_CONSTRAINTS)
  static constexpr ChromeMLConstraintFns kConstraintFns = {
      .Delete = &ConstraintDelete,
      .ComputeMask = &ConstraintComputeMask,
      .CommitToken = &ConstraintCommitToken,
      .IsStopped = &ConstraintIsStopped,
      .GetError = &ConstraintGetError,
      .Clone = &ConstraintClone,
  };
#else
  static constexpr ChromeMLConstraintFns kConstraintFns = {};
#endif
  return &kConstraintFns;
}

}  // namespace ml
