// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_base.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

PressureServiceBase::PressureServiceBase()
    : source_to_client_{PressureClientImpl(this)} {}

PressureServiceBase::~PressureServiceBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
// https://www.w3.org/TR/compute-pressure/#dfn-document-has-implicit-focus
bool PressureServiceBase::HasImplicitFocus(RenderFrameHost* render_frame_host) {
  // 1. If document is not fully active, return false.
  if (!render_frame_host->IsActive()) {
    return false;
  }

  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));
  const auto& current_origin = render_frame_host->GetLastCommittedOrigin();

  // 3. If associated document is same origin with initiators of active
  // Picture-in-Picture sessions, return true.
  if (base::ranges::any_of(
          WebContentsImpl::GetAllWebContents(), [&](WebContentsImpl* wc) {
            if (!wc->HasPictureInPictureVideo()) {
              return false;
            }

            auto origin = PictureInPictureWindowController::
                              GetOrCreateVideoPictureInPictureController(wc)
                                  ->GetOrigin();
            return current_origin == origin.value() &&
                   wc->GetBrowserContext() == web_contents->GetBrowserContext();
          })) {
    return true;
  }

  // 4. If browsing context is capturing, return true.
  // TODO(crbug.com/40945930): Take muted state into account.
  if (static_cast<RenderFrameHostImpl*>(render_frame_host)
          ->HasMediaStreams(
              RenderFrameHostImpl::MediaStreamType::kCapturingMediaStream)) {
    return true;
  }

  // 6. If top-level browsing context does not have system focus, return false.
  RenderWidgetHostImpl* rwh = static_cast<RenderWidgetHostImpl*>(
      render_frame_host->GetRenderWidgetHost());
  if (!rwh->is_focused()) {
    return false;
  }

  // 7. Let focused document be the currently focused area's node document.
  auto* focused_frame = web_contents->GetFocusedFrame();
  if (!focused_frame) {
    return false;
  }

  // 8. If origin is same origin with focused document, return true.
  // 9. Otherwise, return false.
  return current_origin.IsSameOriginWith(
      focused_frame->GetLastCommittedOrigin());
}

bool PressureServiceBase::CanCallAddClient() const {
  return true;
}

void PressureServiceBase::BindReceiver(
    mojo::PendingReceiver<blink::mojom::WebPressureManager> receiver) {
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
      base::BindRepeating(&PressureServiceBase::OnPressureManagerDisconnected,
                          base::Unretained(this)));
}

void PressureServiceBase::AddClient(device::mojom::PressureSource source,
                                    AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanCallAddClient()) {
    std::move(callback).Run(
        device::mojom::PressureManagerAddClientResult::NewError(
            device::mojom::PressureManagerAddClientError::kNotSupported));
    return;
  }

  auto& pressure_client = source_to_client_[static_cast<size_t>(source)];
  if (pressure_client.is_client_remote_bound()) {
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
        base::BindRepeating(&PressureServiceBase::OnPressureManagerDisconnected,
                            base::Unretained(this)));
    GetDeviceService().BindPressureManager(std::move(receiver));
  }

  if (pressure_client.is_client_receiver_bound()) {
    // Calling BindNewPipeAndPassReceiver() is safe because we call
    // PressureClientImpl::is_client_remote_bound() above.
    std::move(callback).Run(
        device::mojom::PressureManagerAddClientResult::NewPressureClient(
            pressure_client.BindNewPipeAndPassReceiver()));
  } else {
    manager_remote_->AddClient(
        source, GetTokenFor(source),
        base::BindOnce(&PressureServiceBase::DidAddClient,
                       weak_ptr_factory_.GetWeakPtr(), source,
                       std::move(callback)));
  }
}

// Disconnection handler for |manager_receiver_| and |manager_remote_|. If
// either of the connections breaks, we should disconnect all connections and
// let //services know we do not need more updates.
void PressureServiceBase::OnPressureManagerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager_receiver_.reset();
  manager_remote_.reset();
}

void PressureServiceBase::DidAddClient(
    device::mojom::PressureSource source,
    AddClientCallback client_callback,
    device::mojom::PressureManagerAddClientResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result->is_error()) {
    std::move(client_callback).Run(std::move(result));
    return;
  }

  auto& pressure_client = source_to_client_[static_cast<size_t>(source)];
  pressure_client.BindReceiver(std::move(result->get_pressure_client()));

  std::move(client_callback)
      .Run(device::mojom::PressureManagerAddClientResult::NewPressureClient(
          // This is safe because AddClient() already checked
          // PressureClientImpl::is_client_remote_bound()'s return value.
          pressure_client.BindNewPipeAndPassReceiver()));
}

}  // namespace content
