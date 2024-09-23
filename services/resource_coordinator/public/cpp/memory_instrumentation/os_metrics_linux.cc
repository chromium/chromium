// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/prctl.h>

#include <memory>

#include "base/android/library_loader/anchor_functions.h"
#include "base/android/library_loader/anchor_functions_buildflags.h"
#include "base/debug/elf_reader.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

// Symbol with virtual address of the start of ELF header of the current binary.
extern char __ehdr_start;

namespace memory_instrumentation {

namespace {

using mojom::VmRegion;
using mojom::VmRegionPtr;

const char kClearPeakRssCommand[] = "5";
const uint32_t kMaxLineSize = 4096;

// TODO(chiniforooshan): Many of the utility functions in this anonymous
// namespace should move to base/process/process_metrics_linux.cc to make the
// code a lot cleaner.  However, we should do so after we made sure the metrics
// we are experimenting with here have real value.
base::FilePath GetProcPidDir(base::ProcessId pid) {
  return base::FilePath("/proc").Append(
      pid == base::kNullProcessId ? "self" : base::NumberToString(pid));
}

bool GetResidentAndSharedPagesFromStatmFile(int fd,
                                            uint64_t* resident_pages,
                                            uint64_t* shared_pages) {
  lseek(fd, 0, SEEK_SET);
  char line[kMaxLineSize];
  int res = read(fd, line, kMaxLineSize - 1);
  if (res <= 0)
    return false;
  line[res] = '\0';
  int num_scanned =
      sscanf(line, "%*s %" SCNu64 " %" SCNu64, resident_pages, shared_pages);
  return num_scanned == 2;
}

bool ResetPeakRSSIfPossible(base::ProcessId pid) {
  static bool is_peak_rss_resettable = true;
  if (!is_peak_rss_resettable)
    return false;
  auto clear_refs_file = GetProcPidDir(pid).Append("clear_refs");
  base::ScopedFD clear_refs_fd(open(clear_refs_file.value().c_str(), O_WRONLY));
  is_peak_rss_resettable =
      clear_refs_fd.get() >= 0 &&
      base::WriteFileDescriptor(clear_refs_fd.get(), kClearPeakRssCommand);
  return is_peak_rss_resettable;
}

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessId pid) {
  if (pid == base::kNullProcessId) {
    return base::ProcessMetrics::CreateCurrentProcessMetrics();
  }
  return base::ProcessMetrics::CreateProcessMetrics(pid);
}

struct ModuleData {
  std::string path;
  std::string build_id;
};

ModuleData GetMainModuleData() {
  ModuleData module_data;
  Dl_info dl_info;
  if (dladdr(&__ehdr_start, &dl_info)) {
    base::debug::ElfBuildIdBuffer build_id;
    size_t build_id_length =
        base::debug::ReadElfBuildId(&__ehdr_start, true, build_id);
    if (build_id_length) {
      base::FilePath module_data_path = base::FilePath(dl_info.dli_fname);
      if (module_data_path.IsAbsolute()) {
        module_data.path = dl_info.dli_fname;
      } else {
        module_data.path = base::MakeAbsoluteFilePath(module_data_path).value();
      }
      module_data.build_id = std::string(build_id, build_id_length);
    }
  }
  return module_data;
}

bool ParseSmapsHeader(const char* header_line,
                      const ModuleData& main_module_data,
                      VmRegion* region) {
  // e.g., "00400000-00421000 r-xp 00000000 fc:01 1234  /foo.so\n"
  bool res = true;  // Whether this region should be appended or skipped.
  uint64_t end_addr = 0;
  char protection_flags[5] = {0};
  char mapped_file[kMaxLineSize];

  if (sscanf(header_line, "%" SCNx64 "-%" SCNx64 " %4c %*s %*s %*s%4095[^\n]\n",
             &region->start_address, &end_addr, protection_flags,
             mapped_file) != 4) {
    return false;
  }

  if (end_addr > region->start_address) {
    region->size_in_bytes = end_addr - region->start_address;
  } else {
    // This is not just paranoia, it can actually happen (See crbug.com/461237).
    region->size_in_bytes = 0;
    res = false;
  }

  region->protection_flags = 0;
  if (protection_flags[0] == 'r') {
    region->protection_flags |= VmRegion::kProtectionFlagsRead;
  }
  if (protection_flags[1] == 'w') {
    region->protection_flags |= VmRegion::kProtectionFlagsWrite;
  }
  if (protection_flags[2] == 'x') {
    region->protection_flags |= VmRegion::kProtectionFlagsExec;
  }
  if (protection_flags[3] == 's') {
    region->protection_flags |= VmRegion::kProtectionFlagsMayshare;
  }

  region->mapped_file = mapped_file;
  base::TrimWhitespaceASCII(region->mapped_file, base::TRIM_ALL,
                            &region->mapped_file);

  // Build ID is needed to symbolize heap profiles, and is generated only on
  // official builds. Build ID is only added for the current library (chrome)
  // since it is racy to read other libraries which can be unmapped any time.
#if defined(OFFICIAL_BUILD)
  if (!region->mapped_file.empty() &&
      base::StartsWith(main_module_data.path, region->mapped_file,
                       base::CompareCase::SENSITIVE) &&
      !main_module_data.build_id.empty()) {
    region->module_debugid = main_module_data.build_id;
  }
#endif  // defined(OFFICIAL_BUILD)

  return res;
}

uint64_t ReadCounterBytes(char* counter_line) {
  uint64_t counter_value = 0;
  int res = sscanf(counter_line, "%*s %" SCNu64 " kB", &counter_value);
  return res == 1 ? counter_value * 1024 : 0;
}

uint32_t ParseSmapsCounter(char* counter_line, VmRegion* region) {
  // A smaps counter lines looks as follows: "RSS:  0 Kb\n"
  uint32_t res = 1;
  char counter_name[20];
  int did_read = sscanf(counter_line, "%19[^\n ]", counter_name);
  if (did_read != 1)
    return 0;

  if (strcmp(counter_name, "Pss:") == 0) {
    region->byte_stats_proportional_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Private_Dirty:") == 0) {
    region->byte_stats_private_dirty_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Private_Clean:") == 0) {
    region->byte_stats_private_clean_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Shared_Dirty:") == 0) {
    region->byte_stats_shared_dirty_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Shared_Clean:") == 0) {
    region->byte_stats_shared_clean_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Swap:") == 0) {
    region->byte_stats_swapped = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Locked:") == 0) {
    region->byte_locked = ReadCounterBytes(counter_line);
  } else {
    res = 0;
  }

  return res;
}

uint32_t ReadLinuxProcSmapsFile(FILE* smaps_file,
                                std::vector<VmRegionPtr>* maps) {
  if (!smaps_file)
    return 0;

  fseek(smaps_file, 0, SEEK_SET);

  char line[kMaxLineSize];
  const uint32_t kNumExpectedCountersPerRegion = 7;
  uint32_t counters_parsed_for_current_region = 0;
  uint32_t num_valid_regions = 0;
  bool should_add_current_region = false;
  VmRegion region;
  ModuleData main_module_data = GetMainModuleData();
  for (;;) {
    line[0] = '\0';
    if (fgets(line, kMaxLineSize, smaps_file) == nullptr || !strlen(line))
      break;
    if (absl::ascii_isxdigit(static_cast<unsigned char>(line[0])) &&
        !absl::ascii_isupper(static_cast<unsigned char>(line[0]))) {
      region = VmRegion();
      counters_parsed_for_current_region = 0;
      should_add_current_region =
          ParseSmapsHeader(line, main_module_data, &region);
    } else if (should_add_current_region) {
      counters_parsed_for_current_region += ParseSmapsCounter(line, &region);
      DCHECK_LE(counters_parsed_for_current_region,
                kNumExpectedCountersPerRegion);
      if (counters_parsed_for_current_region == kNumExpectedCountersPerRegion) {
        maps->push_back(VmRegion::New(region));
        ++num_valid_regions;
        should_add_current_region = false;
      }
    }
  }
  return num_valid_regions;
}

// RAII class making the current process dumpable via prctl(PR_SET_DUMPABLE, 1),
// in case it is not currently dumpable as described in proc(5) and prctl(2).
// Noop if the original dumpable state could not be determined.
class ScopedProcessSetDumpable {
 public:
  ScopedProcessSetDumpable() {
    int result = prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
    if (result < 0) {
      PLOG(ERROR) << "prctl";
      AvoidPrctlOnDestruction();
      return;
    }
    was_dumpable_ = result > 0;

    if (!was_dumpable_) {
      if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0) {
        PLOG(ERROR) << "prctl";
        // PR_SET_DUMPABLE is often disallowed, avoid crashing in this case.
        AvoidPrctlOnDestruction();
      }
    }
  }

  ScopedProcessSetDumpable(const ScopedProcessSetDumpable&) = delete;
  ScopedProcessSetDumpable& operator=(const ScopedProcessSetDumpable&) = delete;

  ~ScopedProcessSetDumpable() {
    if (!was_dumpable_) {
      PCHECK(prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) == 0) << "prctl";
    }
  }

 private:
  void AvoidPrctlOnDestruction() { was_dumpable_ = true; }

  bool was_dumpable_;
};

}  // namespace

FILE* g_proc_smaps_for_testing = nullptr;

// static
void OSMetrics::SetProcSmapsForTesting(FILE* f) {
  g_proc_smaps_for_testing = f;
}

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessId pid,
                                 mojom::RawOSMemDump* dump) {
  // TODO(chiniforooshan): There is no need to read both /statm and /status
  // files. Refactor to get everything from /status using ProcessMetric.
  auto statm_file = GetProcPidDir(pid).Append("statm");
  auto autoclose = base::ScopedFD(open(statm_file.value().c_str(), O_RDONLY));
  int statm_fd = autoclose.get();

  if (statm_fd == -1)
    return false;

  uint64_t resident_pages;
  uint64_t shared_pages;
  bool success = GetResidentAndSharedPagesFromStatmFile(
      statm_fd, &resident_pages, &shared_pages);

  if (!success)
    return false;

  auto process_metrics = CreateProcessMetrics(pid);

  static const size_t page_size = base::GetPageSize();
  uint64_t rss_anon_bytes = (resident_pages - shared_pages) * page_size;
  uint64_t vm_swap_bytes = process_metrics->GetVmSwapBytes();

  dump->platform_private_footprint->rss_anon_bytes = rss_anon_bytes;
  dump->platform_private_footprint->vm_swap_bytes = vm_swap_bytes;
  dump->resident_set_kb = process_metrics->GetResidentSetSize() / 1024;
  dump->peak_resident_set_kb = GetPeakResidentSetSize(pid);
  dump->is_peak_rss_resettable = ResetPeakRSSIfPossible(pid);

#if BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  if (!base::android::AreAnchorsSane()) {
    DLOG(WARNING) << "Incorrect code ordering";
    return false;
  }

  std::vector<uint8_t> accessed_pages_bitmap;
  OSMetrics::MappedAndResidentPagesDumpState state =
      OSMetrics::GetMappedAndResidentPages(base::android::kStartOfText,
                                           base::android::kEndOfText,
                                           &accessed_pages_bitmap);
  UMA_HISTOGRAM_ENUMERATION(
      "Memory.NativeLibrary.MappedAndResidentMemoryFootprintCollectionStatus",
      state);

  // MappedAndResidentPagesDumpState |state| can be |kAccessPagemapDenied|
  // for Android devices running a kernel version < 4.4 or because the process
  // is not "dumpable", as described in proc(5).
  if (state != OSMetrics::MappedAndResidentPagesDumpState::kSuccess)
    return state != OSMetrics::MappedAndResidentPagesDumpState::kFailure;

  dump->native_library_pages_bitmap = std::move(accessed_pages_bitmap);
#endif  // BUILDFLAG(SUPPORTS_CODE_ORDERING)
#endif  //  BUILDFLAG(IS_ANDROID)

  return true;
}

// static
std::vector<VmRegionPtr> OSMetrics::GetProcessMemoryMaps(base::ProcessId pid) {
  std::vector<VmRegionPtr> maps;
  uint32_t res = 0;
  if (g_proc_smaps_for_testing) {
    res = ReadLinuxProcSmapsFile(g_proc_smaps_for_testing, &maps);
  } else {
    std::string file_name =
        "/proc/" +
        (pid == base::kNullProcessId ? "self" : base::NumberToString(pid)) +
        "/smaps";
    base::ScopedFILE smaps_file(fopen(file_name.c_str(), "r"));
    res = ReadLinuxProcSmapsFile(smaps_file.get(), &maps);
  }

  if (!res)
    return std::vector<VmRegionPtr>();

  return maps;
}

// static
OSMetrics::MappedAndResidentPagesDumpState OSMetrics::GetMappedAndResidentPages(
    const size_t start_address,
    const size_t end_address,
    std::vector<uint8_t>* accessed_pages_bitmap) {
  const char* kPagemap = "/proc/self/pagemap";

  base::ScopedFILE pagemap_file(fopen(kPagemap, "r"));
  if (!pagemap_file.get()) {
    {
      ScopedProcessSetDumpable set_dumpable;
      pagemap_file.reset(fopen(kPagemap, "r"));
    }
    if (!pagemap_file.get()) {
      DLOG(WARNING) << "Could not open " << kPagemap;
      return OSMetrics::MappedAndResidentPagesDumpState::kAccessPagemapDenied;
    }
  }

  const size_t kPageSize = base::GetPageSize();
  const size_t start_page = start_address / kPageSize;
  // |end_address| is exclusive.
  const size_t end_page = (end_address - 1) / kPageSize;
  const size_t total_pages = end_page - start_page + 1;

  // The pagemap has one 64 bit entry per page or 8 bytes.
  auto offset = static_cast<long>(start_page * 8);
  if (fseek(pagemap_file.get(), offset, SEEK_SET) != 0) {
    DLOG(ERROR) << "Error in fseek " << kPagemap;
    return OSMetrics::MappedAndResidentPagesDumpState::kFailure;
  }

  // |entries| will be 2kB/MB (if |kPageSize| = 4096),
  // that would only be ~80kB on Android, and up to 200kB on Linux (for 100MB)
  std::vector<uint64_t> entries(total_pages);
  if (fread(&entries[0], sizeof(uint64_t), total_pages, pagemap_file.get()) !=
      total_pages) {
    return OSMetrics::MappedAndResidentPagesDumpState::kFailure;
  }

  accessed_pages_bitmap->resize(1 + (total_pages - 1) / 8);
  for (size_t page = 0; page < total_pages; page++) {
    // Bit 63 is "page present" according to
    // https://www.kernel.org/doc/Documentation/vm/pagemap.txt.
    if (entries[page] & (1LL << 63)) {
      auto byte = page / 8;
      auto bit = page & 0x7;
      CHECK_LT(byte, accessed_pages_bitmap->size());
      (*accessed_pages_bitmap)[byte] |= 1 << bit;
    }
  }
  return OSMetrics::MappedAndResidentPagesDumpState::kSuccess;
}

// static
size_t OSMetrics::GetPeakResidentSetSize(base::ProcessId pid) {
  std::string data;
  {
    // Synchronously reading files in /proc does not hit the disk.
    base::ScopedAllowBlocking allow_blocking;
    if (!base::ReadFileToString(GetProcPidDir(pid).Append("status"), &data))
      return 0;
  }
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(data, ':', '\n', &pairs);
  for (auto& pair : pairs) {
    base::TrimWhitespaceASCII(pair.first, base::TRIM_ALL, &pair.first);
    // VmHWM gives the peak resident set size since the start of the process or
    // since the last time it was reset. HWM stands for "High Water Mark".
    if (pair.first == "VmHWM") {
      base::TrimWhitespaceASCII(pair.second, base::TRIM_ALL, &pair.second);
      auto split_value_str = base::SplitStringPiece(
          pair.second, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      if (split_value_str.size() != 2 || split_value_str[1] != "kB") {
        NOTREACHED_IN_MIGRATION();
        return 0;
      }
      size_t res;
      if (!base::StringToSizeT(split_value_str[0], &res)) {
        NOTREACHED_IN_MIGRATION();
        return 0;
      }
      return res;
    }
  }
  return 0;
}

}  // namespace memory_instrumentation
