// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_impl.h"

#include <utility>

#include "content/public/browser/device_service.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

PressureServiceImpl::PressureServiceImpl(RenderFrameHost* render_frame_host)
    : DocumentUserData<PressureServiceImpl>(render_frame_host) {
  CHECK(render_frame_host);
}

PressureServiceImpl::~PressureServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PressureServiceImpl::BindReceiver(
    mojo::PendingReceiver<device::mojom::PressureManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (manager_receiver_.is_bound()) {
    mojo::ReportBadMessage("PressureService is already connected.");
    return;
  }
  manager_receiver_.Bind(std::move(receiver));
  // base::Unretained is safe because Mojo guarantees the callback will not
  // be called after `manager_receiver_` is deallocated, and `manager_receiver_`
  // is owned by this class.
  manager_receiver_.set_disconnect_handler(
      base::BindRepeating(&PressureServiceImpl::OnPressureManagerDisconnected,
                          base::Unretained(this)));
}

void PressureServiceImpl::AddClient(
    mojo::PendingRemote<device::mojom::PressureClient> client,
    device::mojom::PressureSource source,
    AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!render_frame_host().IsActive() ||
      render_frame_host().IsNestedWithinFencedFrame()) {
    std::move(callback).Run(device::mojom::PressureStatus::kNotSupported);
    return;
  }

  auto& pressure_client = source_to_client_[static_cast<size_t>(source)];
  if (pressure_client.has_remote()) {
    manager_receiver_.ReportBadMessage(
        "PressureClientImpl is already connected.");
    return;
  }

  if (!manager_remote_.is_bound()) {
    auto receiver = manager_remote_.BindNewPipeAndPassReceiver();
    // base::Unretained is safe because Mojo guarantees the callback will not
    // be called after `manager_remote_` is deallocated, and `manager_remote_`
    // is owned by this class.
    manager_remote_.set_disconnect_handler(
        base::BindRepeating(&PressureServiceImpl::OnPressureManagerDisconnected,
                            base::Unretained(this)));
    GetDeviceService().BindPressureManager(std::move(receiver));
  }

  pressure_client.AddClient(manager_remote_.get(), std::move(client), source,
                            std::move(callback));
}

// Disconnection handler for |manager_receiver_| and |manager_remote_|. If
// either of the connections breaks, we should disconnect all connections and
// let //services know we do not need more updates.
void PressureServiceImpl::OnPressureManagerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager_receiver_.reset();
  manager_remote_.reset();
}

DOCUMENT_USER_DATA_KEY_IMPL(PressureServiceImpl);

}  // namespace content
