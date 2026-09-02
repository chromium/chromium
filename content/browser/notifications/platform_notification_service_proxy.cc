// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_service_proxy.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "content/browser/notifications/devtools_event_logging.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

PlatformNotificationServiceProxy::PlatformNotificationServiceProxy(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    BrowserContext* browser_context)
    : service_worker_context_(service_worker_context),
      browser_context_(browser_context),
      notification_service_(browser_context->GetPlatformNotificationService()) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
}

PlatformNotificationServiceProxy::~PlatformNotificationServiceProxy() = default;

base::WeakPtr<PlatformNotificationServiceProxy>
PlatformNotificationServiceProxy::AsWeakPtr() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  return weak_ptr_factory_.GetWeakPtr();
}

void PlatformNotificationServiceProxy::DoDisplayNotification(
    const NotificationDatabaseData& data,
    const GURL& service_worker_scope,
    DisplayResultCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (notification_service_) {
    notification_service_->DisplayPersistentNotification(
        data.notification_id, service_worker_scope, data.origin,
        data.notification_data,
        data.notification_resources.value_or(blink::NotificationResources()));
    notifications::LogNotificationDisplayedEventToDevTools(browser_context_,
                                                           data);
  }
  std::move(callback).Run(/* success= */ true, data.notification_id);
}

void PlatformNotificationServiceProxy::VerifyServiceWorkerScope(
    const NotificationDatabaseData& data,
    DisplayResultCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  base::OnceClosure task;

  if (status == blink::ServiceWorkerStatusCode::kOk &&
      registration->key().origin().GetURL() == data.origin) {
    DoDisplayNotification(data, registration->scope(), std::move(callback));
  } else {
    std::move(callback).Run(/* success= */ false, /* notification_id= */ "");
  }
}

void PlatformNotificationServiceProxy::DisplayNotification(
    const NotificationDatabaseData& data,
    DisplayResultCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (!service_worker_context_) {
    DoDisplayNotification(data, GURL(), std::move(callback));
    return;
  }

  service_worker_context_->FindReadyRegistrationForId(
      data.service_worker_registration_id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(data.origin)),
      base::BindOnce(
          &PlatformNotificationServiceProxy::VerifyServiceWorkerScope,
          weak_ptr_factory_.GetWeakPtr(), data, std::move(callback)));
}

void PlatformNotificationServiceProxy::CloseNotifications(
    const std::set<std::string>& notification_ids) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (!notification_service_)
    return;
  for (const std::string& notification_id : notification_ids)
    notification_service_->ClosePersistentNotification(notification_id);
}

void PlatformNotificationServiceProxy::ScheduleTrigger(base::Time timestamp) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (!notification_service_)
    return;
  notification_service_->ScheduleTrigger(timestamp);
}

void PlatformNotificationServiceProxy::ScheduleNotification(
    const NotificationDatabaseData& data) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  CHECK(data.notification_data.show_trigger_timestamp.has_value(),
        base::NotFatalUntil::M159);
  if (!notification_service_) {
    return;
  }
  base::Time show_trigger_timestamp =
      data.notification_data.show_trigger_timestamp.value();
  notifications::LogNotificationScheduledEventToDevTools(
      browser_context_, data, show_trigger_timestamp);
  notification_service_->ScheduleTrigger(show_trigger_timestamp);
}

base::Time PlatformNotificationServiceProxy::GetNextTrigger() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (!notification_service_)
    return base::Time::Max();
  return notification_service_->ReadNextTriggerTimestamp();
}

void PlatformNotificationServiceProxy::RecordNotificationUkmEvent(
    const NotificationDatabaseData& data) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (!notification_service_)
    return;
  notification_service_->RecordNotificationUkmEvent(data);
}

bool PlatformNotificationServiceProxy::ShouldLogClose(const GURL& origin) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  return notifications::ShouldLogNotificationEventToDevTools(browser_context_,
                                                             origin);
}

void PlatformNotificationServiceProxy::LogClose(
    const NotificationDatabaseData& data) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  notifications::LogNotificationClosedEventToDevTools(browser_context_, data);
}

}  // namespace content
