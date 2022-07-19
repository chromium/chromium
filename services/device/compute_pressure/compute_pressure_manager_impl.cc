// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/compute_pressure_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/compute_pressure/compute_pressure_sample.h"
#include "services/device/compute_pressure/compute_pressure_sampler.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/public/mojom/compute_pressure_state.mojom.h"

namespace device {

constexpr base::TimeDelta ComputePressureManagerImpl::kDefaultSamplingInterval;

// static
std::unique_ptr<ComputePressureManagerImpl>
ComputePressureManagerImpl::Create() {
  return base::WrapUnique(new ComputePressureManagerImpl(
      CpuProbe::Create(),
      ComputePressureManagerImpl::kDefaultSamplingInterval));
}

// static
std::unique_ptr<ComputePressureManagerImpl>
ComputePressureManagerImpl::CreateForTesting(
    std::unique_ptr<CpuProbe> cpu_probe,
    base::TimeDelta sampling_interval) {
  return base::WrapUnique(
      new ComputePressureManagerImpl(std::move(cpu_probe), sampling_interval));
}

ComputePressureManagerImpl::ComputePressureManagerImpl(
    std::unique_ptr<CpuProbe> cpu_probe,
    base::TimeDelta sampling_interval)
    // base::Unretained usage is safe here because the callback is only run
    // while `sampler_` is alive, and `sampler_` is owned by this instance.
    : sampler_(std::move(cpu_probe),
               sampling_interval,
               base::BindRepeating(&ComputePressureManagerImpl::UpdateClients,
                                   base::Unretained(this))) {
  // base::Unretained use is safe because mojo guarantees the callback will not
  // be called after `clients_` is deallocated, and `clients_` is owned by
  // ComputePressureManagerImpl.
  clients_.set_disconnect_handler(base::BindRepeating(
      &ComputePressureManagerImpl::OnClientRemoteDisconnected,
      base::Unretained(this)));
}

ComputePressureManagerImpl::~ComputePressureManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ComputePressureManagerImpl::Bind(
    mojo::PendingReceiver<mojom::ComputePressureManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver));
}

void ComputePressureManagerImpl::AddClient(
    mojo::PendingRemote<mojom::ComputePressureClient> client,
    AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sampler_.has_probe()) {
    std::move(callback).Run(false);
    return;
  }
  clients_.Add(std::move(client));
  sampler_.EnsureStarted();
  std::move(callback).Run(true);
}

void ComputePressureManagerImpl::UpdateClients(ComputePressureSample sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const mojom::ComputePressureState state{sample.cpu_utilization};
  const base::Time timestamp = base::Time::Now();
  for (auto& client : clients_)
    client->ComputePressureStateChanged(state.Clone(), timestamp);
}

void ComputePressureManagerImpl::OnClientRemoteDisconnected(
    mojo::RemoteSetElementId /*id*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clients_.empty())
    sampler_.Stop();
}

}  // namespace device
