// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_MANAGER_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/browser/compute_pressure/compute_pressure_sampler.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom-forward.h"
#include "url/origin.h"

namespace content {

class CpuProbe;
class ComputePressureHost;

// Drives the Compute Pressure API implementation for a user profile.
//
// Coordinates between the objects responsible for serving mojo requests from
// the renderer side of the API implementation, and the objects responsible for
// collecting compute pressure information from the underlying operating system.
//
// Each StoragePartitionImpl owns exactly one instance of this class.
//
// Instances are not thread-safe and should be used on the same sequence.
class CONTENT_EXPORT ComputePressureManager {
 public:
  // The sampling interval must be smaller or equal to the rate-limit for
  // observer updates.
  static constexpr base::TimeDelta kDefaultSamplingInterval = base::Seconds(1);

  // Factory method for production instances.
  static std::unique_ptr<ComputePressureManager> Create();

  // Factory method with dependency injection support for testing.
  static std::unique_ptr<ComputePressureManager> CreateForTesting(
      std::unique_ptr<CpuProbe> cpu_probe,
      base::TimeDelta sampling_interval,
      base::TimeDelta visible_observer_rate_limit);

  // The constructor is public for internal use of std::make_unique.
  //
  // Production code should call ComputePressureManager::Create(). Testing code
  // should call ComputePressureManager::CreateForTesting().
  explicit ComputePressureManager(std::unique_ptr<CpuProbe> cpu_probe,
                                  base::TimeDelta sampling_interval,
                                  base::TimeDelta visible_observer_rate_limit,
                                  base::PassKey<ComputePressureManager>);
  ~ComputePressureManager();

  ComputePressureManager(const ComputePressureManager&) = delete;
  ComputePressureManager& operator=(const ComputePressureManager&) = delete;

  void BindReceiver(
      url::Origin origin,
      GlobalRenderFrameHostId frame_id,
      mojo::PendingReceiver<blink::mojom::ComputePressureHost> receiver);

  // Used by tests that pass in a FakeCpuProbe that they need to direct.
  CpuProbe* cpu_probe_for_testing() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return sampler_.cpu_probe_for_testing();
  }

 private:
  // Called periodically by ComputePressureSampler.
  void UpdateObservers(ComputePressureSample sample);

  // Called by owned ComputePressureHosts to report frame / observer changes.
  void DidHostConnectionsChange(ComputePressureHost* host);

  SEQUENCE_CHECKER(sequence_checker_);

  ComputePressureSampler sampler_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Passed to ComputePressureHost.
  const base::TimeDelta visible_observer_rate_limit_;

  // Keeps track of options for each observers' origin. Only one per origin
  // allowed.
  std::map<url::Origin, ComputePressureHost> hosts_by_origin_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to determine when it's safe to stop the ComputePressureSampler.
  std::set<url::Origin> hosts_with_observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_MANAGER_H_
