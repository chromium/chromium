// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_initializer.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/partition_allocator/src/partition_alloc/page_allocator.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_address_space.h"
#include "base/bits.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gin/array_buffer.h"
#include "gin/gin_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "v8/include/v8-initialization.h"
#include "v8/include/v8-snapshot.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#elif BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

namespace gin {

namespace {

// This global is never freed nor closed.
base::MemoryMappedFile* g_mapped_snapshot = nullptr;

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
absl::optional<gin::V8SnapshotFileType> g_snapshot_file_type;
#endif

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

#if BUILDFLAG(IS_ANDROID)
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

#else  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
const char kV8ContextSnapshotFileName[] =
    BUILDFLAG(V8_CONTEXT_SNAPSHOT_FILENAME);
#endif
const char kSnapshotFileName[] = "snapshot_blob.bin";
#endif  // BUILDFLAG(IS_ANDROID)

const char* GetSnapshotFileName(const V8SnapshotFileType file_type) {
  switch (file_type) {
    case V8SnapshotFileType::kDefault:
      return kSnapshotFileName;
    case V8SnapshotFileType::kWithAdditionalContext:
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
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
#if BUILDFLAG(IS_ANDROID)
  // This is the path within the .apk.
  *path_out =
      base::FilePath(FILE_PATH_LITERAL("assets")).AppendASCII(file_name);
#elif BUILDFLAG(IS_MAC)
  *path_out = base::apple::PathForFrameworkBundleResource(file_name);
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

#if BUILDFLAG(IS_ANDROID)
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
      base::PlatformThread::Sleep(base::Milliseconds(kOpenRetryDelayMillis));
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  UMA_HISTOGRAM_ENUMERATION("V8.Initializer.OpenV8File.Result", result,
                            OpenV8FileResult::MAX_VALUE);
  return file;
}

#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

template <int LENGTH>
void SetV8Flags(const char (&flag)[LENGTH]) {
  v8::V8::SetFlagsFromString(flag, LENGTH - 1);
}

void SetV8FlagsFormatted(const char* format, ...) {
  char buffer[128];
  va_list args;
  va_start(args, format);
  int length = base::vsnprintf(buffer, sizeof(buffer), format, args);
  if (length <= 0 || sizeof(buffer) <= static_cast<unsigned>(length)) {
    PLOG(ERROR) << "Invalid formatted V8 flag: " << format;
    return;
  }
  v8::V8::SetFlagsFromString(buffer, length);
}

template <size_t N, size_t M>
void SetV8FlagsIfOverridden(const base::Feature& feature,
                            const char (&enabling_flag)[N],
                            const char (&disabling_flag)[M]) {
  auto overridden_state = base::FeatureList::GetStateIfOverridden(feature);
  if (!overridden_state.has_value()) {
    return;
  }
  if (overridden_state.value()) {
    SetV8Flags(enabling_flag);
  } else {
    SetV8Flags(disabling_flag);
  }
}

void SetFlags(IsolateHolder::ScriptMode mode,
              const std::string js_command_line_flags) {
  // We assume that all feature flag defaults correspond to the default
  // values of the corresponding V8 flags.
  SetV8FlagsIfOverridden(features::kV8CompactCodeSpaceWithStack,
                         "--compact-code-space-with-stack",
                         "--no-compact-code-space-with-stack");
  SetV8FlagsIfOverridden(features::kV8CompactWithStack, "--compact-with-stack",
                         "--no-compact-with-stack");
  SetV8FlagsIfOverridden(features::kV8OptimizeJavascript, "--opt", "--no-opt");
  SetV8FlagsIfOverridden(features::kV8FlushBytecode, "--flush-bytecode",
                         "--no-flush-bytecode");
  SetV8FlagsIfOverridden(features::kV8FlushBaselineCode,
                         "--flush-baseline-code", "--no-flush-baseline-code");
  SetV8FlagsIfOverridden(features::kV8FlushCodeBasedOnTabVisibility,
                         "--flush-code-based-on-tab-visibility",
                         "--no-flush-code-based-on-tab-visibility");
  SetV8FlagsIfOverridden(features::kV8FlushCodeBasedOnTime,
                         "--flush-code-based-on-time",
                         "--no-flush-code-based-on-time");
  SetV8FlagsIfOverridden(features::kV8OffThreadFinalization,
                         "--finalize-streaming-on-background",
                         "--no-finalize-streaming-on-background");
  if (base::FeatureList::IsEnabled(features::kV8DelayMemoryReducer)) {
    SetV8FlagsFormatted(
        "--gc-memory-reducer-start-delay-ms=%i",
        static_cast<int>(
            features::kV8MemoryReducerStartDelay.Get().InMilliseconds()));
  }
  SetV8FlagsIfOverridden(features::kV8ConcurrentMarkingHighPriorityThreads,
                         "--concurrent-marking-high-priority-threads",
                         "--no-concurrent-marking-high-priority-threads");
  SetV8FlagsIfOverridden(features::kV8LazyFeedbackAllocation,
                         "--lazy-feedback-allocation",
                         "--no-lazy-feedback-allocation");
  SetV8FlagsIfOverridden(features::kV8PerContextMarkingWorklist,
                         "--stress-per-context-marking-worklist",
                         "--no-stress-per-context-marking-worklist");
  SetV8FlagsIfOverridden(features::kV8FlushEmbeddedBlobICache,
                         "--experimental-flush-embedded-blob-icache",
                         "--no-experimental-flush-embedded-blob-icache");
  SetV8FlagsIfOverridden(features::kV8ReduceConcurrentMarkingTasks,
                         "--gc-experiment-reduce-concurrent-marking-tasks",
                         "--no-gc-experiment-reduce-concurrent-marking-tasks");
  SetV8FlagsIfOverridden(features::kV8NoReclaimUnmodifiedWrappers,
                         "--no-reclaim-unmodified-wrappers",
                         "--reclaim-unmodified-wrappers");
  SetV8FlagsIfOverridden(
      features::kV8ExperimentalRegexpEngine,
      "--enable-experimental-regexp-engine-on-excessive-backtracks",
      "--no-enable-experimental-regexp-engine-on-excessive-backtracks");
  SetV8FlagsIfOverridden(features::kV8TurboFastApiCalls,
                         "--turbo-fast-api-calls", "--no-turbo-fast-api-calls");
  SetV8FlagsIfOverridden(features::kV8MegaDomIC, "--mega-dom-ic",
                         "--no-mega-dom-ic");
  SetV8FlagsIfOverridden(features::kV8Maglev, "--maglev", "--no-maglev");
  if (base::FeatureList::IsEnabled(features::kV8MemoryReducer)) {
    SetV8FlagsFormatted("--memory-reducer-gc-count=%i",
                        features::kV8MemoryReducerGCCount.Get());
  }
  SetV8FlagsIfOverridden(features::kV8MinorMS, "--minor-ms", "--no-minor-ms");
  SetV8FlagsIfOverridden(features::kV8Sparkplug, "--sparkplug",
                         "--no-sparkplug");
  SetV8FlagsIfOverridden(features::kV8Turbofan, "--turbofan", "--no-turbofan");
  SetV8FlagsIfOverridden(features::kV8Turboshaft, "--turboshaft",
                         "--no-turboshaft");
  SetV8FlagsIfOverridden(features::kV8TurboshaftInstructionSelection,
                         "--turboshaft-instruction-selection",
                         "--no-turboshaft-instruction-selection");
  SetV8FlagsIfOverridden(features::kV8ConcurrentSparkplug,
                         "--concurrent-sparkplug", "--no-concurrent-sparkplug");
  SetV8FlagsIfOverridden(features::kV8SparkplugNeedsShortBuiltinCalls,
                         "--sparkplug-needs-short-builtins",
                         "--no-sparkplug-needs-short-builtins");
  SetV8FlagsIfOverridden(features::kV8ShortBuiltinCalls,
                         "--short-builtin-calls", "--no-short-builtin-calls");
  SetV8FlagsIfOverridden(features::kV8CodeMemoryWriteProtection,
                         "--write-protect-code-memory",
                         "--no-write-protect-code-memory");
  SetV8FlagsIfOverridden(features::kV8SlowHistograms, "--slow-histograms",
                         "--no-slow-histograms");
  SetV8FlagsIfOverridden(features::kV8SingleThreadedGCInBackground,
                         "--single-threaded-gc-in-background",
                         "--no-single-threaded-gc-in-background");
  SetV8FlagsIfOverridden(features::kV8MidtierRegallocFallback,
                         "--turbo-use-mid-tier-regalloc-for-huge-functions",
                         "--no-turbo-use-mid-tier-regalloc-for-huge-functions");

  if (base::FeatureList::IsEnabled(features::kV8ConcurrentSparkplug)) {
    if (int max_threads = features::kV8ConcurrentSparkplugMaxThreads.Get()) {
      SetV8FlagsFormatted("--concurrent-sparkplug-max-threads=%i", max_threads);
    }
    SetV8FlagsIfOverridden(features::kV8ConcurrentSparkplugHighPriorityThreads,
                           "--concurrent-sparkplug-high-priority-threads",
                           "--no-concurrent-sparkplug-high-priority-threads");
  }

  if (base::FeatureList::IsEnabled(features::kV8FlushBytecode)) {
    if (int old_age = features::kV8FlushBytecodeOldAge.Get()) {
      SetV8FlagsFormatted("--bytecode-old-age=%i", old_age);
    }
  }

  if (base::FeatureList::IsEnabled(features::kV8FlushCodeBasedOnTime)) {
    if (int old_time = features::kV8FlushCodeOldTime.Get()) {
      SetV8FlagsFormatted("--bytecode-old-time=%i", old_time);
    }
  }

  // Make sure aliases of kV8SlowHistograms only enable the feature to
  // avoid contradicting settings between multiple finch experiments.
  bool any_slow_histograms_alias =
      base::FeatureList::IsEnabled(
          features::kV8SlowHistogramsCodeMemoryWriteProtection) ||
      base::FeatureList::IsEnabled(features::kV8SlowHistogramsSparkplug) ||
      base::FeatureList::IsEnabled(
          features::kV8SlowHistogramsSparkplugAndroid) ||
      base::FeatureList::IsEnabled(features::kV8SlowHistogramsNoTurbofan);
  if (any_slow_histograms_alias) {
    SetV8Flags("--slow-histograms");
  } else {
    SetV8FlagsIfOverridden(features::kV8SlowHistograms, "--slow-histograms",
                           "--no-slow-histograms");
  }

  SetV8FlagsIfOverridden(features::kV8IgnitionElideRedundantTdzChecks,
                         "--ignition-elide-redundant-tdz-checks",
                         "--no-ignition-elide-redundant-tdz-checks");

  // JavaScript language features.
  SetV8FlagsIfOverridden(features::kJavaScriptSymbolAsWeakMapKey,
                         "--harmony-symbol-as-weakmap-key",
                         "--no-harmony-symbol-as-weakmap-key");
  SetV8FlagsIfOverridden(features::kJavaScriptChangeArrayByCopy,
                         "--harmony-change-array-by-copy",
                         "--no-harmony-change-array-by-copy");
  if (base::FeatureList::IsEnabled(features::kJavaScriptRabGsab)) {
    SetV8Flags("--harmony-rab-gsab");
  } else {
    SetV8Flags("--no-harmony-rab-gsab");
  }
  SetV8FlagsIfOverridden(features::kJavaScriptRegExpUnicodeSets,
                         "--harmony-regexp-unicode-sets",
                         "--no-harmony-regexp-unicode-sets");
  SetV8FlagsIfOverridden(features::kJavaScriptJsonParseWithSource,
                         "--harmony-json-parse-with-source",
                         "--no-harmony-json-parse-with-source");
  SetV8FlagsIfOverridden(features::kJavaScriptArrayBufferTransfer,
                         "--harmony-rab-gsab-transfer",
                         "--no-harmony-rab-gsab-transfer");
  SetV8FlagsIfOverridden(features::kJavaScriptIteratorHelpers,
                         "--harmony-iterator-helpers",
                         "--no-harmony-iterator-helpers");
  SetV8FlagsIfOverridden(features::kJavaScriptPromiseWithResolvers,
                         "--js-promise-withresolvers",
                         "--no-js-promise-withresolvers");

  if (IsolateHolder::kStrictMode == mode) {
    SetV8Flags("--use_strict");
  }

  SetV8FlagsIfOverridden(features::kV8UseLibmTrigFunctions,
                         "--use-libm-trig-functions",
                         "--no-use-libm-trig-functions");

  SetV8FlagsIfOverridden(features::kJavaScriptCompileHintsMagic,
                         "--compile-hints-magic", "--no-compile-hints-magic");

  // WebAssembly features.

  SetV8FlagsIfOverridden(features::kWebAssemblyTailCall,
                         "--experimental-wasm-return-call",
                         "--no-experimental-wasm-return-call");
  SetV8FlagsIfOverridden(features::kWebAssemblyInlining,
                         "--experimental-wasm-inlining",
                         "--no-experimental-wasm-inlining");
  SetV8FlagsIfOverridden(features::kWebAssemblyGenericWrapper,
                         "--wasm-to-js-generic-wrapper",
                         "--no-wasm-to-js-generic-wrapper");
  SetV8FlagsIfOverridden(features::kWebAssemblyMultipleMemories,
                         "--experimental-wasm-multi-memory",
                         "--no-experimental-wasm-multi-memory");
  SetV8FlagsIfOverridden(features::kWebAssemblyTurboshaft, "--turboshaft-wasm",
                         "--no-turboshaft-wasm");

  if (js_command_line_flags.empty())
    return;

  // Allow the --js-flags switch to override existing flags:
  std::vector<base::StringPiece> flag_list =
      base::SplitStringPiece(js_command_line_flags, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  for (const auto& flag : flag_list) {
    v8::V8::SetFlagsFromString(std::string(flag).c_str(), flag.size());
  }
}

}  // namespace

// static
void V8Initializer::Initialize(IsolateHolder::ScriptMode mode,
                               const std::string js_command_line_flags,
                               v8::OOMErrorCallback oom_error_callback) {
  static bool v8_is_initialized = false;
  if (v8_is_initialized)
    return;

  // Flags need to be set before InitializePlatform as they are used for
  // system instrumentation initialization.
  // See https://crbug.com/v8/11043
  SetFlags(mode, js_command_line_flags);

  v8::V8::InitializePlatform(V8Platform::Get());

  // Set this as early as possible in order to ensure OOM errors are reported
  // correctly.
  v8::V8::SetFatalMemoryErrorCallback(oom_error_callback);

  // Set this early on as some initialization steps, such as the initialization
  // of the virtual memory cage, already use V8's random number generator.
  v8::V8::SetEntropySource(&GenerateEntropy);

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  if (g_mapped_snapshot) {
    v8::StartupData snapshot;
    GetMappedFileData(g_mapped_snapshot, &snapshot);
    v8::V8::SetSnapshotDataBlob(&snapshot);
  }
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

  v8::V8::Initialize();

  v8_is_initialized = true;

#if defined(V8_ENABLE_SANDBOX)
  // Record some sandbox statistics into UMA.
  // The main reason for capturing these histograms here instead of having V8
  // do it is that there are no Isolates available yet, which are required
  // for recording histograms in V8.

  // Record the mode of the sandbox.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This should match enum
  // V8SandboxMode in tools/metrics/histograms/enums.xml.
  enum class V8SandboxMode {
    kSecure = 0,
    kInsecure = 1,
    kMaxValue = kInsecure,
  };
  base::UmaHistogramEnumeration("V8.SandboxMode",
                                v8::V8::IsSandboxConfiguredSecurely()
                                    ? V8SandboxMode::kSecure
                                    : V8SandboxMode::kInsecure);

  // Record the size of the address space reservation backing the sandbox.
  // The size will always be one of a handful of values, so use a sparse
  // histogram to capture it.
  size_t size = v8::V8::GetSandboxReservationSizeInBytes();
  DCHECK_GT(size, 0U);
  size_t sizeInGB = size >> 30;
  DCHECK_EQ(sizeInGB << 30, size);
  base::UmaHistogramSparse("V8.SandboxReservationSizeGB", sizeInGB);

  // When the sandbox is enabled, ArrayBuffers must be allocated inside of
  // it. To achieve that, PA's ConfigurablePool is created inside the sandbox
  // and Blink then creates the ArrayBuffer partition in that Pool.
  v8::VirtualAddressSpace* sandbox_address_space =
      v8::V8::GetSandboxAddressSpace();
  const size_t max_pool_size = partition_alloc::internal::
      PartitionAddressSpace::ConfigurablePoolMaxSize();
  const size_t min_pool_size = partition_alloc::internal::
      PartitionAddressSpace::ConfigurablePoolMinSize();
  size_t pool_size = max_pool_size;
  // Try to reserve the maximum size of the pool at first, then keep halving
  // the size on failure until it succeeds.
  uintptr_t pool_base = 0;
  while (!pool_base && pool_size >= min_pool_size) {
    pool_base = sandbox_address_space->AllocatePages(
        0, pool_size, pool_size, v8::PagePermissions::kNoAccess);
    if (!pool_base) {
      pool_size /= 2;
    }
  }
  // The V8 sandbox is guaranteed to be large enough to host the pool.
  CHECK(pool_base);
  partition_alloc::internal::PartitionAddressSpace::InitConfigurablePool(
      pool_base, pool_size);
  // TODO(saelo) maybe record the size of the Pool into UMA.
#endif  // V8_ENABLE_SANDBOX

  // Initialize the partition used by gin::ArrayBufferAllocator instances. This
  // needs to happen now, after the V8 sandbox has been initialized, so that
  // the partition is placed inside the configurable pool initialized above.
  ArrayBufferAllocator::InitializePartition();
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
    LOG(FATAL) << "Error loading V8 startup snapshot file";
    return;
  }

  g_snapshot_file_type = snapshot_file_type;
  base::MemoryMappedFile::Region region =
      base::MemoryMappedFile::Region::kWholeFile;
  if (snapshot_file_region) {
    region = *snapshot_file_region;
  }

  if (!MapV8File(std::move(snapshot_file), region, &g_mapped_snapshot)) {
    LOG(FATAL) << "Error mapping V8 startup snapshot file";
    return;
  }
}

#if BUILDFLAG(IS_ANDROID)
// static
base::FilePath V8Initializer::GetSnapshotFilePath(
    bool abi_32_bit,
    V8SnapshotFileType snapshot_file_type) {
  base::FilePath path;
  const char* filename = nullptr;
  switch (snapshot_file_type) {
    case V8SnapshotFileType::kDefault:
      filename = abi_32_bit ? kSnapshotFileName32 : kSnapshotFileName64;
      break;
    case V8SnapshotFileType::kWithAdditionalContext:
      filename = abi_32_bit ? kV8ContextSnapshotFileName32
                            : kV8ContextSnapshotFileName64;
      break;
  }
  CHECK(filename);

  GetV8FilePath(filename, &path);
  return path;
}
#endif  // BUILDFLAG(IS_ANDROID)

V8SnapshotFileType GetLoadedSnapshotFileType() {
  DCHECK(g_snapshot_file_type.has_value());
  return *g_snapshot_file_type;
}

#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

}  // namespace gin
