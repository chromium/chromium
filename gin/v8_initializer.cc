// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_initializer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gin/gin_features.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#elif defined(OS_MAC)
#include "base/mac/foundation_util.h"
#endif
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

namespace gin {

namespace {

// This global is never freed nor closed.
base::MemoryMappedFile* g_mapped_snapshot = nullptr;

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  base::RandBytes(buffer, amount);
  return true;
}

void GetMappedFileData(base::MemoryMappedFile* mapped_file,
                       v8::StartupData* data) {
  if (mapped_file) {
    data->data = reinterpret_cast<const char*>(mapped_file->data());
    data->raw_size = static_cast<int>(mapped_file->length());
  } else {
    data->data = nullptr;
    data->raw_size = 0;
  }
}

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)

#if defined(OS_ANDROID)
const char kV8ContextSnapshotFileName64[] = "v8_context_snapshot_64.bin";
const char kV8ContextSnapshotFileName32[] = "v8_context_snapshot_32.bin";
const char kSnapshotFileName64[] = "snapshot_blob_64.bin";
const char kSnapshotFileName32[] = "snapshot_blob_32.bin";

#if defined(__LP64__)
#define kV8ContextSnapshotFileName kV8ContextSnapshotFileName64
#define kSnapshotFileName kSnapshotFileName64
#else
#define kV8ContextSnapshotFileName kV8ContextSnapshotFileName32
#define kSnapshotFileName kSnapshotFileName32
#endif

#else  // defined(OS_ANDROID)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
const char kV8ContextSnapshotFileName[] = V8_CONTEXT_SNAPSHOT_FILENAME;
#endif
const char kSnapshotFileName[] = "snapshot_blob.bin";
#endif  // defined(OS_ANDROID)

const char* GetSnapshotFileName(
    const V8Initializer::V8SnapshotFileType file_type) {
  switch (file_type) {
    case V8Initializer::V8SnapshotFileType::kDefault:
      return kSnapshotFileName;
    case V8Initializer::V8SnapshotFileType::kWithAdditionalContext:
#if defined(USE_V8_CONTEXT_SNAPSHOT)
      return kV8ContextSnapshotFileName;
#else
      NOTREACHED();
      return nullptr;
#endif
  }
  NOTREACHED();
  return nullptr;
}

void GetV8FilePath(const char* file_name, base::FilePath* path_out) {
#if defined(OS_ANDROID)
  // This is the path within the .apk.
  *path_out =
      base::FilePath(FILE_PATH_LITERAL("assets")).AppendASCII(file_name);
#elif defined(OS_MAC)
  base::ScopedCFTypeRef<CFStringRef> bundle_resource(
      base::SysUTF8ToCFStringRef(file_name));
  *path_out = base::mac::PathForFrameworkBundleResource(bundle_resource);
#else
  base::FilePath data_path;
  bool r = base::PathService::Get(base::DIR_ASSETS, &data_path);
  DCHECK(r);
  *path_out = data_path.AppendASCII(file_name);
#endif
}

bool MapV8File(base::File file,
               base::MemoryMappedFile::Region region,
               base::MemoryMappedFile** mmapped_file_out) {
  DCHECK(*mmapped_file_out == NULL);
  std::unique_ptr<base::MemoryMappedFile> mmapped_file(
      new base::MemoryMappedFile());
  if (mmapped_file->Initialize(std::move(file), region)) {
    *mmapped_file_out = mmapped_file.release();
    return true;
  }
  return false;
}

base::File OpenV8File(const char* file_name,
                      base::MemoryMappedFile::Region* region_out) {
  // Re-try logic here is motivated by http://crbug.com/479537
  // for A/V on Windows (https://support.microsoft.com/en-us/kb/316609).

  // These match tools/metrics/histograms.xml
  enum OpenV8FileResult {
    OPENED = 0,
    OPENED_RETRY,
    FAILED_IN_USE,
    FAILED_OTHER,
    MAX_VALUE
  };
  base::FilePath path;
  GetV8FilePath(file_name, &path);

#if defined(OS_ANDROID)
  base::File file(base::android::OpenApkAsset(path.value(), region_out));
  OpenV8FileResult result = file.IsValid() ? OpenV8FileResult::OPENED
                                           : OpenV8FileResult::FAILED_OTHER;
#else
  // Re-try logic here is motivated by http://crbug.com/479537
  // for A/V on Windows (https://support.microsoft.com/en-us/kb/316609).
  const int kMaxOpenAttempts = 5;
  const int kOpenRetryDelayMillis = 250;

  OpenV8FileResult result = OpenV8FileResult::FAILED_IN_USE;
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::File file;
  for (int attempt = 0; attempt < kMaxOpenAttempts; attempt++) {
    file.Initialize(path, flags);
    if (file.IsValid()) {
      *region_out = base::MemoryMappedFile::Region::kWholeFile;
      if (attempt == 0) {
        result = OpenV8FileResult::OPENED;
        break;
      } else {
        result = OpenV8FileResult::OPENED_RETRY;
        break;
      }
    } else if (file.error_details() != base::File::FILE_ERROR_IN_USE) {
      result = OpenV8FileResult::FAILED_OTHER;
      break;
    } else if (kMaxOpenAttempts - 1 != attempt) {
      base::PlatformThread::Sleep(
          base::TimeDelta::FromMilliseconds(kOpenRetryDelayMillis));
    }
  }
#endif  // defined(OS_ANDROID)

  UMA_HISTOGRAM_ENUMERATION("V8.Initializer.OpenV8File.Result",
                            result,
                            OpenV8FileResult::MAX_VALUE);
  return file;
}

enum LoadV8FileResult {
  V8_LOAD_SUCCESS = 0,
  V8_LOAD_FAILED_OPEN,
  V8_LOAD_FAILED_MAP,
  V8_LOAD_FAILED_VERIFY,  // Deprecated.
  V8_LOAD_MAX_VALUE
};

#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

}  // namespace

// static
void V8Initializer::Initialize(IsolateHolder::ScriptMode mode) {
  static bool v8_is_initialized = false;
  if (v8_is_initialized)
    return;

  v8::V8::InitializePlatform(V8Platform::Get());

  if (!base::FeatureList::IsEnabled(features::kV8OptimizeJavascript)) {
    // We avoid explicitly passing --opt if kV8OptimizeJavascript is enabled
    // since it is the default, and doing so would override flags passed
    // explicitly, e.g., via --js-flags=--no-opt.
    static const char no_optimize[] = "--no-opt";
    v8::V8::SetFlagsFromString(no_optimize, sizeof(no_optimize) - 1);
  }

  if (!base::FeatureList::IsEnabled(features::kV8FlushBytecode)) {
    static const char no_flush_bytecode[] = "--no-flush-bytecode";
    v8::V8::SetFlagsFromString(no_flush_bytecode,
                               sizeof(no_flush_bytecode) - 1);
  }

  if (base::FeatureList::IsEnabled(features::kV8OffThreadFinalization)) {
    static const char finalize_streaming_on_background[] =
        "--finalize-streaming-on-background";
    v8::V8::SetFlagsFromString(finalize_streaming_on_background,
                               sizeof(finalize_streaming_on_background) - 1);
  }

  if (!base::FeatureList::IsEnabled(features::kV8LazyFeedbackAllocation)) {
    static const char no_lazy_feedback_allocation[] =
        "--no-lazy-feedback-allocation";
    v8::V8::SetFlagsFromString(no_lazy_feedback_allocation,
                               sizeof(no_lazy_feedback_allocation) - 1);
  }

  if (base::FeatureList::IsEnabled(features::kV8ConcurrentInlining)) {
    static const char tf_experiment_concurrent_inlining[] =
        "--concurrent_inlining";
    v8::V8::SetFlagsFromString(tf_experiment_concurrent_inlining,
                               sizeof(tf_experiment_concurrent_inlining) - 1);
  }

  if (base::FeatureList::IsEnabled(features::kV8PerContextMarkingWorklist)) {
    static const char stress_per_context_marking_worklist[] =
        "--stress-per-context-marking-worklist";
    v8::V8::SetFlagsFromString(stress_per_context_marking_worklist,
                               sizeof(stress_per_context_marking_worklist) - 1);
  }

  if (base::FeatureList::IsEnabled(features::kV8FlushEmbeddedBlobICache)) {
    static const char experimental_flush_embedded_blob_icache[] =
        "--experimental-flush-embedded-blob-icache";
    v8::V8::SetFlagsFromString(
        experimental_flush_embedded_blob_icache,
        sizeof(experimental_flush_embedded_blob_icache) - 1);
  }

  if (base::FeatureList::IsEnabled(features::kV8ReduceConcurrentMarkingTasks)) {
    static const char gc_experiment_reduce_concurrent_marking_tasks[] =
        "--gc-experiment-reduce-concurrent-marking-tasks";
    v8::V8::SetFlagsFromString(
        gc_experiment_reduce_concurrent_marking_tasks,
        sizeof(gc_experiment_reduce_concurrent_marking_tasks) - 1);
  }

  if (base::FeatureList::IsEnabled(features::kV8NoReclaimUnmodifiedWrappers)) {
    static constexpr char no_reclaim_unmodified_wrappers[] =
        "--no-reclaim-unmodified-wrappers";
    v8::V8::SetFlagsFromString(no_reclaim_unmodified_wrappers,
                               sizeof(no_reclaim_unmodified_wrappers) - 1);
  }

  if (!base::FeatureList::IsEnabled(features::kV8LocalHeaps)) {
    // The --local-heaps flag is enabled by default, so we need to explicitly
    // disable it if kV8LocalHeaps is disabled.
    static constexpr char no_local_heaps[] = "--no-local-heaps";
    v8::V8::SetFlagsFromString(no_local_heaps, sizeof(no_local_heaps) - 1);

    // Also disable TurboFan's direct access if local heaps are not enabled.
    static constexpr char no_direct_access[] = "--no-turbo-direct-heap-access";
    v8::V8::SetFlagsFromString(no_direct_access, sizeof(no_direct_access) - 1);
  }

  if (!base::FeatureList::IsEnabled(features::kV8TurboDirectHeapAccess)) {
    // The --turbo-direct-heap-access flag is enabled by default, so we need to
    // explicitly disable it if kV8TurboDirectHeapAccess is disabled.
    static constexpr char no_direct_access[] = "--no-turbo-direct-heap-access";
    v8::V8::SetFlagsFromString(no_direct_access, sizeof(no_direct_access) - 1);
  }

  if (IsolateHolder::kStrictMode == mode) {
    static const char use_strict[] = "--use_strict";
    v8::V8::SetFlagsFromString(use_strict, sizeof(use_strict) - 1);
  }

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  if (g_mapped_snapshot) {
    v8::StartupData snapshot;
    GetMappedFileData(g_mapped_snapshot, &snapshot);
    v8::V8::SetSnapshotDataBlob(&snapshot);
  }
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

  v8::V8::SetEntropySource(&GenerateEntropy);
  v8::V8::Initialize();

  v8_is_initialized = true;
}

// static
void V8Initializer::GetV8ExternalSnapshotData(v8::StartupData* snapshot) {
  GetMappedFileData(g_mapped_snapshot, snapshot);
}

// static
void V8Initializer::GetV8ExternalSnapshotData(const char** snapshot_data_out,
                                              int* snapshot_size_out) {
  v8::StartupData snapshot;
  GetV8ExternalSnapshotData(&snapshot);
  *snapshot_data_out = snapshot.data;
  *snapshot_size_out = snapshot.raw_size;
}

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)

// static
void V8Initializer::LoadV8Snapshot(V8SnapshotFileType snapshot_file_type) {
  if (g_mapped_snapshot) {
    // TODO(crbug.com/802962): Confirm not loading different type of snapshot
    // files in a process.
    return;
  }

  base::MemoryMappedFile::Region file_region;
  base::File file =
      OpenV8File(GetSnapshotFileName(snapshot_file_type), &file_region);
  LoadV8SnapshotFromFile(std::move(file), &file_region, snapshot_file_type);
}

// static
void V8Initializer::LoadV8SnapshotFromFile(
    base::File snapshot_file,
    base::MemoryMappedFile::Region* snapshot_file_region,
    V8SnapshotFileType snapshot_file_type) {
  if (g_mapped_snapshot)
    return;

  if (!snapshot_file.IsValid()) {
    UMA_HISTOGRAM_ENUMERATION("V8.Initializer.LoadV8Snapshot.Result",
                              V8_LOAD_FAILED_OPEN, V8_LOAD_MAX_VALUE);
    return;
  }

  base::MemoryMappedFile::Region region =
      base::MemoryMappedFile::Region::kWholeFile;
  if (snapshot_file_region) {
    region = *snapshot_file_region;
  }

  LoadV8FileResult result = V8_LOAD_SUCCESS;
  if (!MapV8File(std::move(snapshot_file), region, &g_mapped_snapshot))
    result = V8_LOAD_FAILED_MAP;
  UMA_HISTOGRAM_ENUMERATION("V8.Initializer.LoadV8Snapshot.Result", result,
                            V8_LOAD_MAX_VALUE);
}

#if defined(OS_ANDROID)
// static
base::FilePath V8Initializer::GetSnapshotFilePath(
    bool abi_32_bit,
    V8SnapshotFileType snapshot_file_type) {
  base::FilePath path;
  const char* filename = nullptr;
  switch (snapshot_file_type) {
    case V8Initializer::V8SnapshotFileType::kDefault:
      filename = abi_32_bit ? kSnapshotFileName32 : kSnapshotFileName64;
      break;
    case V8Initializer::V8SnapshotFileType::kWithAdditionalContext:
      filename = abi_32_bit ? kV8ContextSnapshotFileName32
                            : kV8ContextSnapshotFileName64;
      break;
  }
  CHECK(filename);

  GetV8FilePath(filename, &path);
  return path;
}
#endif  // defined(OS_ANDROID)
#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

}  // namespace gin
