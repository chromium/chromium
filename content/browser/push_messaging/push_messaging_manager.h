// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "url/gurl.h"

namespace blink {
namespace mojom {
class PushMessagingService;
enum class PushRegistrationStatus;
enum class PushUnregistrationStatus;
}  // namespace mojom
}  // namespace blink

namespace content {

class ServiceWorkerContextWrapper;

// Documented at definition.
extern const char kPushSenderIdServiceWorkerKey[];
extern const char kPushRegistrationIdServiceWorkerKey[];

class PushMessagingManager : public blink::mojom::PushMessaging {
 public:
  PushMessagingManager(int render_process_id,
                       int render_frame_id,
                       ServiceWorkerContextWrapper* service_worker_context);

  void AddPushMessagingReceiver(
      mojo::PendingReceiver<blink::mojom::PushMessaging> receiver);

  base::WeakPtr<PushMessagingManager> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // blink::mojom::PushMessaging impl, run on service worker core thread.
  void Subscribe(int64_t service_worker_registration_id,
                 blink::mojom::PushSubscriptionOptionsPtr options,
                 bool user_gesture,
                 SubscribeCallback callback) override;
  void Unsubscribe(int64_t service_worker_registration_id,
                   UnsubscribeCallback callback) override;
  void GetSubscription(int64_t service_worker_registration_id,
                       GetSubscriptionCallback callback) override;

 private:
  struct RegisterData;
  class Core;

  friend class BrowserThread;
  friend class base::DeleteHelper<PushMessagingManager>;

  ~PushMessagingManager() override;

  void DidCheckForExistingRegistration(
      RegisterData data,
      const std::vector<std::string>& subscription_id_and_sender_id,
      blink::ServiceWorkerStatusCode service_worker_status);

  void DidGetSenderIdFromStorage(
      RegisterData data,
      const std::vector<std::string>& sender_id,
      blink::ServiceWorkerStatusCode service_worker_status);

  // Called via PostTask from UI thread.
  void PersistRegistrationOnSW(RegisterData data,
                               const std::string& push_subscription_id,
                               const GURL& endpoint,
                               const std::vector<uint8_t>& p256dh,
                               const std::vector<uint8_t>& auth,
                               blink::mojom::PushRegistrationStatus status);

  void DidPersistRegistrationOnSW(
      RegisterData data,
      const GURL& endpoint,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth,
      blink::mojom::PushRegistrationStatus push_registration_status,
      blink::ServiceWorkerStatusCode service_worker_status);

  // Called both from "SW core" thread, and via PostTask from UI thread.
  void SendSubscriptionError(RegisterData data,
                             blink::mojom::PushRegistrationStatus status);
  // Called both from "SW core" thread, and via PostTask from UI thread.
  void SendSubscriptionSuccess(RegisterData data,
                               blink::mojom::PushRegistrationStatus status,
                               const GURL& endpoint,
                               const std::vector<uint8_t>& p256dh,
                               const std::vector<uint8_t>& auth);

  void UnsubscribeHavingGottenSenderId(
      UnsubscribeCallback callback,
      int64_t service_worker_registration_id,
      const GURL& requesting_origin,
      const std::vector<std::string>& sender_id,
      blink::ServiceWorkerStatusCode service_worker_status);

  // Called both from "SW core" thread, and via PostTask from UI thread.
  void DidUnregister(
      UnsubscribeCallback callback,
      blink::mojom::PushUnregistrationStatus unregistration_status);

  void DidGetSubscription(
      GetSubscriptionCallback callback,
      int64_t service_worker_registration_id,
      const std::vector<std::string>& push_subscription_id_and_sender_info,
      blink::ServiceWorkerStatusCode service_worker_status);

  // Helper methods on either thread -------------------------------------------

  // Inner core of this message filter which lives on the UI thread.
  std::unique_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;

  // Can be used on the SW core thread as the |this| parameter when binding a
  // callback that will be called on the UI thread (an SW -> UI -> UI chain).
  base::WeakPtr<Core> ui_core_weak_ptr_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Whether the PushMessagingService was available when constructed.
  bool service_available_;

  // Will be ChildProcessHost::kInvalidUniqueID in requests from Service Worker.
  int render_frame_id_;

  mojo::ReceiverSet<blink::mojom::PushMessaging> receivers_;

  base::WeakPtrFactory<PushMessagingManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PushMessagingManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_
