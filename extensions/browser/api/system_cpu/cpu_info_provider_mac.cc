// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

#include <mach/mach.h>
#include <mach/mach_host.h>

#include "base/apple/scoped_mach_port.h"
#include "base/apple/scoped_mach_vm.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/mac/mac_util.h"

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
  mach_msg_type_number_t num_cpu_info;
  processor_cpu_load_info_data_t* cpu_infos;

  if (host_processor_info(host.get(), PROCESSOR_CPU_LOAD_INFO,
                          &num_of_processors,
                          reinterpret_cast<processor_info_array_t*>(&cpu_infos),
                          &num_cpu_info) == KERN_SUCCESS) {
    base::apple::ScopedMachVM vm_owner(
        reinterpret_cast<vm_address_t>(cpu_infos),
        mach_vm_round_page(num_cpu_info * sizeof(integer_t)));

    if (num_of_processors != infos->size() ||
        num_cpu_info !=
            size_t{num_of_processors} * PROCESSOR_CPU_LOAD_INFO_COUNT) {
      return false;
    }

    static_assert(sizeof(processor_cpu_load_info_data_t) ==
                  PROCESSOR_CPU_LOAD_INFO_COUNT * sizeof(integer_t));
    // SAFETY: `host_processor_info()` is a C Mach IPC API that returns a raw
    // out-of-line pointer and count with no span-based alternative. On
    // `KERN_SUCCESS`, the kernel allocates `num_cpu_info` integers at
    // `cpu_infos`, which the check and `static_assert` above verify hold
    // `num_of_processors` `processor_cpu_load_info_data_t` elements.
    base::span<const processor_cpu_load_info_data_t> cpu_infos_span =
        UNSAFE_BUFFERS(base::span(cpu_infos, num_of_processors));

    for (size_t i = 0; i < cpu_infos_span.size(); ++i) {
      double user = static_cast<double>(
                 cpu_infos_span[i].cpu_ticks[CPU_STATE_USER]),
             sys = static_cast<double>(
                 cpu_infos_span[i].cpu_ticks[CPU_STATE_SYSTEM]),
             nice = static_cast<double>(
                 cpu_infos_span[i].cpu_ticks[CPU_STATE_NICE]),
             idle = static_cast<double>(
                 cpu_infos_span[i].cpu_ticks[CPU_STATE_IDLE]);

      infos->at(i).usage.kernel = sys;
      infos->at(i).usage.user = user + nice;
      infos->at(i).usage.idle = idle;
      infos->at(i).usage.total = sys + user + nice + idle;
    }

    return true;
  }

  return false;
}

}  // namespace extensions
