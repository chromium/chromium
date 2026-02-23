// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/save_to_drive/ui_bundled/save_to_drive_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/drive/model/drive_metrics.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/drive/model/test_drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/test_drive_service.h"
#import "ios/chrome/browser/drive/model/upload_task.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/account_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/manage_storage_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const FakeSystemIdentity* kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";

}  // namespace

#pragma mark - FakeDownloadManagerTabHelper

// Fake `DownloadManagerTabHelper` to override `StartDownload()`.
class FakeDownloadManagerTabHelper final : public DownloadManagerTabHelper {
 public:
  explicit FakeDownloadManagerTabHelper(web::WebState* web_state)
      : DownloadManagerTabHelper(web_state) {}

  static void CreateForWebState(web::WebState* web_state) {
    web_state->SetUserData(
        UserDataKey(),
        std::make_unique<FakeDownloadManagerTabHelper>(web_state));
  }

  void StartDownload(web::DownloadTask* task) override {
    download_task_started_ = task;
  }

  raw_ptr<web::DownloadTask, DanglingUntriaged> download_task_started_ =
      nullptr;
};

#pragma mark - SaveToDriveMediatorTest

// Test fixture for testing SaveToDriveMediator class.
class SaveToDriveMediatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();
    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());

    fake_system_identity_manager_->AddIdentity(kPrimaryIdentity);
    signin::MakeAccountAvailable(
        identity_manager_,
        signin::AccountAvailabilityOptionsBuilder()
            .WithGaiaId(kPrimaryIdentity.gaiaId)
            .Build(base::SysNSStringToUTF8(kPrimaryIdentity.userEmail)));
    authentication_service->SignIn(kPrimaryIdentity,
                                   signin_metrics::AccessPoint::kStartPage);

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    DriveTabHelper::CreateForWebState(web_state_.get());
    FakeDownloadManagerTabHelper::CreateForWebState(web_state_.get());
    download_task_ =
        std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
    download_task_->SetWebState(web_state_.get());
    save_to_drive_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SaveToDriveCommands));
    manage_storage_alert_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ManageStorageAlertCommands));
    scene_handler_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    account_picker_commands_handler_ =
        OCMStrictProtocolMock(@protocol(AccountPickerCommands));
    mediator_ = [[SaveToDriveMediator alloc]
             initWithDownloadTask:download_task_.get()
               saveToDriveHandler:save_to_drive_commands_handler_
        manageStorageAlertHandler:manage_storage_alert_commands_handler_
             accountPickerHandler:account_picker_commands_handler_
                      prefService:profile_->GetPrefs()
            authenticationService:authentication_service
            accountManagerService:ChromeAccountManagerServiceFactory::
                                      GetForProfile(profile_.get())
                  identityManager:IdentityManagerFactory::GetForProfile(
                                      profile_.get())
                     driveService:drive::DriveServiceFactory::GetForProfile(
                                      profile_.get())];
  }

  void TearDown() final {
    [mediator_ disconnect];
    mediator_ = nil;
    identity_manager_ = nil;
    fake_system_identity_manager_ = nil;
    PlatformTest::TearDown();
  }

  DriveTabHelper* GetDriveTabHelper() const {
    return DriveTabHelper::FromWebState(web_state_.get());
  }

  drive::TestDriveService* GetTestDriveService() {
    return static_cast<drive::TestDriveService*>(
        drive::DriveServiceFactory::GetForProfile(profile_.get()));
  }

  FakeDownloadManagerTabHelper* GetDownloadManagerTabHelper() const {
    return static_cast<FakeDownloadManagerTabHelper*>(
        DownloadManagerTabHelper::FromWebState(web_state_.get()));
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  // ScopedTestingLocalState needed for the authentication service.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<web::FakeWebState> web_state_;
  std::unique_ptr<web::FakeDownloadTask> download_task_;
  id save_to_drive_commands_handler_;
  id manage_storage_alert_commands_handler_;
  raw_ptr<FakeSystemIdentityManager> fake_system_identity_manager_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  id scene_handler_;
  id account_picker_commands_handler_;
  SaveToDriveMediator* mediator_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// Tests that the Save to Drive UI is hidden when the `DownloadTask` is
// destroyed.
TEST_F(SaveToDriveMediatorTest, HidesSaveToDriveOnDownloadTaskDestroyed) {
  OCMExpect([save_to_drive_commands_handler_ hideSaveToDrive]);
  download_task_.reset();
  EXPECT_OCMOCK_VERIFY(save_to_drive_commands_handler_);
}

// Tests that the Save to Drive UI is hidden when the `WebState` is destroyed.
TEST_F(SaveToDriveMediatorTest, HidesSaveToDriveOnWebStateDestroyed) {
  OCMExpect([save_to_drive_commands_handler_ hideSaveToDrive]);
  download_task_->SetWebState(/*web_state=*/nullptr);
  web_state_.reset();
  EXPECT_OCMOCK_VERIFY(save_to_drive_commands_handler_);
}

// Tests that the Save to Drive UI is hidden when the `WebState` is hidden.
TEST_F(SaveToDriveMediatorTest, HidesSaveToDriveOnWebStateHidden) {
  OCMExpect([save_to_drive_commands_handler_ hideSaveToDrive]);
  web_state_->WasHidden();
  EXPECT_OCMOCK_VERIFY(save_to_drive_commands_handler_);
}

// Tests that the `DownloadManagerTabHelper` is informed and that the
// `DownloadTask` and the selected identity are not sent to the `DriveTabHelper`
// when `startDownloadWithIdentity:` is invoked if the selected destination is
// `FileDestination::kFiles`.
TEST_F(SaveToDriveMediatorTest, DoesNotSaveToDriveIfDestinationIsFiles) {
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  FakeDownloadManagerTabHelper* download_helper = GetDownloadManagerTabHelper();
  DriveTabHelper* drive_helper = GetDriveTabHelper();
  EXPECT_EQ(nullptr, download_helper->download_task_started_);
  EXPECT_EQ(nullptr,
            drive_helper->GetUploadTaskForDownload(download_task_.get()));
  [mediator_ fileDestinationPicker:nil
              didSelectDestination:FileDestination::kFiles];
  OCMExpect([account_picker_commands_handler_ hideAccountPickerAnimated:YES]);
  [mediator_ saveWithSelectedIdentity:identity];
  EXPECT_OCMOCK_VERIFY(account_picker_commands_handler_);
  EXPECT_EQ(download_task_.get(), download_helper->download_task_started_);
  UploadTask* upload_task =
      drive_helper->GetUploadTaskForDownload(download_task_.get());
  ASSERT_EQ(nullptr, upload_task);
}

// Tests that the `DownloadManagerTabHelper` is informed and that the
// `DownloadTask` and the selected identity are sent to the `DriveTabHelper`
// when `startDownloadWithIdentity:` is invoked if the selected destination is
// `FileDestination::kDrive`.
TEST_F(SaveToDriveMediatorTest, SavesToDriveIfDestinationIsDrive) {
  base::HistogramTester histogram_tester;
  FakeDownloadManagerTabHelper* download_helper = GetDownloadManagerTabHelper();
  DriveTabHelper* drive_helper = GetDriveTabHelper();
  // Set up test file uploader with a quit closure.
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  auto test_file_uploader = std::make_unique<TestDriveFileUploader>(identity);
  test_file_uploader->SetFetchStorageQuotaQuitClosure(
      task_environment_.QuitClosure());
  GetTestDriveService()->SetFileUploader(std::move(test_file_uploader));
  // No download/uploader task should have been started/created yet.
  EXPECT_EQ(nullptr, download_helper->download_task_started_);
  EXPECT_EQ(nullptr,
            drive_helper->GetUploadTaskForDownload(download_task_.get()));
  // Select Drive as a destination and try saving with `identity`.
  [mediator_ fileDestinationPicker:nil
              didSelectDestination:FileDestination::kDrive];
  [mediator_ saveWithSelectedIdentity:identity];
  // Expect that the account picker will be hidden when sufficient storage is
  // confirmed by the file uploader.
  OCMExpect([account_picker_commands_handler_ hideAccountPickerAnimated:YES]);
  // Run until storage quota result is reported.
  task_environment_.RunUntilQuit();
  EXPECT_OCMOCK_VERIFY(account_picker_commands_handler_);
  // Download task should have been started, an upload task should be created.
  EXPECT_EQ(download_task_.get(), download_helper->download_task_started_);
  UploadTask* upload_task =
      drive_helper->GetUploadTaskForDownload(download_task_.get());
  ASSERT_NE(nullptr, upload_task);
  EXPECT_EQ(identity, upload_task->GetIdentity());
  // Test that expected histograms were recorded.
  histogram_tester.ExpectUniqueSample(kDriveStorageQuotaResultSuccessful, true,
                                      1);
}

// Tests that the Save to Drive UI is hidden when the user signs out.
TEST_F(SaveToDriveMediatorTest, HidesSaveToDriveOnSignOut) {
  OCMExpect([save_to_drive_commands_handler_ hideSaveToDrive]);
  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_.get()));
  EXPECT_OCMOCK_VERIFY(save_to_drive_commands_handler_);
}
