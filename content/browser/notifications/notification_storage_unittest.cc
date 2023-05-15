// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_storage.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/uuid.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/gurl.h"

namespace content {

class NotificationStorageTest : public ::testing::Test {
 public:
  NotificationStorageTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        url_(GURL("https://example.com")),
        origin_(url::Origin::Create(url_)),
        success_(false),
        service_worker_registration_id_(
            blink::mojom::kInvalidServiceWorkerRegistrationId) {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    storage_ =
        std::make_unique<NotificationStorage>(helper_->context_wrapper());
  }

  void DidRegisterServiceWorker(base::OnceClosure quit_closure,
                                blink::ServiceWorkerStatusCode status,
                                const std::string& status_message,
                                int64_t service_worker_registration_id) {
    DCHECK(service_worker_registration_id_);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status) << status_message;

    service_worker_registration_id_ = service_worker_registration_id;

    std::move(quit_closure).Run();
  }

  void DidFindServiceWorkerRegistration(
      scoped_refptr<ServiceWorkerRegistration>* out_service_worker_registration,
      base::OnceClosure quit_closure,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
    DCHECK(out_service_worker_registration);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
        << blink::ServiceWorkerStatusToString(status);

    *out_service_worker_registration = service_worker_registration;

    std::move(quit_closure).Run();
  }

  // Registers a Service Worker for the testing origin and returns its
  // |service_worker_registration_id|. If registration failed, this will be
  // blink::mojom::kInvalidServiceWorkerRegistrationId. The
  // ServiceWorkerRegistration will be kept alive for the test's lifetime.
  int64_t RegisterServiceWorker() {
    GURL script_url = url_;
    const blink::StorageKey key = blink::StorageKey::CreateFirstParty(origin_);
    {
      blink::mojom::ServiceWorkerRegistrationOptions options;
      options.scope = url_;
      base::RunLoop run_loop;
      helper_->context()->RegisterServiceWorker(
          script_url, key, options,
          blink::mojom::FetchClientSettingsObject::New(),
          base::BindOnce(&NotificationStorageTest::DidRegisterServiceWorker,
                         base::Unretained(this), run_loop.QuitClosure()),
          /*requesting_frame_id=*/GlobalRenderFrameHostId(),
          PolicyContainerPolicies());
      run_loop.Run();
    }

    if (service_worker_registration_id_ ==
        blink::mojom::kInvalidServiceWorkerRegistrationId) {
      ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    scoped_refptr<ServiceWorkerRegistration> service_worker_registration;

    {
      base::RunLoop run_loop;
      helper_->context()->registry()->FindRegistrationForId(
          service_worker_registration_id_, key,
          base::BindOnce(
              &NotificationStorageTest::DidFindServiceWorkerRegistration,
              base::Unretained(this), &service_worker_registration,
              run_loop.QuitClosure()));
      run_loop.Run();
    }

    // Wait for the worker to be activated.
    task_environment_.RunUntilIdle();

    if (!service_worker_registration) {
      ADD_FAILURE() << "Could not find the new Service Worker registration.";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    service_worker_registrations_.push_back(
        std::move(service_worker_registration));

    return service_worker_registration_id_;
  }

  void DidWriteNotificationDataSynchronous(base::OnceClosure quit_closure,
                                           bool success,
                                           const std::string& notification_id) {
    success_ = success;
    notification_id_ = notification_id;
    std::move(quit_closure).Run();
  }

  void WriteNotificationDataSynchronous(const NotificationDatabaseData& data) {
    base::RunLoop run_loop;
    storage_->WriteNotificationData(
        data, base::BindOnce(
                  &NotificationStorageTest::DidWriteNotificationDataSynchronous,
                  base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void DidReadNotificationDataAndRecordInteractionSynchronous(
      base::OnceClosure quit_closure,
      bool success,
      const NotificationDatabaseData& data) {
    success_ = success;
    out_data_ = data;
    std::move(quit_closure).Run();
  }

  NotificationDatabaseData ReadNotificationDataAndRecordInteractionSynchronous(
      int64_t service_worker_registration_id,
      const std::string& notification_id,
      PlatformNotificationContext::Interaction interaction) {
    base::RunLoop run_loop;
    storage_->ReadNotificationDataAndRecordInteraction(
        service_worker_registration_id, notification_id, interaction,
        base::BindOnce(
            &NotificationStorageTest::
                DidReadNotificationDataAndRecordInteractionSynchronous,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return out_data_;
  }

  // Generates a random notification ID. The format of the ID is opaque.
  std::string GenerateNotificationId() {
    return base::Uuid::GenerateRandomV4().AsLowercaseString();
  }

 protected:
  BrowserTaskEnvironment task_environment_;  // Must be first member
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  GURL url_;
  url::Origin origin_;
  TestBrowserContext browser_context_;
  bool success_;
  int64_t service_worker_registration_id_;
  NotificationDatabaseData out_data_;

 private:
  std::unique_ptr<NotificationStorage> storage_;
  std::string notification_id_;

  // Vector of ServiceWorkerRegistration instances that have to be kept alive
  // for the lifetime of this test.
  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      service_worker_registrations_;
};

TEST_F(NotificationStorageTest, WriteReadNotification) {
  NotificationDatabaseData data;
  data.notification_id = GenerateNotificationId();
  data.origin = url_;
  data.service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            data.service_worker_registration_id);
  WriteNotificationDataSynchronous(data);
  ASSERT_TRUE(success_);

  NotificationDatabaseData read_data =
      ReadNotificationDataAndRecordInteractionSynchronous(
          data.service_worker_registration_id, data.notification_id,
          PlatformNotificationContext::Interaction::NONE);
  ASSERT_TRUE(success_);
  EXPECT_EQ(data.origin, read_data.origin);
  EXPECT_EQ(data.notification_id, read_data.notification_id);
  EXPECT_EQ(data.service_worker_registration_id,
            read_data.service_worker_registration_id);
}

TEST_F(NotificationStorageTest, ReadInvalidNotification) {
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);
  ReadNotificationDataAndRecordInteractionSynchronous(
      service_worker_registration_id, "bad_id",
      PlatformNotificationContext::Interaction::NONE);
  ASSERT_FALSE(success_);
}

TEST_F(NotificationStorageTest, ReadAndUpdateInteraction) {
  NotificationDatabaseData data, read_data;
  data.notification_id = GenerateNotificationId();
  data.origin = url_;
  data.service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            data.service_worker_registration_id);

  WriteNotificationDataSynchronous(data);
  ASSERT_TRUE(success_);

  // Check that the time deltas have not yet been set.
  EXPECT_FALSE(read_data.time_until_first_click_millis.has_value());
  EXPECT_FALSE(read_data.time_until_last_click_millis.has_value());
  EXPECT_FALSE(read_data.time_until_close_millis.has_value());

  // Check that when a notification has an interaction, the appropriate field is
  // updated on the read.
  read_data = ReadNotificationDataAndRecordInteractionSynchronous(
      data.service_worker_registration_id, data.notification_id,
      PlatformNotificationContext::Interaction::CLICKED);
  ASSERT_TRUE(success_);
  EXPECT_EQ(1, read_data.num_clicks);

  read_data = ReadNotificationDataAndRecordInteractionSynchronous(
      data.service_worker_registration_id, data.notification_id,
      PlatformNotificationContext::Interaction::ACTION_BUTTON_CLICKED);
  ASSERT_TRUE(success_);
  EXPECT_EQ(1, read_data.num_action_button_clicks);

  read_data = ReadNotificationDataAndRecordInteractionSynchronous(
      data.service_worker_registration_id, data.notification_id,
      PlatformNotificationContext::Interaction::ACTION_BUTTON_CLICKED);
  ASSERT_TRUE(success_);
  EXPECT_EQ(2, read_data.num_action_button_clicks);

  // Check that the click timestamps are correctly updated.
  EXPECT_TRUE(read_data.time_until_first_click_millis.has_value());
  EXPECT_TRUE(read_data.time_until_last_click_millis.has_value());

  // Check that when a read with a CLOSED interaction occurs, the correct
  // field is updated.
  read_data = ReadNotificationDataAndRecordInteractionSynchronous(
      data.service_worker_registration_id, data.notification_id,
      PlatformNotificationContext::Interaction::CLOSED);
  ASSERT_TRUE(success_);
  EXPECT_EQ(true, read_data.time_until_close_millis.has_value());
}

}  // namespace content
