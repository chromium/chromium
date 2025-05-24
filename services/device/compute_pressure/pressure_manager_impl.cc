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

  manager_receivers_.Add(this, std::move(receiver));
}

void PressureManagerImpl::AddClient(
    mojom::PressureSource source,
    const std::optional<base::UnguessableToken>& token,
    mojo::PendingAssociatedRemote<mojom::PressureClient> client,
    AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProbesManager* manager = probes_manager_.get();

  if (token) {
    auto it = virtual_probes_managers_.find(*token);
    if (it == virtual_probes_managers_.end()) {
      // For now, treat a non-existent token just like a non-existent pressure
      // source.
      std::move(callback).Run(
          mojom::PressureManagerAddClientResult::kNotSupported);
      return;
    }
    manager = it->second.get();
  }

  if (!manager->is_supported(source)) {
    std::move(callback).Run(
        mojom::PressureManagerAddClientResult::kNotSupported);
    return;
  }

  manager->RegisterClientRemote(std::move(client), source);

  std::move(callback).Run(mojom::PressureManagerAddClientResult::kOk);
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
    manager_receivers_.ReportBadMessage(
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

void PressureManagerImpl::UpdateVirtualPressureSourceData(
    const base::UnguessableToken& token,
    mojom::PressureSource source,
    mojom::PressureState state,
    double own_contribution_estimate,
    UpdateVirtualPressureSourceDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = virtual_probes_managers_.find(token);
  if (it != virtual_probes_managers_.end() &&
      it->second->IsOverriding(source)) {
    it->second->AddDataUpdate(source, state, own_contribution_estimate);
  }
  std::move(callback).Run();
}

}  // namespace device
