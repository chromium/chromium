// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/blink_notification_service_impl.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/mock_platform_notification_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"

using ::testing::_;
using ::testing::Return;

namespace content {

namespace {

const char kTestOrigin[] = "https://example.com";
const char kTestServiceWorkerUrl[] = "https://example.com/sw.js";
const char kBadMessageImproperNotificationImage[] =
    "Received an unexpected message with image while notification images are "
    "disabled.";
const char kBadMessageInvalidNotificationTriggerTimestamp[] =
    "Received an invalid notification trigger timestamp.";

class MockNonPersistentNotificationListener
    : public blink::mojom::NonPersistentNotificationListener {
 public:
  MockNonPersistentNotificationListener() = default;
  ~MockNonPersistentNotificationListener() override = default;

  mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener>
  GetRemote() {
    mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  // NonPersistentNotificationListener interface.
  void OnShow() override {}
  void OnClick(OnClickCallback completed_closure) override {
    std::move(completed_closure).Run();
  }
  void OnClose(OnCloseCallback completed_closure) override {
    std::move(completed_closure).Run();
  }

 private:
  mojo::Receiver<blink::mojom::NonPersistentNotificationListener> receiver_{
      this};
};

}  // anonymous namespace

class BlinkNotificationServiceImplTest : public ::testing::Test {
 public:
  // Using REAL_IO_THREAD would give better coverage for thread safety, but
  // at time of writing EmbeddedWorkerTestHelper didn't seem to support that.
  BlinkNotificationServiceImplTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        embedded_worker_helper_(
            std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath())),
        render_process_host_(&browser_context_) {
    browser_context_.SetPlatformNotificationService(
        std::make_unique<MockPlatformNotificationService>(&browser_context_));
  }

  BlinkNotificationServiceImplTest(const BlinkNotificationServiceImplTest&) =
      delete;
  BlinkNotificationServiceImplTest& operator=(
      const BlinkNotificationServiceImplTest&) = delete;

  ~BlinkNotificationServiceImplTest() override = default;

  // ::testing::Test overrides.
  void SetUp() override {
    notification_context_ = new PlatformNotificationContextImpl(
        base::FilePath(), &browser_context_,
        embedded_worker_helper_->context_wrapper());
    notification_context_->Initialize();

    // Wait for notification context to be initialized to avoid TSAN detecting
    // a memory race in tests - in production the PlatformNotificationContext
    // will be initialized long before it is read from so this is fine.
    RunAllTasksUntilIdle();

    contents_ = CreateTestWebContents();

    storage_key_ = blink::StorageKey::CreateFirstParty(
        url::Origin::Create(GURL(kTestOrigin)));

    notification_service_ = std::make_unique<BlinkNotificationServiceImpl>(
        notification_context_.get(), &browser_context_,
        embedded_worker_helper_->context_wrapper(), &render_process_host_,
        storage_key_,
        /*document_url=*/GURL(),
        contents_.get()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
        RenderProcessHost::NotificationServiceCreatorType::kDocument,
        notification_service_remote_.BindNewPipeAndPassReceiver());

    // Provide a mock permission manager to the |browser_context_|.
    browser_context_.SetPermissionControllerDelegate(
        std::make_unique<testing::NiceMock<MockPermissionManager>>());

    mojo::SetDefaultProcessErrorHandler(
        base::BindRepeating(&BlinkNotificationServiceImplTest::OnMojoError,
                            base::Unretained(this)));
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    embedded_worker_helper_.reset();

    // Give pending shutdown operations a chance to finish.
    base::RunLoop().RunUntilIdle();
  }

  void RegisterServiceWorker(
      scoped_refptr<ServiceWorkerRegistration>* service_worker_registration) {
    int64_t service_worker_registration_id =
        blink::mojom::kInvalidServiceWorkerRegistrationId;

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GURL(kTestOrigin);

    {
      base::RunLoop run_loop;
      embedded_worker_helper_->context()->RegisterServiceWorker(
          GURL(kTestServiceWorkerUrl), storage_key_, options,
          blink::mojom::FetchClientSettingsObject::New(),
          base::BindOnce(
              &BlinkNotificationServiceImplTest::DidRegisterServiceWorker,
              base::Unretained(this), &service_worker_registration_id,
              run_loop.QuitClosure()),
          /*requesting_frame_id=*/GlobalRenderFrameHostId(),
          PolicyContainerPolicies());
      run_loop.Run();
    }

    if (service_worker_registration_id ==
        blink::mojom::kInvalidServiceWorkerRegistrationId) {
      ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
    }

    {
      base::RunLoop run_loop;
      embedded_worker_helper_->context()->registry()->FindRegistrationForId(
          service_worker_registration_id, storage_key_,
          base::BindOnce(&BlinkNotificationServiceImplTest::
                             DidFindServiceWorkerRegistration,
                         base::Unretained(this), service_worker_registration,
                         run_loop.QuitClosure()));

      run_loop.Run();
    }

    // Wait for the worker to be activated.
    base::RunLoop().RunUntilIdle();

    if (!*service_worker_registration) {
      ADD_FAILURE() << "Could not find the new Service Worker registration.";
    }
  }

  void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                                base::OnceClosure quit_closure,
                                blink::ServiceWorkerStatusCode status,
                                const std::string& status_message,
                                int64_t service_worker_registration_id) {
    DCHECK(out_service_worker_registration_id);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
        << blink::ServiceWorkerStatusToString(status);

    *out_service_worker_registration_id = service_worker_registration_id;

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

  void DidGetPermissionStatus(
      base::OnceClosure quit_closure,
      blink::mojom::PermissionStatus permission_status) {
    permission_callback_result_ = permission_status;
    std::move(quit_closure).Run();
  }

  blink::mojom::PermissionStatus GetPermissionCallbackResult() {
    return permission_callback_result_;
  }

  void DidDisplayPersistentNotification(
      base::OnceClosure quit_closure,
      blink::mojom::PersistentNotificationError error) {
    display_persistent_callback_result_ = error;
    std::move(quit_closure).Run();
  }

  void DidGetNotifications(
      base::OnceClosure quit_closure,
      const std::vector<std::string>& notification_ids,
      const std::vector<blink::PlatformNotificationData>& notification_datas) {
    get_notifications_callback_result_ = notification_ids;
    std::move(quit_closure).Run();
  }

  void DidGetNotificationDataFromContext(
      base::OnceClosure quit_closure,
      bool success,
      const std::vector<NotificationDatabaseData>& notification_datas) {
    get_notifications_data_ = notification_datas;
    std::move(quit_closure).Run();
  }

  void DidGetNotificationResourcesFromContext(
      base::OnceClosure quit_closure,
      bool success,
      const blink::NotificationResources& notification_resources) {
    if (success) {
      get_notification_resources_ = notification_resources;
    } else {
      get_notification_resources_ = std::nullopt;
    }
    std::move(quit_closure).Run();
  }

  void DidGetDisplayedNotifications(base::OnceClosure quit_closure,
                                    std::set<std::string> notification_ids,
                                    bool supports_synchronization) {
    get_displayed_callback_result_ = std::move(notification_ids);
    std::move(quit_closure).Run();
  }

  void DidReadNotificationData(base::OnceClosure quit_closure,
                               bool success,
                               const NotificationDatabaseData& data) {
    read_notification_data_callback_result_ = success;
    std::move(quit_closure).Run();
  }

  void DisplayNonPersistentNotification(
      const std::string& token,
      const blink::PlatformNotificationData& platform_notification_data,
      const blink::NotificationResources& notification_resources,
      mojo::PendingRemote<blink::mojom::NonPersistentNotificationListener>
          event_listener_remote) {
    notification_service_remote_->DisplayNonPersistentNotification(
        token, platform_notification_data, notification_resources,
        std::move(event_listener_remote));
    // TODO(crbug.com/40551046): Pass a callback to
    // DisplayNonPersistentNotification instead of waiting for all tasks to run
    // here; a callback parameter will be needed anyway to enable
    // non-persistent notification event acknowledgements - see bug.
    RunAllTasksUntilIdle();
  }

  void DisplayPersistentNotificationSync(
      int64_t service_worker_registration_id,
      const blink::PlatformNotificationData& platform_notification_data,
      const blink::NotificationResources& notification_resources) {
    base::RunLoop run_loop;
    notification_service_remote_.set_disconnect_handler(run_loop.QuitClosure());
    notification_service_remote_->DisplayPersistentNotification(
        service_worker_registration_id, platform_notification_data,
        notification_resources,
        base::BindOnce(
            &BlinkNotificationServiceImplTest::DidDisplayPersistentNotification,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::vector<std::string> GetNotificationsSync(
      int64_t service_worker_registration_id,
      const std::string& filter_tag,
      bool include_triggered) {
    base::RunLoop run_loop;
    notification_service_->GetNotifications(
        service_worker_registration_id, filter_tag, include_triggered,
        base::BindOnce(&BlinkNotificationServiceImplTest::DidGetNotifications,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return get_notifications_callback_result_;
  }

  size_t CountDisplayedNotificationsSync(int64_t service_worker_registration_id,
                                         const std::string& filter_tag) {
    return GetNotificationsSync(service_worker_registration_id, filter_tag,
                                /* include_triggered= */ false)
        .size();
  }

  size_t CountScheduledNotificationsSync(int64_t service_worker_registration_id,
                                         const std::string& filter_tag) {
    return GetNotificationsSync(service_worker_registration_id, filter_tag,
                                /* include_triggered= */ true)
        .size();
  }

  std::vector<NotificationDatabaseData> GetNotificationDataFromContextSync(
      int64_t service_worker_registration_id,
      const std::string& filter_tag,
      bool include_triggered) {
    base::RunLoop run_loop;
    notification_context_->ReadAllNotificationDataForServiceWorkerRegistration(
        GURL(kTestOrigin), service_worker_registration_id,
        base::BindOnce(&BlinkNotificationServiceImplTest::
                           DidGetNotificationDataFromContext,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return get_notifications_data_;
  }

  std::optional<blink::NotificationResources>
  GetNotificationResourcesFromContextSync(const std::string& notification_id) {
    base::RunLoop run_loop;
    notification_context_->ReadNotificationResources(
        notification_id, GURL(kTestOrigin),
        base::BindOnce(&BlinkNotificationServiceImplTest::
                           DidGetNotificationResourcesFromContext,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return get_notification_resources_;
  }

  // Synchronous wrapper of
  // PlatformNotificationService::GetDisplayedNotifications
  std::set<std::string> GetDisplayedNotifications() {
    base::RunLoop run_loop;
    browser_context_.GetPlatformNotificationService()
        ->GetDisplayedNotifications(base::BindOnce(
            &BlinkNotificationServiceImplTest::DidGetDisplayedNotifications,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return get_displayed_callback_result_;
  }

  // Synchronous wrapper of
  // PlatformNotificationContext::ReadNotificationData
  bool ReadNotificationData(const std::string& notification_id) {
    base::RunLoop run_loop;
    notification_context_->ReadNotificationDataAndRecordInteraction(
        notification_id, GURL(kTestOrigin),
        PlatformNotificationContext::Interaction::NONE,
        base::BindOnce(
            &BlinkNotificationServiceImplTest::DidReadNotificationData,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return read_notification_data_callback_result_;
  }

  // Updates the permission status for the |kTestOrigin| to the given
  // |permission_status| through the PermissionManager.
  void SetPermissionStatus(blink::mojom::PermissionStatus permission_status) {
    MockPermissionManager* mock_permission_manager =
        static_cast<MockPermissionManager*>(
            browser_context_.GetPermissionControllerDelegate());

    ON_CALL(*mock_permission_manager,
            GetPermissionStatusForCurrentDocument(
                blink::PermissionType::NOTIFICATIONS, _, _))
        .WillByDefault(Return(permission_status));
    ON_CALL(*mock_permission_manager,
            GetPermissionStatusForWorker(blink::PermissionType::NOTIFICATIONS,
                                         _, _))
        .WillByDefault(Return(permission_status));
  }

 protected:
  void OnMojoError(const std::string& error) { bad_messages_.push_back(error); }

  BrowserTaskEnvironment task_environment_;  // Must be first member.

  blink::StorageKey storage_key_;

  std::unique_ptr<EmbeddedWorkerTestHelper> embedded_worker_helper_;

  std::unique_ptr<BlinkNotificationServiceImpl> notification_service_;

  mojo::Remote<blink::mojom::NotificationService> notification_service_remote_;

  TestBrowserContext browser_context_;

  MockRenderProcessHost render_process_host_;

  scoped_refptr<PlatformNotificationContextImpl> notification_context_;

  MockNonPersistentNotificationListener non_persistent_notification_listener_;

  blink::mojom::PersistentNotificationError display_persistent_callback_result_;

  std::vector<std::string> bad_messages_;

 private:
  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    auto site_instance = content::SiteInstance::Create(&browser_context_);
    return content::WebContentsTester::CreateTestWebContents(
        &browser_context_, std::move(site_instance));
  }

  blink::mojom::PermissionStatus permission_callback_result_ =
      blink::mojom::PermissionStatus::ASK;

  std::set<std::string> get_displayed_callback_result_;

  std::vector<std::string> get_notifications_callback_result_;

  std::vector<NotificationDatabaseData> get_notifications_data_;

  std::optional<blink::NotificationResources> get_notification_resources_;

  bool read_notification_data_callback_result_ = false;

  RenderViewHostTestEnabler rvh_enabler_;

  std::unique_ptr<WebContents> contents_;
};

TEST_F(BlinkNotificationServiceImplTest, GetPermissionStatus) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  {
    base::RunLoop run_loop;
    notification_service_->GetPermissionStatus(base::BindOnce(
        &BlinkNotificationServiceImplTest::DidGetPermissionStatus,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED,
            GetPermissionCallbackResult());

  SetPermissionStatus(blink::mojom::PermissionStatus::DENIED);

  {
    base::RunLoop run_loop;
    notification_service_->GetPermissionStatus(base::BindOnce(
        &BlinkNotificationServiceImplTest::DidGetPermissionStatus,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED,
            GetPermissionCallbackResult());

  SetPermissionStatus(blink::mojom::PermissionStatus::ASK);

  {
    base::RunLoop run_loop;
    notification_service_->GetPermissionStatus(base::BindOnce(
        &BlinkNotificationServiceImplTest::DidGetPermissionStatus,
        base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(blink::mojom::PermissionStatus::ASK, GetPermissionCallbackResult());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayNonPersistentNotificationWithPermission) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  DisplayNonPersistentNotification(
      "token", blink::PlatformNotificationData(),
      blink::NotificationResources(),
      non_persistent_notification_listener_.GetRemote());

  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayNonPersistentNotificationWithoutPermission) {
  SetPermissionStatus(blink::mojom::PermissionStatus::DENIED);

  DisplayNonPersistentNotification(
      "token", blink::PlatformNotificationData(),
      blink::NotificationResources(),
      non_persistent_notification_listener_.GetRemote());

  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayNonPersistentNotificationWithContentImageSwitchOn) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  blink::NotificationResources resources;
  resources.image = gfx::test::CreateBitmap(200, 100, SK_ColorMAGENTA);
  DisplayNonPersistentNotification(
      "token", blink::PlatformNotificationData(), resources,
      non_persistent_notification_listener_.GetRemote());

  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayNonPersistentNotificationWithContentImageSwitchOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kNotificationContentImage);
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  ASSERT_TRUE(bad_messages_.empty());
  blink::NotificationResources resources;
  resources.image = gfx::test::CreateBitmap(200, 100, SK_ColorMAGENTA);
  DisplayNonPersistentNotification(
      "token", blink::PlatformNotificationData(), resources,
      non_persistent_notification_listener_.GetRemote());
  EXPECT_EQ(1u, bad_messages_.size());
  EXPECT_EQ(kBadMessageImproperNotificationImage, bad_messages_[0]);
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayPersistentNotificationWithContentImageSwitchOn) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  blink::NotificationResources resources;
  resources.image = gfx::test::CreateBitmap(200, 100, SK_ColorMAGENTA);
  DisplayPersistentNotificationSync(
      registration->id(), blink::PlatformNotificationData(), resources);

  EXPECT_EQ(blink::mojom::PersistentNotificationError::NONE,
            display_persistent_callback_result_);

  // Wait for service to receive the Display call.
  RunAllTasksUntilIdle();

  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayPersistentNotificationWithContentImageSwitchOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kNotificationContentImage);
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  ASSERT_TRUE(bad_messages_.empty());
  blink::NotificationResources resources;
  resources.image = gfx::test::CreateBitmap(200, 100, SK_ColorMAGENTA);
  DisplayPersistentNotificationSync(
      registration->id(), blink::PlatformNotificationData(), resources);
  EXPECT_EQ(1u, bad_messages_.size());
  EXPECT_EQ(kBadMessageImproperNotificationImage, bad_messages_[0]);
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayPersistentNotificationWithPermission) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  DisplayPersistentNotificationSync(registration->id(),
                                    blink::PlatformNotificationData(),
                                    blink::NotificationResources());

  EXPECT_EQ(blink::mojom::PersistentNotificationError::NONE,
            display_persistent_callback_result_);

  // Wait for service to receive the Display call.
  RunAllTasksUntilIdle();

  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest, CloseDisplayedPersistentNotification) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  DisplayPersistentNotificationSync(registration->id(),
                                    blink::PlatformNotificationData(),
                                    blink::NotificationResources());

  ASSERT_EQ(blink::mojom::PersistentNotificationError::NONE,
            display_persistent_callback_result_);

  // Wait for service to receive the Display call.
  RunAllTasksUntilIdle();

  std::set<std::string> notification_ids = GetDisplayedNotifications();
  ASSERT_EQ(1u, notification_ids.size());

  notification_service_->ClosePersistentNotification(*notification_ids.begin());

  // Wait for service to receive the Close call.
  RunAllTasksUntilIdle();

  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       ClosePersistentNotificationDeletesFromDatabase) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  DisplayPersistentNotificationSync(registration->id(),
                                    blink::PlatformNotificationData(),
                                    blink::NotificationResources());

  ASSERT_EQ(blink::mojom::PersistentNotificationError::NONE,
            display_persistent_callback_result_);

  // Wait for service to receive the Display call.
  RunAllTasksUntilIdle();

  std::set<std::string> notification_ids = GetDisplayedNotifications();
  ASSERT_EQ(1u, notification_ids.size());

  std::string notification_id = *notification_ids.begin();

  // Check data was indeed written.
  ASSERT_EQ(true /* success */, ReadNotificationData(notification_id));

  notification_service_->ClosePersistentNotification(notification_id);

  // Wait for service to receive the Close call.
  RunAllTasksUntilIdle();

  // Data should now be deleted.
  EXPECT_EQ(false /* success */, ReadNotificationData(notification_id));
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayPersistentNotificationWithoutPermission) {
  SetPermissionStatus(blink::mojom::PermissionStatus::DENIED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  DisplayPersistentNotificationSync(registration->id(),
                                    blink::PlatformNotificationData(),
                                    blink::NotificationResources());

  EXPECT_EQ(blink::mojom::PersistentNotificationError::PERMISSION_DENIED,
            display_persistent_callback_result_);

  // Give Service a chance to receive any unexpected Display calls.
  RunAllTasksUntilIdle();

  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayMultiplePersistentNotifications) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  DisplayPersistentNotificationSync(registration->id(),
                                    blink::PlatformNotificationData(),
                                    blink::NotificationResources());

  DisplayPersistentNotificationSync(registration->id(),
                                    blink::PlatformNotificationData(),
                                    blink::NotificationResources());

  // Wait for service to receive all the Display calls.
  RunAllTasksUntilIdle();

  EXPECT_EQ(2u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest, GetNotifications) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  EXPECT_EQ(0u, CountDisplayedNotificationsSync(registration->id(),
                                                /* filter_tag= */ ""));

  DisplayPersistentNotificationSync(registration->id(),
                                    blink::PlatformNotificationData(),
                                    blink::NotificationResources());

  // Wait for service to receive all the Display calls.
  RunAllTasksUntilIdle();

  EXPECT_EQ(1u, CountDisplayedNotificationsSync(registration->id(),
                                                /* filter_tag= */ ""));
}

TEST_F(BlinkNotificationServiceImplTest, GetNotificationsWithoutPermission) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  DisplayPersistentNotificationSync(registration->id(),
                                    blink::PlatformNotificationData(),
                                    blink::NotificationResources());

  // Wait for service to receive all the Display calls.
  RunAllTasksUntilIdle();

  SetPermissionStatus(blink::mojom::PermissionStatus::DENIED);

  EXPECT_EQ(0u, CountDisplayedNotificationsSync(registration->id(),
                                                /* filter_tag= */ ""));
}

TEST_F(BlinkNotificationServiceImplTest, GetNotificationsWithFilter) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  blink::PlatformNotificationData platform_notification_data;
  platform_notification_data.tag = "tagA";

  blink::PlatformNotificationData other_platform_notification_data;
  other_platform_notification_data.tag = "tagB";

  DisplayPersistentNotificationSync(registration->id(),
                                    platform_notification_data,
                                    blink::NotificationResources());

  DisplayPersistentNotificationSync(registration->id(),
                                    other_platform_notification_data,
                                    blink::NotificationResources());

  // Wait for service to receive all the Display calls.
  RunAllTasksUntilIdle();

  EXPECT_EQ(2u, CountDisplayedNotificationsSync(registration->id(), ""));
  EXPECT_EQ(1u, CountDisplayedNotificationsSync(registration->id(), "tagA"));
  EXPECT_EQ(1u, CountDisplayedNotificationsSync(registration->id(), "tagB"));
  EXPECT_EQ(0u, CountDisplayedNotificationsSync(registration->id(), "tagC"));
  EXPECT_EQ(0u, CountDisplayedNotificationsSync(registration->id(), "tag"));
}

TEST_F(BlinkNotificationServiceImplTest, GetTriggeredNotificationsWithFilter) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNotificationTriggers);

  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  base::Time timestamp = base::Time::Now() + base::Seconds(10);
  blink::PlatformNotificationData platform_notification_data;
  platform_notification_data.tag = "tagA";
  platform_notification_data.show_trigger_timestamp = timestamp;

  blink::PlatformNotificationData other_platform_notification_data;
  other_platform_notification_data.tag = "tagB";
  other_platform_notification_data.show_trigger_timestamp = timestamp;

  blink::PlatformNotificationData displayed_notification_data;
  displayed_notification_data.tag = "tagC";

  DisplayPersistentNotificationSync(registration->id(),
                                    platform_notification_data,
                                    blink::NotificationResources());

  DisplayPersistentNotificationSync(registration->id(),
                                    other_platform_notification_data,
                                    blink::NotificationResources());

  // Wait for service to receive all the Display calls.
  RunAllTasksUntilIdle();

  EXPECT_EQ(0u, CountDisplayedNotificationsSync(registration->id(), ""));
  EXPECT_EQ(2u, CountScheduledNotificationsSync(registration->id(), ""));
  EXPECT_EQ(1u, CountScheduledNotificationsSync(registration->id(), "tagA"));
  EXPECT_EQ(1u, CountScheduledNotificationsSync(registration->id(), "tagB"));
  EXPECT_EQ(0u, CountScheduledNotificationsSync(registration->id(), "tagC"));
  EXPECT_EQ(0u, CountScheduledNotificationsSync(registration->id(), "tag"));
}

TEST_F(BlinkNotificationServiceImplTest, ResourcesStoredForTriggered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNotificationTriggers);

  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  base::Time timestamp = base::Time::Now() + base::Seconds(10);
  blink::PlatformNotificationData scheduled_notification_data;
  scheduled_notification_data.tag = "tagA";
  scheduled_notification_data.show_trigger_timestamp = timestamp;

  blink::NotificationResources resources;
  resources.notification_icon =
      gfx::test::CreateBitmap(/*size=*/10, SK_ColorMAGENTA);

  blink::PlatformNotificationData displayed_notification_data;
  displayed_notification_data.tag = "tagB";

  DisplayPersistentNotificationSync(registration->id(),
                                    scheduled_notification_data, resources);

  DisplayPersistentNotificationSync(registration->id(),
                                    displayed_notification_data, resources);

  // Wait for service to receive all the Display calls.
  RunAllTasksUntilIdle();

  auto notification_data =
      GetNotificationDataFromContextSync(registration->id(), "", true);

  EXPECT_EQ(2u, notification_data.size());

  auto notification_a = notification_data[0].notification_data.tag == "tagA"
                            ? notification_data[0]
                            : notification_data[1];
  auto notification_b = notification_data[0].notification_data.tag == "tagB"
                            ? notification_data[0]
                            : notification_data[1];
  auto stored_resources_a =
      GetNotificationResourcesFromContextSync(notification_a.notification_id);
  auto stored_resources_b =
      GetNotificationResourcesFromContextSync(notification_b.notification_id);

  EXPECT_TRUE(stored_resources_a.has_value());
  EXPECT_EQ(10, stored_resources_a.value().notification_icon.width());

  EXPECT_FALSE(stored_resources_b.has_value());
}

TEST_F(BlinkNotificationServiceImplTest, NotCallingDisplayForTriggered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNotificationTriggers);

  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  base::Time timestamp = base::Time::Now() + base::Seconds(10);
  blink::PlatformNotificationData scheduled_notification_data;
  scheduled_notification_data.show_trigger_timestamp = timestamp;
  blink::NotificationResources resources;

  DisplayPersistentNotificationSync(registration->id(),
                                    scheduled_notification_data, resources);

  // Wait for service to receive all the Display calls.
  RunAllTasksUntilIdle();

  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest, RejectsTriggerTimestampOverAYear) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNotificationTriggers);

  ASSERT_TRUE(bad_messages_.empty());

  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> registration;
  RegisterServiceWorker(&registration);

  base::Time timestamp = base::Time::Now() +
                         blink::kMaxNotificationShowTriggerDelay +
                         base::Days(1);

  blink::PlatformNotificationData scheduled_notification_data;
  scheduled_notification_data.show_trigger_timestamp = timestamp;
  blink::NotificationResources resources;

  DisplayPersistentNotificationSync(registration->id(),
                                    scheduled_notification_data, resources);

  EXPECT_EQ(1u, bad_messages_.size());
  EXPECT_EQ(kBadMessageInvalidNotificationTriggerTimestamp, bad_messages_[0]);
}

}  // namespace content
