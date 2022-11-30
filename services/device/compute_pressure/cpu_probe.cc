// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe.h"

#include <memory>

#include "build/build_config.h"
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "services/device/compute_pressure/cpu_probe_linux.h"
#elif BUILDFLAG(IS_WIN)
#include "services/device/compute_pressure/cpu_probe_win.h"
#elif BUILDFLAG(IS_MAC)
#include "services/device/compute_pressure/cpu_probe_mac.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace device {

CpuProbe::CpuProbe() = default;
CpuProbe::~CpuProbe() = default;

// static
std::unique_ptr<CpuProbe> CpuProbe::Create() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return CpuProbeLinux::Create();
#elif BUILDFLAG(IS_WIN)
  return CpuProbeWin::Create();
#elif BUILDFLAG(IS_MAC)
  return CpuProbeMac::Create();
#else
  return nullptr;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

}  // namespace device
