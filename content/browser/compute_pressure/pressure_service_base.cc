// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_base.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/cpp/device_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/check.h"
#endif

namespace {

constexpr char kVirtualPressureSourceStartConsoleMessage[] =
    "Non-virtual observers are running while a virtual source is being added. "
    "The virtual source will have no effect on any observer as long as "
    "observers are still running. Stop all the observers and restart them if "
    "they want to use the virtual source.";

constexpr char kVirtualPressureSourceStopConsoleMessage[] =
    "Virtual observers are still running after the virtual source was "
    "disconnected. The non-virtual source will have no effect on any observer "
    "as long as observers are still running. Stop all the observers and "
    "restart them if they want to use the non-virtual source.";

}  // namespace

namespace content {

PressureServiceBase::PressureServiceBase()
    : source_to_client_{PressureClientImpl(this)} {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::FeatureList::IsEnabled(
          features::kComputePressureBreakCalibrationMitigation)) {
    converter_.EnableStateRandomizationMitigation();
  }
}

PressureServiceBase::~PressureServiceBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Manually remove the observer here instead of using
  // base::ScopedObserver. In general, this class will be destroyed before
  // the observable (WebContentsPressureManagerProxy) but in some cases
  // (e.g. active PressureObserver instances in both a shared worker and a
  // dedicated worker may cause the PressureServiceForDedicatedWorker to be
  // destroyed only when its DedicatedWorkerHost's RenderProcessHost is
  // destroyed, which happens after WebContentsPressureManagerProxy object is
  // destroyed) this is not true. The condition above can be reproduced by
  // ComputePressureBrowserTest when SupportsSharedWorker() is true and shared
  // workers are used.
  auto* pressure_manager_proxy = GetWebContentsPressureManagerProxy();
  if (pressure_manager_proxy) {
    pressure_manager_proxy->RemoveObserver(this);
  }
}

// static
// https://www.w3.org/TR/compute-pressure/#dfn-document-has-implicit-focus
bool PressureServiceBase::HasImplicitFocus(RenderFrameHost* render_frame_host) {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO: http://crbug.com/407801065
  // On ChromeOS, in rare occasions render_frame_host may be nullptr. The
  // following DUMP_WILL_BE_CHECK() is used to provide additional information
  // for diagnosis.
  DUMP_WILL_BE_CHECK(render_frame_host);
#endif
  if (!render_frame_host) {
    return false;
  }

  // 1. If document is not fully active, return false.
  if (!render_frame_host->IsActive()) {
    return false;
  }

  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host));
  const auto& current_origin = render_frame_host->GetLastCommittedOrigin();

  // 3. If associated document is same origin with initiators of active
  // Picture-in-Picture sessions, return true.
  if (std::ranges::any_of(
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

device::mojom::PressureState PressureServiceBase::CalculateState(
    double pressure_value) {
  return converter_.CalculateState(pressure_value);
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
  // be called after `associated_manager_receiver_` is deallocated,
  //  and `associated_manager_receiver_` is owned by this class.
  manager_receiver_.set_disconnect_handler(
      base::BindRepeating(&PressureServiceBase::OnPressureManagerDisconnected,
                          base::Unretained(this)));

  auto* pressure_manager_proxy =
      GetWebContentsPressureManagerProxy(/*allow_creation*/true);
  if (pressure_manager_proxy) {
    pressure_manager_proxy->AddObserver(this);
  }
}

void PressureServiceBase::AddClient(
    device::mojom::PressureSource source,
    mojo::PendingAssociatedRemote<blink::mojom::WebPressureClient> client,
    AddClientCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanCallAddClient()) {
    std::move(callback).Run(
        device::mojom::PressureManagerAddClientResult::kNotSupported);
    return;
  }

  auto& pressure_client = source_to_client_[static_cast<size_t>(source)];
  if (pressure_client.is_client_associated_remote_bound()) {
    manager_receiver_.ReportBadMessage(
        "PressureClientImpl is already connected.");
    // manager_receiver_.ReportBadMessage() will reset `manager_receiver_` and
    // so clean up as if the pipe had been disconnected.
    OnPressureManagerDisconnected();
    return;
  }

  pressure_client.BindPendingAssociatedRemote(std::move(client));

  if (!manager_remote_.is_bound()) {
    auto manager_receiver = manager_remote_.BindNewPipeAndPassReceiver();
    manager_remote_.set_disconnect_handler(
        base::BindRepeating(&PressureServiceBase::OnPressureManagerDisconnected,
                            base::Unretained(this)));

    GetDeviceService().BindPressureManager(std::move(manager_receiver));
  }

  if (pressure_client.is_client_receiver_bound()) {
    std::move(callback).Run(device::mojom::PressureManagerAddClientResult::kOk);
  } else {
    const std::optional<base::UnguessableToken>& token = GetTokenFor(source);
    manager_remote_->AddClient(
        source, token, pressure_client.BindNewEndpointAndPassRemote(),
        base::BindOnce(&PressureServiceBase::DidAddClient,
                       weak_ptr_factory_.GetWeakPtr(), source, token,
                       std::move(callback)));
  }
}

void PressureServiceBase::DidAddVirtualPressureSource(
    device::mojom::PressureSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& pressure_client = source_to_client_[static_cast<size_t>(source)];

  if (pressure_client.pressure_source_type() ==
      PressureClientImpl::PressureSourceType::kNonVirtual) {
    AddMessageToConsole(kVirtualPressureSourceStartConsoleMessage);
  }
}

void PressureServiceBase::DidRemoveVirtualPressureSource(
    device::mojom::PressureSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& pressure_client = source_to_client_[static_cast<size_t>(source)];
  if (pressure_client.pressure_source_type() ==
      PressureClientImpl::PressureSourceType::kVirtual) {
    AddMessageToConsole(kVirtualPressureSourceStopConsoleMessage);
  }
}

WebContentsPressureManagerProxy*
PressureServiceBase::GetWebContentsPressureManagerProxy(
    bool allow_creation) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RenderFrameHost* rfh = GetRenderFrameHost();
  auto* web_contents = WebContents::FromRenderFrameHost(rfh);
  // Checking the validity of RenderFrameHost* because in some cases as
  // explained in ~PressureServiceBase(), the order of destruction might not be
  // as expected.
  if (rfh && web_contents) {
    return allow_creation
               ? WebContentsPressureManagerProxy::GetOrCreate(web_contents)
               : WebContentsPressureManagerProxy::FromWebContents(web_contents);
  } else {
    return nullptr;
  }
}

RenderFrameHost* PressureServiceBase::GetRenderFrameHost() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return nullptr;
}

void PressureServiceBase::AddMessageToConsole(
    const std::string& message) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RenderFrameHost* rfh = GetRenderFrameHost();
  CHECK(rfh);
  rfh->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kInfo, message);
}

// Disconnection handler for |associated_manager_receiver_| and
// |associated_manager_remote_|. If either of the connections breaks, we should
// disconnect all connections and let //services know we do not need more
// updates.
void PressureServiceBase::OnPressureManagerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager_receiver_.reset();
  manager_remote_.reset();

  // In case the client was removed from Blink before service binding
  // has occurred. Reset is needed.
  for (auto& client : source_to_client_) {
    client.Reset();
  }

  auto* pressure_manager_proxy = GetWebContentsPressureManagerProxy();
  if (pressure_manager_proxy) {
    pressure_manager_proxy->RemoveObserver(this);
  }
}

void PressureServiceBase::DidAddClient(
    device::mojom::PressureSource source,
    const std::optional<base::UnguessableToken>& token,
    AddClientCallback client_callback,
    device::mojom::PressureManagerAddClientResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& pressure_client = source_to_client_[static_cast<size_t>(source)];

  if (result == device::mojom::PressureManagerAddClientResult::kNotSupported) {
    std::move(client_callback)
        .Run(device::mojom::PressureManagerAddClientResult::kNotSupported);
    pressure_client.Reset();
    return;
  }

  pressure_client.SetPressureSourceType(token.has_value());

  std::move(client_callback)
      .Run(device::mojom::PressureManagerAddClientResult::kOk);
}

}  // namespace content
