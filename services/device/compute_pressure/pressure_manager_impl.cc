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
#include "services/device/compute_pressure/probes_manager.h"

namespace device {

constexpr base::TimeDelta PressureManagerImpl::kDefaultSamplingInterval;

// static
std::unique_ptr<PressureManagerImpl> PressureManagerImpl::Create() {
  return base::WrapUnique(new PressureManagerImpl(kDefaultSamplingInterval));
}

PressureManagerImpl::PressureManagerImpl(base::TimeDelta sampling_interval)
    : probes_manager_(std::make_unique<ProbesManager>(sampling_interval)) {}

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
  std::move(callback).Run(
      probes_manager_->AddClient(std::move(client), source));
}

ProbesManager* PressureManagerImpl::GetProbesManagerForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return probes_manager_.get();
}

}  // namespace device
