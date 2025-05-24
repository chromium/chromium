// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_client_impl.h"

#include "content/browser/compute_pressure/pressure_service_base.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_update.mojom.h"

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
    device::mojom::PressureState state;
    switch (update->source) {
      case device::mojom::PressureSource::kCpu:
        // No update from the virtual pressure source.
        if (update->data->own_contribution_estimate ==
            device::mojom::kDefaultOwnContributionEstimate) {
          update->data->own_contribution_estimate =
              service_->CalculateOwnContributionEstimate(
                  update->data->cpu_utilization);
        }
        state = service_->CalculateState(update->data->cpu_utilization);
        break;
      default:
        NOTREACHED();
    }
    client_associated_remote_->OnPressureUpdated(
        blink::mojom::WebPressureUpdate::New(
            update->source, state, update->data->own_contribution_estimate,
            update->timestamp));
  }
}

// Disconnection handler for |client_receiver_| and |client_remote_|. If the
// PressureClient connection from //services or to Blink breaks, we should stop
// delivering updates.
void PressureClientImpl::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_associated_receiver_.reset();
  client_associated_remote_.reset();
  pressure_source_type_ = PressureSourceType::kUnknown;
}

// Set pressure source type from //service.
void PressureClientImpl::SetPressureSourceType(bool is_virtual_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_virtual_source) {
    pressure_source_type_ = PressureSourceType::kVirtual;
  } else {
    pressure_source_type_ = PressureSourceType::kNonVirtual;
  }
}

mojo::PendingAssociatedRemote<device::mojom::PressureClient>
PressureClientImpl::BindNewEndpointAndPassRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto pending_associated_remote =
      client_associated_receiver_.BindNewEndpointAndPassRemote();

  client_associated_receiver_.set_disconnect_handler(
      base::BindOnce(&PressureClientImpl::Reset, base::Unretained(this)));
  return pending_associated_remote;
}

// Bind PressureClient pendingRemote from Blink.
void PressureClientImpl::BindPendingAssociatedRemote(
    mojo::PendingAssociatedRemote<blink::mojom::WebPressureClient>
        pending_associated_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_associated_remote_.Bind(std::move(pending_associated_remote));
  client_associated_remote_.set_disconnect_handler(
      base::BindOnce(&PressureClientImpl::Reset, base::Unretained(this)));
}

}  // namespace content
