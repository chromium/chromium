// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_VIRTUAL_PLATFORM_NOTIFICATION_SERVICE_H_
#define CONTENT_BROWSER_NOTIFICATIONS_VIRTUAL_PLATFORM_NOTIFICATION_SERVICE_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/persistent_notification_status.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;

// Keeps notifications in memory instead of showing them, and lets automation
// observe and interact with them. Exists for a browser context while at least
// one observer is attached. Must be used on the UI thread.
//
// Simulated clicks mimic how PersistentNotificationHandler and
// NonPersistentNotificationHandler handle real ones, except that a click on a
// non-persistent notification without a listener does not open a tab.
//
// Notifications that the embedder showed before this service existed stay with
// the embedder, and closing them is forwarded to it. A tagged non-persistent
// notification that replaces one of them leaves that one shown, as closing it
// could send a close event to the new one.
//
// A persistent notification is displayed after it is written to the database,
// so one that a page shows just before Attach() or the last Detach() may be
// displayed by the embedder instead of this service, or the other way round.
// TODO(crbug.com/576760752): Fix this race by picking the service when the
// notification is written, or hand off notifications on Attach()/Detach().
class CONTENT_EXPORT VirtualPlatformNotificationService
    : public PlatformNotificationService,
      public base::SupportsUserData::Data {
 public:
  struct Notification {
    Notification();
    Notification(const Notification&);
    Notification& operator=(const Notification&);
    Notification(Notification&&);
    Notification& operator=(Notification&&);
    ~Notification();

    std::string id;
    GURL origin;
    bool is_persistent = false;
    // Empty for non-persistent notifications.
    GURL service_worker_scope;
    blink::PlatformNotificationData data;
  };

  enum class CloseReason {
    // Closed through SimulateClose().
    kClient,
    // Closed by the page, the service worker or the browser.
    kOther,
  };

  // Observers must not re-enter the service by calling Detach(), showing or
  // closing notifications from their callbacks.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a notification is shown, including when it replaces one with
    // the same ID, which is not reported as closed. `notification` is valid
    // only during the call.
    virtual void OnNotificationShown(
        VirtualPlatformNotificationService* service,
        const Notification& notification) = 0;
    // Called when the notification with `notification_id` is closed. Not
    // called for the notifications that remain when the service is destroyed.
    virtual void OnNotificationClosed(
        VirtualPlatformNotificationService* service,
        const std::string& notification_id,
        CloseReason reason) = 0;
    // Called when the browser context is destroyed, but not when the last
    // observer is detached. The service must not be used afterwards, not even
    // to call Detach().
    virtual void OnServiceDestroyed(
        VirtualPlatformNotificationService* service) = 0;
  };

  VirtualPlatformNotificationService(
      const VirtualPlatformNotificationService&) = delete;
  VirtualPlatformNotificationService& operator=(
      const VirtualPlatformNotificationService&) = delete;
  ~VirtualPlatformNotificationService() override;

  // Returns the service for `browser_context`, or null if there is none.
  static VirtualPlatformNotificationService* FromBrowserContext(
      BrowserContext* browser_context);

  // Returns the service for `browser_context`, creating it if needed, and
  // attaches `observer` to it. `observer` must not be attached already.
  static VirtualPlatformNotificationService* Attach(
      BrowserContext* browser_context,
      Observer* observer);

  // Detaches `observer`. When the last observer is gone, closes the remaining
  // notifications like the platform would, without close events for service
  // workers, and destroys the service.
  void Detach(Observer* observer);

  BrowserContext* browser_context() const { return browser_context_; }
  const std::map<std::string, Notification>& notifications() const {
    return notifications_;
  }

  // Simulates the user activating the notification, or the action at
  // `action_index` of it. Closes the notification if the click cannot be
  // delivered. Does nothing if there is no such notification. `action_index`
  // must be empty for non-persistent notifications, and a valid index into the
  // actions otherwise.
  void SimulateClick(const std::string& notification_id,
                     std::optional<int> action_index);

  // Simulates the user dismissing the notification. Does nothing if there is
  // no such notification.
  void SimulateClose(const std::string& notification_id);

  // PlatformNotificationService:
  void DisplayNotification(
      const std::string& notification_id,
      const GURL& origin,
      const GURL& document_url,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources) override;
  void DisplayPersistentNotification(
      const std::string& notification_id,
      const GURL& service_worker_scope,
      const GURL& origin,
      const blink::PlatformNotificationData& notification_data,
      const blink::NotificationResources& notification_resources) override;
  void CloseNotification(const std::string& notification_id) override;
  void ClosePersistentNotification(const std::string& notification_id) override;
  void GetDisplayedNotifications(
      DisplayedNotificationsCallback callback) override;
  void GetDisplayedNotificationsForOrigin(
      const GURL& origin,
      DisplayedNotificationsCallback callback) override;
  void ScheduleTrigger(base::Time timestamp) override;
  base::Time ReadNextTriggerTimestamp() override;
  int64_t ReadNextPersistentNotificationId() override;
  void RecordNotificationUkmEvent(
      const NotificationDatabaseData& data) override;

 private:
  explicit VirtualPlatformNotificationService(BrowserContext* browser_context);

  void AddNotification(Notification notification);
  void RemoveNotification(const std::string& notification_id,
                          CloseReason reason);
  // Replies with `notification_ids`, allowing synchronization only if there is
  // no embedder service.
  void ReplyWithDisplayedNotifications(std::set<std::string> notification_ids,
                                       DisplayedNotificationsCallback callback);
  void OnPersistentClickDispatched(const std::string& notification_id,
                                   PersistentNotificationStatus status);
  void OnNonPersistentClickDispatched(const std::string& notification_id,
                                      bool success);

  raw_ptr<BrowserContext> browser_context_;
  std::map<std::string, Notification> notifications_;
  base::ObserverList<Observer> observers_;
  // Whether observers are being notified.
  bool notifying_ = false;
  base::WeakPtrFactory<VirtualPlatformNotificationService> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_VIRTUAL_PLATFORM_NOTIFICATION_SERVICE_H_
