// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_EVENT_DISPATCHER_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_EVENT_DISPATCHER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class BrowserContext;
enum class PersistentNotificationStatus;

// This is the dispatcher to be used for firing events related to notifications.
// This class is a singleton, the instance of which can be retrieved using the
// static GetInstance() method. All methods must be called on the UI thread.
class CONTENT_EXPORT NotificationEventDispatcher {
 public:
  static NotificationEventDispatcher* GetInstance();

  using NotificationClickEventCallback = base::OnceCallback<void(bool)>;
  using NotificationDispatchCompleteCallback =
      base::OnceCallback<void(PersistentNotificationStatus)>;

  // Dispatch methods for persistent (SW backed) notifications.
  // TODO(miguelg) consider merging them with the non persistent ones below.

  // Dispatches the "notificationclick" event on the Service Worker associated
  // with |notification_id| belonging to |origin|. The |callback| will be
  // invoked when it's known whether the event successfully executed.
  virtual void DispatchNotificationClickEvent(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      const std::optional<int>& action_index,
      const std::optional<std::u16string>& reply,
      NotificationDispatchCompleteCallback dispatch_complete_callback) = 0;

  // Dispatches the "notificationclose" event on the Service Worker associated
  // with |notification_id| belonging to |origin|. The
  // |dispatch_complete_callback| will be invoked when it's known whether the
  // event successfully executed.
  virtual void DispatchNotificationCloseEvent(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      bool by_user,
      NotificationDispatchCompleteCallback dispatch_complete_callback) = 0;

  // Dispatch methods for the different non persistent (not backed by a service
  // worker) notification events.
  virtual void DispatchNonPersistentShowEvent(
      const std::string& notification_id) = 0;
  virtual void DispatchNonPersistentClickEvent(
      const std::string& notification_id,
      NotificationClickEventCallback callback) = 0;
  virtual void DispatchNonPersistentCloseEvent(
      const std::string& notification_id,
      base::OnceClosure completed_closure) = 0;

 protected:
  virtual ~NotificationEventDispatcher() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_EVENT_DISPATCHER_H_
