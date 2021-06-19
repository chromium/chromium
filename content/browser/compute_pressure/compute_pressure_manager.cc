// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/compute_pressure_manager.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "content/browser/compute_pressure/compute_pressure_host.h"
#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/browser/compute_pressure/compute_pressure_sampler.h"
#include "content/browser/compute_pressure/cpu_probe.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"

namespace content {

constexpr base::TimeDelta ComputePressureManager::kDefaultSamplingInterval;

// static
std::unique_ptr<ComputePressureManager> ComputePressureManager::Create() {
  return std::make_unique<ComputePressureManager>(
      CpuProbe::Create(), ComputePressureManager::kDefaultSamplingInterval,
      ComputePressureHost::kDefaultVisibleObserverRateLimit,
      base::PassKey<ComputePressureManager>());
}

// static
std::unique_ptr<ComputePressureManager>
ComputePressureManager::CreateForTesting(
    std::unique_ptr<CpuProbe> cpu_probe,
    base::TimeDelta sampling_interval,
    base::TimeDelta visible_observer_rate_limit) {
  return std::make_unique<ComputePressureManager>(
      std::move(cpu_probe), sampling_interval, visible_observer_rate_limit,
      base::PassKey<ComputePressureManager>());
}

ComputePressureManager::ComputePressureManager(
    std::unique_ptr<CpuProbe> cpu_probe,
    base::TimeDelta sampling_interval,
    base::TimeDelta visible_observer_rate_limit,
    base::PassKey<ComputePressureManager>)
    // base::Unretained usage is safe here because the callback is only run
    // while `sampler_` is alive, and `sampler_` is owned by this instance.
    : sampler_(std::move(cpu_probe),
               sampling_interval,
               base::BindRepeating(&ComputePressureManager::UpdateObservers,
                                   base::Unretained(this))),
      visible_observer_rate_limit_(visible_observer_rate_limit) {}

ComputePressureManager::~ComputePressureManager() = default;

void ComputePressureManager::BindReceiver(
    url::Origin origin,
    GlobalRenderFrameHostId frame_id,
    mojo::PendingReceiver<blink::mojom::ComputePressureHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_id);

  if (!base::FeatureList::IsEnabled(blink::features::kComputePressure)) {
    mojo::ReportBadMessage("Compute Pressure not enabled");
    return;
  }

  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    mojo::ReportBadMessage("Compute Pressure access from an insecure origin");
    return;
  }

  auto it = hosts_by_origin_.lower_bound(origin);
  if (it == hosts_by_origin_.end() || it->first != origin) {
    // base::Unretained use is safe here because the callback will only be Run()
    // while the ComputePressureHost instance is alive, and the
    // ComputePressureHost instance is owned by this ComputePressureManager
    // indirectly, via `hosts_by_origin_`.
    it = hosts_by_origin_.emplace_hint(
        it, std::piecewise_construct, std::forward_as_tuple(origin),
        std::forward_as_tuple(
            origin, sampler_.has_probe(), visible_observer_rate_limit_,
            base::BindRepeating(
                &ComputePressureManager::DidHostConnectionsChange,
                base::Unretained(this))));
  }

  ComputePressureHost& host = it->second;
  DCHECK_EQ(origin, host.origin());
  host.BindReceiver(frame_id, std::move(receiver));
}

void ComputePressureManager::UpdateObservers(ComputePressureSample sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time sample_time = base::Time::Now();
  for (auto& origin_and_host : hosts_by_origin_)
    origin_and_host.second.UpdateObservers(sample, sample_time);
}

void ComputePressureManager::DidHostConnectionsChange(
    ComputePressureHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host);
  DCHECK(hosts_by_origin_.count(host->origin()));
  DCHECK_EQ(&hosts_by_origin_.at(host->origin()), host);

  if (host->has_observers()) {
    hosts_with_observers_.insert(host->origin());
    sampler_.EnsureStarted();
  } else {
    hosts_with_observers_.erase(host->origin());
    if (hosts_with_observers_.empty()) {
      // If there are no observers left, we can save CPU cycles (and therefore
      // power) by stopping the sampler.
      sampler_.Stop();
    }

    if (!host->has_receivers()) {
      // `host` is no longer valid after the erase.
      hosts_by_origin_.erase(host->origin());
    }
  }
}

}  // namespace content
