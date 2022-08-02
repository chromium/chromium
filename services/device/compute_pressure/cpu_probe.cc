// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe.h"

#include <memory>

#include "base/sequence_checker.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "services/device/compute_pressure/pressure_sample.h"
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "services/device/compute_pressure/cpu_probe_linux.h"
#elif BUILDFLAG(IS_WIN)
#include "services/device/compute_pressure/cpu_probe_win.h"
#elif BUILDFLAG(IS_MAC)
#include "services/device/compute_pressure/cpu_probe_mac.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace device {

constexpr PressureSample CpuProbe::kUnsupportedValue;

CpuProbe::CpuProbe() = default;
CpuProbe::~CpuProbe() = default;

// Default implementation for platforms that don't have one.
class NullCpuProbe : public CpuProbe {
 public:
  NullCpuProbe() { DETACH_FROM_SEQUENCE(sequence_checker_); }
  ~NullCpuProbe() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // CpuProbe implementation.
  void Update() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Ensure that this method is called on a sequence that is allowed to do
    // IO, even on OSes that don't have a CpuProbe implementation yet.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
  }

  PressureSample LastSample() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return CpuProbe::kUnsupportedValue;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

// static
std::unique_ptr<CpuProbe> CpuProbe::Create() {
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return CpuProbeLinux::Create();
#elif BUILDFLAG(IS_WIN)
  return CpuProbeWin::Create();
#elif BUILDFLAG(IS_MAC)
  return CpuProbeMac::Create();
#else
  return std::make_unique<NullCpuProbe>();
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace device
