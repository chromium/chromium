// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

#include <windows.h>

#include <winternl.h>

#include <memory>

#include "base/system/sys_info.h"

namespace extensions {

namespace {

const wchar_t kNtdll[] = L"ntdll.dll";
const char kNtQuerySystemInformationName[] = "NtQuerySystemInformation";

// See MSDN about NtQuerySystemInformation definition.
typedef DWORD(WINAPI* NtQuerySystemInformationPF)(DWORD system_info_class,
                                                  PVOID system_info,
                                                  ULONG system_info_length,
                                                  PULONG return_length);

}  // namespace

bool CpuInfoProvider::QueryCpuTimePerProcessor(
    std::vector<api::system_cpu::ProcessorInfo>* infos) {
  DCHECK(infos);

  HMODULE ntdll = GetModuleHandle(kNtdll);
  CHECK(ntdll != NULL);
  NtQuerySystemInformationPF NtQuerySystemInformation =
      reinterpret_cast<NtQuerySystemInformationPF>(
          ::GetProcAddress(ntdll, kNtQuerySystemInformationName));

  CHECK(NtQuerySystemInformation != NULL);

  int num_of_processors = base::SysInfo::NumberOfProcessors();
  std::unique_ptr<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[]> processor_info(
      new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[num_of_processors]);

  ULONG returned_bytes = 0,
        bytes = sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) *
                num_of_processors;
  if (!NT_SUCCESS(NtQuerySystemInformation(
          SystemProcessorPerformanceInformation, processor_info.get(), bytes,
          &returned_bytes))) {
    return false;
  }

  int returned_num_of_processors =
      returned_bytes / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);

  if (returned_num_of_processors != num_of_processors) {
    return false;
  }

  DCHECK_EQ(num_of_processors, static_cast<int>(infos->size()));
  for (int i = 0; i < returned_num_of_processors; ++i) {
    double kernel = static_cast<double>(processor_info[i].KernelTime.QuadPart),
           user = static_cast<double>(processor_info[i].UserTime.QuadPart),
           idle = static_cast<double>(processor_info[i].IdleTime.QuadPart);

    // KernelTime needs to be fixed-up, because it includes both idle time and
    // real kernel time.
    infos->at(i).usage.kernel = kernel - idle;
    infos->at(i).usage.user = user;
    infos->at(i).usage.idle = idle;
    infos->at(i).usage.total = kernel + user;
  }

  return true;
}

}  // namespace extensions
