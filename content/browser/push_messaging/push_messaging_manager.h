// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "url/gurl.h"

namespace blink {
namespace mojom {
enum class PushRegistrationStatus;
enum class PushUnregistrationStatus;
}  // namespace mojom
}  // namespace blink

namespace url {
class Origin;
}  // namespace url

namespace content {

class PushMessagingService;
class RenderProcessHost;
class ServiceWorkerContextWrapper;

// Documented at definition.
extern const char kPushSenderIdServiceWorkerKey[];
extern const char kPushRegistrationIdServiceWorkerKey[];

// Owned by RenderFrameHostImpl (if `this` handles requests from a document) or
// RenderProcessHostImpl (if `this` handles requests from a service worker).
// Lives on the UI thread.
class PushMessagingManager : public blink::mojom::PushMessaging {
 public:
  PushMessagingManager(
      RenderProcessHost& render_process_host,
      int render_frame_id,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  PushMessagingManager(const PushMessagingManager&) = delete;
  PushMessagingManager& operator=(const PushMessagingManager&) = delete;

  ~PushMessagingManager() override;

  void AddPushMessagingReceiver(
      mojo::PendingReceiver<blink::mojom::PushMessaging> receiver);

  base::WeakPtr<PushMessagingManager> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // blink::mojom::PushMessaging impl.
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

  void Register(RegisterData data);
  void DidRegister(RegisterData data,
                   const std::string& push_subscription_id,
                   const GURL& endpoint,
                   const std::optional<base::Time>& expiration_time,
                   const std::vector<uint8_t>& p256dh,
                   const std::vector<uint8_t>& auth,
                   blink::mojom::PushRegistrationStatus status);
  void DidRequestPermissionInIncognito(RegisterData data,
                                       blink::mojom::PermissionStatus status);

  void DidCheckForExistingRegistration(
      RegisterData data,
      const std::vector<std::string>& subscription_id_and_sender_id,
      blink::ServiceWorkerStatusCode service_worker_status);

  void DidGetSenderIdFromStorage(
      RegisterData data,
      const std::vector<std::string>& sender_id,
      blink::ServiceWorkerStatusCode service_worker_status);

  void PersistRegistration(RegisterData data,
                           const std::string& push_subscription_id,
                           const GURL& endpoint,
                           const std::optional<base::Time>& expiration_time,
                           const std::vector<uint8_t>& p256dh,
                           const std::vector<uint8_t>& auth,
                           blink::mojom::PushRegistrationStatus status);

  void DidPersistRegistration(
      RegisterData data,
      const GURL& endpoint,
      const std::optional<base::Time>& expiration_time,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth,
      blink::mojom::PushRegistrationStatus push_registration_status,
      blink::ServiceWorkerStatusCode service_worker_status);

  void SendSubscriptionError(RegisterData data,
                             blink::mojom::PushRegistrationStatus status);
  void SendSubscriptionSuccess(RegisterData data,
                               blink::mojom::PushRegistrationStatus status,
                               const GURL& endpoint,
                               const std::optional<base::Time>& expiration_time,
                               const std::vector<uint8_t>& p256dh,
                               const std::vector<uint8_t>& auth);

  void GetSubscriptionDidUnsubscribe(
      GetSubscriptionCallback callback,
      blink::mojom::PushGetRegistrationStatus get_status,
      blink::mojom::PushUnregistrationStatus unsubscribe_status);

  void GetSubscriptionDidGetInfo(
      GetSubscriptionCallback callback,
      const url::Origin& origin,
      int64_t service_worker_registration_id,
      const std::string& application_server_key,
      bool is_valid,
      const GURL& endpoint,
      const std::optional<base::Time>& expiration_time,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth);

  void GetSubscriptionInfo(
      const url::Origin& origin,
      int64_t service_worker_registration_id,
      const std::string& sender_id,
      const std::string& push_subscription_id,
      PushMessagingService::SubscriptionInfoCallback callback);

  void UnsubscribeHavingGottenSenderId(
      UnsubscribeCallback callback,
      int64_t service_worker_registration_id,
      const url::Origin& requesting_origin,
      const std::vector<std::string>& sender_id,
      blink::ServiceWorkerStatusCode service_worker_status);

  void DidUnregister(
      UnsubscribeCallback callback,
      blink::mojom::PushUnregistrationStatus unregistration_status);

  void DidGetSubscription(
      GetSubscriptionCallback callback,
      int64_t service_worker_registration_id,
      const std::vector<std::string>& push_subscription_id_and_sender_info,
      blink::ServiceWorkerStatusCode service_worker_status);

  PushMessagingService* GetService();

  const raw_ref<RenderProcessHost> render_process_host_;

  // Will be ChildProcessHost::kInvalidUniqueID in requests from Service Worker.
  const int render_frame_id_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  const bool is_incognito_;

  // Whether the PushMessagingService was available when constructed.
  const bool service_available_;

  mojo::ReceiverSet<blink::mojom::PushMessaging> receivers_;

  base::WeakPtrFactory<PushMessagingManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MANAGER_H_
