// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_platform_notification_service.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

using ::testing::Return;

namespace content {

// Fake Service Worker registration id to use in tests requiring one.
const int64_t kFakeServiceWorkerRegistrationId = 42;

class NotificationBrowserClient : public TestContentBrowserClient {
 public:
  explicit NotificationBrowserClient(BrowserContext* browser_context)
      : platform_notification_service_(
            std::make_unique<MockPlatformNotificationService>(
                browser_context)) {}

  PlatformNotificationService* GetPlatformNotificationService(
      BrowserContext* browser_context) override {
    return platform_notification_service_.get();
  }

 private:
  std::unique_ptr<PlatformNotificationService> platform_notification_service_;
};

class PlatformNotificationContextTest : public ::testing::Test {
 public:
  PlatformNotificationContextTest() : success_(false) {}

  void SetUp() override {
    // Provide a mock permission manager to the |browser_context_|.
    permission_manager_ = new ::testing::NiceMock<MockPermissionManager>();
    browser_context_.SetPermissionControllerDelegate(
        base::WrapUnique(permission_manager_));
  }

  // Callback to provide when reading a single notification from the database.
  void DidReadNotificationData(bool success,
                               const NotificationDatabaseData& database_data) {
    success_ = success;
    database_data_ = database_data;
  }

  // Callback to provide when writing a notification to the database.
  void DidWriteNotificationData(bool success,
                                const std::string& notification_id) {
    success_ = success;
    notification_id_ = notification_id;
  }

  // Callback to provide when deleting notification data from the database.
  void DidDeleteNotificationData(bool success) { success_ = success; }

  // Callback to provide when deleting all blocked notification data.
  void DidDeleteAllNotificationData(bool success, size_t deleted_count) {
    success_ = success;
    deleted_count_ = deleted_count;
  }

  // Callback to provide when registering a Service Worker with a Service
  // Worker Context. Will write the registration id to |store_registration_id|.
  void DidRegisterServiceWorker(int64_t* store_registration_id,
                                blink::ServiceWorkerStatusCode status,
                                const std::string& status_message,
                                int64_t service_worker_registration_id) {
    DCHECK(store_registration_id);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);

    *store_registration_id = service_worker_registration_id;
  }

  // Callback to provide when unregistering a Service Worker. Will write the
  // resulting status code to |store_status|.
  void DidUnregisterServiceWorker(blink::ServiceWorkerStatusCode* store_status,
                                  blink::ServiceWorkerStatusCode status) {
    DCHECK(store_status);
    *store_status = status;
  }

 protected:
  // Creates a new PlatformNotificationContextImpl instance. When using this
  // method, the underlying database will always be created in memory.
  scoped_refptr<PlatformNotificationContextImpl>
  CreatePlatformNotificationContext() {
    auto context = base::MakeRefCounted<PlatformNotificationContextImpl>(
        base::FilePath(), &browser_context_, nullptr);
    OverrideTaskRunnerForTesting(context.get());
    context->Initialize();
    // Wait until initialization is done as we query the displayed notifications
    // from PlatformNotificationService and would delete notifications stored
    // after Initialize has run but before DoSyncNotificationData has finished.
    base::RunLoop().RunUntilIdle();
    return context;
  }

  // Overrides the task runner in |context| with the current message loop
  // proxy, to reduce the number of threads involved in the tests.
  void OverrideTaskRunnerForTesting(PlatformNotificationContextImpl* context) {
    context->SetTaskRunnerForTesting(base::ThreadTaskRunnerHandle::Get());
  }

  // Gets the currently displayed notifications from |service| synchronously.
  std::set<std::string> GetDisplayedNotificationsSync(
      PlatformNotificationService* service) {
    std::set<std::string> displayed_notification_ids;
    base::RunLoop run_loop;
    service->GetDisplayedNotifications(
        base::BindLambdaForTesting(
            [&](std::set<std::string> notification_ids, bool supports_sync) {
              displayed_notification_ids = std::move(notification_ids);
              run_loop.Quit();
            }));
    run_loop.Run();
    return displayed_notification_ids;
  }

  // Gets the number of notifications stored in |context| for |origin|.
  std::vector<NotificationDatabaseData> GetStoredNotificationsSync(
      PlatformNotificationContextImpl* context,
      const GURL& origin) {
    std::vector<NotificationDatabaseData> notification_database_datas;
    base::RunLoop run_loop;
    context->ReadAllNotificationDataForServiceWorkerRegistration(
        origin, kFakeServiceWorkerRegistrationId,
        base::BindLambdaForTesting(
            [&](bool success, const std::vector<NotificationDatabaseData>&
                                  notification_datas) {
              DCHECK(success);
              notification_database_datas = notification_datas;
              run_loop.Quit();
            }));
    base::RunLoop().RunUntilIdle();
    return notification_database_datas;
  }

  // Reads the resources stored in |context| for |notification_id| and |origin|.
  // Returns if reading the resources succeeded or not.
  bool ReadNotificationResourcesSync(PlatformNotificationContextImpl* context,
                                     const std::string& notification_id,
                                     const GURL& origin) {
    bool result = false;
    base::RunLoop run_loop;
    context->ReadNotificationResources(
        notification_id, origin,
        base::BindLambdaForTesting(
            [&](bool success,
                const blink::NotificationResources& notification_resources) {
              result = success;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  // Writes the |resources| to |context| and returns the success status.
  bool WriteNotificationResourcesSync(
      PlatformNotificationContextImpl* context,
      std::vector<NotificationResourceData> resources) {
    bool result = false;
    base::RunLoop run_loop;
    context->WriteNotificationResources(
        std::move(resources), base::BindLambdaForTesting([&](bool success) {
          result = success;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  // Writes notification |data| for |origin| to |context| and verifies the
  // result callback synchronously. Returns the written notification id.
  std::string WriteNotificationDataSync(
      PlatformNotificationContextImpl* context,
      const GURL& origin,
      const NotificationDatabaseData& data) {
    std::string notification_id;
    base::RunLoop run_loop;
    context->WriteNotificationData(
        next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
        origin, data,
        base::BindLambdaForTesting([&](bool success, const std::string& id) {
          DCHECK(success);
          notification_id = id;
          run_loop.Quit();
        }));
    run_loop.Run();
    DCHECK(!notification_id.empty());
    return notification_id;
  }

  void SetPermissionStatus(const GURL& origin,
                           blink::mojom::PermissionStatus permission_status) {
    ON_CALL(*permission_manager_,
            GetPermissionStatus(PermissionType::NOTIFICATIONS, origin, origin))
        .WillByDefault(Return(permission_status));
  }

  // Returns the testing browsing context that can be used for this test.
  BrowserContext* browser_context() { return &browser_context_; }

  // Returns whether the last invoked callback finished successfully.
  bool success() const { return success_; }

  // Returns the next persistent notification id for tests.
  int64_t next_persistent_notification_id() {
    return next_persistent_notification_id_++;
  }

  // Returns the NotificationDatabaseData associated with the last invoked
  // ReadNotificationData callback.
  const NotificationDatabaseData& database_data() const {
    return database_data_;
  }

  // Returns the notification id of the notification last written.
  const std::string& notification_id() const { return notification_id_; }

 private:
  BrowserTaskEnvironment task_environment_;  // Must be first member
  TestBrowserContext browser_context_;
  MockPermissionManager* permission_manager_ = nullptr;

  bool success_;
  size_t deleted_count_;
  NotificationDatabaseData database_data_;
  std::string notification_id_;
  int64_t next_persistent_notification_id_ = 1;
};

TEST_F(PlatformNotificationContextTest, ReadNonExistentNotification) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  context->ReadNotificationDataAndRecordInteraction(
      "invalid-notification-id", GURL("https://example.com"),
      PlatformNotificationContext::Interaction::NONE,
      base::BindOnce(&PlatformNotificationContextTest::DidReadNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The read operation should have failed, as it does not exist.
  ASSERT_FALSE(success());
}

TEST_F(PlatformNotificationContextTest, WriteReadNotification) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;
  notification_database_data.origin = origin;

  context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The write operation should have succeeded with a notification id.
  ASSERT_TRUE(success());
  EXPECT_FALSE(notification_id().empty());

  context->ReadNotificationDataAndRecordInteraction(
      notification_id(), origin, PlatformNotificationContext::Interaction::NONE,
      base::BindOnce(&PlatformNotificationContextTest::DidReadNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The read operation should have succeeded, with the right notification.
  ASSERT_TRUE(success());

  const NotificationDatabaseData& read_database_data = database_data();
  EXPECT_EQ(notification_database_data.origin, read_database_data.origin);
}

TEST_F(PlatformNotificationContextTest, WriteReadReplacedNotification) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  const GURL origin("https://example.com");
  const std::string tag = "foo";

  NotificationDatabaseData notification_database_data;
  notification_database_data.service_worker_registration_id =
      kFakeServiceWorkerRegistrationId;
  notification_database_data.origin = origin;
  notification_database_data.notification_data.title =
      base::ASCIIToUTF16("First");
  notification_database_data.notification_data.tag = tag;

  // Write the first notification with the given |tag|.
  context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  std::string read_notification_id = notification_id();

  // The write operation should have succeeded with a notification id.
  ASSERT_TRUE(success());
  EXPECT_FALSE(read_notification_id.empty());

  notification_database_data.notification_data.title =
      base::ASCIIToUTF16("Second");

  // Write the second notification with the given |tag|.
  context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(success());
  ASSERT_FALSE(notification_id().empty());
  ASSERT_EQ(notification_id(), read_notification_id);

  // Reading the notifications should only yield the second, replaced one.
  std::vector<NotificationDatabaseData> notification_database_datas =
      GetStoredNotificationsSync(context.get(), origin);

  ASSERT_EQ(1u, notification_database_datas.size());

  EXPECT_EQ(tag, notification_database_datas[0].notification_data.tag);
  EXPECT_EQ(base::ASCIIToUTF16("Second"),
            notification_database_datas[0].notification_data.title);
}

TEST_F(PlatformNotificationContextTest, DeleteInvalidNotification) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  context->DeleteNotificationData(
      "invalid-notification-id", GURL("https://example.com"),
      /* close_notification= */ false,
      base::BindOnce(
          &PlatformNotificationContextTest::DidDeleteNotificationData,
          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The notification may not have existed, but since the goal of deleting data
  // is to make sure that it's gone, the goal has been satisfied. As such,
  // deleting a non-existent notification is considered to be a success.
  EXPECT_TRUE(success());
}

TEST_F(PlatformNotificationContextTest, DeleteNotification) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;

  context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The write operation should have succeeded with a notification id.
  ASSERT_TRUE(success());
  EXPECT_FALSE(notification_id().empty());

  context->DeleteNotificationData(
      notification_id(), origin,
      /* close_notification= */ false,
      base::BindOnce(
          &PlatformNotificationContextTest::DidDeleteNotificationData,
          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The notification existed, so it should have been removed successfully.
  ASSERT_TRUE(success());

  context->ReadNotificationDataAndRecordInteraction(
      notification_id(), origin, PlatformNotificationContext::Interaction::NONE,
      base::BindOnce(&PlatformNotificationContextTest::DidReadNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The notification was removed, so we shouldn't be able to read it from
  // the database anymore.
  EXPECT_FALSE(success());
}

TEST_F(PlatformNotificationContextTest, DeleteClosesNotification) {
  NotificationBrowserClient notification_browser_client(browser_context());
  SetBrowserClientForTesting(&notification_browser_client);
  PlatformNotificationService* service =
      notification_browser_client.GetPlatformNotificationService(
          browser_context());

  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;

  context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The write operation should have displayed a notification.
  ASSERT_TRUE(success());
  EXPECT_EQ(1u, GetDisplayedNotificationsSync(service).size());

  context->DeleteNotificationData(
      notification_id(), origin,
      /* close_notification= */ true,
      base::BindOnce(
          &PlatformNotificationContextTest::DidDeleteNotificationData,
          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // Deleting the notification data should have closed the notification.
  ASSERT_TRUE(success());
  EXPECT_EQ(0u, GetDisplayedNotificationsSync(service).size());
}

TEST_F(PlatformNotificationContextTest,
       DeleteAllNotificationDataForBlockedOrigins) {
  NotificationBrowserClient notification_browser_client(browser_context());
  SetBrowserClientForTesting(&notification_browser_client);

  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();
  PlatformNotificationService* service =
      notification_browser_client.GetPlatformNotificationService(
          browser_context());

  GURL origin1("https://example1.com");
  GURL origin2("https://example.com");

  NotificationDatabaseData notification_database_data;
  notification_database_data.service_worker_registration_id =
      kFakeServiceWorkerRegistrationId;

  // Store 3 notifications with |origin1|.
  notification_database_data.origin = origin1;
  for (size_t i = 0; i < 3; ++i) {
    context->WriteNotificationData(
        next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
        origin1, notification_database_data,
        base::BindOnce(
            &PlatformNotificationContextTest::DidWriteNotificationData,
            base::Unretained(this)));
  }

  // Store 5 notifications with |origin2|.
  notification_database_data.origin = origin2;
  for (size_t i = 0; i < 5; ++i) {
    context->WriteNotificationData(
        next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
        origin2, notification_database_data,
        base::BindOnce(
            &PlatformNotificationContextTest::DidWriteNotificationData,
            base::Unretained(this)));
  }

  base::RunLoop().RunUntilIdle();

  // Verify that the 8 notifications are present.
  EXPECT_EQ(3u, GetStoredNotificationsSync(context.get(), origin1).size());
  EXPECT_EQ(5u, GetStoredNotificationsSync(context.get(), origin2).size());
  EXPECT_EQ(8u, GetDisplayedNotificationsSync(service).size());

  // Delete the 5 notifications for |origin2|.
  SetPermissionStatus(origin1, blink::mojom::PermissionStatus::GRANTED);
  SetPermissionStatus(origin2, blink::mojom::PermissionStatus::DENIED);
  context->DeleteAllNotificationDataForBlockedOrigins(base::BindOnce(
      &PlatformNotificationContextTest::DidDeleteAllNotificationData,
      base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  // The notifications should have been removed successfully.
  ASSERT_TRUE(success());

  // Verify that only the 3 notifications for |origin1| are left.
  EXPECT_EQ(3u, GetStoredNotificationsSync(context.get(), origin1).size());
  EXPECT_EQ(0u, GetStoredNotificationsSync(context.get(), origin2).size());
  EXPECT_EQ(3u, GetDisplayedNotificationsSync(service).size());
}

TEST_F(PlatformNotificationContextTest, ServiceWorkerUnregistered) {
  NotificationBrowserClient notification_browser_client(browser_context());
  SetBrowserClientForTesting(&notification_browser_client);
  PlatformNotificationService* service =
      notification_browser_client.GetPlatformNotificationService(
          browser_context());
  std::unique_ptr<EmbeddedWorkerTestHelper> embedded_worker_test_helper(
      new EmbeddedWorkerTestHelper(base::FilePath()));

  // Manually create the PlatformNotificationContextImpl so that the Service
  // Worker context wrapper can be passed in.
  scoped_refptr<PlatformNotificationContextImpl> notification_context(
      new PlatformNotificationContextImpl(
          base::FilePath(), browser_context(),
          embedded_worker_test_helper->context_wrapper()));
  OverrideTaskRunnerForTesting(notification_context.get());
  notification_context->Initialize();
  base::RunLoop().RunUntilIdle();

  GURL origin("https://example.com");
  GURL script_url("https://example.com/worker.js");

  int64_t service_worker_registration_id =
      blink::mojom::kInvalidServiceWorkerRegistrationId;

  // Register a Service Worker to get a valid registration id.
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = origin;
  embedded_worker_test_helper->context()->RegisterServiceWorker(
      script_url, options, blink::mojom::FetchClientSettingsObject::New(),
      base::BindOnce(&PlatformNotificationContextTest::DidRegisterServiceWorker,
                     base::Unretained(this), &service_worker_registration_id));

  base::RunLoop().RunUntilIdle();
  ASSERT_NE(service_worker_registration_id,
            blink::mojom::kInvalidServiceWorkerRegistrationId);

  NotificationDatabaseData notification_database_data;

  // Create a notification for that Service Worker registration.
  notification_context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(success());
  EXPECT_FALSE(notification_id().empty());
  ASSERT_EQ(1u, GetDisplayedNotificationsSync(service).size());

  blink::ServiceWorkerStatusCode unregister_status;

  // Now drop the Service Worker registration which owns that notification.
  embedded_worker_test_helper->context()->UnregisterServiceWorker(
      origin, base::BindOnce(
                  &PlatformNotificationContextTest::DidUnregisterServiceWorker,
                  base::Unretained(this), &unregister_status));

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, unregister_status);

  // And verify that the associated notification has indeed been dropped.
  notification_context->ReadNotificationDataAndRecordInteraction(
      notification_id(), origin, PlatformNotificationContext::Interaction::NONE,
      base::BindOnce(&PlatformNotificationContextTest::DidReadNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(success());

  // Verify that the notification is closed.
  ASSERT_EQ(0u, GetDisplayedNotificationsSync(service).size());
}

TEST_F(PlatformNotificationContextTest, DestroyDatabaseOnStorageWiped) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;

  context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The write operation should have succeeded with a notification id.
  ASSERT_TRUE(success());
  EXPECT_FALSE(notification_id().empty());

  // Call the OnStorageWiped override from the ServiceWorkerContextCoreObserver,
  // which indicates that the database should go away entirely.
  context->OnStorageWiped();

  // Verify that reading notification data fails because the data does not
  // exist anymore. Deliberately omit RunUntilIdle(), since this is unlikely to
  // be the case when OnStorageWiped gets called in production.
  context->ReadNotificationDataAndRecordInteraction(
      notification_id(), origin, PlatformNotificationContext::Interaction::NONE,
      base::BindOnce(&PlatformNotificationContextTest::DidReadNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(success());
}

TEST_F(PlatformNotificationContextTest, DestroyOnDiskDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  // Manually construct the PlatformNotificationContextImpl because this test
  // requires the database to be created on the filesystem.
  scoped_refptr<PlatformNotificationContextImpl> context(
      new PlatformNotificationContextImpl(database_dir.GetPath(),
                                          browser_context(), nullptr));

  OverrideTaskRunnerForTesting(context.get());

  // Trigger a read-operation to force creating the database.
  context->ReadNotificationDataAndRecordInteraction(
      "invalid-notification-id", GURL("https://example.com"),
      PlatformNotificationContext::Interaction::NONE,
      base::BindOnce(&PlatformNotificationContextTest::DidReadNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsDirectoryEmpty(database_dir.GetPath()));
  EXPECT_FALSE(success());

  // Blow away the database by faking a Service Worker Context wipe-out.
  context->OnStorageWiped();

  base::RunLoop().RunUntilIdle();

  // The database's directory should be empty at this point.
  EXPECT_TRUE(IsDirectoryEmpty(database_dir.GetPath()));
}

TEST_F(PlatformNotificationContextTest, ReadAllServiceWorkerDataEmpty) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");

  ASSERT_EQ(0u, GetStoredNotificationsSync(context.get(), origin).size());
}

TEST_F(PlatformNotificationContextTest, ReadAllServiceWorkerDataFilled) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");

  NotificationDatabaseData notification_database_data;
  notification_database_data.origin = origin;
  notification_database_data.service_worker_registration_id =
      kFakeServiceWorkerRegistrationId;

  // Insert ten notifications into the database belonging to origin and the
  // test Service Worker Registration id.
  for (int i = 0; i < 10; ++i) {
    context->WriteNotificationData(
        next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
        origin, notification_database_data,
        base::BindOnce(
            &PlatformNotificationContextTest::DidWriteNotificationData,
            base::Unretained(this)));

    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(success());
  }

  // Now read the notifications from the database again. There should be ten,
  // all set with the correct origin and Service Worker Registration id.
  std::vector<NotificationDatabaseData> notification_database_datas =
      GetStoredNotificationsSync(context.get(), origin);

  ASSERT_EQ(10u, notification_database_datas.size());

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(origin, notification_database_datas[i].origin);
    EXPECT_EQ(kFakeServiceWorkerRegistrationId,
              notification_database_datas[i].service_worker_registration_id);
  }
}

TEST_F(PlatformNotificationContextTest, SynchronizeNotifications) {
  NotificationBrowserClient notification_browser_client(browser_context());
  SetBrowserClientForTesting(&notification_browser_client);

  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();
  // Let PlatformNotificationContext synchronize displayed notifications.
  base::RunLoop().RunUntilIdle();

  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;
  notification_database_data.service_worker_registration_id =
      kFakeServiceWorkerRegistrationId;
  blink::PlatformNotificationData notification_data;
  blink::NotificationResources notification_resources;

  context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(success());
  EXPECT_FALSE(notification_id().empty());

  ASSERT_EQ(1u, GetStoredNotificationsSync(context.get(), origin).size());

  // Delete the notification from the display service without removing it from
  // the database. It should automatically synchronize on the next read.
  PlatformNotificationService* service =
      notification_browser_client.GetPlatformNotificationService(
          browser_context());
  service->ClosePersistentNotification(notification_id());

  ASSERT_EQ(0u, GetStoredNotificationsSync(context.get(), origin).size());

  context->ReadNotificationDataAndRecordInteraction(
      notification_id(), origin,
      PlatformNotificationContext::Interaction::CLOSED,
      base::BindOnce(&PlatformNotificationContextTest::DidReadNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The notification was removed, so we shouldn't be able to read it from
  // the database anymore.
  EXPECT_FALSE(success());
}

TEST_F(PlatformNotificationContextTest, WriteDisplaysNotification) {
  NotificationBrowserClient notification_browser_client(browser_context());
  SetBrowserClientForTesting(&notification_browser_client);

  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();

  GURL origin("https://example.com");
  NotificationDatabaseData notification_database_data;

  context->WriteNotificationData(
      next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
      origin, notification_database_data,
      base::BindOnce(&PlatformNotificationContextTest::DidWriteNotificationData,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The write operation should have succeeded with a notification id.
  ASSERT_TRUE(success());
  EXPECT_FALSE(notification_id().empty());

  // The written notification should be shown now.
  std::set<std::string> displayed_notification_ids =
      GetDisplayedNotificationsSync(
          notification_browser_client.GetPlatformNotificationService(
              browser_context()));

  ASSERT_EQ(1u, displayed_notification_ids.size());
  EXPECT_EQ(notification_id(), *displayed_notification_ids.begin());
}

TEST_F(PlatformNotificationContextTest, WriteReadNotificationResources) {
  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();
  GURL origin("https://example.com");

  // Write a notification to the database.
  NotificationDatabaseData data;
  std::string notification_id =
      WriteNotificationDataSync(context.get(), origin, data);

  // There should not be any stored resources yet.
  ASSERT_FALSE(
      ReadNotificationResourcesSync(context.get(), notification_id, origin));

  // Store resources for the new notification.
  std::vector<NotificationResourceData> resources;
  resources.push_back(
      {notification_id, origin, blink::NotificationResources()});
  // Also try inserting resources for an invalid notification id.
  std::string invalid_id = "invalid-id";
  resources.push_back({invalid_id, origin, blink::NotificationResources()});
  // Writing resources should succeed.
  ASSERT_TRUE(
      WriteNotificationResourcesSync(context.get(), std::move(resources)));

  // Reading resources should succeed now.
  ASSERT_TRUE(
      ReadNotificationResourcesSync(context.get(), notification_id, origin));

  // The resources for the missing notification id should have been ignored.
  ASSERT_FALSE(
      ReadNotificationResourcesSync(context.get(), invalid_id, origin));
}

TEST_F(PlatformNotificationContextTest, ReDisplayNotifications) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNotificationTriggers);

  NotificationBrowserClient notification_browser_client(browser_context());
  SetBrowserClientForTesting(&notification_browser_client);
  PlatformNotificationService* service =
      notification_browser_client.GetPlatformNotificationService(
          browser_context());

  scoped_refptr<PlatformNotificationContextImpl> context =
      CreatePlatformNotificationContext();
  GURL origin("https://example.com");

  // Store the initial data:
  // 1 notification with a future trigger and resources.
  NotificationDatabaseData data1;
  data1.notification_resources = blink::NotificationResources();
  data1.notification_data.show_trigger_timestamp =
      base::Time::Now() + base::TimeDelta::FromDays(10);
  WriteNotificationDataSync(context.get(), origin, data1);
  // 1 notification with stored resources.
  NotificationDatabaseData data2;
  std::string notification_id =
      WriteNotificationDataSync(context.get(), origin, data2);
  std::vector<NotificationResourceData> resources;
  resources.push_back(
      {notification_id, origin, blink::NotificationResources()});
  WriteNotificationResourcesSync(context.get(), std::move(resources));
  // 1 notification without resources.
  NotificationDatabaseData data3;
  WriteNotificationDataSync(context.get(), origin, data3);

  // Expect to see the two notifications without trigger.
  ASSERT_EQ(2u, GetDisplayedNotificationsSync(service).size());

  // Close the notification with resources without deleting it.
  service->ClosePersistentNotification(notification_id);
  ASSERT_EQ(1u, GetDisplayedNotificationsSync(service).size());

  base::RunLoop run_loop;
  context->ReDisplayNotifications(
      {origin}, base::BindLambdaForTesting([&](size_t display_count) {
        // Expect the notification with resources to be reshown.
        ASSERT_EQ(1u, display_count);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Expect to see the two notifications again.
  ASSERT_EQ(2u, GetDisplayedNotificationsSync(service).size());
  // The resources for the reshown notification should have been deleted.
  ASSERT_FALSE(
      ReadNotificationResourcesSync(context.get(), notification_id, origin));
}

}  // namespace content
