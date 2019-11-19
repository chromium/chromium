// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"

namespace content {

const int kExampleServiceWorkerRegistrationId = 42;

const struct {
  const char* origin;
  const char* tag;
  int64_t service_worker_registration_id;
} kExampleNotificationData[] = {
    {"https://example.com", "" /* tag */, 0},
    {"https://example.com", "" /* tag */, kExampleServiceWorkerRegistrationId},
    {"https://example.com", "" /* tag */, kExampleServiceWorkerRegistrationId},
    {"https://example.com", "" /* tag */,
     kExampleServiceWorkerRegistrationId + 1},
    {"https://chrome.com", "" /* tag */, 0},
    {"https://chrome.com", "" /* tag */, 0},
    {"https://chrome.com", "" /* tag */, kExampleServiceWorkerRegistrationId},
    {"https://chrome.com", "foo" /* tag */, 0}};

class NotificationDatabaseTest : public ::testing::Test {
 public:
  NotificationDatabaseTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

 protected:
  // Creates a new NotificationDatabase instance in memory.
  NotificationDatabase* CreateDatabaseInMemory() {
    return new NotificationDatabase(base::FilePath(), callback());
  }

  // Creates a new NotificationDatabase instance in |path|.
  NotificationDatabase* CreateDatabaseOnFileSystem(const base::FilePath& path) {
    return new NotificationDatabase(path, callback());
  }

  // Creates a new notification for |service_worker_registration_id| belonging
  // to |origin| and writes it to the database. The written notification id
  // will be stored in |notification_id|.
  void CreateAndWriteNotification(NotificationDatabase* database,
                                  const GURL& origin,
                                  const std::string& tag,
                                  int64_t service_worker_registration_id,
                                  std::string* notification_id) {
    DCHECK(notification_id);

    NotificationDatabaseData database_data;
    database_data.notification_id = GenerateNotificationId();
    database_data.origin = origin;
    database_data.service_worker_registration_id =
        service_worker_registration_id;
    database_data.notification_data.tag = tag;

    ASSERT_EQ(NotificationDatabase::STATUS_OK,
              database->WriteNotificationData(origin, database_data));

    *notification_id = database_data.notification_id;
  }

  // Populates |database| with a series of example notifications that differ in
  // their origin and Service Worker registration id.
  void PopulateDatabaseWithExampleData(NotificationDatabase* database) {
    std::string notification_id;
    for (size_t i = 0; i < base::size(kExampleNotificationData); ++i) {
      ASSERT_NO_FATAL_FAILURE(CreateAndWriteNotification(
          database, GURL(kExampleNotificationData[i].origin),
          kExampleNotificationData[i].tag,
          kExampleNotificationData[i].service_worker_registration_id,
          &notification_id));
    }
  }

  // Returns if |database| has been opened.
  bool IsDatabaseOpen(NotificationDatabase* database) {
    return database->IsOpen();
  }

  // Returns if |database| is an in-memory only database.
  bool IsInMemoryDatabase(NotificationDatabase* database) {
    return database->IsInMemoryDatabase();
  }

  // Writes a LevelDB key-value pair directly to the LevelDB backing the
  // notification database in |database|.
  void WriteLevelDBKeyValuePair(NotificationDatabase* database,
                                const std::string& key,
                                const std::string& value) {
    leveldb::Status status =
        database->GetDBForTesting()->Put(leveldb::WriteOptions(), key, value);
    ASSERT_TRUE(status.ok());
  }

  // Generates a random notification ID. The format of the ID is opaque.
  std::string GenerateNotificationId() { return base::GenerateGUID(); }

  NotificationDatabase::UkmCallback callback() { return callback_; }

  BrowserTaskEnvironment task_environment_;  // Must be first member.

  NotificationDatabase::UkmCallback callback_;
};

TEST_F(NotificationDatabaseTest, OpenCloseMemory) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());

  // Should return false because the database does not exist in memory.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));

  // Should return true, indicating that the database could be created.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  EXPECT_TRUE(IsDatabaseOpen(database.get()));
  EXPECT_TRUE(IsInMemoryDatabase(database.get()));

  // Verify that in-memory databases do not persist when being re-created.
  database.reset(CreateDatabaseInMemory());

  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, OpenCloseFileSystem) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  std::unique_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.GetPath()));

  // Should return false because the database does not exist on the file system.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));

  // Should return true, indicating that the database could be created.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  EXPECT_TRUE(IsDatabaseOpen(database.get()));
  EXPECT_FALSE(IsInMemoryDatabase(database.get()));

  // Close the database, and re-open it without attempting to create it because
  // the files on the file system should still exist as expected.
  database.reset(CreateDatabaseOnFileSystem(database_dir.GetPath()));
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, DestroyDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  std::unique_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.GetPath()));

  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));
  EXPECT_TRUE(IsDatabaseOpen(database.get()));

  // Destroy the database. This will immediately close it as well.
  ASSERT_EQ(NotificationDatabase::STATUS_OK, database->Destroy());
  EXPECT_FALSE(IsDatabaseOpen(database.get()));

  // Try to re-open the database (but not re-create it). This should fail as
  // the files associated with the database should have been blown away.
  database.reset(CreateDatabaseOnFileSystem(database_dir.GetPath()));
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, NotificationIdIncrements) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  std::unique_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.GetPath()));

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  std::string notification_id;
  ASSERT_NO_FATAL_FAILURE(
      CreateAndWriteNotification(database.get(), origin, "" /* tag */,
                                 0 /* sw_registration_id */, &notification_id));

  database.reset(CreateDatabaseOnFileSystem(database_dir.GetPath()));
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, NotificationIdIncrementsStorage) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  NotificationDatabaseData database_data, read_database_data;
  database_data.notification_id = GenerateNotificationId();

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationData(database_data.notification_id,
                                           origin, &read_database_data));

  EXPECT_EQ(database_data.notification_id, read_database_data.notification_id);
}

TEST_F(NotificationDatabaseTest, ReadInvalidNotificationData) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  NotificationDatabaseData database_data;

  // Reading the notification data for a notification that does not exist should
  // return the ERROR_NOT_FOUND status code.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadNotificationData("bad-id", GURL("https://chrome.com"),
                                           &database_data));
}

TEST_F(NotificationDatabaseTest, ReadNotificationDataDifferentOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  NotificationDatabaseData database_data, read_database_data;
  database_data.notification_id = GenerateNotificationId();
  database_data.notification_data.title = base::UTF8ToUTF16("My Notification");

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  // Reading the notification from the database when given a different origin
  // should return the ERROR_NOT_FOUND status code.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadNotificationData(database_data.notification_id,
                                           GURL("https://chrome.com"),
                                           &read_database_data));

  // However, reading the notification from the database with the same origin
  // should return STATUS_OK and the associated notification data.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationData(database_data.notification_id,
                                           origin, &read_database_data));

  EXPECT_EQ(database_data.notification_data.title,
            read_database_data.notification_data.title);
}

TEST_F(NotificationDatabaseTest, ReadNotificationDataReflection) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  blink::PlatformNotificationData notification_data;
  notification_data.title = base::UTF8ToUTF16("My Notification");
  notification_data.direction =
      blink::mojom::NotificationDirection::RIGHT_TO_LEFT;
  notification_data.lang = "nl-NL";
  notification_data.body = base::UTF8ToUTF16("Hello, world!");
  notification_data.tag = "replace id";
  notification_data.icon = GURL("https://example.com/icon.png");
  notification_data.silent = true;

  NotificationDatabaseData database_data;
  database_data.notification_id = GenerateNotificationId();
  database_data.origin = origin;
  database_data.service_worker_registration_id = 42;
  database_data.notification_data = notification_data;

  // Write the constructed notification to the database, and then immediately
  // read it back from the database again as well.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  NotificationDatabaseData read_database_data;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationData(database_data.notification_id,
                                           origin, &read_database_data));

  // Verify that all members retrieved from the database are exactly the same
  // as the ones that were written to it. This tests the serialization behavior.

  EXPECT_EQ(database_data.notification_id, read_database_data.notification_id);

  EXPECT_EQ(database_data.origin, read_database_data.origin);
  EXPECT_EQ(database_data.service_worker_registration_id,
            read_database_data.service_worker_registration_id);

  const blink::PlatformNotificationData& read_notification_data =
      read_database_data.notification_data;

  EXPECT_EQ(notification_data.title, read_notification_data.title);
  EXPECT_EQ(notification_data.direction, read_notification_data.direction);
  EXPECT_EQ(notification_data.lang, read_notification_data.lang);
  EXPECT_EQ(notification_data.body, read_notification_data.body);
  EXPECT_EQ(notification_data.tag, read_notification_data.tag);
  EXPECT_EQ(notification_data.icon, read_notification_data.icon);
  EXPECT_EQ(notification_data.silent, read_notification_data.silent);
}

TEST_F(NotificationDatabaseTest, ReadInvalidNotificationResources) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  blink::NotificationResources database_resources;

  // Reading the notification resources for a notification that does not exist
  // should return the ERROR_NOT_FOUND status code.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadNotificationResources(
                "bad-id", GURL("https://chrome.com"), &database_resources));
}

TEST_F(NotificationDatabaseTest, ReadNotificationResourcesDifferentOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  NotificationDatabaseData database_data;
  blink::NotificationResources database_resources;
  database_data.notification_id = GenerateNotificationId();
  database_data.notification_data.title = base::UTF8ToUTF16("My Notification");
  database_data.notification_resources = blink::NotificationResources();

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  // Reading the notification resources from the database when given a different
  // origin should return the ERROR_NOT_FOUND status code.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadNotificationResources(database_data.notification_id,
                                                GURL("https://chrome.com"),
                                                &database_resources));

  // However, reading the notification from the database with the same origin
  // should return STATUS_OK and the associated notification data.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationResources(database_data.notification_id,
                                                origin, &database_resources));
}

TEST_F(NotificationDatabaseTest, ReadNotificationResourcesReflection) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  blink::NotificationResources notification_resources;
  NotificationDatabaseData database_data;
  database_data.notification_id = GenerateNotificationId();
  database_data.origin = origin;
  database_data.service_worker_registration_id = 42;
  database_data.notification_resources = notification_resources;

  // Write the constructed notification to the database, and then immediately
  // read it back from the database again as well.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  NotificationDatabaseData read_database_data;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationData(database_data.notification_id,
                                           origin, &read_database_data));

  // Verify that all members retrieved from the database are exactly the same
  // as the ones that were written to it. This tests the serialization behavior.

  EXPECT_EQ(database_data.notification_id, read_database_data.notification_id);

  EXPECT_EQ(database_data.origin, read_database_data.origin);
  EXPECT_EQ(database_data.service_worker_registration_id,
            read_database_data.service_worker_registration_id);

  // We do not populate the resources when reading from the database.
  EXPECT_FALSE(read_database_data.notification_resources.has_value());

  blink::NotificationResources read_notification_resources;
  EXPECT_EQ(
      NotificationDatabase::STATUS_OK,
      database->ReadNotificationResources(database_data.notification_id, origin,
                                          &read_notification_resources));
}

TEST_F(NotificationDatabaseTest, ReadWriteMultipleNotificationData) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  std::vector<std::string> notification_ids;
  std::string notification_id;

  // Write ten notifications to the database, each with a unique title and
  // notification id (it is the responsibility of the user to increment this).
  for (int i = 1; i <= 10; ++i) {
    ASSERT_NO_FATAL_FAILURE(CreateAndWriteNotification(
        database.get(), origin, "" /* tag */, i /* sw_registration_id */,
        &notification_id));

    EXPECT_FALSE(notification_id.empty());

    notification_ids.push_back(notification_id);
  }

  NotificationDatabaseData database_data;

  int64_t service_worker_registration_id = 1;

  // Read the ten notifications from the database, and verify that the titles
  // of each of them matches with how they were created.
  for (const std::string& notification_id : notification_ids) {
    ASSERT_EQ(NotificationDatabase::STATUS_OK,
              database->ReadNotificationData(notification_id, origin,
                                             &database_data));

    EXPECT_EQ(service_worker_registration_id++,
              database_data.service_worker_registration_id);
  }
}

TEST_F(NotificationDatabaseTest, ReadNotificationUpdateInteraction) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  NotificationDatabaseData database_data, read_database_data;
  database_data.notification_id = GenerateNotificationId();
  database_data.notification_data.title = base::UTF8ToUTF16("My Notification");

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  // Check that the time deltas have not yet been set.
  EXPECT_EQ(false,
            read_database_data.time_until_first_click_millis.has_value());
  EXPECT_EQ(false, read_database_data.time_until_last_click_millis.has_value());
  EXPECT_EQ(false, read_database_data.time_until_close_millis.has_value());

  // Check that when a notification has an interaction, the appropriate field is
  // updated on the read.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationDataAndRecordInteraction(
                database_data.notification_id, origin,
                PlatformNotificationContext::Interaction::CLICKED,
                &read_database_data));
  EXPECT_EQ(1, read_database_data.num_clicks);

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationDataAndRecordInteraction(
                database_data.notification_id, origin,
                PlatformNotificationContext::Interaction::ACTION_BUTTON_CLICKED,
                &read_database_data));
  EXPECT_EQ(1, read_database_data.num_action_button_clicks);

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationDataAndRecordInteraction(
                database_data.notification_id, origin,
                PlatformNotificationContext::Interaction::ACTION_BUTTON_CLICKED,
                &read_database_data));
  EXPECT_EQ(2, read_database_data.num_action_button_clicks);

  // Check that the click timestamps are correctly updated.
  EXPECT_EQ(true, read_database_data.time_until_first_click_millis.has_value());
  EXPECT_EQ(true, read_database_data.time_until_last_click_millis.has_value());

  // Check that when a read with a CLOSED interaction occurs, the correct
  // field is updated.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationDataAndRecordInteraction(
                database_data.notification_id, origin,
                PlatformNotificationContext::Interaction::CLOSED,
                &read_database_data));
  EXPECT_EQ(true, read_database_data.time_until_close_millis.has_value());
}

TEST_F(NotificationDatabaseTest, DeleteInvalidNotificationData) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  // Deleting non-existing notifications is not considered to be a failure.
  ASSERT_EQ(
      NotificationDatabase::STATUS_OK,
      database->DeleteNotificationData("bad-id", GURL("https://chrome.com")));
}

TEST_F(NotificationDatabaseTest, DeleteNotificationDataSameOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  const std::string notification_id = GenerateNotificationId();

  NotificationDatabaseData database_data;
  database_data.notification_id = notification_id;

  GURL origin("https://example.com");

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  // Reading a notification after writing one should succeed.
  EXPECT_EQ(
      NotificationDatabase::STATUS_OK,
      database->ReadNotificationData(notification_id, origin, &database_data));

  // Delete the notification which was just written to the database, and verify
  // that reading it again will fail.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteNotificationData(notification_id, origin));
  EXPECT_EQ(
      NotificationDatabase::STATUS_ERROR_NOT_FOUND,
      database->ReadNotificationData(notification_id, origin, &database_data));
}

TEST_F(NotificationDatabaseTest, DeleteNotificationResourcesSameOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  const std::string notification_id = GenerateNotificationId();

  blink::NotificationResources notification_resources;
  NotificationDatabaseData database_data;
  database_data.notification_id = notification_id;
  database_data.notification_resources = notification_resources;

  GURL origin("https://example.com");

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  // Reading notification resources after writing should succeed.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationResources(notification_id, origin,
                                                &notification_resources));

  // Delete the notification which was just written to the database, and verify
  // that reading the resources again will fail.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteNotificationData(notification_id, origin));
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadNotificationResources(notification_id, origin,
                                                &notification_resources));
}

TEST_F(NotificationDatabaseTest, DeleteNotificationDataDifferentOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  const std::string notification_id = GenerateNotificationId();

  NotificationDatabaseData database_data;
  database_data.notification_id = notification_id;

  GURL origin("https://example.com");

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  // Attempting to delete the notification with a different origin, but with the
  // same |notification_id|, should not return an error (the notification could
  // not be found, but that's not considered a failure). However, it should not
  // remove the notification either.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteNotificationData(notification_id,
                                             GURL("https://chrome.com")));

  EXPECT_EQ(
      NotificationDatabase::STATUS_OK,
      database->ReadNotificationData(notification_id, origin, &database_data));
}

TEST_F(NotificationDatabaseTest, DeleteInvalidNotificationResources) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  // Deleting non-existing resources is not considered to be a failure.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteNotificationResources("bad-id",
                                                  GURL("https://chrome.com")));
}

TEST_F(NotificationDatabaseTest, DeleteNotificationResources) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  const std::string notification_id = GenerateNotificationId();

  blink::NotificationResources notification_resources;
  NotificationDatabaseData database_data;
  database_data.notification_id = notification_id;
  database_data.notification_resources = notification_resources;

  GURL origin("https://example.com");

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->WriteNotificationData(origin, database_data));

  // Reading notification resources after writing should succeed.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadNotificationResources(notification_id, origin,
                                                &notification_resources));

  // Delete the notification resources for the notification which was just
  // written to the database, and verify that reading them again will fail.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteNotificationResources(notification_id, origin));
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->ReadNotificationResources(notification_id, origin,
                                                &notification_resources));
}

TEST_F(NotificationDatabaseTest, ReadAllNotificationData) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationData(&notifications));

  EXPECT_EQ(base::size(kExampleNotificationData), notifications.size());
}

TEST_F(NotificationDatabaseTest, ReadAllNotificationDataEmpty) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationData(&notifications));

  EXPECT_EQ(0u, notifications.size());
}

TEST_F(NotificationDatabaseTest, ReadAllNotificationDataForOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://example.com");

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForOrigin(origin, &notifications));

  EXPECT_EQ(4u, notifications.size());
}

TEST_F(NotificationDatabaseTest,
       ReadAllNotificationDataForServiceWorkerRegistration) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://example.com:443");

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForServiceWorkerRegistration(
                origin, kExampleServiceWorkerRegistrationId, &notifications));

  EXPECT_EQ(2u, notifications.size());
}

TEST_F(NotificationDatabaseTest, DeleteAllNotificationDataForOrigin) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://example.com:443");

  std::set<std::string> deleted_notification_ids;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteAllNotificationDataForOrigin(
                origin, "" /* tag */, &deleted_notification_ids));

  EXPECT_EQ(4u, deleted_notification_ids.size());

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForOrigin(origin, &notifications));

  EXPECT_EQ(0u, notifications.size());
}

TEST_F(NotificationDatabaseTest, DeleteAllNotificationDataForOriginWithTag) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://chrome.com");

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForOrigin(origin, &notifications));

  const std::string& tag = "foo";

  size_t notifications_with_tag = 0;
  size_t notifications_without_tag = 0;

  for (const auto& database_data : notifications) {
    if (database_data.notification_data.tag == tag)
      ++notifications_with_tag;
    else
      ++notifications_without_tag;
  }

  ASSERT_GT(notifications_with_tag, 0u);
  ASSERT_GT(notifications_without_tag, 0u);

  std::set<std::string> deleted_notification_ids;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteAllNotificationDataForOrigin(
                origin, "foo" /* tag */, &deleted_notification_ids));

  EXPECT_EQ(notifications_with_tag, deleted_notification_ids.size());

  std::vector<NotificationDatabaseData> updated_notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForOrigin(origin,
                                                       &updated_notifications));

  EXPECT_EQ(notifications_without_tag, updated_notifications.size());

  size_t updated_notifications_with_tag = 0;
  size_t updated_notifications_without_tag = 0;

  for (const auto& database_data : updated_notifications) {
    if (database_data.notification_data.tag == tag)
      ++updated_notifications_with_tag;
    else
      ++updated_notifications_without_tag;
  }

  EXPECT_EQ(0u, updated_notifications_with_tag);
  EXPECT_EQ(notifications_without_tag, updated_notifications_without_tag);
}

TEST_F(NotificationDatabaseTest, DeleteAllNotificationDataForOriginEmpty) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  GURL origin("https://example.com");

  std::set<std::string> deleted_notification_ids;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteAllNotificationDataForOrigin(
                origin, "" /* tag */, &deleted_notification_ids));

  EXPECT_EQ(0u, deleted_notification_ids.size());
}

TEST_F(NotificationDatabaseTest,
       DeleteAllNotificationDataForServiceWorkerRegistration) {
  std::unique_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  ASSERT_NO_FATAL_FAILURE(PopulateDatabaseWithExampleData(database.get()));

  GURL origin("https://example.com:443");
  std::set<std::string> deleted_notification_ids;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->DeleteAllNotificationDataForServiceWorkerRegistration(
                origin, kExampleServiceWorkerRegistrationId,
                &deleted_notification_ids));

  EXPECT_EQ(2u, deleted_notification_ids.size());

  std::vector<NotificationDatabaseData> notifications;
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->ReadAllNotificationDataForServiceWorkerRegistration(
                origin, kExampleServiceWorkerRegistrationId, &notifications));

  EXPECT_EQ(0u, notifications.size());
}

}  // namespace content
