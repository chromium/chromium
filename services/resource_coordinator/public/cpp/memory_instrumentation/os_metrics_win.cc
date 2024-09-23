// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include <tchar.h>
#include <windows.h>

#include <psapi.h>

#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/pe_image.h"
#include "base/win/win_util.h"

namespace memory_instrumentation {

namespace {

// Gets the unique build ID for a module. Windows build IDs are created by a
// concatenation of a GUID and AGE fields found in the headers of a module. The
// GUID is stored in the first 16 bytes and the AGE is stored in the last 4
// bytes. Returns the empty string if the function fails to get the build ID.
std::string MakeDebugID(const GUID& guid, DWORD age) {
  return base::StringPrintf("%08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%ld",
                            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
                            guid.Data4[1], guid.Data4[2], guid.Data4[3],
                            guid.Data4[4], guid.Data4[5], guid.Data4[6],
                            guid.Data4[7], age);
}

}  // namespace

// static
bool OSMetrics::FillOSMemoryDump(base::ProcessId pid,
                                 mojom::RawOSMemDump* dump) {
  base::Process process;
  if (pid == base::kNullProcessId) {
    process = base::Process::Current();
  } else {
    process = base::Process::Open(pid);
  }
  if (!process.IsValid()) {
    return false;
  }
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (::GetProcessMemoryInfo(process.Handle(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
    dump->platform_private_footprint->private_bytes = pmc.PrivateUsage;
    dump->resident_set_kb =
        base::saturated_cast<uint32_t>(pmc.WorkingSetSize / 1024);
    return true;
  }
  return false;
}

// static
std::vector<mojom::VmRegionPtr> OSMetrics::GetProcessMemoryMaps(
    base::ProcessId pid) {
  std::vector<mojom::VmRegionPtr> maps;
  std::vector<HMODULE> modules;
  if (!base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &modules))
    return maps;

  // Query the base address for each module, and attach it to the dump.
  for (size_t i = 0; i < modules.size(); ++i) {
    wchar_t module_name[MAX_PATH];
    if (!::GetModuleFileName(modules[i], module_name, MAX_PATH))
      continue;

    MODULEINFO module_info;
    if (!::GetModuleInformation(::GetCurrentProcess(), modules[i], &module_info,
                                sizeof(MODULEINFO))) {
      continue;
    }
    mojom::VmRegionPtr region = mojom::VmRegion::New();
    region->size_in_bytes = module_info.SizeOfImage;
    region->mapped_file = base::SysWideToNativeMB(module_name);
    region->start_address = reinterpret_cast<uint64_t>(module_info.lpBaseOfDll);

    // The PE header field |TimeDateStamp| is required to build the PE code
    // identifier which is used as a key to query symbols servers.
    base::win::PEImage pe_image(module_info.lpBaseOfDll);
    region->module_timestamp =
        pe_image.GetNTHeaders()->FileHeader.TimeDateStamp;

    GUID module_guid;
    DWORD module_age;
    const char* pdb_file = nullptr;
    size_t pdb_file_length = 0;
    if (pe_image.GetDebugId(&module_guid, &module_age, &pdb_file,
                            &pdb_file_length)) {
      region->module_debugid = MakeDebugID(module_guid, module_age);
      region->module_debug_path.assign(pdb_file, pdb_file_length);
    }

    maps.push_back(std::move(region));
  }
  return maps;
}

}  // namespace memory_instrumentation
