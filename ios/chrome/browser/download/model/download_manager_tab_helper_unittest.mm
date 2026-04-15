// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"

#import <memory>

#import "base/files/scoped_temp_dir.h"
#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/scoped_observation.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/drive/model/drive_policy.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/enterprise_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/fakes/fake_download_manager_tab_helper_delegate.h"
#import "ios/chrome/test/fakes/fake_enterprise_commands_handler.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test observer class to wait for download task destruction.
class TestDownloadTaskObserver : public web::DownloadTaskObserver {
 public:
  TestDownloadTaskObserver(web::DownloadTask* task, base::OnceClosure closure)
      : closure_(std::move(closure)) {
    task_observation_.Observe(task);
  }

  void OnDownloadDestroyed(web::DownloadTask* task) override {
    task_observation_.Reset();
    if (closure_) {
      std::move(closure_).Run();
    }
  }

 private:
  base::ScopedObservation<web::DownloadTask, web::DownloadTaskObserver>
      task_observation_{this};
  base::OnceClosure closure_;
};

namespace {
char kUrl[] = "https://test.test/";
const char kMimeType[] = "";
}  // namespace

// Test fixture for testing DownloadManagerTabHelper class.
class DownloadManagerTabHelperTest : public PlatformTest {
 protected:
  DownloadManagerTabHelperTest() {}

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test::SetDownloadsDirectoryForTesting(&temp_dir_.GetPath());

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserListFactory::GetForProfile(profile_.get())
        ->AddBrowser(browser_.get());

    fake_enterprise_commands_handler_ =
        [[FakeEnterpriseCommandsHandler alloc] init];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:fake_enterprise_commands_handler_
                     forProtocol:@protocol(EnterpriseCommands)];

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state_->SetBrowserState(profile_.get());
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));

    delegate_ = [[FakeDownloadManagerTabHelperDelegate alloc] init];
    DriveTabHelper::CreateForWebState(web_state_);
    DownloadManagerTabHelper::CreateForWebState(web_state_);
    DownloadManagerTabHelper::FromWebState(web_state_)->SetDelegate(delegate_);
  }

  void TearDown() override {
    test::SetDownloadsDirectoryForTesting(nullptr);
    PlatformTest::TearDown();
  }

  DownloadManagerTabHelper* tab_helper() {
    return DownloadManagerTabHelper::FromWebState(web_state_);
  }

  bool has_files_request_handler() {
    return !!tab_helper()->files_request_handler_;
  }

  bool has_content_analysis_info() {
    return !!tab_helper()->content_analysis_info_;
  }

  // Creates a fake download task associated with `web_state_`.
  std::unique_ptr<web::FakeDownloadTask> CreateFakeDownloadTask(
      const GURL& original_url,
      const std::string& mime_type) {
    auto task =
        std::make_unique<web::FakeDownloadTask>(original_url, mime_type);
    task->SetWebState(web_state_);
    return task;
  }

  // Set up download restrictions.
  void SetUpDownloadRestrictions() {
    PrefService* pref_service = profile_.get()->GetPrefs();
    pref_service->SetInteger(
        policy::policy_prefs::kDownloadRestrictions,
        static_cast<int>(policy::DownloadRestriction::ALL_FILES));
    pref_service->SetInteger(
        prefs::kIosSaveToDriveDownloadManagerPolicySettings,
        static_cast<int>(SaveToDrivePolicySettings::kDisabled));
  }

  // Fake a sign in.
  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auth_service->SignIn(fake_identity,
                         signin_metrics::AccessPoint::kStartPage);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<web::FakeWebState> web_state_ = nullptr;
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
  FakeSystemIdentity* fake_identity_ = nullptr;
  FakeDownloadManagerTabHelperDelegate* delegate_;
  FakeEnterpriseCommandsHandler* fake_enterprise_commands_handler_;
};

// Tests that created download has NotStarted state for visible web state.
TEST_F(DownloadManagerTabHelperTest, DownloadCreationForVisibleWebState) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);

  tab_helper()->SetCurrentDownload(std::move(task));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests creating a second download after the first download is completed.
TEST_F(DownloadManagerTabHelperTest, DownloadAcceptationOnceCompleted) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  task->SetDone(true);
  tab_helper()->SetCurrentDownload(std::move(task));
  EXPECT_EQ(web::DownloadTask::State::kComplete, *delegate_.state);

  auto task2 = CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  task2->SetWebState(web_state_.get());
  tab_helper()->SetCurrentDownload(std::move(task2));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests creating the second download while the first download is still in
// progress. Second download will be rejected by the delegate.
TEST_F(DownloadManagerTabHelperTest, DownloadRejectionViaDelegate) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  tab_helper()->SetCurrentDownload(std::move(task));

  auto task2 = CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  const web::FakeDownloadTask* task2_ptr = task2.get();
  tab_helper()->SetCurrentDownload(std::move(task2));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(task2_ptr, delegate_.decidingPolicyForDownload);

  // Ask the delegate to discard the new download.
  BOOL discarded = [delegate_ decidePolicy:kNewDownloadPolicyDiscard];
  ASSERT_TRUE(discarded);
}

// Tests creating the second download while the first download is still in
// progress. Second download will be acccepted by the delegate.
TEST_F(DownloadManagerTabHelperTest, DownloadReplacingViaDelegate) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  tab_helper()->SetCurrentDownload(std::move(task));

  auto task2 = CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  const web::FakeDownloadTask* task2_ptr = task2.get();
  tab_helper()->SetCurrentDownload(std::move(task2));

  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(task2_ptr, delegate_.decidingPolicyForDownload);

  // Ask the delegate to replace the new download.
  BOOL replaced = [delegate_ decidePolicy:kNewDownloadPolicyReplace];
  ASSERT_TRUE(replaced);

  // The state of a new download task is kNotStarted.
  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests that created download has null state for hidden web state.
TEST_F(DownloadManagerTabHelperTest, DownloadCreationForHiddenWebState) {
  web_state_->WasHidden();
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);

  tab_helper()->SetCurrentDownload(std::move(task));

  ASSERT_FALSE(delegate_.state);
}

// Tests hiding and showing WebState.
TEST_F(DownloadManagerTabHelperTest, HideAndShowWebState) {
  web_state_->WasShown();
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  tab_helper()->SetCurrentDownload(std::move(task));
  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);

  web_state_->WasHidden();
  EXPECT_FALSE(delegate_.state);

  web_state_->WasShown();
  ASSERT_TRUE(delegate_.state);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, *delegate_.state);
}

// Tests that has_download_task() returns correct result.
TEST_F(DownloadManagerTabHelperTest, HasDownloadTask) {
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);

  web::FakeDownloadTask* task_ptr = task.get();
  ASSERT_FALSE(tab_helper()->has_download_task());
  tab_helper()->SetCurrentDownload(std::move(task));
  task_ptr->Start(base::FilePath());
  ASSERT_TRUE(tab_helper()->has_download_task());

  base::RunLoop run_loop;
  TestDownloadTaskObserver observer(task_ptr, run_loop.QuitClosure());
  task_ptr->Cancel();
  run_loop.Run();

  EXPECT_FALSE(tab_helper()->has_download_task());
}

// Tests that download is restricted for a visible web state when the download
// restrictions policy is enabled and the Save to Drive policy is disabled. The
// test verifies that the delegate state remains nil. Additionally, the test
// checks that a snackbar is displayed to the user.
TEST_F(DownloadManagerTabHelperTest, DownloadRestrictedForVisibleWebState) {
  SignIn();
  PrefService* pref_service = profile_.get()->GetPrefs();
  pref_service->SetInteger(
      policy::policy_prefs::kDownloadRestrictions,
      static_cast<int>(policy::DownloadRestriction::ALL_FILES));
  pref_service->SetInteger(
      prefs::kIosSaveToDriveDownloadManagerPolicySettings,
      static_cast<int>(SaveToDrivePolicySettings::kDisabled));

  web_state_->WasShown();
  id mock_snackbar_command_handler_ =
      OCMProtocolMock(@protocol(SnackbarCommands));

  OCMExpect([mock_snackbar_command_handler_ showSnackbarWithMessage:[OCMArg any]
                                                         buttonText:[OCMArg any]
                                                      messageAction:nil
                                                   completionAction:nil]);
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  tab_helper()->SetSnackbarHandler(mock_snackbar_command_handler_);
  tab_helper()->SetCurrentDownload(std::move(task));
  ASSERT_FALSE(delegate_.state);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_command_handler_);
}

// Tests that download is restricted for a visible web state when the download
// restrictions policy is enabled and browser is incognito. The test verifies
// that the delegate state remains nil. Additionally, the test checks
// that a snackbar is displayed to the user.
TEST_F(DownloadManagerTabHelperTest,
       DownloadRestrictedAndIncognitoForVisibleWebState) {
  web_state_->SetBrowserState(profile_->GetOffTheRecordProfile());
  SignIn();
  PrefService* pref_service = profile_.get()->GetPrefs();
  pref_service->SetInteger(
      policy::policy_prefs::kDownloadRestrictions,
      static_cast<int>(policy::DownloadRestriction::ALL_FILES));
  pref_service->SetInteger(
      prefs::kIosSaveToDriveDownloadManagerPolicySettings,
      static_cast<int>(SaveToDrivePolicySettings::kEnabled));
  web_state_->WasShown();
  id mock_snackbar_command_handler_ =
      OCMProtocolMock(@protocol(SnackbarCommands));

  OCMExpect([mock_snackbar_command_handler_ showSnackbarWithMessage:[OCMArg any]
                                                         buttonText:[OCMArg any]
                                                      messageAction:nil
                                                   completionAction:nil]);
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  tab_helper()->SetSnackbarHandler(mock_snackbar_command_handler_);
  tab_helper()->SetCurrentDownload(std::move(task));
  ASSERT_FALSE(delegate_.state);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_command_handler_);
}

// Tests that download is not restricted for a visible web state when the
// download restrictions policy is enabled but the Save to Drive policy is also
// enable. The test verifies that the delegate state is set. Additionally, the
// test checks that a snackbar is not displayed to the user.
TEST_F(DownloadManagerTabHelperTest, NoDownloadRestrictionForVisibleWebState) {
  SignIn();
  PrefService* pref_service = profile_.get()->GetPrefs();
  pref_service->SetInteger(
      policy::policy_prefs::kDownloadRestrictions,
      static_cast<int>(policy::DownloadRestriction::ALL_FILES));
  pref_service->SetInteger(
      prefs::kIosSaveToDriveDownloadManagerPolicySettings,
      static_cast<int>(SaveToDrivePolicySettings::kEnabled));
  web_state_->WasShown();
  id mock_snackbar_command_handler_ =
      OCMProtocolMock(@protocol(SnackbarCommands));

  OCMReject([mock_snackbar_command_handler_ showSnackbarWithMessage:[OCMArg any]
                                                         buttonText:[OCMArg any]
                                                      messageAction:nil
                                                   completionAction:nil]);
  ASSERT_FALSE(delegate_.state);
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  tab_helper()->SetSnackbarHandler(mock_snackbar_command_handler_);
  tab_helper()->SetCurrentDownload(std::move(task));
  ASSERT_TRUE(delegate_.state);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_command_handler_);
}

// Tests that after a download task is complete, it finishes by moving the file.
// This verifies that when the feature is disabled, the scan result is SUCCESS
// and it proceeds without a warning dialog.
TEST_F(DownloadManagerTabHelperTest, DownloadCompleteProceeds) {
  scoped_feature_list_.InitAndDisableFeature(
      enterprise_connectors::kEnableFileDownloadConnectorIOS);

  web_state_->WasShown();
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  task_ptr->SetIdentifier(@"test_id");
  task_ptr->SetGeneratedFileName(base::FilePath("test.txt"));
  tab_helper()->SetCurrentDownload(std::move(task));

  EXPECT_TRUE(tab_helper()->GetDownloadTaskFinalFilePath().empty());
  task_ptr->SetDone(true);

  // Wait for the task to finish (moving the file).
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !tab_helper()->GetDownloadTaskFinalFilePath().empty(); }));
  EXPECT_FALSE(fake_enterprise_commands_handler_->_callback);
}

// Tests that if a download is being saved to Drive, the scanning is NOT
// triggered in OnDownloadUpdated.
TEST_F(DownloadManagerTabHelperTest, DownloadCompleteSavedToDriveDoesNotMove) {
  web_state_->WasShown();
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  task_ptr->SetIdentifier(@"test_id");
  task_ptr->SetGeneratedFileName(base::FilePath("test.txt"));

  // Add download to Drive.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  DriveTabHelper::FromWebState(web_state_)
      ->AddDownloadToSaveToDrive(task_ptr, identity);
  tab_helper()->SetCurrentDownload(std::move(task));
  task_ptr->SetDone(true);

  // Verify that scanning didn't trigger and move the file.
  EXPECT_TRUE(tab_helper()->GetDownloadTaskFinalFilePath().empty());
}

// Tests that when scanning is ENABLED and no policy
// is set, the scan result will be SUCCESS and the download proceeds,
TEST_F(DownloadManagerTabHelperTest,
       DownloadCompleteWithScanningEnabledProceeds) {
  scoped_feature_list_.InitAndEnableFeature(
      enterprise_connectors::kEnableFileDownloadConnectorIOS);

  web_state_->WasShown();
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  task_ptr->SetIdentifier(@"test_id");
  task_ptr->SetGeneratedFileName(base::FilePath("test.txt"));
  tab_helper()->SetCurrentDownload(std::move(task));
  task_ptr->SetDone(true);

  // It should move the file because the scan results in SUCCESS (no policy
  // set).
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !tab_helper()->GetDownloadTaskFinalFilePath().empty(); }));
}

// Tests that enterprise metadata (files_request_handler_ and
// content_analysis_info_) is correctly cleared after a download completes or is
// replaced.
TEST_F(DownloadManagerTabHelperTest, EnterpriseMetadataCleanup) {
  web_state_->WasShown();
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  task_ptr->SetIdentifier(@"test_id");
  task_ptr->SetGeneratedFileName(base::FilePath("test.txt"));
  tab_helper()->SetCurrentDownload(std::move(task));

  // Scanning should NOT be triggered yet.
  EXPECT_FALSE(has_files_request_handler());
  EXPECT_FALSE(has_content_analysis_info());

  task_ptr->SetDone(true);

  // Scanning triggers. Since no policy is set, it might complete immediately or
  // asynchronously. In our case, it should complete quickly.
  // Wait for the final path to be set, which happens after scanning.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !tab_helper()->GetDownloadTaskFinalFilePath().empty(); }));

  // After completion, metadata should be cleared.
  EXPECT_FALSE(has_files_request_handler());
  EXPECT_FALSE(has_content_analysis_info());

  // Now test that replacing a download also clears metadata (if it was somehow
  // set).
  auto task2 = CreateFakeDownloadTask(GURL(kUrl), kMimeType);
  tab_helper()->SetCurrentDownload(std::move(task2));
  EXPECT_FALSE(has_files_request_handler());
  EXPECT_FALSE(has_content_analysis_info());
}
