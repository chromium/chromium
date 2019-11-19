// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/public/browser/platform_notification_context.h"

class GURL;

namespace blink {
struct NotificationResources;
}  // namespace blink

namespace leveldb {
class DB;
class Env;
class FilterPolicy;
}  // namespace leveldb

namespace content {

struct NotificationDatabaseData;

// Implementation of the persistent notification database.
//
// The database is built on top of a LevelDB database, either in memory or on
// the filesystem depending on the path passed to the constructor. When writing
// a new notification, it will be assigned an id guaranteed to be unique for the
// lifetime of the database.
//
// This class must only be used on a thread or sequenced task runner that allows
// file I/O. The same thread or task runner must be used for all method calls.
class CONTENT_EXPORT NotificationDatabase {
 public:
  using UkmCallback =
      base::RepeatingCallback<void(const NotificationDatabaseData&)>;
  using ReadAllNotificationsCallback =
      base::RepeatingCallback<void(const NotificationDatabaseData&)>;

  // Result status codes for interactions with the database. Will be used for
  // UMA, so the assigned ids must remain stable.
  enum Status {
    STATUS_OK = 0,

    // The database, a notification, or a LevelDB key associated with the
    // operation could not be found.
    STATUS_ERROR_NOT_FOUND = 1,

    // The database, or data in the database, could not be parsed as valid data.
    STATUS_ERROR_CORRUPTED = 2,

    // General failure code. More specific failures should be used if available.
    STATUS_ERROR_FAILED = 3,

    // leveldb failed due to I/O error (read-only, full disk, etc.).
    STATUS_IO_ERROR = 4,

    // leveldb operation not supported
    STATUS_NOT_SUPPORTED = 5,

    // Invalid database ID or snapshot ID provided.
    STATUS_INVALID_ARGUMENT = 6,

    // Number of entries in the status enumeration. Used by UMA. Must always be
    // one higher than the otherwise highest value in this enumeration.
    STATUS_COUNT = 7
  };

  NotificationDatabase(const base::FilePath& path, UkmCallback callback);

  ~NotificationDatabase();

  // Opens the database. If |path| is non-empty, it will be created on the given
  // directory on the filesystem. If |path| is empty, the database will be
  // created in memory instead, and its lifetime will be tied to this instance.
  // |create_if_missing| determines whether to create the database if necessary.
  Status Open(bool create_if_missing);

  // Gets the next assignable persistent notification ID. Subsequent calls to
  // this method will yield unique identifiers for the same database. The last
  // used ID will be written to the database when a notification is created.
  int64_t GetNextPersistentNotificationId();

  // Reads the notification data for the notification identified by
  // |notification_id| and belonging to |origin| from the database, and stores
  // it in |*notification_data|. Returns the status code.
  Status ReadNotificationData(
      const std::string& notification_id,
      const GURL& origin,
      NotificationDatabaseData* notification_data) const;

  // Reads the notification resources for the notification identified by
  // |notification_id| and belonging to |origin| from the database, and stores
  // it in |*notification_resources|. Returns the status code.
  Status ReadNotificationResources(
      const std::string& notification_id,
      const GURL& origin,
      blink::NotificationResources* notification_resources) const;

  // This function is identical to ReadNotificationData above, but also records
  // an interaction with that notification in the database for UKM logging
  // purposes.
  Status ReadNotificationDataAndRecordInteraction(
      const std::string& notification_id,
      const GURL& origin,
      PlatformNotificationContext::Interaction interaction,
      NotificationDatabaseData* notification_data);

  // Iterates over all notification data for all origins from the database, and
  // calls |callback| with each notification data. Returns the status code.
  Status ForEachNotificationData(ReadAllNotificationsCallback callback) const;

  // Reads all notification data for all origins from the database, and appends
  // the data to |notification_data_vector|. Returns the status code.
  Status ReadAllNotificationData(
      std::vector<NotificationDatabaseData>* notification_data_vector) const;

  // Reads all notification data associated with |origin| from the database, and
  // appends the data to |notification_data_vector|. Returns the status code.
  Status ReadAllNotificationDataForOrigin(
      const GURL& origin,
      std::vector<NotificationDatabaseData>* notification_data_vector) const;

  // Reads all notification data associated to |service_worker_registration_id|
  // belonging to |origin| from the database, and appends the data to the
  // |notification_data_vector|. Returns the status code.
  Status ReadAllNotificationDataForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      std::vector<NotificationDatabaseData>* notification_data_vector) const;

  // Writes the |notification_data| for a new notification belonging to |origin|
  // to the database, and returns the status code of the writing operation. The
  // notification's ID must have been set in the |notification_data|.
  Status WriteNotificationData(
      const GURL& origin,
      const NotificationDatabaseData& notification_data);

  // Deletes all data associated with the notification identified by
  // |notification_id| belonging to |origin| from the database. Returns the
  // status code of the deletion operation. Note that it is not considered a
  // failure if the to-be-deleted notification does not exist.
  Status DeleteNotificationData(const std::string& notification_id,
                                const GURL& origin);

  // Deletes resources associated with the notification identified by
  // |notification_id| belonging to |origin| from the database. Returns the
  // status code of the deletion operation. Note that it is not considered a
  // failure if the to-be-deleted resources do not exist.
  Status DeleteNotificationResources(const std::string& notification_id,
                                     const GURL& origin);

  // Deletes all data associated with |origin| from the database, optionally
  // filtered by the |tag|, and appends the deleted notification ids to
  // |deleted_notification_ids|. Returns the status code of the deletion
  // operation.
  Status DeleteAllNotificationDataForOrigin(
      const GURL& origin,
      const std::string& tag,
      std::set<std::string>* deleted_notification_ids);

  // Deletes all data associated with the |service_worker_registration_id|
  // belonging to |origin| from the database, and appends the deleted
  // notification ids to |deleted_notification_set|. Returns the status code
  // of the deletion operation.
  Status DeleteAllNotificationDataForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      std::set<std::string>* deleted_notification_ids);

  // Completely destroys the contents of this database.
  Status Destroy();

 private:
  friend class NotificationDatabaseTest;

  enum class State {
    UNINITIALIZED,
    INITIALIZED,
    DISABLED,
  };

  // Reads the next available persistent notification id from the database and
  // returns the status code of the reading operation. The value will be stored
  // in the |next_persistent_notification_id_| member.
  Status ReadNextPersistentNotificationId();

  // Iterates over all notifications and pushes matching ones onto
  // |notification_data_vector|. See ForEachNotificationDataInternal for deails.
  Status ReadAllNotificationDataInternal(
      const GURL& origin,
      int64_t service_worker_registration_id,
      std::vector<NotificationDatabaseData>* notification_data_vector) const;

  // Reads all notification data with the given constraints. |origin| may be
  // empty to read all notification data from all origins. If |origin| is
  // set, but |service_worker_registration_id| is invalid, then all notification
  // data for |origin| will be read. If both are set, then all notification data
  // for the given |service_worker_registration_id| will be read.
  Status ForEachNotificationDataInternal(
      const GURL& origin,
      int64_t service_worker_registration_id,
      ReadAllNotificationsCallback callback) const;

  // Deletes all notification data with the given constraints. |origin| must
  // always be set - use Destroy() when the goal is to empty the database. If
  // |service_worker_registration_id| is invalid, all notification data for the
  // |origin| will be deleted, optionally filtered by the |tag| when non-empty.
  // All deleted notification ids will be written to |deleted_notification_ids|.
  Status DeleteAllNotificationDataInternal(
      const GURL& origin,
      const std::string& tag,
      int64_t service_worker_registration_id,
      std::set<std::string>* deleted_notification_ids);

  // Returns whether the database has been opened.
  bool IsOpen() const { return db_ != nullptr; }

  // Returns whether the database should only exist in memory.
  bool IsInMemoryDatabase() const { return path_.empty(); }

  // Exposes the LevelDB database used to back this notification database.
  // Should only be used for testing purposes.
  leveldb::DB* GetDBForTesting() const { return db_.get(); }

  base::FilePath path_;

  std::unique_ptr<const leveldb::FilterPolicy> filter_policy_;

  // The declaration order for these members matters, as |db_| depends on |env_|
  // and thus has to be destructed first.
  std::unique_ptr<leveldb::Env> env_;
  std::unique_ptr<leveldb::DB> db_;

  State state_ = State::UNINITIALIZED;

  base::SequenceChecker sequence_checker_;

  // Callback to use for recording UKM metrics. Must be posted to the UI thread.
  UkmCallback record_notification_to_ukm_callback_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_
