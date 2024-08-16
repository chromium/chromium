// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include <set>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/page_size.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include <libgen.h>
#include <mach-o/dyld.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/sys_string_conversions.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <sys/mman.h>
#endif

namespace memory_instrumentation {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
namespace {
const char kTestSmaps1[] =
    "00400000-004be000 r-xp 00000000 fc:01 1234              /file/1\n"
    "Size:                760 kB\n"
    "Rss:                 296 kB\n"
    "Pss:                 162 kB\n"
    "Shared_Clean:        228 kB\n"
    "Shared_Dirty:          0 kB\n"
    "Private_Clean:         0 kB\n"
    "Private_Dirty:        68 kB\n"
    "Referenced:          296 kB\n"
    "Anonymous:            68 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  4 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd ex mr mw me dw sd\n"
    "ff000000-ff800000 -w-p 00001080 fc:01 0            /file/name with space\n"
    "Size:                  0 kB\n"
    "Rss:                 192 kB\n"
    "Pss:                 128 kB\n"
    "Shared_Clean:        120 kB\n"
    "Shared_Dirty:          4 kB\n"
    "Private_Clean:        60 kB\n"
    "Private_Dirty:         8 kB\n"
    "Referenced:          296 kB\n"
    "Anonymous:             0 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  0 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                1 kB\n"
    "VmFlags: rd ex mr mw me dw sd";

const char kTestSmaps2[] =
    // An invalid region, with zero size and overlapping with the last one
    // (See crbug.com/461237).
    "7fe7ce79c000-7fe7ce79c000 ---p 00000000 00:00 0 \n"
    "Size:                  4 kB\n"
    "Rss:                   0 kB\n"
    "Pss:                   0 kB\n"
    "Shared_Clean:          0 kB\n"
    "Shared_Dirty:          0 kB\n"
    "Private_Clean:         0 kB\n"
    "Private_Dirty:         0 kB\n"
    "Referenced:            0 kB\n"
    "Anonymous:             0 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  0 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd ex mr mw me dw sd\n"
    // A invalid region with its range going backwards.
    "00400000-00200000 ---p 00000000 00:00 0 \n"
    "Size:                  4 kB\n"
    "Rss:                   0 kB\n"
    "Pss:                   0 kB\n"
    "Shared_Clean:          0 kB\n"
    "Shared_Dirty:          0 kB\n"
    "Private_Clean:         0 kB\n"
    "Private_Dirty:         0 kB\n"
    "Referenced:            0 kB\n"
    "Anonymous:             0 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  0 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd ex mr mw me dw sd\n"
    // A good anonymous region at the end.
    "7fe7ce79c000-7fe7ce7a8000 ---p 00000000 00:00 0 \n"
    "Size:                 48 kB\n"
    "Rss:                  40 kB\n"
    "Pss:                  32 kB\n"
    "Shared_Clean:         16 kB\n"
    "Shared_Dirty:         12 kB\n"
    "Private_Clean:         8 kB\n"
    "Private_Dirty:         4 kB\n"
    "Referenced:           40 kB\n"
    "Anonymous:            16 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  0 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                11 kB\n"
    "VmFlags: rd wr mr mw me ac sd\n";

void CreateTempFileWithContents(const char* contents, base::ScopedFILE* file) {
  base::FilePath temp_path;
  *file = CreateAndOpenTemporaryStream(&temp_path);
  ASSERT_TRUE(*file);

  ASSERT_TRUE(base::WriteFileDescriptor(fileno(file->get()), contents));
}

}  // namespace
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

TEST(OSMetricsTest, GivesNonZeroResults) {
  base::ProcessId pid = base::kNullProcessId;
  mojom::RawOSMemDump dump;
  dump.platform_private_footprint = mojom::PlatformPrivateFootprint::New();
  EXPECT_TRUE(OSMetrics::FillOSMemoryDump(pid, &dump));
  EXPECT_TRUE(dump.platform_private_footprint);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA)
  EXPECT_GT(dump.platform_private_footprint->rss_anon_bytes, 0u);
#elif BUILDFLAG(IS_WIN)
  EXPECT_GT(dump.platform_private_footprint->private_bytes, 0u);
#elif BUILDFLAG(IS_APPLE)
  EXPECT_GT(dump.platform_private_footprint->internal_bytes, 0u);
#endif
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
TEST(OSMetricsTest, ParseProcSmaps) {
  const uint32_t kProtR = mojom::VmRegion::kProtectionFlagsRead;
  const uint32_t kProtW = mojom::VmRegion::kProtectionFlagsWrite;
  const uint32_t kProtX = mojom::VmRegion::kProtectionFlagsExec;

  // Emulate an empty /proc/self/smaps.
  base::ScopedFILE empty_file(OpenFile(base::FilePath("/dev/null"), "r"));
  ASSERT_TRUE(empty_file.get());
  OSMetrics::SetProcSmapsForTesting(empty_file.get());
  auto no_maps = OSMetrics::GetProcessMemoryMaps(base::kNullProcessId);
  ASSERT_TRUE(no_maps.empty());

  // Parse the 1st smaps file.
  base::ScopedFILE temp_file1;
  CreateTempFileWithContents(kTestSmaps1, &temp_file1);
  OSMetrics::SetProcSmapsForTesting(temp_file1.get());
  auto maps_1 = OSMetrics::GetProcessMemoryMaps(base::kNullProcessId);
  ASSERT_EQ(2UL, maps_1.size());

  EXPECT_EQ(0x00400000UL, maps_1[0]->start_address);
  EXPECT_EQ(0x004be000UL - 0x00400000UL, maps_1[0]->size_in_bytes);
  EXPECT_EQ(kProtR | kProtX, maps_1[0]->protection_flags);
  EXPECT_EQ("/file/1", maps_1[0]->mapped_file);
  EXPECT_EQ(162 * 1024UL, maps_1[0]->byte_stats_proportional_resident);
  EXPECT_EQ(228 * 1024UL, maps_1[0]->byte_stats_shared_clean_resident);
  EXPECT_EQ(0UL, maps_1[0]->byte_stats_shared_dirty_resident);
  EXPECT_EQ(0UL, maps_1[0]->byte_stats_private_clean_resident);
  EXPECT_EQ(68 * 1024UL, maps_1[0]->byte_stats_private_dirty_resident);
  EXPECT_EQ(4 * 1024UL, maps_1[0]->byte_stats_swapped);
  EXPECT_EQ(0 * 1024UL, maps_1[0]->byte_locked);

  EXPECT_EQ(0xff000000UL, maps_1[1]->start_address);
  EXPECT_EQ(0xff800000UL - 0xff000000UL, maps_1[1]->size_in_bytes);
  EXPECT_EQ(kProtW, maps_1[1]->protection_flags);
  EXPECT_EQ("/file/name with space", maps_1[1]->mapped_file);
  EXPECT_EQ(128 * 1024UL, maps_1[1]->byte_stats_proportional_resident);
  EXPECT_EQ(120 * 1024UL, maps_1[1]->byte_stats_shared_clean_resident);
  EXPECT_EQ(4 * 1024UL, maps_1[1]->byte_stats_shared_dirty_resident);
  EXPECT_EQ(60 * 1024UL, maps_1[1]->byte_stats_private_clean_resident);
  EXPECT_EQ(8 * 1024UL, maps_1[1]->byte_stats_private_dirty_resident);
  EXPECT_EQ(0 * 1024UL, maps_1[1]->byte_stats_swapped);
  EXPECT_EQ(1 * 1024UL, maps_1[1]->byte_locked);

  // Parse the 2nd smaps file.
  base::ScopedFILE temp_file2;
  CreateTempFileWithContents(kTestSmaps2, &temp_file2);
  OSMetrics::SetProcSmapsForTesting(temp_file2.get());
  auto maps_2 = OSMetrics::GetProcessMemoryMaps(base::kNullProcessId);
  ASSERT_EQ(1UL, maps_2.size());
  EXPECT_EQ(0x7fe7ce79c000UL, maps_2[0]->start_address);
  EXPECT_EQ(0x7fe7ce7a8000UL - 0x7fe7ce79c000UL, maps_2[0]->size_in_bytes);
  EXPECT_EQ(0U, maps_2[0]->protection_flags);
  EXPECT_EQ("", maps_2[0]->mapped_file);
  EXPECT_EQ(32 * 1024UL, maps_2[0]->byte_stats_proportional_resident);
  EXPECT_EQ(16 * 1024UL, maps_2[0]->byte_stats_shared_clean_resident);
  EXPECT_EQ(12 * 1024UL, maps_2[0]->byte_stats_shared_dirty_resident);
  EXPECT_EQ(8 * 1024UL, maps_2[0]->byte_stats_private_clean_resident);
  EXPECT_EQ(4 * 1024UL, maps_2[0]->byte_stats_private_dirty_resident);
  EXPECT_EQ(0 * 1024UL, maps_2[0]->byte_stats_swapped);
  EXPECT_EQ(11 * 1024UL, maps_2[0]->byte_locked);
}

TEST(OSMetricsTest, GetMappedAndResidentPages) {
  const size_t kPages = 16;
  const size_t kPageSize = base::GetPageSize();
  const size_t kLength = kPages * kPageSize;

  // mmap guarantees addr is aligned with kPagesize.
  void* addr = mmap(NULL, kLength, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  ASSERT_NE(MAP_FAILED, addr) << "mmap() failed";

  std::set<size_t> pages;
  uint8_t* array = static_cast<uint8_t*>(addr);
  for (unsigned int i = 0; i < kPages / 2; ++i) {
    int page = rand() % kPages;
    int offset = rand() % kPageSize;
    *static_cast<volatile uint8_t*>(array + page * kPageSize + offset) =
        rand() % 256;
    pages.insert(page);
  }

  size_t start_address = reinterpret_cast<size_t>(addr);

  std::vector<uint8_t> accessed_pages_bitmap;
  OSMetrics::MappedAndResidentPagesDumpState state =
      OSMetrics::GetMappedAndResidentPages(
          start_address, start_address + kLength, &accessed_pages_bitmap);

  ASSERT_EQ(munmap(addr, kLength), 0);
  if (state == OSMetrics::MappedAndResidentPagesDumpState::kAccessPagemapDenied)
    return;

  EXPECT_EQ(state == OSMetrics::MappedAndResidentPagesDumpState::kSuccess,
            true);
  std::set<size_t> accessed_pages_set;
  for (size_t i = 0; i < accessed_pages_bitmap.size(); i++) {
    for (int j = 0; j < 8; j++)
      if (accessed_pages_bitmap[i] & (1 << j))
        accessed_pages_set.insert(i * 8 + j);
  }

  EXPECT_EQ(pages == accessed_pages_set, true);
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
void DummyFunction() {}

TEST(OSMetricsTest, TestWinModuleReading) {
  auto maps = OSMetrics::GetProcessMemoryMaps(base::kNullProcessId);

  wchar_t module_name[MAX_PATH];
  DWORD result = GetModuleFileName(nullptr, module_name, MAX_PATH);
  ASSERT_TRUE(result);
  std::string executable_name = base::SysWideToNativeMB(module_name);

  HMODULE module_containing_dummy = nullptr;
  uintptr_t dummy_function_address =
      reinterpret_cast<uintptr_t>(&DummyFunction);
  result = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                             reinterpret_cast<LPCWSTR>(dummy_function_address),
                             &module_containing_dummy);
  ASSERT_TRUE(result);
  result = GetModuleFileName(nullptr, module_name, MAX_PATH);
  ASSERT_TRUE(result);
  std::string module_containing_dummy_name =
      base::SysWideToNativeMB(module_name);

  bool found_executable = false;
  bool found_region_with_dummy = false;
  for (const mojom::VmRegionPtr& region : maps) {
    // We add a region just for byte_stats_proportional_resident which
    // is empty other than that one stat.
    if (region->byte_stats_proportional_resident > 0) {
      EXPECT_EQ(0u, region->start_address);
      EXPECT_EQ(0u, region->size_in_bytes);
      continue;
    }
    EXPECT_NE(0u, region->start_address);
    EXPECT_NE(0u, region->size_in_bytes);

    if (region->mapped_file.find(executable_name) != std::string::npos)
      found_executable = true;

    if (dummy_function_address >= region->start_address &&
        dummy_function_address <
            region->start_address + region->size_in_bytes) {
      found_region_with_dummy = true;
      EXPECT_EQ(module_containing_dummy_name, region->mapped_file);
    }
  }
  EXPECT_TRUE(found_executable);
  EXPECT_TRUE(found_region_with_dummy);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
namespace {

void CheckMachORegions(const std::vector<mojom::VmRegionPtr>& maps) {
  constexpr uint32_t kSize = 100;
  char full_path[kSize];
  uint32_t buf_size = kSize;
  int result = _NSGetExecutablePath(full_path, &buf_size);
  ASSERT_EQ(0, result);
  std::string name = basename(full_path);

  bool found_appkit = false;
  bool found_components_unittests = false;
  for (const mojom::VmRegionPtr& region : maps) {
    EXPECT_NE(0u, region->start_address);
    EXPECT_NE(0u, region->size_in_bytes);

    EXPECT_LT(region->size_in_bytes, 1ull << 32);
    uint32_t required_protection_flags = mojom::VmRegion::kProtectionFlagsRead |
                                         mojom::VmRegion::kProtectionFlagsExec;
    if (region->mapped_file.find(name) != std::string::npos &&
        region->protection_flags == required_protection_flags) {
      found_components_unittests = true;
    }

    if (region->mapped_file.find("AppKit") != std::string::npos) {
      found_appkit = true;
    }
  }
  EXPECT_TRUE(found_components_unittests);
  EXPECT_TRUE(found_appkit);
}

}  // namespace

// Test failing on Mac ASan 64: https://crbug.com/852690
TEST(OSMetricsTest, DISABLED_TestMachOReading) {
  auto maps = OSMetrics::GetProcessMemoryMaps(base::kNullProcessId);
  CheckMachORegions(maps);
  maps = OSMetrics::GetProcessModules(base::kNullProcessId);
  CheckMachORegions(maps);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace memory_instrumentation
