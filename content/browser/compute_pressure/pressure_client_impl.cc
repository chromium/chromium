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

// Disconnection handler for |client_receiver_| and |client_remote_|. If the
// PressureClient connection from //services or to Blink breaks, we should stop
// delivering updates.
void PressureClientImpl::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_receiver_.reset();
  client_remote_.reset();
}

mojo::PendingReceiver<device::mojom::PressureClient>
PressureClientImpl::BindNewPipeAndPassReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto pending_receiver = client_remote_.BindNewPipeAndPassReceiver();
  // base::Unretained is safe because Mojo guarantees the callback will not
  // be called after `client_remote_` is deallocated, and `client_remote_`
  // is owned by this class.
  client_remote_.set_disconnect_handler(
      base::BindRepeating(&PressureClientImpl::Reset, base::Unretained(this)));
  return pending_receiver;
}

void PressureClientImpl::BindReceiver(
    mojo::PendingReceiver<device::mojom::PressureClient> pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_receiver_.Bind(std::move(pending_receiver));
  client_receiver_.set_disconnect_handler(
      base::BindOnce(&PressureClientImpl::Reset, base::Unretained(this)));
}

}  // namespace content
