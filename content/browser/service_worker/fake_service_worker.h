// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_FAKE_SERVICE_WORKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_FAKE_SERVICE_WORKER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"

namespace content {

class EmbeddedWorkerTestHelper;

// The default fake for blink::mojom::ServiceWorker. It responds to event
// dispatches with success. It is owned by EmbeddedWorkerTestHelper and
// by default the lifetime is tied to the Mojo connection.
class FakeServiceWorker : public blink::mojom::ServiceWorker {
 public:
  using FetchHandlerExistence = blink::mojom::FetchHandlerExistence;

  // |helper| must outlive this instance.
  explicit FakeServiceWorker(EmbeddedWorkerTestHelper* helper);

  ~FakeServiceWorker() override;

  mojo::AssociatedRemote<blink::mojom::ServiceWorkerHost>& host() {
    return host_;
  }

  EmbeddedWorkerTestHelper* helper() { return helper_; }

  void Bind(mojo::PendingReceiver<blink::mojom::ServiceWorker> receiver);

  // Returns after InitializeGlobalScope() is called.
  void RunUntilInitializeGlobalScope();

  bool is_zero_idle_timer_delay() const { return is_zero_idle_timer_delay_; }

  FetchHandlerExistence fetch_handler_existence() const {
    return fetch_handler_existence_;
  }

 protected:
  // blink::mojom::ServiceWorker overrides:
  void InitializeGlobalScope(
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerHost>
          service_worker_host,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info,
      blink::mojom::ServiceWorkerObjectInfoPtr service_worker_info,
      FetchHandlerExistence fetch_handler_existence) override;
  void DispatchInstallEvent(DispatchInstallEventCallback callback) override;
  void DispatchActivateEvent(DispatchActivateEventCallback callback) override;
  void DispatchBackgroundFetchAbortEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchAbortEventCallback callback) override;
  void DispatchBackgroundFetchClickEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchClickEventCallback callback) override;
  void DispatchBackgroundFetchFailEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchFailEventCallback callback) override;
  void DispatchBackgroundFetchSuccessEvent(
      blink::mojom::BackgroundFetchRegistrationPtr registration,
      DispatchBackgroundFetchSuccessEventCallback callback) override;
  void DispatchCookieChangeEvent(
      const net::CookieChangeInfo& change,
      DispatchCookieChangeEventCallback callback) override;
  void DispatchFetchEventForMainResource(
      blink::mojom::DispatchFetchEventParamsPtr params,
      mojo::PendingRemote<blink::mojom::ServiceWorkerFetchResponseCallback>
          response_callback,
      DispatchFetchEventForMainResourceCallback callback) override;
  void DispatchNotificationClickEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      DispatchNotificationClickEventCallback callback) override;
  void DispatchNotificationCloseEvent(
      const std::string& notification_id,
      const blink::PlatformNotificationData& notification_data,
      DispatchNotificationCloseEventCallback callback) override;
  void DispatchPushEvent(const base::Optional<std::string>& payload,
                         DispatchPushEventCallback callback) override;
  void DispatchPushSubscriptionChangeEvent(
      blink::mojom::PushSubscriptionPtr old_subscription,
      blink::mojom::PushSubscriptionPtr new_subscription,
      DispatchPushSubscriptionChangeEventCallback callback) override;
  void DispatchSyncEvent(const std::string& tag,
                         bool last_chance,
                         base::TimeDelta timeout,
                         DispatchSyncEventCallback callback) override;
  void DispatchPeriodicSyncEvent(
      const std::string& tag,
      base::TimeDelta timeout,
      DispatchPeriodicSyncEventCallback callback) override;
  void DispatchAbortPaymentEvent(
      mojo::PendingRemote<payments::mojom::PaymentHandlerResponseCallback>
          pending_response_callback,
      DispatchAbortPaymentEventCallback callback) override;
  void DispatchCanMakePaymentEvent(
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      mojo::PendingRemote<payments::mojom::PaymentHandlerResponseCallback>
          pending_response_callback,
      DispatchCanMakePaymentEventCallback callback) override;
  void DispatchPaymentRequestEvent(
      payments::mojom::PaymentRequestEventDataPtr event_data,
      mojo::PendingRemote<payments::mojom::PaymentHandlerResponseCallback>
          pending_response_callback,
      DispatchPaymentRequestEventCallback callback) override;
  void DispatchExtendableMessageEvent(
      blink::mojom::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback) override;
  void DispatchContentDeleteEvent(
      const std::string& id,
      DispatchContentDeleteEventCallback callback) override;
  void Ping(PingCallback callback) override;
  void SetIdleTimerDelayToZero() override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;

  virtual void OnConnectionError();

 private:
  void CallOnConnectionError();

  // |helper_| owns |this|.
  EmbeddedWorkerTestHelper* const helper_;

  mojo::AssociatedRemote<blink::mojom::ServiceWorkerHost> host_;
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info_;
  blink::mojom::ServiceWorkerObjectInfoPtr service_worker_info_;
  FetchHandlerExistence fetch_handler_existence_ =
      FetchHandlerExistence::UNKNOWN;
  base::OnceClosure quit_closure_for_initialize_global_scope_;

  mojo::Receiver<blink::mojom::ServiceWorker> receiver_{this};

  bool is_zero_idle_timer_delay_ = false;
};

}  // namespace content
#endif  // CONTENT_BROWSER_SERVICE_WORKER_FAKE_SERVICE_WORKER_H_
