// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/compute_pressure_test_support.h"

#include <initializer_list>
#include <ostream>
#include <utility>

#include "base/barrier_closure.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/browser/compute_pressure/cpu_core_speed_info.h"

namespace content {

bool operator==(const ComputePressureSample& lhs,
                const ComputePressureSample& rhs) noexcept {
  return std::make_pair(lhs.cpu_utilization, lhs.cpu_speed) ==
         std::make_pair(rhs.cpu_utilization, rhs.cpu_speed);
}
std::ostream& operator<<(std::ostream& os,
                         const ComputePressureSample& sample) {
  os << "[utilization: " << sample.cpu_utilization
     << " speed: " << sample.cpu_speed << "]";
  return os;
}

std::ostream& operator<<(std::ostream& os, const CpuCoreSpeedInfo& info) {
  os << "[min: " << info.min_frequency << " max: " << info.max_frequency
     << " base: " << info.base_frequency
     << " current: " << info.current_frequency << "]";
  return os;
}

ComputePressureHostSync::ComputePressureHostSync(
    blink::mojom::ComputePressureHost* host)
    : host_(*host) {
  DCHECK(host);
}

ComputePressureHostSync::~ComputePressureHostSync() = default;

blink::mojom::ComputePressureStatus ComputePressureHostSync::AddObserver(
    const blink::mojom::ComputePressureQuantization& quantization,
    mojo::PendingRemote<blink::mojom::ComputePressureObserver> observer) {
  blink::mojom::ComputePressureStatus result;
  base::RunLoop run_loop;
  host_.AddObserver(std::move(observer), quantization.Clone(),
                    base::BindLambdaForTesting(
                        [&](blink::mojom::ComputePressureStatus status) {
                          result = status;
                          run_loop.Quit();
                        }));
  run_loop.Run();
  return result;
}

FakeComputePressureObserver::FakeComputePressureObserver() : receiver_(this) {}

FakeComputePressureObserver::~FakeComputePressureObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeComputePressureObserver::OnUpdate(
    blink::mojom::ComputePressureStatePtr state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  updates_.push_back(*state);
  if (update_callback_) {
    std::move(update_callback_).Run();
    update_callback_.Reset();
  }
}

void FakeComputePressureObserver::SetNextUpdateCallback(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!update_callback_)
      << __func__ << " already called before update received";
  update_callback_ = std::move(callback);
}

void FakeComputePressureObserver::WaitForUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::RunLoop run_loop;
  SetNextUpdateCallback(run_loop.QuitClosure());
  run_loop.Run();
}

// static
void FakeComputePressureObserver::WaitForUpdates(
    std::initializer_list<FakeComputePressureObserver*> observers) {
  base::RunLoop run_loop;
  base::RepeatingClosure update_barrier =
      base::BarrierClosure(observers.size(), run_loop.QuitClosure());
  for (FakeComputePressureObserver* observer : observers)
    observer->SetNextUpdateCallback(update_barrier);
  run_loop.Run();
}

mojo::PendingRemote<blink::mojom::ComputePressureObserver>
FakeComputePressureObserver::BindNewPipeAndPassRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return receiver_.BindNewPipeAndPassRemote();
}

constexpr ComputePressureSample FakeCpuProbe::kInitialSample;

FakeCpuProbe::FakeCpuProbe() : last_sample_(kInitialSample) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FakeCpuProbe::~FakeCpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeCpuProbe::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // In DCHECKed builds, the ScopedBlockingCall ensures that Update() is only
  // called on sequences where I/O is allowed.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
}

ComputePressureSample FakeCpuProbe::LastSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);
  return last_sample_;
}

void FakeCpuProbe::SetLastSample(ComputePressureSample sample) {
  base::AutoLock auto_lock(lock_);
  last_sample_ = sample;
}

}  // namespace content
