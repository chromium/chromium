// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_PROXY_H_
#define CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_PROXY_H_

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class GURL;

namespace base {
class Time;
}

namespace blink {
enum class ServiceWorkerStatusCode;
}  // namespace blink

namespace content {

class BrowserContext;
struct NotificationDatabaseData;
class PlatformNotificationService;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;

class PlatformNotificationServiceProxy {
 public:
  using DisplayResultCallback =
      base::OnceCallback<void(bool /* success */,
                              const std::string& /* notification_id */)>;

  PlatformNotificationServiceProxy(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      BrowserContext* browser_context);

  PlatformNotificationServiceProxy(const PlatformNotificationServiceProxy&) =
      delete;
  PlatformNotificationServiceProxy& operator=(
      const PlatformNotificationServiceProxy&) = delete;

  ~PlatformNotificationServiceProxy();

  // Gets a weak pointer to be used on the UI thread.
  base::WeakPtr<PlatformNotificationServiceProxy> AsWeakPtr();

  // Displays a notification with |data| and calls |callback| with the result.
  // This will verify against the given |service_worker_context_| if available.
  void DisplayNotification(const NotificationDatabaseData& data,
                           DisplayResultCallback callback);

  // Closes the notifications with |notification_ids|.
  void CloseNotifications(const std::set<std::string>& notification_ids);

  // Schedules a notification trigger for |timestamp|.
  void ScheduleTrigger(base::Time timestamp);

  // Schedules a notification with |data|.
  void ScheduleNotification(const NotificationDatabaseData& data);

  // Gets the next notification trigger or base::Time::Max if none set. Must be
  // called on the UI thread.
  base::Time GetNextTrigger();

  // Records a given notification to UKM. Must be called on the UI thread.
  void RecordNotificationUkmEvent(const NotificationDatabaseData& data);

  // Returns if we should log a notification close event by calling LogClose.
  // Must be called on the UI thread.
  bool ShouldLogClose(const GURL& origin);

  // Logs the event of closing a notification.
  void LogClose(const NotificationDatabaseData& data);

 private:
  // Actually calls |notification_service_| to display the notification after
  // verifying the |service_worker_scope|. Must be called on the UI thread.
  void DoDisplayNotification(const NotificationDatabaseData& data,
                             const GURL& service_worker_scope,
                             DisplayResultCallback callback);

  // Verifies that the service worker exists and is valid for the given
  // notification origin.
  void VerifyServiceWorkerScope(
      const NotificationDatabaseData& data,
      DisplayResultCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  raw_ptr<BrowserContext> browser_context_;
  raw_ptr<PlatformNotificationService> notification_service_;
  base::WeakPtrFactory<PlatformNotificationServiceProxy> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_PROXY_H_
