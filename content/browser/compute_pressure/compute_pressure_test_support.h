// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_TEST_SUPPORT_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_TEST_SUPPORT_H_

#include <initializer_list>
#include <ostream>
#include <utility>

#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/thread_annotations.h"
#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/browser/compute_pressure/cpu_probe.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"

namespace content {

// googletest integration with ComputePressureSample.
bool operator==(const ComputePressureSample& lhs,
                const ComputePressureSample& rhs) noexcept;

std::ostream& operator<<(std::ostream& os, const ComputePressureSample& sample);

// googletest integration with CpuCoreSpeedInfo.
struct CpuCoreSpeedInfo;
std::ostream& operator<<(std::ostream& os, const CpuCoreSpeedInfo& info);

// Synchronous proxy to a blink::mojom::ComputePressureHost.
class ComputePressureHostSync {
 public:
  // The caller must ensure that `manager` outlives the newly created instance.
  explicit ComputePressureHostSync(blink::mojom::ComputePressureHost* host);
  ~ComputePressureHostSync();

  ComputePressureHostSync(const ComputePressureHostSync&) = delete;
  ComputePressureHostSync& operator=(const ComputePressureHostSync&) = delete;

  blink::mojom::ComputePressureStatus AddObserver(
      const blink::mojom::ComputePressureQuantization& quantization,
      mojo::PendingRemote<blink::mojom::ComputePressureObserver> observer);

 private:
  // The reference is immutable, so accessing it is thread-safe. The referenced
  // blink::mojom::ComputePressureHost implementation is called synchronously,
  // so it's acceptable to rely on its own thread-safety checks.
  blink::mojom::ComputePressureHost& host_;
};

// Test double for ComputePressureObserver that records all updates.
class FakeComputePressureObserver
    : public blink::mojom::ComputePressureObserver {
 public:
  FakeComputePressureObserver();
  ~FakeComputePressureObserver() override;

  FakeComputePressureObserver(const FakeComputePressureObserver&) = delete;
  FakeComputePressureObserver& operator=(const FakeComputePressureObserver&) =
      delete;

  // blink::mojom::ComputePressureObserver implementation.
  void OnUpdate(blink::mojom::ComputePressureStatePtr state) override;

  std::vector<blink::mojom::ComputePressureState>& updates() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return updates_;
  }

  void SetNextUpdateCallback(base::OnceClosure callback);
  void WaitForUpdate();
  static void WaitForUpdates(
      std::initializer_list<FakeComputePressureObserver*> observers);

  mojo::PendingRemote<blink::mojom::ComputePressureObserver>
  BindNewPipeAndPassRemote();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<blink::mojom::ComputePressureState> updates_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<blink::mojom::ComputePressureObserver> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

// Test double for CpuProbe that always returns a predetermined value.
class FakeCpuProbe : public CpuProbe {
 public:
  // Value returned by LastSample() if SetLastSample() is not called.
  static constexpr ComputePressureSample kInitialSample = {
      .cpu_utilization = 0.42,
      .cpu_speed = 0.84};

  FakeCpuProbe();
  ~FakeCpuProbe() override;

  // CpuProbe implementation.
  void Update() override;
  ComputePressureSample LastSample() override;

  // Can be called from any thread.
  void SetLastSample(ComputePressureSample sample);

 private:
  // Bound to the sequence for Update() and LastSample().
  SEQUENCE_CHECKER(sequence_checker_);

  base::Lock lock_;
  ComputePressureSample last_sample_ GUARDED_BY_CONTEXT(lock_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_TEST_SUPPORT_H_
