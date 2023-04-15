// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "services/device/compute_pressure/cpu_probe_linux.h"
#elif BUILDFLAG(IS_WIN)
#include "services/device/compute_pressure/cpu_probe_win.h"
#elif BUILDFLAG(IS_MAC)
#include "services/device/compute_pressure/cpu_probe_mac.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace device {

namespace {

// Delta for the state decision hysteresis.
constexpr double kThresholdDelta = 0.03;

constexpr std::array<double,
                     static_cast<size_t>(mojom::PressureState::kMaxValue) + 1>
    kStateThresholds = {0.3,   // kNominal
                        0.6,   // kFair
                        0.9,   // kSerious
                        1.0};  // kCritical

}  // namespace

// static
std::unique_ptr<CpuProbe> CpuProbe::Create(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return CpuProbeLinux::Create(sampling_interval, std::move(sampling_callback));
#elif BUILDFLAG(IS_WIN)
  return CpuProbeWin::Create(sampling_interval, std::move(sampling_callback));
#elif BUILDFLAG(IS_MAC)
  return CpuProbeMac::Create(sampling_interval, std::move(sampling_callback));
#else
  return nullptr;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

CpuProbe::CpuProbe(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback)
    : sampling_interval_(sampling_interval),
      sampling_callback_(std::move(sampling_callback)) {
  CHECK(sampling_callback_);
}

CpuProbe::~CpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbe::EnsureStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (timer_.IsRunning()) {
    return;
  }

  CHECK(!got_probe_baseline_) << "got_probe_baseline_ incorrectly reset";

  // Schedule the first CpuProbe update right away. This update result will
  // not be reported, thanks to the accounting done by `got_probe_baseline_`.
  Update();

  timer_.Start(FROM_HERE, sampling_interval_,
               base::BindRepeating(&CpuProbe::Update, base::Unretained(this)));
}

void CpuProbe::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.AbandonAndStop();
  got_probe_baseline_ = false;
}

void CpuProbe::OnPressureSampleAvailable(PressureSample sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop sending data when Stop() was already called.
  if (!timer_.IsRunning()) {
    return;
  }

  // Don't report the first update result.
  if (!got_probe_baseline_) {
    got_probe_baseline_ = true;
    return;
  }

  sampling_callback_.Run(CalculateState(sample));
}

mojom::PressureState CpuProbe::CalculateState(const PressureSample& sample) {
  // TODO(crbug.com/1342528): A more advanced algorithm that calculates
  // PressureState using PressureSample needs to be determined.
  // At this moment the algorithm is the simplest possible
  // with thresholds defining the state.
  auto* it =
      base::ranges::lower_bound(kStateThresholds, sample.cpu_utilization);
  if (it == kStateThresholds.end()) {
    NOTREACHED_NORETURN() << "unexpected value: " << sample.cpu_utilization;
  }

  size_t state_index = std::distance(kStateThresholds.begin(), it);

  // Hysteresis to avoid flip-flop between state.
  // Threshold needs to drop by level and
  // cpu_utilization needs a drop of kThresholdDelta below the state
  // threshold to be validated as a lower pressure state.
  if (last_state_index_ - state_index != 1 ||
      kStateThresholds[state_index] - sample.cpu_utilization >=
          kThresholdDelta) {
    last_state_index_ = state_index;
  }

  return static_cast<mojom::PressureState>(last_state_index_);
}

}  // namespace device
