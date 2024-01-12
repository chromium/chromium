// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "url/gurl.h"

namespace blink {
namespace mojom {
enum class PermissionStatus;
enum class PushRegistrationStatus;
enum class PushUnregistrationReason;
enum class PushUnregistrationStatus;
}  // namespace mojom
}  // namespace blink

namespace content {

class BrowserContext;

// A push service-agnostic interface that the Push API uses for talking to
// push messaging services like GCM. Must only be used on the UI thread.
class CONTENT_EXPORT PushMessagingService {
 public:
  using RegisterCallback =
      base::OnceCallback<void(const std::string& registration_id,
                              const GURL& endpoint,
                              const std::optional<base::Time>& expiration_time,
                              const std::vector<uint8_t>& p256dh,
                              const std::vector<uint8_t>& auth,
                              blink::mojom::PushRegistrationStatus status)>;
  using UnregisterCallback =
      base::OnceCallback<void(blink::mojom::PushUnregistrationStatus)>;
  using SubscriptionInfoCallback =
      base::OnceCallback<void(bool is_valid,
                              const GURL& endpoint,
                              const std::optional<base::Time>& expiration_time,
                              const std::vector<uint8_t>& p256dh,
                              const std::vector<uint8_t>& auth)>;
  using RegistrationUserDataCallback =
      base::OnceCallback<void(const std::vector<std::string>& data)>;

  using SWDataCallback =
      base::OnceCallback<void(const std::string& sender_id,
                              const std::string& subscription_id)>;

  using SenderIdCallback =
      base::OnceCallback<void(const std::string& sender_id)>;

  virtual ~PushMessagingService() {}

  // Subscribes the given |options->sender_info| with the push messaging service
  // in a document context. The frame is known and a permission UI may be
  // displayed to the user. It's safe to call this method multiple times for
  // the same registration information, in which case the existing subscription
  // will be returned by the server.
  virtual void SubscribeFromDocument(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      int render_process_id,
      int render_frame_id,
      blink::mojom::PushSubscriptionOptionsPtr options,
      bool user_gesture,
      RegisterCallback callback) = 0;

  // Subscribes the given |options->sender_info| with the push messaging
  // service. The frame is not known so if permission was not previously granted
  // by the user this request should fail. It's safe to call this method
  // multiple times for the same registration information, in which case the
  // existing subscription will be returned by the server.
  virtual void SubscribeFromWorker(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      int render_process_id,
      blink::mojom::PushSubscriptionOptionsPtr options,
      RegisterCallback callback) = 0;

  // Retrieves the subscription associated with |origin| and
  // |service_worker_registration_id|, validates that the provided
  // |subscription_id| matches the stored one, then passes the encryption
  // information to the callback. |sender_id| is also required since an
  // InstanceID might have multiple tokens associated with different senders,
  // though in practice Push doesn't yet use that.
  virtual void GetSubscriptionInfo(const GURL& origin,
                                   int64_t service_worker_registration_id,
                                   const std::string& sender_id,
                                   const std::string& subscription_id,
                                   SubscriptionInfoCallback callback) = 0;

  // Unsubscribe the given |sender_id| from the push messaging service. Locally
  // deactivates the subscription, then runs |callback|, then asynchronously
  // attempts to unsubscribe with the push service.
  virtual void Unsubscribe(blink::mojom::PushUnregistrationReason reason,
                           const GURL& requesting_origin,
                           int64_t service_worker_registration_id,
                           const std::string& sender_id,
                           UnregisterCallback callback) = 0;

  // Returns whether subscriptions that do not mandate user visible UI upon
  // receiving a push message are supported. Influences permission request and
  // permission check behaviour.
  virtual bool SupportNonVisibleMessages() = 0;

  // Unsubscribes the push subscription associated with this service worker
  // registration, if such a push subscription exists.
  virtual void DidDeleteServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id) = 0;

  // Unsubscribes all existing push subscriptions because the Service Worker
  // database has been deleted.
  virtual void DidDeleteServiceWorkerDatabase() = 0;

 protected:
  static void GetSenderId(BrowserContext* browser_context,
                          const GURL& origin,
                          int64_t service_worker_registration_id,
                          SenderIdCallback callback);

  // Get |sender_id| and |subscription_id| from Service Worker database. Can be
  // empty if no data found in the database.
  static void GetSWData(BrowserContext* browser_context,
                        const GURL& origin,
                        int64_t service_worker_registration_id,
                        SWDataCallback callback);

  // Clear the push subscription id stored in the service worker with the given
  // |service_worker_registration_id| for the given |origin|.
  static void ClearPushSubscriptionId(BrowserContext* browser_context,
                                      const GURL& origin,
                                      int64_t service_worker_registration_id,
                                      base::OnceClosure callback);

  // Update |subscription_id| stored in Service Worker database.
  static void UpdatePushSubscriptionId(BrowserContext* browser_context,
                                       const GURL& origin,
                                       int64_t service_worker_registration_id,
                                       const std::string& subscription_id,
                                       base::OnceClosure callback);

  // Stores a push subscription in the service worker for the given |origin|.
  // Must only be used by tests.
  static void StorePushSubscriptionForTesting(
      BrowserContext* browser_context,
      const GURL& origin,
      int64_t service_worker_registration_id,
      const std::string& subscription_id,
      const std::string& sender_id,
      base::OnceClosure callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_
