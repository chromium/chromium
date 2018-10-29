/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_proxy.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_data.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/headers.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_event.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_event_init.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_update_ui_event.h"
#include "third_party/blink/renderer/modules/background_sync/sync_event.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"
#include "third_party/blink/renderer/modules/cookie_store/extendable_cookie_change_event.h"
#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"
#include "third_party/blink/renderer/modules/notifications/notification.h"
#include "third_party/blink/renderer/modules/notifications/notification_event.h"
#include "third_party/blink/renderer/modules/notifications/notification_event_init.h"
#include "third_party/blink/renderer/modules/payments/abort_payment_event.h"
#include "third_party/blink/renderer/modules/payments/abort_payment_respond_with_observer.h"
#include "third_party/blink/renderer/modules/payments/can_make_payment_event.h"
#include "third_party/blink/renderer/modules/payments/can_make_payment_respond_with_observer.h"
#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"
#include "third_party/blink/renderer/modules/payments/payment_request_event.h"
#include "third_party/blink/renderer/modules/payments/payment_request_event_init.h"
#include "third_party/blink/renderer/modules/payments/payment_request_respond_with_observer.h"
#include "third_party/blink/renderer/modules/push_messaging/push_event.h"
#include "third_party/blink/renderer/modules/push_messaging/push_message_data.h"
#include "third_party/blink/renderer/modules/service_worker/extendable_event.h"
#include "third_party/blink/renderer/modules/service_worker/extendable_message_event.h"
#include "third_party/blink/renderer/modules/service_worker/fetch_event.h"
#include "third_party/blink/renderer/modules/service_worker/fetch_respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/install_event.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace mojo {

namespace {

blink::mojom::NotificationDirection ToMojomNotificationDirection(
    blink::WebNotificationData::Direction input) {
  switch (input) {
    case blink::WebNotificationData::kDirectionLeftToRight:
      return blink::mojom::NotificationDirection::LEFT_TO_RIGHT;
    case blink::WebNotificationData::kDirectionRightToLeft:
      return blink::mojom::NotificationDirection::RIGHT_TO_LEFT;
    case blink::WebNotificationData::kDirectionAuto:
      return blink::mojom::NotificationDirection::AUTO;
  }

  NOTREACHED();
  return blink::mojom::NotificationDirection::AUTO;
}

blink::mojom::NotificationActionType ToMojomNotificationActionType(
    blink::WebNotificationAction::Type input) {
  switch (input) {
    case blink::WebNotificationAction::kButton:
      return blink::mojom::NotificationActionType::BUTTON;
    case blink::WebNotificationAction::kText:
      return blink::mojom::NotificationActionType::TEXT;
  }

  NOTREACHED();
  return blink::mojom::NotificationActionType::BUTTON;
}

}  // namespace

// Inside Blink we're using mojom structs to represent notification data, not
// WebNotification{Action,Data}, however, we still need WebNotificationData to
// carry data from Content into Blink, so, for now we need these type
// converters. They would disappear once we eliminate the abstract interface
// layer blink::WebServiceWorkerContextProxy via Onion Soup effort later.
template <>
struct TypeConverter<blink::mojom::blink::NotificationActionPtr,
                     blink::WebNotificationAction> {
  static blink::mojom::blink::NotificationActionPtr Convert(
      const blink::WebNotificationAction& input) {
    return blink::mojom::blink::NotificationAction::New(
        ToMojomNotificationActionType(input.type), input.action, input.title,
        input.icon, input.placeholder);
  }
};

template <>
struct TypeConverter<blink::mojom::blink::NotificationDataPtr,
                     blink::WebNotificationData> {
  static blink::mojom::blink::NotificationDataPtr Convert(
      const blink::WebNotificationData& input) {
    Vector<int32_t> vibration_pattern;
    vibration_pattern.Append(input.vibrate.Data(),
                             SafeCast<wtf_size_t>(input.vibrate.size()));

    Vector<uint8_t> data;
    data.Append(input.data.Data(), SafeCast<wtf_size_t>(input.data.size()));

    Vector<blink::mojom::blink::NotificationActionPtr> actions;
    for (const auto& web_action : input.actions) {
      actions.push_back(
          blink::mojom::blink::NotificationAction::From(web_action));
    }

    return blink::mojom::blink::NotificationData::New(
        input.title, ToMojomNotificationDirection(input.direction), input.lang,
        input.body, input.tag, input.image, input.icon, input.badge,
        std::move(vibration_pattern), input.timestamp, input.renotify,
        input.silent, input.require_interaction, std::move(data),
        std::move(actions));
  }
};

}  // namespace mojo

namespace blink {
ServiceWorkerGlobalScopeProxy* ServiceWorkerGlobalScopeProxy::Create(
    WebEmbeddedWorkerImpl& embedded_worker,
    WebServiceWorkerContextClient& client) {
  return new ServiceWorkerGlobalScopeProxy(embedded_worker, client);
}

ServiceWorkerGlobalScopeProxy::~ServiceWorkerGlobalScopeProxy() {
  DCHECK(IsMainThread());
  // Verify that the proxy has been detached.
  DCHECK(!embedded_worker_);
}

void ServiceWorkerGlobalScopeProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_execution_context_task_runners_);
}

void ServiceWorkerGlobalScopeProxy::BindServiceWorkerHost(
    mojo::ScopedInterfaceEndpointHandle service_worker_host) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WorkerGlobalScope()->BindServiceWorkerHost(
      mojom::blink::ServiceWorkerHostAssociatedPtrInfo(
          std::move(service_worker_host),
          mojom::blink::ServiceWorkerHost::Version_));
}

void ServiceWorkerGlobalScopeProxy::SetRegistration(
    WebServiceWorkerRegistrationObjectInfo info) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WorkerGlobalScope()->SetRegistration(std::move(info));
}

void ServiceWorkerGlobalScopeProxy::ReadyToEvaluateScript() {
  WorkerGlobalScope()->ReadyToEvaluateScript();
}

void ServiceWorkerGlobalScopeProxy::DispatchBackgroundFetchAbortEvent(
    int event_id,
    const WebBackgroundFetchRegistration& registration) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kBackgroundFetchAbort, event_id);

  ScriptState* script_state =
      WorkerGlobalScope()->ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit init;
  init.setRegistration(new BackgroundFetchRegistration(
      WorkerGlobalScope()->registration() /* service_worker_registration */,
      registration));

  BackgroundFetchEvent* event = BackgroundFetchEvent::Create(
      EventTypeNames::backgroundfetchabort, init, observer);

  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchBackgroundFetchClickEvent(
    int event_id,
    const WebBackgroundFetchRegistration& registration) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kBackgroundFetchClick, event_id);

  BackgroundFetchEventInit init;
  init.setRegistration(new BackgroundFetchRegistration(
      WorkerGlobalScope()->registration() /* service_worker_registration */,
      registration));

  BackgroundFetchEvent* event = BackgroundFetchEvent::Create(
      EventTypeNames::backgroundfetchclick, init, observer);

  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchBackgroundFetchFailEvent(
    int event_id,
    const WebBackgroundFetchRegistration& registration) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kBackgroundFetchFail, event_id);

  ScriptState* script_state =
      WorkerGlobalScope()->ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchSettledEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit init;
  init.setRegistration(new BackgroundFetchRegistration(
      WorkerGlobalScope()->registration() /* service_worker_registration */,
      registration));

  BackgroundFetchUpdateUIEvent* event = BackgroundFetchUpdateUIEvent::Create(
      EventTypeNames::backgroundfetchfail, init, observer,
      worker_global_scope_->registration());

  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchBackgroundFetchSuccessEvent(
    int event_id,
    const WebBackgroundFetchRegistration& registration) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kBackgroundFetchSuccess,
      event_id);

  ScriptState* script_state =
      WorkerGlobalScope()->ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchSettledEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit init;
  init.setRegistration(new BackgroundFetchRegistration(
      WorkerGlobalScope()->registration() /* service_worker_registration */,
      registration));

  BackgroundFetchUpdateUIEvent* event = BackgroundFetchUpdateUIEvent::Create(
      EventTypeNames::backgroundfetchsuccess, init, observer,
      worker_global_scope_->registration());

  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchActivateEvent(int event_id) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kActivate, event_id);
  Event* event = ExtendableEvent::Create(EventTypeNames::activate,
                                         ExtendableEventInit(), observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchCookieChangeEvent(
    int event_id,
    const WebCanonicalCookie& cookie,
    network::mojom::CookieChangeCause change_cause) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kCookieChange, event_id);

  HeapVector<CookieListItem> changed;
  HeapVector<CookieListItem> deleted;
  CookieChangeEvent::ToEventInfo(cookie, change_cause, changed, deleted);
  Event* event = ExtendableCookieChangeEvent::Create(
      EventTypeNames::cookiechange, std::move(changed), std::move(deleted),
      observer);

  // TODO(pwnall): Handle handle the case when
  //               (changed.IsEmpty() && deleted.IsEmpty()).

  // TODO(pwnall): Investigate dispatching this on cookieStore.
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchExtendableMessageEvent(
    int event_id,
    TransferableMessage message,
    const WebSecurityOrigin& source_origin,
    const WebServiceWorkerClientInfo& client) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  auto msg = ToBlinkTransferableMessage(std::move(message));
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*worker_global_scope_, std::move(msg.ports));
  String origin;
  if (!source_origin.IsOpaque())
    origin = source_origin.ToString();
  ServiceWorkerClient* source = nullptr;
  if (client.client_type == mojom::ServiceWorkerClientType::kWindow)
    source = ServiceWorkerWindowClient::Create(client);
  else
    source = ServiceWorkerClient::Create(client);
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kMessage, event_id);

  Event* event = ExtendableMessageEvent::Create(std::move(msg.message), origin,
                                                ports, source, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchExtendableMessageEvent(
    int event_id,
    TransferableMessage message,
    const WebSecurityOrigin& source_origin,
    WebServiceWorkerObjectInfo info) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  auto msg = ToBlinkTransferableMessage(std::move(message));
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*worker_global_scope_, std::move(msg.ports));
  String origin;
  if (!source_origin.IsOpaque())
    origin = source_origin.ToString();
  ServiceWorker* source = ServiceWorker::From(
      worker_global_scope_->GetExecutionContext(), std::move(info));
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kMessage, event_id);

  Event* event = ExtendableMessageEvent::Create(std::move(msg.message), origin,
                                                ports, source, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchFetchEvent(
    int fetch_event_id,
    const WebServiceWorkerRequest& web_request,
    bool navigation_preload_sent) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  ScriptState::Scope scope(
      WorkerGlobalScope()->ScriptController()->GetScriptState());
  WaitUntilObserver* wait_until_observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kFetch, fetch_event_id);
  FetchRespondWithObserver* respond_with_observer =
      FetchRespondWithObserver::Create(
          WorkerGlobalScope(), fetch_event_id, web_request.Url(),
          web_request.Mode(), web_request.RedirectMode(),
          web_request.GetFrameType(), web_request.GetRequestContext(),
          wait_until_observer);
  Request* request = Request::Create(
      WorkerGlobalScope()->ScriptController()->GetScriptState(), web_request);
  request->getHeaders()->SetGuard(Headers::kImmutableGuard);
  FetchEventInit event_init;
  event_init.setCancelable(true);
  event_init.setRequest(request);
  event_init.setClientId(
      web_request.IsMainResourceLoad() ? WebString() : web_request.ClientId());
  event_init.setIsReload(web_request.IsReload());
  ScriptState* script_state =
      WorkerGlobalScope()->ScriptController()->GetScriptState();
  FetchEvent* fetch_event = FetchEvent::Create(
      script_state, EventTypeNames::fetch, event_init, respond_with_observer,
      wait_until_observer, navigation_preload_sent);
  if (navigation_preload_sent) {
    // Keep |fetchEvent| until OnNavigationPreloadComplete() or
    // onNavigationPreloadError() will be called.
    pending_preload_fetch_events_.insert(fetch_event_id, fetch_event);
  }

  WorkerGlobalScope()->DispatchExtendableEventWithRespondWith(
      fetch_event, wait_until_observer, respond_with_observer);
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<WebURLResponse> response,
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  auto it = pending_preload_fetch_events_.find(fetch_event_id);
  DCHECK(it != pending_preload_fetch_events_.end());
  FetchEvent* fetch_event = it->value.Get();
  DCHECK(fetch_event);
  fetch_event->OnNavigationPreloadResponse(
      WorkerGlobalScope()->ScriptController()->GetScriptState(),
      std::move(response), std::move(data_pipe));
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadError(
    int fetch_event_id,
    std::unique_ptr<WebServiceWorkerError> error) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  FetchEvent* fetch_event = pending_preload_fetch_events_.Take(fetch_event_id);
  DCHECK(fetch_event);
  // Display an error message to the console, preferring the unsanitized one if
  // available.
  const WebString& error_message = error->unsanitized_message.IsEmpty()
                                       ? error->message
                                       : error->unsanitized_message;
  if (!error_message.IsEmpty()) {
    WorkerGlobalScope()->AddConsoleMessage(ConsoleMessage::Create(
        kWorkerMessageSource, blink::MessageLevel::kErrorMessageLevel,
        error_message));
  }
  // Reject the preloadResponse promise.
  fetch_event->OnNavigationPreloadError(
      WorkerGlobalScope()->ScriptController()->GetScriptState(),
      std::move(error));
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadComplete(
    int fetch_event_id,
    TimeTicks completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  FetchEvent* fetch_event = pending_preload_fetch_events_.Take(fetch_event_id);
  DCHECK(fetch_event);
  fetch_event->OnNavigationPreloadComplete(
      WorkerGlobalScope(), completion_time, encoded_data_length,
      encoded_body_length, decoded_body_length);
}

void ServiceWorkerGlobalScopeProxy::DispatchInstallEvent(int event_id) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kInstall, event_id);
  Event* event = InstallEvent::Create(
      EventTypeNames::install, ExtendableEventInit(), event_id, observer);
  WorkerGlobalScope()->SetIsInstalling(true);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchNotificationClickEvent(
    int event_id,
    const WebString& notification_id,
    const WebNotificationData& data,
    int action_index,
    const WebString& reply) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kNotificationClick, event_id);
  NotificationEventInit event_init;
  event_init.setNotification(Notification::Create(
      WorkerGlobalScope(), notification_id,
      mojom::blink::NotificationData::From(data), true /* showing */));
  if (0 <= action_index && action_index < static_cast<int>(data.actions.size()))
    event_init.setAction(data.actions[action_index].action);
  event_init.setReply(reply);
  Event* event = NotificationEvent::Create(EventTypeNames::notificationclick,
                                           event_init, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchNotificationCloseEvent(
    int event_id,
    const WebString& notification_id,
    const WebNotificationData& data) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kNotificationClose, event_id);
  NotificationEventInit event_init;
  event_init.setAction(WTF::String());  // initialize as null.
  event_init.setNotification(Notification::Create(
      WorkerGlobalScope(), notification_id,
      mojom::blink::NotificationData::From(data), false /* showing */));
  Event* event = NotificationEvent::Create(EventTypeNames::notificationclose,
                                           event_init, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchPushEvent(int event_id,
                                                      const WebString& data) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kPush, event_id);
  Event* event = PushEvent::Create(EventTypeNames::push,
                                   PushMessageData::Create(data), observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchSyncEvent(int event_id,
                                                      const WebString& id,
                                                      bool last_chance) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kSync, event_id);
  Event* event =
      SyncEvent::Create(EventTypeNames::sync, id, last_chance, observer);
  WorkerGlobalScope()->DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchAbortPaymentEvent(int event_id) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* wait_until_observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kAbortPayment, event_id);
  AbortPaymentRespondWithObserver* respond_with_observer =
      new AbortPaymentRespondWithObserver(WorkerGlobalScope(), event_id,
                                          wait_until_observer);

  Event* event = AbortPaymentEvent::Create(
      EventTypeNames::abortpayment, ExtendableEventInit(),
      respond_with_observer, wait_until_observer);

  WorkerGlobalScope()->DispatchExtendableEventWithRespondWith(
      event, wait_until_observer, respond_with_observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchCanMakePaymentEvent(
    int event_id,
    const WebCanMakePaymentEventData& web_event_data) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* wait_until_observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kCanMakePayment, event_id);
  CanMakePaymentRespondWithObserver* respond_with_observer =
      new CanMakePaymentRespondWithObserver(WorkerGlobalScope(), event_id,
                                            wait_until_observer);

  Event* event = CanMakePaymentEvent::Create(
      EventTypeNames::canmakepayment,
      PaymentEventDataConversion::ToCanMakePaymentEventInit(
          WorkerGlobalScope()->ScriptController()->GetScriptState(),
          web_event_data),
      respond_with_observer, wait_until_observer);

  WorkerGlobalScope()->DispatchExtendableEventWithRespondWith(
      event, wait_until_observer, respond_with_observer);
}

void ServiceWorkerGlobalScopeProxy::DispatchPaymentRequestEvent(
    int event_id,
    const WebPaymentRequestEventData& web_app_request) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WaitUntilObserver* wait_until_observer = WaitUntilObserver::Create(
      WorkerGlobalScope(), WaitUntilObserver::kPaymentRequest, event_id);
  PaymentRequestRespondWithObserver* respond_with_observer =
      PaymentRequestRespondWithObserver::Create(WorkerGlobalScope(), event_id,
                                                wait_until_observer);

  Event* event = PaymentRequestEvent::Create(
      EventTypeNames::paymentrequest,
      PaymentEventDataConversion::ToPaymentRequestEventInit(
          WorkerGlobalScope()->ScriptController()->GetScriptState(),
          web_app_request),
      respond_with_observer, wait_until_observer);

  WorkerGlobalScope()->DispatchExtendableEventWithRespondWith(
      event, wait_until_observer, respond_with_observer);
}

bool ServiceWorkerGlobalScopeProxy::HasFetchEventHandler() {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  return WorkerGlobalScope()->HasEventListeners(EventTypeNames::fetch);
}

void ServiceWorkerGlobalScopeProxy::CountFeature(WebFeature feature) {
  Client().CountFeature(feature);
}

void ServiceWorkerGlobalScopeProxy::CountDeprecation(WebFeature feature) {
  // Go through the same code path with countFeature() because a deprecation
  // message is already shown on the worker console and a remaining work is
  // just to record an API use.
  CountFeature(feature);
}

void ServiceWorkerGlobalScopeProxy::ReportException(
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    int exception_id) {
  Client().ReportException(error_message, location->LineNumber(),
                           location->ColumnNumber(), location->Url());
}

void ServiceWorkerGlobalScopeProxy::ReportConsoleMessage(
    MessageSource source,
    MessageLevel level,
    const String& message,
    SourceLocation* location) {
  Client().ReportConsoleMessage(source, level, message, location->LineNumber(),
                                location->Url());
}

void ServiceWorkerGlobalScopeProxy::PostMessageToPageInspector(
    int session_id,
    const String& message) {
  DCHECK(embedded_worker_);
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(
          TaskType::kInternalInspector),
      FROM_HERE,
      CrossThreadBind(&WebEmbeddedWorkerImpl::PostMessageToPageInspector,
                      CrossThreadUnretained(embedded_worker_), session_id,
                      message));
}

void ServiceWorkerGlobalScopeProxy::DidCreateWorkerGlobalScope(
    WorkerOrWorkletGlobalScope* worker_global_scope) {
  DCHECK(!worker_global_scope_);
  worker_global_scope_ =
      static_cast<ServiceWorkerGlobalScope*>(worker_global_scope);
  Client().WorkerContextStarted(this);
}

void ServiceWorkerGlobalScopeProxy::DidInitializeWorkerContext() {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  ScriptState::Scope scope(
      WorkerGlobalScope()->ScriptController()->GetScriptState());
  Client().DidInitializeWorkerContext(
      WorkerGlobalScope()->ScriptController()->GetContext());
}

void ServiceWorkerGlobalScopeProxy::DidLoadInstalledScript() {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  Client().WorkerScriptLoaded();
}

void ServiceWorkerGlobalScopeProxy::WillEvaluateClassicScript(
    size_t script_size,
    size_t cached_metadata_size) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  // TODO(asamidoi): Remove CountWorkerScript which is called for recording
  // metrics if the metrics are no longer referenced, and then merge
  // WillEvaluateClassicScript and WillEvaluateModuleScript for cleanup.
  worker_global_scope_->CountWorkerScript(script_size, cached_metadata_size);
  Client().WillEvaluateScript();
}

void ServiceWorkerGlobalScopeProxy::WillEvaluateImportedClassicScript(
    size_t script_size,
    size_t cached_metadata_size) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  worker_global_scope_->CountImportedScript(script_size, cached_metadata_size);
}

void ServiceWorkerGlobalScopeProxy::WillEvaluateModuleScript() {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  Client().WillEvaluateScript();
}

void ServiceWorkerGlobalScopeProxy::DidEvaluateClassicScript(bool success) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WorkerGlobalScope()->DidEvaluateScript();
  Client().DidEvaluateScript(success);
}

void ServiceWorkerGlobalScopeProxy::DidEvaluateModuleScript(bool success) {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  WorkerGlobalScope()->DidEvaluateScript();
  Client().DidEvaluateScript(success);
}

void ServiceWorkerGlobalScopeProxy::DidCloseWorkerGlobalScope() {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  // close() is not web-exposed. This is called when ServiceWorkerGlobalScope
  // internally requests close() due to failure on startup when installed
  // scripts couldn't be read.
  //
  // This may look like a roundabout way to terminate the thread, but close()
  // seems like the standard way to initiate termination from inside the thread.

  // Tell ServiceWorkerContextClient about the failure. The generic
  // WorkerContextFailedToStart() wouldn't make sense because
  // WorkerContextStarted() was already called.
  Client().FailedToLoadInstalledScript();

  // ServiceWorkerGlobalScope expects us to terminate the thread, so request
  // that here.
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBind(&WebEmbeddedWorkerImpl::TerminateWorkerContext,
                      CrossThreadUnretained(embedded_worker_)));

  // NOTE: WorkerThread calls WillDestroyWorkerGlobalScope() synchronously after
  // this function returns, since it calls DidCloseWorkerGlobalScope() then
  // PrepareForShutdownOnWorkerThread().
}

void ServiceWorkerGlobalScopeProxy::WillDestroyWorkerGlobalScope() {
  DCHECK(WorkerGlobalScope()->IsContextThread());
  v8::HandleScope handle_scope(WorkerGlobalScope()->GetThread()->GetIsolate());
  Client().WillDestroyWorkerContext(
      WorkerGlobalScope()->ScriptController()->GetContext());
  worker_global_scope_ = nullptr;
}

void ServiceWorkerGlobalScopeProxy::DidTerminateWorkerThread() {
  // This must be called after WillDestroyWorkerGlobalScope().
  DCHECK(!worker_global_scope_);
  Client().WorkerContextDestroyed();
}

ServiceWorkerGlobalScopeProxy::ServiceWorkerGlobalScopeProxy(
    WebEmbeddedWorkerImpl& embedded_worker,
    WebServiceWorkerContextClient& client)
    : embedded_worker_(&embedded_worker),
      client_(&client),
      worker_global_scope_(nullptr) {
  DCHECK(IsMainThread());
  // ServiceWorker can sometimes run tasks that are initiated by/associated
  // with a document's frame but these documents can be from a different
  // process. So we intentionally populate the task runners with default task
  // runners of the main thread.
  parent_execution_context_task_runners_ =
      ParentExecutionContextTaskRunners::Create();
}

void ServiceWorkerGlobalScopeProxy::Detach() {
  DCHECK(IsMainThread());
  embedded_worker_ = nullptr;
  client_ = nullptr;
}

void ServiceWorkerGlobalScopeProxy::TerminateWorkerContext() {
  DCHECK(IsMainThread());
  embedded_worker_->TerminateWorkerContext();
}

WebServiceWorkerContextClient& ServiceWorkerGlobalScopeProxy::Client() const {
  DCHECK(client_);
  return *client_;
}

ServiceWorkerGlobalScope* ServiceWorkerGlobalScopeProxy::WorkerGlobalScope()
    const {
  DCHECK(worker_global_scope_);
  DCHECK(worker_global_scope_->IsContextThread());
  return worker_global_scope_;
}

}  // namespace blink
