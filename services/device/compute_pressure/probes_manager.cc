// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/probes_manager.h"

#include "base/time/time.h"
#include "components/system_cpu/cpu_probe.h"
#include "services/device/compute_pressure/cpu_probe_manager.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"

namespace device {

ProbesManager::ProbesManager(base::TimeDelta sampling_interval)
    // base::Unretained usage is safe here because the callback is only run
    // while `cpu_probe_manager_` is alive, and `cpu_probe_manager_` is owned by
    // this instance.
    : cpu_probe_manager_(CpuProbeManager::Create(
          sampling_interval,
          base::BindRepeating(&ProbesManager::UpdateClients,
                              base::Unretained(this),
                              mojom::PressureSource::kCpu))) {
  constexpr size_t kPressureSourceSize =
      static_cast<size_t>(mojom::PressureSource::kMaxValue) + 1u;
  for (size_t source_index = 0u; source_index < kPressureSourceSize;
       ++source_index) {
    auto source = static_cast<mojom::PressureSource>(source_index);
    // base::Unretained use is safe because mojo guarantees the callback will
    // not be called after `clients_` is deallocated, and `clients_` is owned by
    // this instance.
    clients_[source].set_disconnect_handler(
        base::BindRepeating(&ProbesManager::OnClientRemoteDisconnected,
                            base::Unretained(this), source));
  }
}

ProbesManager::~ProbesManager() = default;

mojom::PressureStatus ProbesManager::AddClient(
    mojo::PendingRemote<mojom::PressureClient> client,
    mojom::PressureSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (source) {
    case mojom::PressureSource::kCpu: {
      if (!cpu_probe_manager_) {
        return mojom::PressureStatus::kNotSupported;
      }
      clients_[source].Add(std::move(client));
      cpu_probe_manager_->EnsureStarted();
      return mojom::PressureStatus::kOk;
    }
  }
}

void ProbesManager::UpdateClients(mojom::PressureSource source,
                                  mojom::PressureState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeTicks timestamp = base::TimeTicks::Now();

  mojom::PressureUpdate update(source, state, timestamp);
  for (auto& client : clients_[source]) {
    client->OnPressureUpdated(update.Clone());
  }
}

void ProbesManager::OnClientRemoteDisconnected(
    mojom::PressureSource source,
    mojo::RemoteSetElementId /*id*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clients_[source].empty()) {
    switch (source) {
      case mojom::PressureSource::kCpu: {
        cpu_probe_manager_->Stop();
        return;
      }
    }
  }
}

void ProbesManager::SetCpuProbeForTesting(
    std::unique_ptr<system_cpu::CpuProbe> cpu_probe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr base::TimeDelta kDefaultSamplingIntervalForTesting =
      base::Milliseconds(10);

  // base::Unretained usage is safe here because the callback is only run
  // while `cpu_probe_manager_` is alive, and `cpu_probe_manager_` is owned by
  // this instance.
  cpu_probe_manager_ = CpuProbeManager::CreateForTesting(
      std::move(cpu_probe), kDefaultSamplingIntervalForTesting,
      base::BindRepeating(&ProbesManager::UpdateClients, base::Unretained(this),
                          mojom::PressureSource::kCpu));
}

}  // namespace device
