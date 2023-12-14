// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_client_impl.h"

#include "content/browser/compute_pressure/pressure_service_base.h"
#include "services/device/public/mojom/pressure_update.mojom.h"

namespace content {

PressureClientImpl::PressureClientImpl(PressureServiceBase* service)
    : service_(service) {}

PressureClientImpl::~PressureClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PressureClientImpl::OnPressureUpdated(
    device::mojom::PressureUpdatePtr update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (service_->ShouldDeliverUpdate()) {
    client_remote_->OnPressureUpdated(std::move(update));
  }
}

void PressureClientImpl::AddClient(
    device::mojom::PressureManager* pressure_manager,
    mojo::PendingRemote<device::mojom::PressureClient> pending_client,
    device::mojom::PressureSource source,
    device::mojom::PressureManager::AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_remote_.Bind(std::move(pending_client));
  // base::Unretained is safe because Mojo guarantees the callback will not
  // be called after `client_remote_` is deallocated, and `client_remote_`
  // is owned by this class.
  client_remote_.set_disconnect_handler(
      base::BindRepeating(&PressureClientImpl::Reset, base::Unretained(this)));

  if (!client_receiver_.is_bound()) {
    auto pending_remote = client_receiver_.BindNewPipeAndPassRemote();
    // base::Unretained is safe because Mojo guarantees the callback will not
    // be called after `client_receiver_` is deallocated, and `client_receiver_`
    // is owned by this class.
    client_receiver_.set_disconnect_handler(
        base::BindOnce(&PressureClientImpl::Reset, base::Unretained(this)));
    pressure_manager->AddClient(std::move(pending_remote), source,
                                std::move(callback));
  } else {
    std::move(callback).Run(device::mojom::PressureStatus::kOk);
  }
}

// Disconnection handler for |client_receiver_| and |client_remote_|. If the
// PressureClient connection from //services or to Blink breaks, we should stop
// delivering updates.
void PressureClientImpl::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_receiver_.reset();
  client_remote_.reset();
}

}  // namespace content
