// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"

using blink::mojom::PresentationConnectionState;
using blink::mojom::PresentationError;
using blink::mojom::PresentationErrorPtr;
using blink::mojom::PresentationErrorType;
using blink::mojom::PresentationInfo;
using blink::mojom::PresentationInfoPtr;
using blink::mojom::ScreenAvailability;

namespace content {

namespace {

static constexpr int kInvalidRequestId = -1;
static constexpr size_t kMaxPresentationIdLength = 256;

int GetNextRequestId() {
  static int next_request_id = 0;
  return ++next_request_id;
}

void InvokeNewPresentationCallbackWithError(
    PresentationServiceImpl::NewPresentationCallback callback) {
  std::move(callback).Run(
      /** PresentationConnectionResultPtr */ nullptr,
      PresentationError::New(
          PresentationErrorType::PREVIOUS_START_IN_PROGRESS,
          "There is already an unsettled Promise from a previous call "
          "to start."));
}

}  // namespace

PresentationServiceImpl::PresentationServiceImpl(
    RenderFrameHost* render_frame_host,
    WebContents* web_contents,
    ControllerPresentationServiceDelegate* controller_delegate,
    ReceiverPresentationServiceDelegate* receiver_delegate)
    : WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host),
      controller_delegate_(controller_delegate),
      receiver_delegate_(receiver_delegate),
      start_presentation_request_id_(kInvalidRequestId),
      // TODO(imcheng): Consider using RenderFrameHost* directly instead of IDs.
      render_process_id_(render_frame_host->GetProcess()->GetID()),
      render_frame_id_(render_frame_host->GetRoutingID()),
      is_outermost_document_(!render_frame_host->GetParentOrOuterDocument()) {
  DCHECK(render_frame_host_);
  DCHECK(web_contents);
  CHECK(render_frame_host_->IsRenderFrameLive());

  DVLOG(2) << "PresentationServiceImpl: " << render_process_id_ << ", "
           << render_frame_id_
           << " is outermost document: " << is_outermost_document_;

  if (auto* delegate = GetPresentationServiceDelegate())
    delegate->AddObserver(render_process_id_, render_frame_id_, this);
}

PresentationServiceImpl::~PresentationServiceImpl() {
  DVLOG(2) << __FUNCTION__ << ": " << render_process_id_ << ", "
           << render_frame_id_;

  // Call Reset() to inform the PresentationServiceDelegate to clean up.
  Reset();

  if (auto* delegate = GetPresentationServiceDelegate())
    delegate->RemoveObserver(render_process_id_, render_frame_id_);
}

// static
std::unique_ptr<PresentationServiceImpl> PresentationServiceImpl::Create(
    RenderFrameHost* render_frame_host) {
  DVLOG(2) << __func__ << ": " << render_frame_host->GetProcess()->GetID()
           << ", " << render_frame_host->GetRoutingID();
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  auto* browser = GetContentClient()->browser();
  auto* receiver_delegate =
      browser->GetReceiverPresentationServiceDelegate(web_contents);

  // In current implementation, web_contents can be controller or receiver
  // but not both.
  auto* controller_delegate =
      receiver_delegate
          ? nullptr
          : browser->GetControllerPresentationServiceDelegate(web_contents);

  return base::WrapUnique(new PresentationServiceImpl(
      render_frame_host, web_contents, controller_delegate, receiver_delegate));
}

void PresentationServiceImpl::Bind(
    mojo::PendingReceiver<blink::mojom::PresentationService> receiver) {
  presentation_service_receivers_.Add(this, std::move(receiver));
  presentation_service_receivers_.set_disconnect_handler(base::BindRepeating(
      &PresentationServiceImpl::OnConnectionError, base::Unretained(this)));
}

void PresentationServiceImpl::SetController(
    mojo::PendingRemote<blink::mojom::PresentationController>
        presentation_controller_remote) {
  if (presentation_controller_remote_) {
    presentation_service_receivers_.ReportBadMessage(
        "There can only be one PresentationController at any given time.");
    return;
  }

  presentation_controller_remote_.Bind(
      std::move(presentation_controller_remote));
  presentation_controller_remote_.set_disconnect_handler(base::BindOnce(
      &PresentationServiceImpl::OnConnectionError, base::Unretained(this)));
}

void PresentationServiceImpl::SetReceiver(
    mojo::PendingRemote<blink::mojom::PresentationReceiver>
        presentation_receiver_remote) {
  // Mojo interfaces for Presentation API are disabled during pre-rendering.
  DCHECK_NE(render_frame_host_->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  // Presentation receiver virtual web tests (which have the flag set) has no
  // ReceiverPresentationServiceDelegate implementation.
  // TODO(imcheng): Refactor content_browser_client to return a no-op
  // PresentationService instead.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePresentationReceiverForTesting)) {
    return;
  }

  if (!receiver_delegate_ || !is_outermost_document_) {
    presentation_service_receivers_.ReportBadMessage(
        "SetReceiver can only be called from a "
        "presentation receiver outermost document.");
    return;
  }

  if (presentation_receiver_remote_) {
    presentation_service_receivers_.ReportBadMessage(
        "SetReceiver can only be called once.");
    return;
  }

  presentation_receiver_remote_.Bind(std::move(presentation_receiver_remote));
  presentation_receiver_remote_.set_disconnect_handler(base::BindOnce(
      &PresentationServiceImpl::OnConnectionError, base::Unretained(this)));
  receiver_delegate_->RegisterReceiverConnectionAvailableCallback(
      base::BindRepeating(
          &PresentationServiceImpl::OnReceiverConnectionAvailable,
          weak_factory_.GetWeakPtr()));
}

void PresentationServiceImpl::ListenForScreenAvailability(const GURL& url) {
  DVLOG(2) << "ListenForScreenAvailability " << url.spec();
  if (!controller_delegate_ || !url.is_valid()) {
    if (presentation_controller_remote_) {
      presentation_controller_remote_->OnScreenAvailabilityUpdated(
          url, ScreenAvailability::UNAVAILABLE);
    }
    return;
  }

  if (screen_availability_listeners_.count(url))
    return;

  std::unique_ptr<ScreenAvailabilityListenerImpl> listener(
      new ScreenAvailabilityListenerImpl(url, this));
  if (controller_delegate_->AddScreenAvailabilityListener(
          render_process_id_, render_frame_id_, listener.get())) {
    screen_availability_listeners_[url] = std::move(listener);
  } else {
    DVLOG(1) << "AddScreenAvailabilityListener failed. Ignoring request.";
  }
}

void PresentationServiceImpl::StopListeningForScreenAvailability(
    const GURL& url) {
  DVLOG(2) << "StopListeningForScreenAvailability " << url.spec();
  if (!controller_delegate_)
    return;

  auto listener_it = screen_availability_listeners_.find(url);
  if (listener_it == screen_availability_listeners_.end())
    return;

  controller_delegate_->RemoveScreenAvailabilityListener(
      render_process_id_, render_frame_id_, listener_it->second.get());
  screen_availability_listeners_.erase(listener_it);
}

void PresentationServiceImpl::StartPresentation(
    const std::vector<GURL>& presentation_urls,
    NewPresentationCallback callback) {
  DVLOG(2) << "StartPresentation";

  // There is a StartPresentation request in progress. To avoid queueing up
  // requests, the incoming request is rejected.
  if (start_presentation_request_id_ != kInvalidRequestId) {
    InvokeNewPresentationCallbackWithError(std::move(callback));
    return;
  }

  if (!controller_delegate_) {
    std::move(callback).Run(
        /** PresentationConnectionResultPtr */ nullptr,
        PresentationError::New(PresentationErrorType::NO_AVAILABLE_SCREENS,
                               "No screens found."));
    return;
  }

  start_presentation_request_id_ = GetNextRequestId();
  pending_start_presentation_cb_ =
      std::make_unique<NewPresentationCallbackWrapper>(std::move(callback));
  PresentationRequest request({render_process_id_, render_frame_id_},
                              presentation_urls,
                              render_frame_host_->GetLastCommittedOrigin());
  controller_delegate_->StartPresentation(
      request,
      base::BindOnce(&PresentationServiceImpl::OnStartPresentationSucceeded,
                     weak_factory_.GetWeakPtr(),
                     start_presentation_request_id_),
      base::BindOnce(&PresentationServiceImpl::OnStartPresentationError,
                     weak_factory_.GetWeakPtr(),
                     start_presentation_request_id_));
}

void PresentationServiceImpl::ReconnectPresentation(
    const std::vector<GURL>& presentation_urls,
    const std::string& presentation_id,
    NewPresentationCallback callback) {
  DVLOG(2) << "ReconnectPresentation";
  if (!controller_delegate_) {
    std::move(callback).Run(
        /** PresentationConnectionResultPtr */ nullptr,
        PresentationError::New(PresentationErrorType::NO_PRESENTATION_FOUND,
                               "Error joining route: No matching route"));
    return;
  }

  int request_id = RegisterReconnectPresentationCallback(&callback);
  if (request_id == kInvalidRequestId) {
    InvokeNewPresentationCallbackWithError(std::move(callback));
    return;
  }

  PresentationRequest request({render_process_id_, render_frame_id_},
                              presentation_urls,
                              render_frame_host_->GetLastCommittedOrigin());
  controller_delegate_->ReconnectPresentation(
      request, presentation_id,
      base::BindOnce(&PresentationServiceImpl::OnReconnectPresentationSucceeded,
                     weak_factory_.GetWeakPtr(), request_id),
      base::BindOnce(&PresentationServiceImpl::OnReconnectPresentationError,
                     weak_factory_.GetWeakPtr(), request_id));
}

int PresentationServiceImpl::RegisterReconnectPresentationCallback(
    NewPresentationCallback* callback) {
  if (pending_reconnect_presentation_cbs_.size() >= kMaxQueuedRequests)
    return kInvalidRequestId;

  int request_id = GetNextRequestId();
  pending_reconnect_presentation_cbs_[request_id] =
      std::make_unique<NewPresentationCallbackWrapper>(std::move(*callback));
  DCHECK_NE(kInvalidRequestId, request_id);
  return request_id;
}

void PresentationServiceImpl::ListenForConnectionStateChange(
    const PresentationInfo& connection) {
  // NOTE: Blink will automatically transition the connection's state to
  // 'connected'.
  if (controller_delegate_) {
    controller_delegate_->ListenForConnectionStateChange(
        render_process_id_, render_frame_id_, connection,
        base::BindRepeating(&PresentationServiceImpl::OnConnectionStateChanged,
                            weak_factory_.GetWeakPtr(), connection));
  }
}

void PresentationServiceImpl::OnStartPresentationSucceeded(
    int request_id,
    blink::mojom::PresentationConnectionResultPtr result) {
  if (request_id != start_presentation_request_id_)
    return;

  auto presentation_info = *result->presentation_info;
  DCHECK(pending_start_presentation_cb_.get());
  DCHECK(presentation_info.id.length() <= kMaxPresentationIdLength);
  pending_start_presentation_cb_->Run(std::move(result),
                                      /** PresentationErrorPtr */ nullptr);
  ListenForConnectionStateChange(presentation_info);
  pending_start_presentation_cb_.reset();
  start_presentation_request_id_ = kInvalidRequestId;
}

void PresentationServiceImpl::OnStartPresentationError(
    int request_id,
    const blink::mojom::PresentationError& error) {
  if (request_id != start_presentation_request_id_)
    return;

  CHECK(pending_start_presentation_cb_.get());
  pending_start_presentation_cb_->Run(
      /** PresentationConnectionResultPtr */ nullptr,
      PresentationError::New(error));
  pending_start_presentation_cb_.reset();
  start_presentation_request_id_ = kInvalidRequestId;
}

void PresentationServiceImpl::OnReconnectPresentationSucceeded(
    int request_id,
    blink::mojom::PresentationConnectionResultPtr result) {
  auto presentation_info = *result->presentation_info;
  if (RunAndEraseReconnectPresentationMojoCallback(
          request_id, std::move(result), /** PresentationErrorPtr */ nullptr)) {
    ListenForConnectionStateChange(presentation_info);
  }
}

void PresentationServiceImpl::OnReconnectPresentationError(
    int request_id,
    const blink::mojom::PresentationError& error) {
  RunAndEraseReconnectPresentationMojoCallback(
      request_id, blink::mojom::PresentationConnectionResultPtr(),
      PresentationError::New(error));
}

bool PresentationServiceImpl::RunAndEraseReconnectPresentationMojoCallback(
    int request_id,
    blink::mojom::PresentationConnectionResultPtr result,
    blink::mojom::PresentationErrorPtr error) {
  auto it = pending_reconnect_presentation_cbs_.find(request_id);
  if (it == pending_reconnect_presentation_cbs_.end())
    return false;

  DCHECK(it->second.get());
  it->second->Run(std::move(result), std::move(error));
  pending_reconnect_presentation_cbs_.erase(it);
  return true;
}

void PresentationServiceImpl::SetDefaultPresentationUrls(
    const std::vector<GURL>& presentation_urls) {
  DVLOG(2) << "SetDefaultPresentationUrls";
  if (!controller_delegate_ || !is_outermost_document_)
    return;

  if (default_presentation_urls_ == presentation_urls)
    return;

  default_presentation_urls_ = presentation_urls;
  PresentationRequest request({render_process_id_, render_frame_id_},
                              presentation_urls,
                              render_frame_host_->GetLastCommittedOrigin());
  controller_delegate_->SetDefaultPresentationUrls(
      request, base::BindRepeating(
                   &PresentationServiceImpl::OnDefaultPresentationStarted,
                   weak_factory_.GetWeakPtr()));
}

void PresentationServiceImpl::CloseConnection(
    const GURL& presentation_url,
    const std::string& presentation_id) {
  DVLOG(2) << "CloseConnection " << presentation_id;
  if (controller_delegate_)
    controller_delegate_->CloseConnection(render_process_id_, render_frame_id_,
                                          presentation_id);
}

void PresentationServiceImpl::Terminate(const GURL& presentation_url,
                                        const std::string& presentation_id) {
  DVLOG(2) << "Terminate " << presentation_id;
  if (controller_delegate_)
    controller_delegate_->Terminate(render_process_id_, render_frame_id_,
                                    presentation_id);
}

void PresentationServiceImpl::SetControllerDelegateForTesting(
    ControllerPresentationServiceDelegate* controller_delegate) {
  controller_delegate_ = controller_delegate;
}

void PresentationServiceImpl::OnConnectionStateChanged(
    const PresentationInfo& connection,
    const PresentationConnectionStateChangeInfo& info) {
  DVLOG(2) << "PresentationServiceImpl::OnConnectionStateChanged "
           << "[presentation_id]: " << connection.id
           << " [state]: " << info.state;
  if (!presentation_controller_remote_)
    return;

  if (info.state == PresentationConnectionState::CLOSED) {
    presentation_controller_remote_->OnConnectionClosed(
        PresentationInfo::New(connection), info.close_reason, info.message);
  } else {
    presentation_controller_remote_->OnConnectionStateChanged(
        PresentationInfo::New(connection), info.state);
  }
}

bool PresentationServiceImpl::FrameMatches(
    content::RenderFrameHost* render_frame_host) const {
  if (!render_frame_host)
    return false;

  return render_frame_host->GetProcess()->GetID() == render_process_id_ &&
         render_frame_host->GetRoutingID() == render_frame_id_;
}

void PresentationServiceImpl::OnConnectionError() {
  Reset();
}

PresentationServiceDelegate*
PresentationServiceImpl::GetPresentationServiceDelegate() {
  return receiver_delegate_
             ? static_cast<PresentationServiceDelegate*>(receiver_delegate_)
             : static_cast<PresentationServiceDelegate*>(controller_delegate_);
}

void PresentationServiceImpl::OnReceiverConnectionAvailable(
    blink::mojom::PresentationConnectionResultPtr result) {
  DVLOG(2) << "PresentationServiceImpl::OnReceiverConnectionAvailable";

  presentation_receiver_remote_->OnReceiverConnectionAvailable(
      std::move(result));
}

void PresentationServiceImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // Since the PresentationServiceImpl is tied to the lifetime of a
  // RenderFrameHost, we should reset the connections when a navigation
  // finished but we're still using the same RenderFrameHost.
  // We don't need to do anything when the navigation didn't actually commit,
  // won't use the same RenderFrameHost, is restoring a RenderFrameHost from
  // the back-forward cache, or is activating a prerendered page.
  DVLOG(2) << "PresentationServiceImpl::DidNavigateAnyFrame";
  if (!navigation_handle->HasCommitted() ||
      !FrameMatches(navigation_handle->GetRenderFrameHost()) ||
      navigation_handle->IsServedFromBackForwardCache() ||
      navigation_handle->IsPrerenderedPageActivation()) {
    return;
  }

  // If a frame navigation is same-document (e.g. navigating to a fragment in
  // same page) then we do not unregister listeners.
  DVLOG(2) << "DidNavigateAnyFrame: "
           << ", is_same_document: " << navigation_handle->IsSameDocument();
  if (navigation_handle->IsSameDocument())
    return;

  // Reset if the frame actually navigated.
  Reset();
}

void PresentationServiceImpl::Reset() {
  DVLOG(2) << "PresentationServiceImpl::Reset";
  if (controller_delegate_)
    controller_delegate_->Reset(render_process_id_, render_frame_id_);

  if (receiver_delegate_ && is_outermost_document_)
    receiver_delegate_->Reset(render_process_id_, render_frame_id_);

  default_presentation_urls_.clear();

  screen_availability_listeners_.clear();

  start_presentation_request_id_ = kInvalidRequestId;
  pending_start_presentation_cb_.reset();

  pending_reconnect_presentation_cbs_.clear();

  presentation_service_receivers_.Clear();
  presentation_controller_remote_.reset();
  presentation_receiver_remote_.reset();
}

void PresentationServiceImpl::OnDelegateDestroyed() {
  DVLOG(2) << "PresentationServiceImpl::OnDelegateDestroyed";
  controller_delegate_ = nullptr;
  receiver_delegate_ = nullptr;
  Reset();
}

void PresentationServiceImpl::OnDefaultPresentationStarted(
    blink::mojom::PresentationConnectionResultPtr result) {
  auto presentation_info = *result->presentation_info;
  if (presentation_controller_remote_) {
    presentation_controller_remote_->OnDefaultPresentationStarted(
        std::move(result));
  }

  // TODO(btolsch): Remove the state-change API in favor of direct
  // PresentationConnection state use.
  ListenForConnectionStateChange(presentation_info);
}

PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    ScreenAvailabilityListenerImpl(const GURL& availability_url,
                                   PresentationServiceImpl* service)
    : availability_url_(availability_url), service_(service) {
  DCHECK(availability_url_.is_valid());
  DCHECK(service_);
}

PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    ~ScreenAvailabilityListenerImpl() = default;

GURL PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    GetAvailabilityUrl() {
  return availability_url_;
}

void PresentationServiceImpl::ScreenAvailabilityListenerImpl::
    OnScreenAvailabilityChanged(blink::mojom::ScreenAvailability availability) {
  if (service_->presentation_controller_remote_) {
    service_->presentation_controller_remote_->OnScreenAvailabilityUpdated(
        availability_url_, availability);
  }
}

PresentationServiceImpl::NewPresentationCallbackWrapper::
    NewPresentationCallbackWrapper(NewPresentationCallback callback)
    : callback_(std::move(callback)) {}

PresentationServiceImpl::NewPresentationCallbackWrapper::
    ~NewPresentationCallbackWrapper() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        /** PresentationConnectionResultPtr */ nullptr,
        PresentationError::New(
            PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
            "The frame is navigating or being destroyed."));
  }
}

void PresentationServiceImpl::NewPresentationCallbackWrapper::Run(
    blink::mojom::PresentationConnectionResultPtr result,
    blink::mojom::PresentationErrorPtr error) {
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(result), std::move(error));
}

}  // namespace content
