// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/devtools_event_logging.h"

#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {
namespace notifications {

namespace {

using EventMetadata = std::map<std::string, std::string>;
using DevToolsBaseCallback =
    base::OnceCallback<void(const std::string& event_name,
                            const std::string& instance_id,
                            const EventMetadata& event_metadata)>;
using DevToolsCallback =
    base::OnceCallback<void(const std::string& event_name,
                            const EventMetadata& event_metadata)>;

DevToolsBackgroundServicesContext* GetDevToolsContext(
    BrowserContext* browser_context,
    const GURL& origin) {
  auto* storage_partition = browser_context->GetStoragePartitionForUrl(origin);
  if (!storage_partition)
    return nullptr;

  auto* devtools_context =
      storage_partition->GetDevToolsBackgroundServicesContext();
  if (!devtools_context || !devtools_context->IsRecording(
                               DevToolsBackgroundService::kNotifications)) {
    return nullptr;
  }

  return devtools_context;
}

DevToolsCallback GetDevToolsCallback(BrowserContext* browser_context,
                                     const NotificationDatabaseData& data) {
  if (data.service_worker_registration_id ==
      blink::mojom::kInvalidServiceWorkerRegistrationId) {
    return DevToolsCallback();
  }

  auto* devtools_context = GetDevToolsContext(browser_context, data.origin);
  if (!devtools_context)
    return DevToolsCallback();

  url::Origin origin = url::Origin::Create(data.origin);

  // Passing the |devtools_context| as base::Unretained is safe as the callback
  // is executed synchronously.
  auto base_callback = base::BindOnce(
      &DevToolsBackgroundServicesContext::LogBackgroundServiceEvent,
      base::Unretained(devtools_context), data.service_worker_registration_id,
      blink::StorageKey::CreateFirstParty(origin),
      DevToolsBackgroundService::kNotifications);

  return base::BindOnce(
      [](DevToolsBaseCallback callback, const std::string& notification_id,
         const std::string& event_name, const EventMetadata& metadata) {
        std::move(callback).Run(event_name, notification_id, metadata);
      },
      std::move(base_callback), data.notification_data.tag);
}

}  // namespace

bool ShouldLogNotificationEventToDevTools(BrowserContext* browser_context,
                                          const GURL& origin) {
  return GetDevToolsContext(browser_context, origin) != nullptr;
}

void LogNotificationDisplayedEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data) {
  DevToolsCallback callback = GetDevToolsCallback(browser_context, data);
  if (!callback)
    return;

  std::move(callback).Run(
      /* event_name= */ "Notification displayed",
      {{"Title", base::UTF16ToUTF8(data.notification_data.title)},
       {"Body", base::UTF16ToUTF8(data.notification_data.body)}});
}

void LogNotificationClosedEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data) {
  DevToolsCallback callback = GetDevToolsCallback(browser_context, data);
  if (!callback)
    return;

  std::move(callback).Run(/* event_name= */ "Notification closed",
                          /* event_metadata= */ {});
}

void LogNotificationScheduledEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data,
    base::Time show_trigger_timestamp) {
  DevToolsCallback callback = GetDevToolsCallback(browser_context, data);
  if (!callback)
    return;

  std::move(callback).Run(
      /* event_name= */ "Notification scheduled",
      {{"Show Trigger Timestamp",
        base::TimeFormatAsIso8601(show_trigger_timestamp)},
       {"Title", base::UTF16ToUTF8(data.notification_data.title)},
       {"Body", base::UTF16ToUTF8(data.notification_data.body)}});
}

void LogNotificationClickedEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply) {
  DevToolsCallback callback = GetDevToolsCallback(browser_context, data);
  if (!callback)
    return;

  EventMetadata event_metadata;
  if (action_index)
    event_metadata["Action Index"] = base::NumberToString(*action_index);
  if (reply)
    event_metadata["Reply"] = base::UTF16ToUTF8(*reply);

  std::move(callback).Run(/* event_name= */ "Notification clicked",
                          event_metadata);
}

}  // namespace notifications
}  // namespace content
