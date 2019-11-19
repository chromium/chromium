// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_storage.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/notifications/notification_database_conversions.h"

namespace content {

namespace {

// Schema is as follows:
// KEY: NOTIFICATION_<|data.notification_id|>
// VALUE: <serialized content::proto::NotificationDatabaseData>

const char kNotificationPrefix[] = "NOTIFICATION_";

// Create the key that will be used for the service worker database.
std::string CreateDataKey(const std::string& notification_id) {
  DCHECK(!notification_id.empty());
  return kNotificationPrefix + notification_id;
}

// Updates the time of the last click on the notification, and the first if
// necessary.
void UpdateNotificationClickTimestamps(NotificationDatabaseData* data) {
  base::TimeDelta delta = base::Time::Now() - data->creation_time_millis;
  if (!data->time_until_first_click_millis.has_value())
    data->time_until_first_click_millis = delta;
  data->time_until_last_click_millis = delta;
}

}  // namespace

NotificationStorage::NotificationStorage(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(std::move(service_worker_context)) {}

NotificationStorage::~NotificationStorage() = default;

void NotificationStorage::WriteNotificationData(
    const NotificationDatabaseData& data,
    PlatformNotificationContext::WriteResultCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::string serialized_data;
  if (!SerializeNotificationDatabaseData(data, &serialized_data)) {
    DLOG(ERROR) << "Unable to serialize data for a notification belonging "
                << "to: " << data.origin;
    std::move(callback).Run(/* success= */ false, std::string());
    return;
  }

  service_worker_context_->StoreRegistrationUserData(
      data.service_worker_registration_id, data.origin,
      {{CreateDataKey(data.notification_id), std::move(serialized_data)}},
      base::BindOnce(&NotificationStorage::OnWriteComplete,
                     weak_ptr_factory_.GetWeakPtr(), data,
                     std::move(callback)));
}

void NotificationStorage::OnWriteComplete(
    const NotificationDatabaseData& data,
    PlatformNotificationContext::WriteResultCallback callback,
    blink::ServiceWorkerStatusCode status) {
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(/* success= */ true, data.notification_id);
  } else {
    std::move(callback).Run(/* success= */ false,
                            /* notification_id= */ std::string());
  }
}

void NotificationStorage::ReadNotificationDataAndRecordInteraction(
    int64_t service_worker_registration_id,
    const std::string& notification_id,
    PlatformNotificationContext::Interaction interaction,
    PlatformNotificationContext::ReadResultCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id, {CreateDataKey(notification_id)},
      base::BindOnce(&NotificationStorage::OnReadCompleteUpdateInteraction,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker_registration_id, interaction,
                     std::move(callback)));
}

void NotificationStorage::OnReadCompleteUpdateInteraction(
    int64_t service_worker_registration_id,
    PlatformNotificationContext::Interaction interaction,
    PlatformNotificationContext::ReadResultCallback callback,
    const std::vector<std::string>& database_data,
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk || database_data.empty()) {
    std::move(callback).Run(/* success= */ false, NotificationDatabaseData());
    return;
  }

  auto data = std::make_unique<NotificationDatabaseData>();
  if (!DeserializeNotificationDatabaseData(database_data[0], data.get())) {
    DLOG(ERROR) << "Unable to deserialize data for a notification belonging "
                << "to: " << data->origin;
    std::move(callback).Run(/* success= */ false, NotificationDatabaseData());
    return;
  }

  switch (interaction) {
    case PlatformNotificationContext::Interaction::CLOSED:
      data->time_until_close_millis =
          base::Time::Now() - data->creation_time_millis;
      break;
    case PlatformNotificationContext::Interaction::NONE:
      break;
    case PlatformNotificationContext::Interaction::ACTION_BUTTON_CLICKED:
      data->num_action_button_clicks += 1;
      UpdateNotificationClickTimestamps(data.get());
      break;
    case PlatformNotificationContext::Interaction::CLICKED:
      data->num_clicks += 1;
      UpdateNotificationClickTimestamps(data.get());
      break;
  }
  std::string serialized_data;
  if (!SerializeNotificationDatabaseData(*data, &serialized_data)) {
    DLOG(ERROR) << "Unable to serialize data for a notification belonging "
                << "to: " << data->origin;
    std::move(callback).Run(/* success= */ false, NotificationDatabaseData());
    return;
  }

  GURL origin = data->origin;
  std::string notification_id = data->notification_id;
  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id, origin,
      {{CreateDataKey(notification_id), std::move(serialized_data)}},
      base::BindOnce(&NotificationStorage::OnInteractionUpdateComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(data),
                     std::move(callback)));
}

void NotificationStorage::OnInteractionUpdateComplete(
    std::unique_ptr<NotificationDatabaseData> data,
    PlatformNotificationContext::ReadResultCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK(data);
  if (status == blink::ServiceWorkerStatusCode::kOk)
    std::move(callback).Run(/* success= */ true, *data);
  else
    std::move(callback).Run(/* success= */ false, NotificationDatabaseData());
}

}  // namespace content
