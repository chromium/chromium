// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/aggregate_metrics_processor.h"

#include <set>
#include <string>
#include <vector>

#include "base/android/library_loader/anchor_functions.h"
#include "base/android/library_loader/anchor_functions_buildflags.h"
#include "base/bits.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

#if BUILDFLAG(SUPPORTS_CODE_ORDERING)

namespace {

void LogNativeCodeResidentPages(const std::set<size_t>& accessed_pages_set) {
  // |SUPPORTS_CODE_ORDERING| can only be enabled on Android.
  const auto kResidentPagesPath = base::FilePath(
      "/data/local/tmp/chrome/native-library-resident-pages.txt");

  auto file = base::File(kResidentPagesPath, base::File::FLAG_CREATE_ALWAYS |
                                                 base::File::FLAG_WRITE);

  if (!file.IsValid()) {
    DLOG(ERROR) << "Could not open " << kResidentPagesPath;
    return;
  }

  for (size_t page : accessed_pages_set) {
    std::string page_str = base::StringPrintf("%" PRIuS "\n", page);

    if (file.WriteAtCurrentPos(page_str.c_str(),
                               static_cast<int>(page_str.size())) < 0) {
      DLOG(WARNING) << "Error while dumping Resident pages";
      return;
    }
  }
}

}  // namespace

namespace memory_instrumentation {

mojom::AggregatedMetricsPtr ComputeGlobalNativeCodeResidentMemoryKb(
    const std::map<base::ProcessId, mojom::RawOSMemDump*>& pid_to_pmd) {
  std::vector<uint8_t> common_map;
  auto metrics = mojom::AggregatedMetricsPtr(mojom::AggregatedMetrics::New());

  for (const auto& pmd : pid_to_pmd) {
    if (!pmd.second || pmd.second->native_library_pages_bitmap.empty()) {
      DLOG(WARNING) << "No process pagemap entry for " << pmd.first;
      return metrics;
    }

    if (common_map.size() < pmd.second->native_library_pages_bitmap.size()) {
      common_map.resize(pmd.second->native_library_pages_bitmap.size());
    }
    for (size_t i = 0; i < pmd.second->native_library_pages_bitmap.size();
         ++i) {
      common_map[i] |= pmd.second->native_library_pages_bitmap[i];
    }
  }

  // |accessed_pages_set| will be ~40kB on 32 bit mode and ~80kB on 64 bit mode.
  std::set<size_t> accessed_pages_set;
  for (size_t i = 0; i < common_map.size(); i++) {
    for (int j = 0; j < 8; j++) {
      if (common_map[i] & (1 << j))
        accessed_pages_set.insert(i * 8 + j);
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "log-native-library-residency")) {
    LogNativeCodeResidentPages(accessed_pages_set);
  }

  const size_t kPageSize = base::GetPageSize();
  const size_t kb_per_page = kPageSize / 1024;

  int32_t native_library_resident_not_ordered_kb =
      GlobalMemoryDump::AggregatedMetrics::kInvalid;
  int32_t native_library_not_resident_ordered_kb =
      GlobalMemoryDump::AggregatedMetrics::kInvalid;
  int32_t native_library_resident_kb =
      static_cast<int32_t>(accessed_pages_set.size() * kb_per_page);

  // Reporting the resident code data only requires |AreAnchorsSane()|, not
  // |IsOrderingSane()| which is a stronger guarantee.
  if (base::android::IsOrderingSane()) {
    // Start and end markers are not necessarily aligned with page boundaries.
    size_t start_of_text_page_index = base::android::kStartOfText / kPageSize;
    // Range is [start_of_ordered_section_page_offset,
    //           end_of_ordered_section_page_offset)
    size_t start_of_ordered_section_page_offset =
        base::android::kStartOfOrderedText / kPageSize -
        start_of_text_page_index;
    size_t end_of_ordered_section_page_offset =
        base::bits::Align(base::android::kEndOfOrderedText, kPageSize) /
            kPageSize -
        start_of_text_page_index;

    size_t resident_pages_in_ordered_section = 0;
    size_t resident_pages_outside_ordered_section = 0;
    for (size_t page : accessed_pages_set) {
      if (page >= start_of_ordered_section_page_offset &&
          page < end_of_ordered_section_page_offset) {
        resident_pages_in_ordered_section++;
      } else {
        resident_pages_outside_ordered_section++;
      }
    }
    size_t ordered_section_pages = end_of_ordered_section_page_offset -
                                   start_of_ordered_section_page_offset;
    size_t not_resident_ordered_pages =
        ordered_section_pages - resident_pages_in_ordered_section;

    native_library_resident_not_ordered_kb = static_cast<int32_t>(
        resident_pages_outside_ordered_section * kb_per_page);
    native_library_not_resident_ordered_kb =
        static_cast<int32_t>(not_resident_ordered_pages * kb_per_page);
  }

  // TODO(crbug.com/956464) replace adding |NativeCodeResidentMemory| to trace
  // this way by adding it through |tracing_observer| in Finalize().
  TRACE_EVENT_INSTANT1(base::trace_event::MemoryDumpManager::kTraceCategory,
                       "ReportGlobalNativeCodeResidentMemoryKb",
                       TRACE_EVENT_SCOPE_GLOBAL, "NativeCodeResidentMemory",
                       native_library_resident_kb);

  metrics->native_library_resident_kb = native_library_resident_kb;
  metrics->native_library_resident_not_ordered_kb =
      native_library_resident_not_ordered_kb;
  metrics->native_library_not_resident_ordered_kb =
      native_library_not_resident_ordered_kb;
  return metrics;
}

}  // namespace memory_instrumentation

#else

namespace memory_instrumentation {

mojom::AggregatedMetricsPtr ComputeGlobalNativeCodeResidentMemoryKb(
    const std::map<base::ProcessId, mojom::RawOSMemDump*>& pid_to_pmd) {
  return mojom::AggregatedMetricsPtr(mojom::AggregatedMetrics::New());
}

}  // namespace memory_instrumentation

#endif  // #if BUILDFLAG(SUPPORTS_CODE_ORDERING)
