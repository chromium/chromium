// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

#include <mach/mach_host.h>

#include "base/apple/scoped_mach_port.h"
#include "base/mac/mac_util.h"
#include "base/system/sys_info.h"

namespace extensions {

bool CpuInfoProvider::QueryCpuTimePerProcessor(
    std::vector<api::system_cpu::ProcessorInfo>* infos) {
  if (base::mac::GetCPUType() == base::mac::CPUType::kTranslatedIntel) {
    // In writing Rosetta, Apple needed to stop simulating an x86 environment
    // somewhere, and they did so before they got to `host_processor_info()`.
    // `host_processor_info()` is a Mach call to a host server in the kernel,
    // and that server does not maintain data corresponding to the simulated
    // processors. See https://crbug.com/1138707#c42 for details. See also
    // FB8832191.
    return false;
  }

  DCHECK(infos);

  natural_t num_of_processors;
  base::apple::ScopedMachSendRight host(mach_host_self());
  mach_msg_type_number_t type;
  processor_cpu_load_info_data_t* cpu_infos;

  if (host_processor_info(host.get(), PROCESSOR_CPU_LOAD_INFO,
                          &num_of_processors,
                          reinterpret_cast<processor_info_array_t*>(&cpu_infos),
                          &type) == KERN_SUCCESS) {
    DCHECK_EQ(num_of_processors,
              static_cast<natural_t>(base::SysInfo::NumberOfProcessors()));
    DCHECK_EQ(num_of_processors, static_cast<natural_t>(infos->size()));

    for (natural_t i = 0; i < num_of_processors; ++i) {
      double user = static_cast<double>(cpu_infos[i].cpu_ticks[CPU_STATE_USER]),
             sys =
                 static_cast<double>(cpu_infos[i].cpu_ticks[CPU_STATE_SYSTEM]),
             nice = static_cast<double>(cpu_infos[i].cpu_ticks[CPU_STATE_NICE]),
             idle = static_cast<double>(cpu_infos[i].cpu_ticks[CPU_STATE_IDLE]);

      infos->at(i).usage.kernel = sys;
      infos->at(i).usage.user = user + nice;
      infos->at(i).usage.idle = idle;
      infos->at(i).usage.total = sys + user + nice + idle;
    }

    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpu_infos),
                  num_of_processors * sizeof(processor_cpu_load_info));

    return true;
  }

  return false;
}

}  // namespace extensions
