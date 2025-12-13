// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <sys/param.h>

#include "base/compiler_specific.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

#if BUILDFLAG(IS_IOS)
#include "base/ios/sim_header_shims.h"
#else
#include <libproc.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#endif

namespace memory_instrumentation {

namespace {

using VMRegion = mojom::VmRegion;

bool IsAddressInSharedRegion(uint64_t address) {
#if BUILDFLAG(IS_IOS)
  return address >= SHARED_REGION_BASE_ARM64 &&
         address < (SHARED_REGION_BASE_ARM64 + SHARED_REGION_SIZE_ARM64);
#else
  // TODO: Need to fix this for ARM64 Mac.
  return address >= SHARED_REGION_BASE_X86_64 &&
         address < (SHARED_REGION_BASE_X86_64 + SHARED_REGION_SIZE_X86_64);
#endif
}

bool IsRegionContainedInRegion(const VMRegion& containee,
                               const VMRegion& container) {
  uint64_t containee_end_address =
      containee.start_address + containee.size_in_bytes;
  uint64_t container_end_address =
      container.start_address + container.size_in_bytes;
  return containee.start_address >= container.start_address &&
         containee_end_address <= container_end_address;
}

bool DoRegionsIntersect(const VMRegion& a, const VMRegion& b) {
  uint64_t a_end_address = a.start_address + a.size_in_bytes;
  uint64_t b_end_address = b.start_address + b.size_in_bytes;
  return a.start_address < b_end_address && b.start_address < a_end_address;
}

// Creates VMRegions for all dyld images. Returns whether the operation
// succeeded.
bool GetDyldRegions(std::vector<VMRegion>* regions) {
  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t kr =
      task_info(mach_task_self(), TASK_DYLD_INFO,
                reinterpret_cast<task_info_t>(&dyld_info), &count);
  if (kr != KERN_SUCCESS)
    return false;

  const struct dyld_all_image_infos* all_image_infos =
      reinterpret_cast<const struct dyld_all_image_infos*>(
          dyld_info.all_image_info_addr);

  bool emitted_linkedit_from_dyld_shared_cache = false;
  for (size_t i = 0; i < all_image_infos->infoArrayCount; i++) {
    const char* image_name =
        UNSAFE_TODO(all_image_infos->infoArray[i]).imageFilePath;

    // The public definition for dyld_all_image_infos/dyld_image_info is wrong
    // for 64-bit platforms. We explicitly cast to struct mach_header_64 even
    // though the public definition claims that this is a struct mach_header.
    const struct mach_header_64* const header =
        reinterpret_cast<const struct mach_header_64* const>(
            UNSAFE_TODO(all_image_infos->infoArray[i]).imageLoadAddress);

    uint64_t next_command = reinterpret_cast<uint64_t>(UNSAFE_TODO(header + 1));
    uint64_t command_end = next_command + header->sizeofcmds;
    uint64_t slide = 0;

    std::vector<VMRegion> temp_regions;
    std::string debug_id;
    for (unsigned int j = 0; j < header->ncmds; ++j) {
      // Ensure that next_command doesn't run past header->sizeofcmds.
      if (next_command + sizeof(struct load_command) > command_end)
        return false;
      const struct load_command* load_cmd =
          reinterpret_cast<const struct load_command*>(next_command);
      next_command += load_cmd->cmdsize;

      if (load_cmd->cmd == LC_SEGMENT_64) {
        if (load_cmd->cmdsize < sizeof(segment_command_64))
          return false;
        const segment_command_64* seg =
            reinterpret_cast<const segment_command_64*>(load_cmd);
        if (UNSAFE_TODO(strcmp(seg->segname, SEG_PAGEZERO)) == 0) {
          continue;
        }
        if (UNSAFE_TODO(strcmp(seg->segname, SEG_TEXT)) == 0) {
          slide = reinterpret_cast<uint64_t>(header) - seg->vmaddr;
        }

        // Avoid emitting LINKEDIT regions in the dyld shared cache, since they
        // all overlap.
        if (IsAddressInSharedRegion(seg->vmaddr) &&
            UNSAFE_TODO(strcmp(seg->segname, SEG_LINKEDIT)) == 0) {
          if (emitted_linkedit_from_dyld_shared_cache) {
            continue;
          } else {
            emitted_linkedit_from_dyld_shared_cache = true;
            image_name = "dyld shared cache combined __LINKEDIT";
          }
        }

        uint32_t protection_flags = 0;
        if (seg->initprot & VM_PROT_READ)
          protection_flags |= VMRegion::kProtectionFlagsRead;
        if (seg->initprot & VM_PROT_WRITE)
          protection_flags |= VMRegion::kProtectionFlagsWrite;
        if (seg->initprot & VM_PROT_EXECUTE)
          protection_flags |= VMRegion::kProtectionFlagsExec;

        VMRegion region;
        region.size_in_bytes = seg->vmsize;
        region.protection_flags = protection_flags;
        region.mapped_file = image_name;
        region.start_address = slide + seg->vmaddr;
        temp_regions.push_back(std::move(region));
      }

      if (load_cmd->cmd == LC_UUID) {
        if (load_cmd->cmdsize < sizeof(uuid_command))
          return false;
        const uuid_command* uuid_cmd =
            reinterpret_cast<const uuid_command*>(load_cmd);
        // The ID is comprised of the UUID concatenated with the module's "age"
        // value which is always 0.
        debug_id = base::HexEncode(uuid_cmd->uuid) + "0";
      }
    }

    for (VMRegion& region : temp_regions) {
      region.module_debugid = debug_id;
      regions->push_back(region);
    }
  }
  return true;
}

// Creates VMRegions using mach vm syscalls. Returns whether the operation
// succeeded.
bool GetAllRegions(std::vector<VMRegion>* regions) {
  const int pid = getpid();
  task_t task = mach_task_self();
  mach_vm_size_t size = 0;
  mach_vm_address_t address = MACH_VM_MIN_ADDRESS;
  while (true) {
    base::CheckedNumeric<mach_vm_address_t> next_address(address);
    next_address += size;
    if (!next_address.IsValid())
      return false;
    address = next_address.ValueOrDie();

    vm_region_basic_info_64 basic_info;
    base::MachVMRegionResult result =
        base::GetBasicInfo(task, &size, &address, &basic_info);
    if (result == base::MachVMRegionResult::Error)
      return false;
    if (result == base::MachVMRegionResult::Finished)
      break;

    VMRegion region;

    if (basic_info.protection & VM_PROT_READ)
      region.protection_flags |= VMRegion::kProtectionFlagsRead;
    if (basic_info.protection & VM_PROT_WRITE)
      region.protection_flags |= VMRegion::kProtectionFlagsWrite;
    if (basic_info.protection & VM_PROT_EXECUTE)
      region.protection_flags |= VMRegion::kProtectionFlagsExec;

    char buffer[MAXPATHLEN];
    int length = proc_regionfilename(pid, address, buffer, MAXPATHLEN);
    if (length > 0) {
      region.mapped_file.assign(buffer, length);
      if (!base::IsStringUTF8AllowingNoncharacters(region.mapped_file))
        region.mapped_file = "region file name is not UTF-8";
    }

    // There's no way to get swapped or clean bytes without doing a
    // very expensive syscalls that crawls every single page in the memory
    // object.
    region.start_address = address;
    region.size_in_bytes = size;
    regions->push_back(region);
  }
  return true;
}

void AddRegionByteStats(VMRegion* dest, const VMRegion& source) {
  dest->byte_stats_private_dirty_resident +=
      source.byte_stats_private_dirty_resident;
  dest->byte_stats_private_clean_resident +=
      source.byte_stats_private_clean_resident;
  dest->byte_stats_shared_dirty_resident +=
      source.byte_stats_shared_dirty_resident;
  dest->byte_stats_shared_clean_resident +=
      source.byte_stats_shared_clean_resident;
  dest->byte_stats_swapped += source.byte_stats_swapped;
  dest->byte_stats_proportional_resident +=
      source.byte_stats_proportional_resident;
}

}  // namespace

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessHandle handle,
                                 const MemDumpFlagSet& flags,
                                 mojom::RawOSMemDump* dump) {
  auto current_handle = base::GetCurrentProcessHandle();
  if (handle != base::kNullProcessId && handle != current_handle) {
    return false;
  }
  return FillOSMemoryDump(current_handle, flags, nullptr, dump);
}

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessHandle handle,
                                 const MemDumpFlagSet& flags,
                                 base::PortProvider* port_provider,
                                 mojom::RawOSMemDump* dump) {
  auto process_metrics =
#if BUILDFLAG(IS_IOS)
      base::ProcessMetrics::CreateProcessMetrics(handle);
#else
      base::ProcessMetrics::CreateProcessMetrics(handle, port_provider);
#endif
  auto info = process_metrics->GetMemoryInfo();
  if (!info.has_value()) {
    return false;
  }

  dump->platform_private_footprint->phys_footprint_bytes =
      info->physical_footprint_bytes;
  dump->platform_private_footprint->internal_bytes = info->internal_bytes;
  dump->platform_private_footprint->compressed_bytes = info->compressed_bytes;
  dump->resident_set_kb =
      base::saturated_cast<uint32_t>(info->resident_set_bytes / 1024);
  return true;
}

// static
std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessHandle handle) {
  std::vector<mojom::VmRegionPtr> maps;

  std::vector<VMRegion> dyld_regions;
  if (!GetDyldRegions(&dyld_regions))
    return maps;
  std::vector<VMRegion> all_regions;
  if (!GetAllRegions(&all_regions))
    return maps;

  // Merge information from dyld regions and all regions.
  for (const VMRegion& region : all_regions) {
    bool skip = false;
    const bool in_shared_region = IsAddressInSharedRegion(region.start_address);
    for (VMRegion& dyld_region : dyld_regions) {
      // If this region is fully contained in a dyld region, then add the bytes
      // stats.
      if (IsRegionContainedInRegion(region, dyld_region)) {
        AddRegionByteStats(&dyld_region, region);
        skip = true;
        break;
      }

      // Check to see if the region is likely used for the dyld shared cache.
      if (in_shared_region) {
        // This region is likely used for the dyld shared cache. Don't record
        // any byte stats since:
        //   1. It's not possible to figure out which dyld regions the byte
        //      stats correspond to.
        //   2. The region is likely shared by non-Chrome processes, so there's
        //      no point in charging the pages towards Chrome.
        if (DoRegionsIntersect(region, dyld_region)) {
          skip = true;
          break;
        }
      }
    }
    if (skip)
      continue;

    maps.push_back(VMRegion::New(region));
  }

  for (VMRegion& region : dyld_regions) {
    maps.push_back(VMRegion::New(region));
  }

  return maps;
}

#if !BUILDFLAG(IS_IOS)
std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessModules(
    base::ProcessHandle handle) {
  std::vector<mojom::VmRegionPtr> maps;

  std::vector<VMRegion> dyld_regions;
  if (!GetDyldRegions(&dyld_regions))
    return maps;

  for (VMRegion& region : dyld_regions) {
    maps.push_back(VMRegion::New(region));
  }

  return maps;
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace memory_instrumentation
