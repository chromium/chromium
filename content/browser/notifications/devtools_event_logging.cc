// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/devtools_event_logging.h"

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time_to_iso8601.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/storage_partition.h"
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
  auto* storage_partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);
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

  // Passing the |devtools_context| as base::Unretained is safe as the callback
  // is executed synchronously.
  auto base_callback = base::BindOnce(
      &DevToolsBackgroundServicesContext::LogBackgroundServiceEvent,
      base::Unretained(devtools_context), data.service_worker_registration_id,
      url::Origin::Create(data.origin),
      DevToolsBackgroundService::kNotifications);

  // TODO(knollr): Reorder parameters of LogBackgroundServiceEvent instead.
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
      {{"Show Trigger Timestamp", base::TimeToISO8601(show_trigger_timestamp)},
       {"Title", base::UTF16ToUTF8(data.notification_data.title)},
       {"Body", base::UTF16ToUTF8(data.notification_data.body)}});
}

void LogNotificationClickedEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
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
