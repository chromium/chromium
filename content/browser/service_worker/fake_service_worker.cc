// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/fake_service_worker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom.h"

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
  if (host_) {
    return;
  }
  base::RunLoop loop;
  quit_closure_for_initialize_global_scope_ = loop.QuitClosure();
  loop.Run();
}

void FakeServiceWorker::FlushForTesting() {
  receiver_.FlushForTesting();
}

base::WeakPtr<FakeServiceWorker> FakeServiceWorker::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeServiceWorker::InitializeGlobalScope(
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerHost>
        service_worker_host,
    mojo::PendingAssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        associated_interfaces_from_browser,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        associated_interfaces_to_browser,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info,
    blink::mojom::ServiceWorkerObjectInfoPtr service_worker_info,
    blink::mojom::FetchHandlerExistence fetch_handler_existence,
    mojo::PendingReceiver<blink::mojom::ReportingObserver>
        reporting_observer_receiver,
    blink::mojom::AncestorFrameType ancestor_frame_type,
    const blink::StorageKey& storage_key) {
  host_.Bind(std::move(service_worker_host));

  // Enable callers to use these endpoints without us actually binding them
  // to an implementation.
  registration_info->receiver.EnableUnassociatedUsage();
  if (registration_info->installing) {
    registration_info->installing->receiver.EnableUnassociatedUsage();
  }
  if (registration_info->waiting) {
    registration_info->waiting->receiver.EnableUnassociatedUsage();
  }
  if (registration_info->active) {
    registration_info->active->receiver.EnableUnassociatedUsage();
  }
  if (service_worker_info) {
    service_worker_info->receiver.EnableUnassociatedUsage();
  }

  registration_info_ = std::move(registration_info);
  service_worker_info_ = std::move(service_worker_info);
  if (quit_closure_for_initialize_global_scope_) {
    std::move(quit_closure_for_initialize_global_scope_).Run();
  }

  fetch_handler_existence_ = fetch_handler_existence;
}

void FakeServiceWorker::DispatchInstallEvent(
    DispatchInstallEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          /*fetch_count=*/0);
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
  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  auto now = base::TimeTicks::Now();
  timing->respond_with_settled_time = now;
  timing->dispatch_event_time = now;
  response_callback->OnResponse(std::move(response), std::move(timing));
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchNotificationClickEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const std::optional<std::u16string>& reply,
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
    const std::optional<std::string>& payload,
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
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void FakeServiceWorker::DispatchPeriodicSyncEvent(
    const std::string& tag,
    base::TimeDelta timeout,
    DispatchPeriodicSyncEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
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
  response_callback->OnResponseForCanMakePayment(
      payments::mojom::CanMakePaymentResponse::New());
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

void FakeServiceWorker::SetIdleDelay(base::TimeDelta delay) {
  idle_delay_ = delay;
}

void FakeServiceWorker::AddKeepAlive() {
  idle_delay_.reset();
}

void FakeServiceWorker::ClearKeepAlive() {
  idle_delay_ = base::Seconds(30);
}

void FakeServiceWorker::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  NOTIMPLEMENTED();
}

void FakeServiceWorker::ExecuteScriptForTest(
    const std::u16string& script,
    bool wants_result,
    ExecuteScriptForTestCallback callback) {
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
