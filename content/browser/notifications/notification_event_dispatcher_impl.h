// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_

#include <map>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/types/optional_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"

namespace content {

class CONTENT_EXPORT NotificationEventDispatcherImpl
    : public NotificationEventDispatcher {
 public:
  // Returns the instance of the NotificationEventDispatcherImpl. Must be called
  // on the UI thread.
  static NotificationEventDispatcherImpl* GetInstance();

  NotificationEventDispatcherImpl(const NotificationEventDispatcherImpl&) =
      delete;
  NotificationEventDispatcherImpl& operator=(
      const NotificationEventDispatcherImpl&) = delete;

  // NotificationEventDispatcher implementation.
  void DispatchNotificationClickEvent(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      const std::optional<int>& action_index,
      const std::optional<std::u16string>& reply,
      NotificationDispatchCompleteCallback dispatch_complete_callback) override;
  void DispatchNotificationCloseEvent(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      bool by_user,
      NotificationDispatchCompleteCallback dispatch_complete_callback) override;
  void DispatchNonPersistentShowEvent(
      const std::string& notification_id) override;
  void DispatchNonPersistentClickEvent(
      const std::string& notification_id,
      NotificationClickEventCallback callback) override;
  void DispatchNonPersistentCloseEvent(
      const std::string& notification_id,
      base::OnceClosure completed_closure) override;

  // Registers the associated document weak pointer and the listener to receive
  // the show, click and close events of the non-persistent notification
  // identified by `notification_id`. For more information about the
  // `event_document_ptr`, see the comments of `weak_document_ptr_` property in
  // `BlinkNotificationServiceImpl`.
  void RegisterNonPersistentNotificationListener(
      const std::string& notification_id,
      mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener>
          event_listener_remote,
      const WeakDocumentPtr& event_document_ptr,
      RenderProcessHost::NotificationServiceCreatorType creator_type);

 private:
  struct NonPersistentNotificationListenerInfo {
    NonPersistentNotificationListenerInfo(
        mojo::Remote<blink::mojom::NonPersistentNotificationListener> remote,
        WeakDocumentPtr document,
        RenderProcessHost::NotificationServiceCreatorType creator_type);
    NonPersistentNotificationListenerInfo(
        NonPersistentNotificationListenerInfo&&);
    NonPersistentNotificationListenerInfo(
        const NonPersistentNotificationListenerInfo&) = delete;
    NonPersistentNotificationListenerInfo& operator=(
        const NonPersistentNotificationListenerInfo&) = delete;
    ~NonPersistentNotificationListenerInfo();

    mojo::Remote<blink::mojom::NonPersistentNotificationListener> remote;
    // This is used to determine if the associated document that registers the
    // listeners is in back/forward cache when the event is dispatched.
    // See weak_document_ptr_ in BlinkNotificationServiceImpl.
    WeakDocumentPtr document;
    // This is used to determine if the notification service is created by
    // document, shared worker, dedicated worker or service worker.
    RenderProcessHost::NotificationServiceCreatorType creator_type;
  };

  friend class NotificationEventDispatcherImplTest;
  friend struct base::DefaultSingletonTraits<NotificationEventDispatcherImpl>;

  NotificationEventDispatcherImpl();
  ~NotificationEventDispatcherImpl() override;

  // Gets the event listener for the `notification_id` if it should be fired.
  // It returns std::nullopt if:
  // 1. The event listener is not found from the map, or
  // 2. The document is currently in back/forward cache.
  base::optional_ref<NonPersistentNotificationListenerInfo>
  GetListenerIfNotifiable(const std::string& notification_id);

  // Removes all references to the listener registered to receive events
  // from the non-persistent notification identified by |notification_id|,
  // and executes |completed_closure|. This method is called after OnClose has
  // been dispatched to the non-persistent notification listener.
  void OnNonPersistentCloseComplete(const std::string& notification_id,
                                    base::OnceClosure completed_closure);

  // Removes all references to the listener registered to receive events
  // from the non-persistent notification identified by |notification_id|.
  // Should be called when the connection to this listener goes away.
  void HandleConnectionErrorForNonPersistentNotificationListener(
      const std::string& notification_id);

  // Mapping between the notification id and the event listener.
  std::map<std::string, NonPersistentNotificationListenerInfo>
      non_persistent_notification_listeners_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_
