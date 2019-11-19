// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/fake_service_worker.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace content {

FakeServiceWorker::FakeServiceWorker(EmbeddedWorkerTestHelper* helper)
    : helper_(helper) {}

FakeServiceWorker::~FakeServiceWorker() = default;

void FakeServiceWorker::Bind(
    mojo::PendingReceiver<blink::mojom::ServiceWorker> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &FakeServiceWorker::CallOnConnectionError, base::Unretained(this)));
}

void FakeServiceWorker::RunUntilInitializeGlobalScope() {
  if (host_)
    return;
  base::RunLoop loop;
  quit_closure_for_initialize_global_scope_ = loop.QuitClosure();
  loop.Run();
}

void FakeServiceWorker::InitializeGlobalScope(
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerHost>
        service_worker_host,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info,
    blink::mojom::ServiceWorkerObjectInfoPtr service_worker_info,
    blink::mojom::FetchHandlerExistence fetch_handler_existence) {
  host_.Bind(std::move(service_worker_host));

  // Enable callers to use these endpoints without us actually binding them
  // to an implementation.
  mojo::AssociateWithDisconnectedPipe(registration_info->receiver.PassHandle());
  if (registration_info->installing) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->installing->receiver.PassHandle());
  }
  if (registration_info->waiting) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->waiting->receiver.PassHandle());
  }
  if (registration_info->active) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->active->receiver.PassHandle());
  }

  if (service_worker_info) {
    mojo::AssociateWithDisconnectedPipe(
        service_worker_info->receiver.PassHandle());
  }

  registration_info_ = std::move(registration_info);
  service_worker_info_ = std::move(service_worker_info);
  if (quit_closure_for_initialize_global_scope_)
    std::move(quit_closure_for_initialize_global_scope_).Run();

  fetch_handler_existence_ = fetch_handler_existence;
}

void FakeServiceWorker::DispatchInstallEvent(
    DispatchInstallEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          true /* has_fetch_handler */);
}

void FakeServiceWorker::DispatchActivateEvent(
    DispatchActivateEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchBackgroundFetchAbortEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchAbortEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchBackgroundFetchClickEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchClickEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchBackgroundFetchFailEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchFailEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchBackgroundFetchSuccessEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchSuccessEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchCookieChangeEvent(
    const net::CookieChangeInfo& change,
    DispatchCookieChangeEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchFetchEventForMainResource(
    blink::mojom::DispatchFetchEventParamsPtr params,
    mojo::PendingRemote<blink::mojom::ServiceWorkerFetchResponseCallback>
        pending_response_callback,
    DispatchFetchEventForMainResourceCallback callback) {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kDefault;
  mojo::Remote<blink::mojom::ServiceWorkerFetchResponseCallback>
      response_callback(std::move(pending_response_callback));
  response_callback->OnResponse(
      std::move(response), blink::mojom::ServiceWorkerFetchEventTiming::New());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchNotificationClickEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    DispatchNotificationClickEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchNotificationCloseEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    DispatchNotificationCloseEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchPushEvent(
    const base::Optional<std::string>& payload,
    DispatchPushEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchPushSubscriptionChangeEvent(
    blink::mojom::PushSubscriptionPtr old_subscription,
    blink::mojom::PushSubscriptionPtr new_subscription,
    DispatchPushSubscriptionChangeEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchSyncEvent(const std::string& tag,
                                          bool last_chance,
                                          base::TimeDelta timeout,
                                          DispatchSyncEventCallback callback) {
  NOTIMPLEMENTED();
}

void FakeServiceWorker::DispatchPeriodicSyncEvent(
    const std::string& tag,
    base::TimeDelta timeout,
    DispatchPeriodicSyncEventCallback callback) {
  NOTIMPLEMENTED();
}

void FakeServiceWorker::DispatchAbortPaymentEvent(
    mojo::PendingRemote<payments::mojom::PaymentHandlerResponseCallback>
        pending_response_callback,
    DispatchAbortPaymentEventCallback callback) {
  mojo::Remote<payments::mojom::PaymentHandlerResponseCallback>
      response_callback(std::move(pending_response_callback));
  response_callback->OnResponseForAbortPayment(true);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    mojo::PendingRemote<payments::mojom::PaymentHandlerResponseCallback>
        pending_response_callback,
    DispatchCanMakePaymentEventCallback callback) {
  mojo::Remote<payments::mojom::PaymentHandlerResponseCallback>
      response_callback(std::move(pending_response_callback));
  response_callback->OnResponseForCanMakePayment(true);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    mojo::PendingRemote<payments::mojom::PaymentHandlerResponseCallback>
        pending_response_callback,
    DispatchPaymentRequestEventCallback callback) {
  mojo::Remote<payments::mojom::PaymentHandlerResponseCallback>
      response_callback(std::move(pending_response_callback));
  response_callback->OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponse::New());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchExtendableMessageEvent(
    blink::mojom::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchContentDeleteEvent(
    const std::string& id,
    DispatchContentDeleteEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::Ping(PingCallback callback) {
  std::move(callback).Run();
}

void FakeServiceWorker::SetIdleTimerDelayToZero() {
  is_zero_idle_timer_delay_ = true;
}

void FakeServiceWorker::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  NOTIMPLEMENTED();
}

void FakeServiceWorker::OnConnectionError() {
  // Destroys |this|.
  helper_->RemoveServiceWorker(this);
}

void FakeServiceWorker::CallOnConnectionError() {
  // Call OnConnectionError(), which subclasses can override.
  OnConnectionError();
}

}  // namespace content
