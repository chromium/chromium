// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/pressure_manager_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/compute_pressure/probes_manager.h"
#include "services/device/compute_pressure/virtual_probes_manager.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"

namespace device {

// static
std::unique_ptr<PressureManagerImpl> PressureManagerImpl::Create(
    base::TimeDelta sampling_interval) {
  return base::WrapUnique(new PressureManagerImpl(sampling_interval));
}

PressureManagerImpl::PressureManagerImpl(base::TimeDelta sampling_interval)
    : sampling_interval_(sampling_interval),
      probes_manager_(std::make_unique<ProbesManager>(sampling_interval)) {}

PressureManagerImpl::~PressureManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PressureManagerImpl::Bind(
    mojo::PendingReceiver<mojom::PressureManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver));
}

void PressureManagerImpl::AddClient(
    mojom::PressureSource source,
    const std::optional<base::UnguessableToken>& token,
    AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProbesManager* manager = probes_manager_.get();

  if (token) {
    auto it = virtual_probes_managers_.find(*token);
    if (it == virtual_probes_managers_.end()) {
      // For now, treat a non-existent token just like a non-existent pressure
      // source.
      std::move(callback).Run(mojom::PressureManagerAddClientResult::NewError(
          mojom::PressureManagerAddClientError::kNotSupported));
      return;
    }
    manager = it->second.get();
  }

  if (!manager->is_supported(source)) {
    std::move(callback).Run(mojom::PressureManagerAddClientResult::NewError(
        mojom::PressureManagerAddClientError::kNotSupported));
    return;
  }

  mojo::Remote<mojom::PressureClient> pressure_client;
  auto pending_receiver = pressure_client.BindNewPipeAndPassReceiver();
  manager->RegisterClientRemote(std::move(pressure_client), source);

  std::move(callback).Run(
      mojom::PressureManagerAddClientResult::NewPressureClient(
          std::move(pending_receiver)));
}

ProbesManager* PressureManagerImpl::GetProbesManagerForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return probes_manager_.get();
}

void PressureManagerImpl::AddVirtualPressureSource(
    const base::UnguessableToken& token,
    mojom::PressureSource source,
    mojom::VirtualPressureSourceMetadataPtr metadata,
    AddVirtualPressureSourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& virtual_probes_manager = virtual_probes_managers_[token];
  if (!virtual_probes_manager) {
    virtual_probes_manager =
        std::make_unique<VirtualProbesManager>(sampling_interval_);
  }

  if (!virtual_probes_manager->AddOverrideForSource(source,
                                                    std::move(metadata))) {
    receivers_.ReportBadMessage(
        "The provided pressure source is already being overridden");
    return;
  }

  std::move(callback).Run();
}

void PressureManagerImpl::RemoveVirtualPressureSource(
    const base::UnguessableToken& token,
    mojom::PressureSource source,
    RemoveVirtualPressureSourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = virtual_probes_managers_.find(token);
  if (it != virtual_probes_managers_.end()) {
    auto& virtual_probe = it->second;
    virtual_probe->RemoveOverrideForSource(source);
  }

  std::move(callback).Run();
}

void PressureManagerImpl::UpdateVirtualPressureSourceState(
    const base::UnguessableToken& token,
    mojom::PressureSource source,
    mojom::PressureState state,
    UpdateVirtualPressureSourceStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = virtual_probes_managers_.find(token);
  if (it != virtual_probes_managers_.end() &&
      it->second->IsOverriding(source)) {
    it->second->AddUpdate(source, state);
  }
  std::move(callback).Run();
}

}  // namespace device
