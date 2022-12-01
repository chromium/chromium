// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/compute_pressure/pressure_service.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"

namespace content {

// static
void PressureServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::PressureService> receiver) {
  if (!network::IsOriginPotentiallyTrustworthy(
          render_frame_host->GetLastCommittedOrigin())) {
    mojo::ReportBadMessage("Compute Pressure access from an insecure origin");
    return;
  }

  GetOrCreateForCurrentDocument(render_frame_host)
      ->BindReceiver(std::move(receiver));
}

PressureServiceImpl::PressureServiceImpl(RenderFrameHost* render_frame_host)
    : DocumentUserData<PressureServiceImpl>(render_frame_host) {
  DCHECK(render_frame_host);
}

PressureServiceImpl::~PressureServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PressureServiceImpl::BindReceiver(
    mojo::PendingReceiver<blink::mojom::PressureService> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (receiver_.is_bound()) {
    mojo::ReportBadMessage("PressureService is already connected");
    return;
  }
  receiver_.Bind(std::move(receiver));
}

void PressureServiceImpl::BindObserver(
    mojo::PendingRemote<blink::mojom::PressureObserver> observer,
    BindObserverCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!render_frame_host().IsActive() ||
      render_frame_host().IsNestedWithinFencedFrame()) {
    std::move(callback).Run(blink::mojom::PressureStatus::kNotSupported);
    return;
  }

  if (!remote_.is_bound()) {
    auto receiver = remote_.BindNewPipeAndPassReceiver();
    // base::Unretained is safe because Mojo guarantees the callback will not
    // be called after `remote_` is deallocated, and `remote_` is owned by
    // this class.
    remote_.set_disconnect_handler(
        base::BindRepeating(&PressureServiceImpl::OnManagerRemoteDisconnected,
                            base::Unretained(this)));
    GetDeviceService().BindPressureManager(std::move(receiver));
  }

  ResetObserverState();

  observer_.Bind(std::move(observer));
  // base::Unretained is safe because Mojo guarantees the callback will not
  // be called after `observers_` is deallocated, and `observers_` is owned by
  // this class.
  observer_.set_disconnect_handler(
      base::BindRepeating(&PressureServiceImpl::OnObserverRemoteDisconnected,
                          base::Unretained(this)));

  client_.reset();
  remote_->AddClient(
      client_.BindNewPipeAndPassRemote(),
      base::BindOnce(&PressureServiceImpl::DidBindObserver,
                     base::Unretained(this), std::move(callback)));

  // base::Unretained is safe because Mojo guarantees the callback will not
  // be called after `client_` is deallocated, and `client_` is owned by
  // this class.
  client_.set_disconnect_handler(base::BindOnce(
      &PressureServiceImpl::ResetObserverState, base::Unretained(this)));
}

void PressureServiceImpl::PressureStateChanged(
    device::mojom::PressureUpdatePtr update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1385588): Remove this when "passes privacy test" steps are
  // implemented.
  if (!render_frame_host().IsActive()) {
    // TODO(jsbell): Is it safe to disconnect observers in this state?
    return;
  }

  // TODO(crbug.com/1385588): Remove this when "passes privacy test" steps are
  // implemented.
  if (render_frame_host().GetVisibilityState() !=
      blink::mojom::PageVisibilityState::kVisible) {
    // TODO(jsbell): Rate-limit observers in non-visible frames instead of
    //               cutting off their updates completely.
    return;
  }

  observer_->OnUpdate(update.Clone());
}

void PressureServiceImpl::OnObserverRemoteDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_.reset();
  ResetObserverState();
}

void PressureServiceImpl::OnManagerRemoteDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_.reset();
  client_.reset();
  remote_.reset();
}

void PressureServiceImpl::DidBindObserver(BindObserverCallback callback,
                                          bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    std::move(callback).Run(blink::mojom::PressureStatus::kNotSupported);
    ResetObserverState();
    return;
  }

  std::move(callback).Run(blink::mojom::PressureStatus::kOk);
}

void PressureServiceImpl::ResetObserverState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_.reset();
}

DOCUMENT_USER_DATA_KEY_IMPL(PressureServiceImpl);

}  // namespace content
