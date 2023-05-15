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
                              base::Unretained(this),
                              mojom::PressureSource::kCpu))) {
  constexpr size_t kPressureSourceSize =
      static_cast<size_t>(mojom::PressureSource::kMaxValue) + 1u;
  for (size_t source_index = 0u; source_index < kPressureSourceSize;
       ++source_index) {
    auto source = static_cast<mojom::PressureSource>(source_index);
    // base::Unretained use is safe because mojo guarantees the callback will
    // not be called after `clients_` is deallocated, and `clients_` is owned by
    // PressureManagerImpl.
    clients_[source].set_disconnect_handler(
        base::BindRepeating(&PressureManagerImpl::OnClientRemoteDisconnected,
                            base::Unretained(this), source));
  }
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
    mojom::PressureSource source,
    AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (source) {
    case mojom::PressureSource::kCpu: {
      if (!cpu_probe_) {
        std::move(callback).Run(mojom::PressureStatus::kNotSupported);
        return;
      }
      clients_[source].Add(std::move(client));
      cpu_probe_->EnsureStarted();
      std::move(callback).Run(mojom::PressureStatus::kOk);
      break;
    }
  }
}

void PressureManagerImpl::UpdateClients(mojom::PressureSource source,
                                        mojom::PressureState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Time timestamp = base::Time::Now();

  mojom::PressureUpdate update(source, state, timestamp);
  for (auto& client : clients_[source]) {
    client->OnPressureUpdated(update.Clone());
  }
}

void PressureManagerImpl::OnClientRemoteDisconnected(
    mojom::PressureSource source,
    mojo::RemoteSetElementId /*id*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clients_[source].empty()) {
    switch (source) {
      case mojom::PressureSource::kCpu: {
        cpu_probe_->Stop();
        return;
      }
    }
  }
}

void PressureManagerImpl::SetCpuProbeForTesting(
    std::unique_ptr<CpuProbe> cpu_probe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_ = std::move(cpu_probe);
}

}  // namespace device
