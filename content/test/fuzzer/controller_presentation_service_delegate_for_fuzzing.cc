// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "content/test/fuzzer/controller_presentation_service_delegate_for_fuzzing.h"

#include "base/notreached.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-mojolpm.h"
#include "url/mojom/url.mojom-mojolpm.h"

// Code copy of a limit of the same name
// This allows preventing the fake making bad calls
// See "content/browser/presentation/presentation_service_impl.cc"
static constexpr size_t kMaxPresentationIdLength = 256;

ControllerPresentationServiceDelegateForFuzzing::
    ControllerPresentationServiceDelegateForFuzzing() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ControllerPresentationServiceDelegateForFuzzing::
    ~ControllerPresentationServiceDelegateForFuzzing() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer_pair : observers_)
    observer_pair.second->OnDelegateDestroyed();
}

void ControllerPresentationServiceDelegateForFuzzing::AddObserver(
    int render_process_id,
    int render_frame_id,
    ControllerPresentationServiceDelegate::Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_[content::GlobalRenderFrameHostId(render_process_id,
                                              render_frame_id)] = observer;
}

void ControllerPresentationServiceDelegateForFuzzing::RemoveObserver(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.erase(
      content::GlobalRenderFrameHostId(render_process_id, render_frame_id));
}

void ControllerPresentationServiceDelegateForFuzzing::Reset(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Intentionally, we do not reset all callbacks, in order to mimic
  // `ControllerPresentationServiceDelegateImpl` as closely as possible.
  listeners_.clear();
  set_default_presentation_urls_callback_.Reset();
}

void ControllerPresentationServiceDelegateForFuzzing::
    SetDefaultPresentationUrls(
        const content::PresentationRequest& request,
        content::DefaultPresentationConnectionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  set_default_presentation_urls_callback_ = std::move(callback);
}

void ControllerPresentationServiceDelegateForFuzzing::StartPresentation(
    const content::PresentationRequest& request,
    content::PresentationConnectionCallback success_cb,
    content::PresentationConnectionErrorCallback error_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  start_presentation_success_cb_ = std::move(success_cb);
  start_presentation_error_cb_ = std::move(error_cb);
}

void ControllerPresentationServiceDelegateForFuzzing::ReconnectPresentation(
    const content::PresentationRequest& request,
    const std::string& presentation_id,
    content::PresentationConnectionCallback success_cb,
    content::PresentationConnectionErrorCallback error_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  reconnect_presentation_success_cb_ = std::move(success_cb);
  reconnect_presentation_error_cb_ = std::move(error_cb);
}

void ControllerPresentationServiceDelegateForFuzzing::CloseConnection(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  start_presentation_success_cb_.Reset();
  start_presentation_error_cb_.Reset();
  reconnect_presentation_success_cb_.Reset();
  reconnect_presentation_error_cb_.Reset();
}

void ControllerPresentationServiceDelegateForFuzzing::Terminate(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  start_presentation_success_cb_.Reset();
  start_presentation_error_cb_.Reset();
  reconnect_presentation_success_cb_.Reset();
  reconnect_presentation_error_cb_.Reset();
}

std::unique_ptr<media::FlingingController>
ControllerPresentationServiceDelegateForFuzzing::GetFlingingController(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This should not fail. As the MojoLPM fuzzer this fake was designed to work
  // with has no way to call this.
  NOTIMPLEMENTED();
  return nullptr;
}

void ControllerPresentationServiceDelegateForFuzzing::
    ListenForConnectionStateChange(
        int render_process_id,
        int render_frame_id,
        const blink::mojom::PresentationInfo& connection,
        const content::PresentationConnectionStateChangedCallback&
            state_changed_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  listen_for_connection_state_change_state_changed_cb_ =
      std::move(state_changed_cb);
}

bool ControllerPresentationServiceDelegateForFuzzing::
    AddScreenAvailabilityListener(
        int render_process_id,
        int render_frame_id,
        content::PresentationScreenAvailabilityListener* listener) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  listeners_[listener->GetAvailabilityUrl()] = listener;
  return true;
}

void ControllerPresentationServiceDelegateForFuzzing::
    RemoveScreenAvailabilityListener(
        int render_process_id,
        int render_frame_id,
        content::PresentationScreenAvailabilityListener* listener) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  listeners_.erase(listener->GetAvailabilityUrl());
}
void ControllerPresentationServiceDelegateForFuzzing::
    HandleListenersGetAvailabilityUrl(
        const mojolpm::url::mojom::Url& proto_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GURL url;
  if (!mojolpm::FromProto(proto_url, url))
    return;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerPresentationServiceDelegateForFuzzing::
                         CallListenersGetAvailabilityUrl,
                     GetWeakPtr(), std::move(url)));
}

void ControllerPresentationServiceDelegateForFuzzing::
    CallListenersGetAvailabilityUrl(GURL url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!listeners_.count(url))
    return;
  listeners_[url]->GetAvailabilityUrl();
}
void ControllerPresentationServiceDelegateForFuzzing::
    HandleListenersOnScreenAvailabilityChanged(
        const mojolpm::url::mojom::Url& proto_url,
        const mojolpm::blink::mojom::ScreenAvailability&
            proto_screen_availability) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GURL url;
  if (!mojolpm::FromProto(proto_url, url))
    return;
  blink::mojom::ScreenAvailability screen_availability;
  if (!mojolpm::FromProto(proto_screen_availability, screen_availability))
    return;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerPresentationServiceDelegateForFuzzing::
                         CallListenersOnScreenAvailabilityChanged,
                     GetWeakPtr(), std::move(url),
                     std::move(screen_availability)));
}

void ControllerPresentationServiceDelegateForFuzzing::
    CallListenersOnScreenAvailabilityChanged(
        GURL url,
        blink::mojom::ScreenAvailability screen_availability) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!listeners_.count(url))
    return;
  listeners_[url]->OnScreenAvailabilityChanged(std::move(screen_availability));
}

void ControllerPresentationServiceDelegateForFuzzing::
    HandleSetDefaultPresentationUrls(
        const mojolpm::blink::mojom::PresentationConnectionResult&
            proto_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blink::mojom::PresentationConnectionResultPtr result;
  if (!mojolpm::FromProto(proto_result, result))
    return;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerPresentationServiceDelegateForFuzzing::
                         CallSetDefaultPresentationUrls,
                     GetWeakPtr(), std::move(result)));
}

void ControllerPresentationServiceDelegateForFuzzing::
    CallSetDefaultPresentationUrls(
        blink::mojom::PresentationConnectionResultPtr result_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (set_default_presentation_urls_callback_.is_null())
    return;
  set_default_presentation_urls_callback_.Run(std::move(result_ptr));
}

void ControllerPresentationServiceDelegateForFuzzing::
    HandleStartPresentationSuccess(
        const mojolpm::blink::mojom::PresentationConnectionResult&
            proto_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blink::mojom::PresentationConnectionResultPtr result;
  if (!mojolpm::FromProto(proto_result, result))
    return;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerPresentationServiceDelegateForFuzzing::
                         CallStartPresentationSuccess,
                     GetWeakPtr(), std::move(result)));
}

void ControllerPresentationServiceDelegateForFuzzing::
    CallStartPresentationSuccess(
        blink::mojom::PresentationConnectionResultPtr result_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result_ptr->presentation_info->id.length() > kMaxPresentationIdLength)
    return;
  if (start_presentation_success_cb_.is_null())
    return;
  std::move(start_presentation_success_cb_).Run(std::move(result_ptr));
}

void ControllerPresentationServiceDelegateForFuzzing::
    HandleStartPresentationError(
        const mojolpm::blink::mojom::PresentationError& proto_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blink::mojom::PresentationErrorPtr error;
  if (!mojolpm::FromProto(proto_error, error))
    return;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerPresentationServiceDelegateForFuzzing::
                         CallStartPresentationError,
                     GetWeakPtr(), std::move(error)));
}

void ControllerPresentationServiceDelegateForFuzzing::
    CallStartPresentationError(blink::mojom::PresentationErrorPtr error_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (start_presentation_error_cb_.is_null())
    return;
  std::move(start_presentation_error_cb_).Run(*std::move(error_ptr));
}

void ControllerPresentationServiceDelegateForFuzzing::
    HandleReconnectPresentationSuccess(
        const mojolpm::blink::mojom::PresentationConnectionResult&
            proto_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blink::mojom::PresentationConnectionResultPtr result;
  if (!mojolpm::FromProto(proto_result, result))
    return;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerPresentationServiceDelegateForFuzzing::
                         CallReconnectPresentationSuccess,
                     GetWeakPtr(), std::move(result)));
}

void ControllerPresentationServiceDelegateForFuzzing::
    CallReconnectPresentationSuccess(
        blink::mojom::PresentationConnectionResultPtr result_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (reconnect_presentation_success_cb_.is_null())
    return;
  std::move(reconnect_presentation_success_cb_).Run(std::move(result_ptr));
}

void ControllerPresentationServiceDelegateForFuzzing::
    HandleReconnectPresentationError(
        const mojolpm::blink::mojom::PresentationError& proto_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blink::mojom::PresentationErrorPtr error;
  if (!mojolpm::FromProto(proto_error, error))
    return;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerPresentationServiceDelegateForFuzzing::
                         CallReconnectPresentationError,
                     GetWeakPtr(), std::move(error)));
}

void ControllerPresentationServiceDelegateForFuzzing::
    CallReconnectPresentationError(
        blink::mojom::PresentationErrorPtr error_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (reconnect_presentation_error_cb_.is_null())
    return;
  std::move(reconnect_presentation_error_cb_).Run(*std::move(error_ptr));
}

void ControllerPresentationServiceDelegateForFuzzing::
    HandleListenForConnectionStateChangeStateChanged(
        const mojolpm::blink::mojom::PresentationConnectionState&
            proto_connection_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blink::mojom::PresentationConnectionState connection_state;
  if (!mojolpm::FromProto(proto_connection_state, connection_state))
    return;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ControllerPresentationServiceDelegateForFuzzing::
                         CallListenForConnectionStateChangeStateChanged,
                     GetWeakPtr(), std::move(connection_state)));
}

void ControllerPresentationServiceDelegateForFuzzing::
    CallListenForConnectionStateChangeStateChanged(
        blink::mojom::PresentationConnectionState connection_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (listen_for_connection_state_change_state_changed_cb_.is_null())
    return;
  content::PresentationConnectionStateChangeInfo state_change_info{
      connection_state};
  std::move(listen_for_connection_state_change_state_changed_cb_)
      .Run(state_change_info);
}

void ControllerPresentationServiceDelegateForFuzzing::NextAction(
    const Action& action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (action.action_case()) {
    case Action::kListenerGetAvailabilityUrl: {
      HandleListenersGetAvailabilityUrl(
          action.listener_get_availability_url().listener_availability_url());
    } break;

    case Action::kListenerOnScreenAvailabilityChanged: {
      HandleListenersOnScreenAvailabilityChanged(
          action.listener_on_screen_availability_changed()
              .listener_availability_url(),
          action.listener_on_screen_availability_changed()
              .screen_availability());
    } break;

    case Action::kSetDefaultPresentationUrls: {
      HandleSetDefaultPresentationUrls(action.set_default_presentation_urls()
                                           .presentation_connection_result());
    } break;

    case Action::kStartPresentationSuccess: {
      HandleStartPresentationSuccess(
          action.start_presentation_success().presentation_connection_result());
    } break;

    case Action::kStartPresentationError: {
      HandleStartPresentationError(
          action.start_presentation_error().presentation_error());
    } break;

    case Action::kReconnectPresentationSuccess: {
      HandleReconnectPresentationSuccess(action.reconnect_presentation_success()
                                             .presentation_connection_result());
    } break;

    case Action::kReconnectPresentationError: {
      HandleReconnectPresentationError(
          action.reconnect_presentation_error().presentation_error());
    } break;

    case Action::kListenForConnectionStateChangeStateChanged: {
      HandleListenForConnectionStateChangeStateChanged(
          action.listen_for_connection_state_change_state_changed()
              .presentation_connection_state());
    } break;

    case Action::ACTION_NOT_SET:
      break;
  }
}
