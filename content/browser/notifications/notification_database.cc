// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/notifications/notification_database_conversions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"

// Notification LevelDB database schema (in alphabetized order)
// =======================
//
// key: "DATA:" <origin identifier> '\x00' <notification_id>
// value: String containing the NotificationDatabaseDataProto protocol buffer
//        in serialized form.
//
// key: "RESOURCES:" <origin identifier> '\x00' <notification_id>
// value: String containing the NotificationDatabaseResourcesProto protocol
//        buffer in serialized form.

namespace content {
namespace {

// Keys of the fields defined in the database.
const char kDataKeyPrefix[] = "DATA:";
const char kResourcesKeyPrefix[] = "RESOURCES:";

// Separates the components of compound keys.
const char kNotificationKeySeparator = '\x00';

// Converts the LevelDB |status| to one of the notification database's values.
NotificationDatabase::Status LevelDBStatusToNotificationDatabaseStatus(
    const leveldb::Status& status) {
  if (status.ok())
    return NotificationDatabase::STATUS_OK;
  else if (status.IsNotFound())
    return NotificationDatabase::STATUS_ERROR_NOT_FOUND;
  else if (status.IsCorruption())
    return NotificationDatabase::STATUS_ERROR_CORRUPTED;
  else if (status.IsIOError())
    return NotificationDatabase::STATUS_IO_ERROR;
  else if (status.IsNotSupportedError())
    return NotificationDatabase::STATUS_NOT_SUPPORTED;
  else if (status.IsInvalidArgument())
    return NotificationDatabase::STATUS_INVALID_ARGUMENT;

  return NotificationDatabase::STATUS_ERROR_FAILED;
}

// Creates a prefix for the data entries based on |origin|.
std::string CreateDataPrefix(const GURL& origin) {
  if (!origin.is_valid())
    return kDataKeyPrefix;

  return base::StringPrintf("%s%s%c", kDataKeyPrefix,
                            storage::GetIdentifierFromOrigin(origin).c_str(),
                            kNotificationKeySeparator);
}

// Creates the compound data key in which notification data is stored.
std::string CreateDataKey(const GURL& origin,
                          const std::string& notification_id) {
  DCHECK(origin.is_valid());
  DCHECK(!notification_id.empty());

  return CreateDataPrefix(origin) + notification_id;
}

// Creates a prefix for the resources entries based on |origin|.
std::string CreateResourcesPrefix(const GURL& origin) {
  if (!origin.is_valid())
    return kResourcesKeyPrefix;

  return base::StringPrintf("%s%s%c", kResourcesKeyPrefix,
                            storage::GetIdentifierFromOrigin(origin).c_str(),
                            kNotificationKeySeparator);
}

// Creates the compound data key in which notification resources are stored.
std::string CreateResourcesKey(const GURL& origin,
                               const std::string& notification_id) {
  DCHECK(origin.is_valid());
  DCHECK(!notification_id.empty());

  return CreateResourcesPrefix(origin) + notification_id;
}

// Deserializes data in |serialized_data| to |notification_database_data|.
// Will return if the deserialization was successful.
NotificationDatabase::Status DeserializedNotificationData(
    const std::string& serialized_data,
    NotificationDatabaseData* notification_database_data) {
  DCHECK(notification_database_data);
  if (DeserializeNotificationDatabaseData(serialized_data,
                                          notification_database_data)) {
    return NotificationDatabase::STATUS_OK;
  }

  DLOG(ERROR) << "Unable to deserialize a notification's data.";
  return NotificationDatabase::STATUS_ERROR_CORRUPTED;
}

// Deserializes resources in |serialized_resources| to |notification_resources|.
// Will return if the deserialization was successful.
NotificationDatabase::Status DeserializedNotificationResources(
    const std::string& serialized_resources,
    blink::NotificationResources* notification_resources) {
  DCHECK(notification_resources);
  if (DeserializeNotificationDatabaseResources(serialized_resources,
                                               notification_resources)) {
    return NotificationDatabase::STATUS_OK;
  }

  DLOG(ERROR) << "Unable to deserialize a notification's resources.";
  return NotificationDatabase::STATUS_ERROR_CORRUPTED;
}

// Updates the time of the last click on the notification, and the first if
// necessary.
void UpdateNotificationTimestamps(NotificationDatabaseData* data) {
  base::TimeDelta delta = base::Time::Now() - data->creation_time_millis;
  if (!data->time_until_first_click_millis.has_value())
    data->time_until_first_click_millis = delta;
  data->time_until_last_click_millis = delta;
}

}  // namespace

NotificationDatabase::NotificationDatabase(const base::FilePath& path,
                                           UkmCallback callback)
    : path_(path), record_notification_to_ukm_callback_(std::move(callback)) {}

NotificationDatabase::~NotificationDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

NotificationDatabase::Status NotificationDatabase::Open(
    bool create_if_missing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::UNINITIALIZED, state_);

  if (!create_if_missing) {
    if (IsInMemoryDatabase() || !base::PathExists(path_) ||
        base::IsDirectoryEmpty(path_)) {
      return NotificationDatabase::STATUS_ERROR_NOT_FOUND;
    }
  }

  filter_policy_.reset(leveldb::NewBloomFilterPolicy(10));

  leveldb_env::Options options;
  options.create_if_missing = create_if_missing;
  options.paranoid_checks = true;
  options.filter_policy = filter_policy_.get();
  options.block_cache = leveldb_chrome::GetSharedWebBlockCache();
  if (IsInMemoryDatabase()) {
    env_ = leveldb_chrome::NewMemEnv("notification");
    options.env = env_.get();
  }

  Status status = LevelDBStatusToNotificationDatabaseStatus(
      leveldb_env::OpenDB(options, path_.AsUTF8Unsafe(), &db_));
  if (status == STATUS_OK)
    state_ = State::INITIALIZED;

  return status;
}

NotificationDatabase::Status NotificationDatabase::ReadNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    NotificationDatabaseData* notification_database_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::INITIALIZED, state_);
  DCHECK(!notification_id.empty());
  DCHECK(origin.is_valid());
  DCHECK(notification_database_data);

  std::string key = CreateDataKey(origin, notification_id);
  std::string serialized_data;

  Status status = LevelDBStatusToNotificationDatabaseStatus(
      db_->Get(leveldb::ReadOptions(), key, &serialized_data));
  if (status != STATUS_OK)
    return status;

  return DeserializedNotificationData(serialized_data,
                                      notification_database_data);
}

NotificationDatabase::Status NotificationDatabase::ReadNotificationResources(
    const std::string& notification_id,
    const GURL& origin,
    blink::NotificationResources* notification_resources) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::INITIALIZED, state_);
  DCHECK(!notification_id.empty());
  DCHECK(origin.is_valid());
  DCHECK(notification_resources);

  std::string key = CreateResourcesKey(origin, notification_id);
  std::string serialized_resources;

  Status status = LevelDBStatusToNotificationDatabaseStatus(
      db_->Get(leveldb::ReadOptions(), key, &serialized_resources));
  if (status != STATUS_OK)
    return status;

  return DeserializedNotificationResources(serialized_resources,
                                           notification_resources);
}

NotificationDatabase::Status
NotificationDatabase::ReadNotificationDataAndRecordInteraction(
    const std::string& notification_id,
    const GURL& origin,
    PlatformNotificationContext::Interaction interaction,
    NotificationDatabaseData* notification_database_data) {
  Status status =
      ReadNotificationData(notification_id, origin, notification_database_data);
  if (status != STATUS_OK)
    return status;

  // Update the appropriate fields for UKM logging purposes.
  switch (interaction) {
    case PlatformNotificationContext::Interaction::CLOSED:
      notification_database_data->closed_reason =
          NotificationDatabaseData::ClosedReason::USER;
      notification_database_data->time_until_close_millis =
          base::Time::Now() - notification_database_data->creation_time_millis;
      break;
    case PlatformNotificationContext::Interaction::NONE:
      break;
    case PlatformNotificationContext::Interaction::ACTION_BUTTON_CLICKED:
      notification_database_data->num_action_button_clicks += 1;
      UpdateNotificationTimestamps(notification_database_data);
      break;
    case PlatformNotificationContext::Interaction::CLICKED:
      notification_database_data->num_clicks += 1;
      UpdateNotificationTimestamps(notification_database_data);
      break;
  }

  return WriteNotificationData(origin, *notification_database_data);
}

NotificationDatabase::Status NotificationDatabase::ForEachNotificationData(
    ReadAllNotificationsCallback callback) const {
  return ForEachNotificationDataInternal(
      GURL() /* origin */, blink::mojom::kInvalidServiceWorkerRegistrationId,
      std::nullopt /* is_shown_by_browser */, std::move(callback));
}

NotificationDatabase::Status
NotificationDatabase::ForEachNotificationDataForServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id,
    ReadAllNotificationsCallback callback) const {
  return ForEachNotificationDataInternal(origin, service_worker_registration_id,
                                         std::nullopt /* is_shown_by_browser */,
                                         std::move(callback));
}

NotificationDatabase::Status
NotificationDatabase::ReadAllNotificationDataForOrigin(
    const GURL& origin,
    std::vector<NotificationDatabaseData>* notification_data_vector) const {
  return ReadAllNotificationDataInternal(
      origin, blink::mojom::kInvalidServiceWorkerRegistrationId,
      std::nullopt /* is_shown_by_browser */, notification_data_vector);
}

NotificationDatabase::Status
NotificationDatabase::ReadAllNotificationDataForServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id,
    std::optional<bool> is_shown_by_browser,
    std::vector<NotificationDatabaseData>* notification_data_vector) const {
  return ReadAllNotificationDataInternal(origin, service_worker_registration_id,
                                         is_shown_by_browser,
                                         notification_data_vector);
}

NotificationDatabase::Status NotificationDatabase::WriteNotificationData(
    const GURL& origin,
    const NotificationDatabaseData& notification_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::INITIALIZED, state_);
  DCHECK(origin.is_valid());

  const std::string& notification_id = notification_data.notification_id;
  DCHECK(!notification_id.empty());

  std::string serialized_data;
  if (!SerializeNotificationDatabaseData(notification_data, &serialized_data)) {
    DLOG(ERROR) << "Unable to serialize data for a notification belonging "
                << "to: " << origin;
    return STATUS_ERROR_FAILED;
  }

  leveldb::WriteBatch batch;
  batch.Put(CreateDataKey(origin, notification_id), serialized_data);

  if (notification_data.notification_resources.has_value()) {
    std::string serialized_resources;
    if (!SerializeNotificationDatabaseResources(
            notification_data.notification_resources.value(),
            &serialized_resources)) {
      DLOG(ERROR) << "Unable to serialize resources for a notification "
                  << "belonging to: " << origin;
      return STATUS_ERROR_FAILED;
    }

    batch.Put(CreateResourcesKey(origin, notification_id),
              serialized_resources);
  }

  return LevelDBStatusToNotificationDatabaseStatus(
      db_->Write(leveldb::WriteOptions(), &batch));
}

NotificationDatabase::Status NotificationDatabase::DeleteNotificationData(
    const std::string& notification_id,
    const GURL& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::INITIALIZED, state_);
  DCHECK(!notification_id.empty());
  DCHECK(origin.is_valid());

  NotificationDatabaseData data;
  Status status = ReadNotificationData(notification_id, origin, &data);
  if (status == STATUS_OK && record_notification_to_ukm_callback_) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(record_notification_to_ukm_callback_, data));
  }

  leveldb::WriteBatch batch;
  batch.Delete(CreateDataKey(origin, notification_id));
  batch.Delete(CreateResourcesKey(origin, notification_id));

  return LevelDBStatusToNotificationDatabaseStatus(
      db_->Write(leveldb::WriteOptions(), &batch));
}

NotificationDatabase::Status NotificationDatabase::DeleteNotificationResources(
    const std::string& notification_id,
    const GURL& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::INITIALIZED, state_);
  DCHECK(!notification_id.empty());
  DCHECK(origin.is_valid());

  std::string key = CreateResourcesKey(origin, notification_id);
  return LevelDBStatusToNotificationDatabaseStatus(
      db_->Delete(leveldb::WriteOptions(), key));
}

NotificationDatabase::Status
NotificationDatabase::DeleteAllNotificationDataForOrigin(
    const GURL& origin,
    const std::string& tag,
    std::optional<bool> is_shown_by_browser,
    std::set<std::string>* deleted_notification_ids) {
  return DeleteAllNotificationDataInternal(
      origin, tag, is_shown_by_browser,
      blink::mojom::kInvalidServiceWorkerRegistrationId,
      deleted_notification_ids);
}

NotificationDatabase::Status
NotificationDatabase::DeleteAllNotificationDataForServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id,
    std::set<std::string>* deleted_notification_ids) {
  return DeleteAllNotificationDataInternal(
      origin, "" /* tag */, std::nullopt /* is_shown_by_browser */,
      service_worker_registration_id, deleted_notification_ids);
}

NotificationDatabase::Status NotificationDatabase::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  leveldb_env::Options options;
  if (IsInMemoryDatabase()) {
    if (!env_)
      return STATUS_OK;  // The database has not been initialized.

    options.env = env_.get();
  }

  state_ = State::DISABLED;
  db_.reset();

  return LevelDBStatusToNotificationDatabaseStatus(
      leveldb::DestroyDB(path_.AsUTF8Unsafe(), options));
}

NotificationDatabase::Status
NotificationDatabase::ReadAllNotificationDataInternal(
    const GURL& origin,
    int64_t service_worker_registration_id,
    std::optional<bool> is_shown_by_browser,
    std::vector<NotificationDatabaseData>* notification_data_vector) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(notification_data_vector);

  return ForEachNotificationDataInternal(
      origin, service_worker_registration_id, is_shown_by_browser,
      base::BindRepeating(
          [](std::vector<NotificationDatabaseData>* datas,
             const NotificationDatabaseData& data) { datas->push_back(data); },
          notification_data_vector));
}

NotificationDatabase::Status
NotificationDatabase::ForEachNotificationDataInternal(
    const GURL& origin,
    int64_t service_worker_registration_id,
    std::optional<bool> is_shown_by_browser,
    ReadAllNotificationsCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string prefix = CreateDataPrefix(origin);

  leveldb::Slice prefix_slice(prefix);

  NotificationDatabaseData notification_database_data;
  std::unique_ptr<leveldb::Iterator> iter(
      db_->NewIterator(leveldb::ReadOptions()));
  for (iter->Seek(prefix_slice); iter->Valid(); iter->Next()) {
    if (!iter->key().starts_with(prefix_slice))
      break;

    Status status = DeserializedNotificationData(iter->value().ToString(),
                                                 &notification_database_data);
    if (status != STATUS_OK)
      return status;

    if (service_worker_registration_id !=
            blink::mojom::kInvalidServiceWorkerRegistrationId &&
        notification_database_data.service_worker_registration_id !=
            service_worker_registration_id) {
      continue;
    }

    if (is_shown_by_browser && notification_database_data.is_shown_by_browser !=
                                   *is_shown_by_browser) {
      continue;
    }

    callback.Run(notification_database_data);
  }

  return LevelDBStatusToNotificationDatabaseStatus(iter->status());
}

NotificationDatabase::Status
NotificationDatabase::DeleteAllNotificationDataInternal(
    const GURL& origin,
    const std::string& tag,
    std::optional<bool> is_shown_by_browser,
    int64_t service_worker_registration_id,
    std::set<std::string>* deleted_notification_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleted_notification_ids);
  DCHECK(origin.is_valid());

  const std::string prefix = CreateDataPrefix(origin);

  leveldb::Slice prefix_slice(prefix);
  leveldb::WriteBatch batch;
  bool did_delete = false;

  NotificationDatabaseData notification_database_data;
  std::unique_ptr<leveldb::Iterator> iter(
      db_->NewIterator(leveldb::ReadOptions()));
  for (iter->Seek(prefix_slice); iter->Valid(); iter->Next()) {
    if (!iter->key().starts_with(prefix_slice))
      break;

    Status status = DeserializedNotificationData(iter->value().ToString(),
                                                 &notification_database_data);
    if (status != STATUS_OK)
      return status;

    if (!tag.empty() &&
        notification_database_data.notification_data.tag != tag) {
      continue;
    }

    if (is_shown_by_browser && notification_database_data.is_shown_by_browser !=
                                   *is_shown_by_browser) {
      continue;
    }

    if (service_worker_registration_id !=
            blink::mojom::kInvalidServiceWorkerRegistrationId &&
        notification_database_data.service_worker_registration_id !=
            service_worker_registration_id) {
      continue;
    }

    if (record_notification_to_ukm_callback_) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(record_notification_to_ukm_callback_,
                                    notification_database_data));
    }

    std::string notification_id = notification_database_data.notification_id;
    DCHECK(!notification_id.empty());

    batch.Delete(iter->key());
    batch.Delete(CreateResourcesKey(origin, notification_id));
    did_delete = true;

    deleted_notification_ids->insert(notification_id);
  }

  if (!did_delete)
    return STATUS_OK;

  return LevelDBStatusToNotificationDatabaseStatus(
      db_->Write(leveldb::WriteOptions(), &batch));
}

}  // namespace content
