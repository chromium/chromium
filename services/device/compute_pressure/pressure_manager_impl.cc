// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/pressure_manager_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/public/mojom/pressure_update.mojom.h"

namespace device {

constexpr base::TimeDelta PressureManagerImpl::kDefaultSamplingInterval;

// static
std::unique_ptr<PressureManagerImpl> PressureManagerImpl::Create() {
  return base::WrapUnique(new PressureManagerImpl(kDefaultSamplingInterval));
}

PressureManagerImpl::PressureManagerImpl(base::TimeDelta sampling_interval)
    // base::Unretained usage is safe here because the callback is only run
    // while `cpu_probe_` is alive, and `cpu_probe_` is owned by this instance.
    : cpu_probe_(CpuProbe::Create(
          sampling_interval,
          base::BindRepeating(&PressureManagerImpl::UpdateClients,
                              base::Unretained(this)))) {
  // base::Unretained use is safe because mojo guarantees the callback will not
  // be called after `clients_` is deallocated, and `clients_` is owned by
  // PressureManagerImpl.
  clients_.set_disconnect_handler(
      base::BindRepeating(&PressureManagerImpl::OnClientRemoteDisconnected,
                          base::Unretained(this)));
}

PressureManagerImpl::~PressureManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PressureManagerImpl::Bind(
    mojo::PendingReceiver<mojom::PressureManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver));
}

void PressureManagerImpl::AddClient(
    mojo::PendingRemote<mojom::PressureClient> client,
    AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!cpu_probe_) {
    std::move(callback).Run(mojom::PressureStatus::kNotSupported);
    return;
  }
  clients_.Add(std::move(client));
  cpu_probe_->EnsureStarted();
  std::move(callback).Run(mojom::PressureStatus::kOk);
}

void PressureManagerImpl::UpdateClients(mojom::PressureState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Time timestamp = base::Time::Now();

  // TODO(crbug.com/1365627): Implement algorithm for pressure factors.
  // https://wicg.github.io/compute-pressure/#contributing-factors

  mojom::PressureUpdate update(state, {}, timestamp);
  for (auto& client : clients_) {
    client->OnPressureUpdated(update.Clone());
  }
}

void PressureManagerImpl::OnClientRemoteDisconnected(
    mojo::RemoteSetElementId /*id*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clients_.empty())
    cpu_probe_->Stop();
}

void PressureManagerImpl::SetCpuProbeForTesting(
    std::unique_ptr<CpuProbe> cpu_probe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_ = std::move(cpu_probe);
}

}  // namespace device
