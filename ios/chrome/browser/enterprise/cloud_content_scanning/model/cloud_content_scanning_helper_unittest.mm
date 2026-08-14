// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/cloud_content_scanning_helper.h"

#import <optional>

#import "base/files/file_path.h"
#import "base/functional/callback_helpers.h"
#import "base/json/json_reader.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/background_cloud_scanner_manager_factory.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/enterprise_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/fakes/fake_enterprise_commands_handler.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace enterprise_connectors {

constexpr char kBlockingAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ],
    "block_until_verdict": 1
  }
])";

constexpr char kNonBlockingAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ],
    "block_until_verdict": 0
  }
])";

// Unit tests for CloudContentScanningHelper testing:
// 1. Starting the cloud content scanning process and preparing scanning
// resources correctly.
// 2. Handling the scan decision and displaying correct warning
// dialogs/snackbars based on results.
// 3. Ensuring file downloads are allowed or blocked as expected.
class CloudContentScanningHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder profile_builder;
    profile_ = std::move(profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    fake_commands_handler_ = [[FakeEnterpriseCommandsHandler alloc] init];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:fake_commands_handler_
                     forProtocol:@protocol(EnterpriseCommands)];
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];

    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state->SetBrowserState(profile_.get());

    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());

    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));
    web_state_->WasShown();
  }

  void TearDown() override {
    web_state_ = nullptr;
    profile_ = nullptr;
    mock_snackbar_handler_ = nullptr;
    fake_commands_handler_ = nullptr;
    browser_.reset();
  }

  // Creates a RequestHandlerResult with the FinalContentAnalysisRsult, which
  // holds the file scanning result.
  RequestHandlerResult CreateResult(FinalContentAnalysisResult final_result) {
    RequestHandlerResult result;
    result.final_result = final_result;
    return result;
  }

  // Configures FakeBrowserDMTokenStorage and sets the AnalysisConnector policy
  // pref.
  void SetUpAnalysisConnectorPolicy(const char* settings_pref) {
    fake_browser_dm_token_storage_.SetDMToken("fake_dm_token");
    fake_browser_dm_token_storage_.SetClientId("fake_client_id");

    profile_->GetPrefs()->Set(
        enterprise_connectors::AnalysisConnectorPref(
            enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED),
        *base::JSONReader::Read(settings_pref,
                                base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    profile_->GetPrefs()->SetInteger(
        enterprise_connectors::AnalysisConnectorScopePref(
            enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED),
        policy::POLICY_SCOPE_MACHINE);
  }

  web::WebTaskEnvironment task_environment_;
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<web::FakeWebState> web_state_;
  FakeEnterpriseCommandsHandler* fake_commands_handler_;
  id mock_snackbar_handler_;
};

// Tests that file download can proceed if scan result is success.
TEST_F(CloudContentScanningHelperTest, ScanResultSuccess) {
  base::test::TestFuture<bool> future;

  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::SUCCESS);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  EXPECT_TRUE(future.Get());
}

// Tests that file download can proceed if scan result is warning and user
// choose to ignore.
TEST_F(CloudContentScanningHelperTest, ScanResultWarnProcceed) {
  base::test::TestFuture<bool> future;

  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::WARNING);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  std::move(fake_commands_handler_->_callback).Run(true);
  EXPECT_TRUE(future.Get());
}

// Tests that file download is blocked if scan result is warning and user
// choose to cancel.
TEST_F(CloudContentScanningHelperTest, ScanResultWarnCancel) {
  base::test::TestFuture<bool> future;

  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::WARNING);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  std::move(fake_commands_handler_->_callback).Run(false);
  EXPECT_FALSE(future.Get());
}

// Tests that file download is blocked if scan result is file too large.
TEST_F(CloudContentScanningHelperTest, ScanResultLargeFiles) {
  base::test::TestFuture<bool> future;

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::LARGE_FILES);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  EXPECT_FALSE(future.Get());
  [mock_snackbar_handler_ verify];
}

// Tests that file download is blocked if scan result is failure.
TEST_F(CloudContentScanningHelperTest, ScanResultFailure) {
  base::test::TestFuture<bool> future;

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::FAILURE);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  EXPECT_FALSE(future.Get());
  [mock_snackbar_handler_ verify];
}

// Tests that file download is blocked if scan result is file closed.
TEST_F(CloudContentScanningHelperTest, ScanResultClosed) {
  base::test::TestFuture<bool> future;

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::FAIL_CLOSED);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  EXPECT_FALSE(future.Get());
  [mock_snackbar_handler_ verify];
}

// Tests that file download is blocked if web_state is null.
TEST_F(CloudContentScanningHelperTest, NullWebState) {
  base::test::TestFuture<bool> future;

  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::SUCCESS);
  HandleScanDecision(nullptr, TriggerType::kSavePrompt, future.GetCallback(),
                     result);

  EXPECT_FALSE(future.Get());
}

// Tests that the scan decision UI is deferred when the WebState is not active.
TEST_F(CloudContentScanningHelperTest, DeferredNotification) {
  base::test::TestFuture<bool> future;

  // Insert a second WebState and activate it.
  std::unique_ptr<web::FakeWebState> web_state2 =
      std::make_unique<web::FakeWebState>();
  web_state2->SetBrowserState(profile_.get());
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state2),
      WebStateList::InsertionParams::Automatic().Activate(true));
  web_state_->WasHidden();
  ASSERT_FALSE(web_state_->IsVisible());

  // Trigger a warning scan decision for the inactive web_state_.
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::WARNING);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  // Verify that the dialog has NOT been shown yet.
  EXPECT_TRUE(fake_commands_handler_->_callback.is_null());
  web_state_->WasShown();
  EXPECT_FALSE(fake_commands_handler_->_callback.is_null());
  std::move(fake_commands_handler_->_callback).Run(true);
  EXPECT_TRUE(future.Get());
}

// Tests that the snackbar notification is deferred when the WebState is not
// active.
TEST_F(CloudContentScanningHelperTest, DeferredSnackbarNotification) {
  base::test::TestFuture<bool> future;
  web_state_->WasHidden();
  ASSERT_FALSE(web_state_->IsVisible());

  // Trigger a failure scan decision for the inactive web_state_.
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::FAILURE);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  // Verify that the download is blocked immediately even if UI is deferred.
  EXPECT_FALSE(future.Get());
  OCMExpect([mock_snackbar_handler_
      showSnackbarMessageAfterDismissingKeyboard:[OCMArg any]]);
  web_state_->WasShown();
  [mock_snackbar_handler_ verify];
}

// Tests that closing the tab while a decision is pending correctly blocks the
// download and cleans up.
TEST_F(CloudContentScanningHelperTest, CloseTabWithPendingDecision) {
  base::test::TestFuture<bool> future;
  web_state_->WasHidden();

  // Trigger a warning scan decision for the inactive web_state_.
  RequestHandlerResult result =
      CreateResult(FinalContentAnalysisResult::WARNING);
  HandleScanDecision(web_state_->GetWeakPtr(), TriggerType::kSavePrompt,
                     future.GetCallback(), result);

  // Verify that the dialog has NOT been shown.
  EXPECT_TRUE(fake_commands_handler_->_callback.is_null());
  web_state_ = nullptr;
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kUserAction);
  EXPECT_FALSE(future.Get());
}

// Tests that PrepareCloudContentScanning constructs
// `FileDownloadScanningResources` correctly.
TEST_F(CloudContentScanningHelperTest, PrepareCloudContentScanning) {
  base::test::TestFuture<bool> future;
  base::FilePath file_path(FILE_PATH_LITERAL("/path/to/fake/file.txt"));
  FileDownloadScanningResources resources = PrepareCloudContentScanning(
      web_state_, GURL("https://example.com/download"), file_path,
      TriggerType::kSavePrompt, future.GetCallback());

  EXPECT_NE(resources.content_analysis_info, nullptr);
  EXPECT_NE(resources.files_request_handler, nullptr);
}

// Tests that PrepareCloudContentScanning correctly parses and sets up resources
// for a blocking scan (block_until_verdict = kBlock).
TEST_F(CloudContentScanningHelperTest, PrepareCloudContentScanningBlocking) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableFileDownloadConnectorIOS);

  // Set up the policy pref to enable blocking scans.
  SetUpAnalysisConnectorPolicy(kBlockingAnalysisSettingsPref);

  base::test::TestFuture<bool> future;
  base::FilePath file_path(FILE_PATH_LITERAL("/path/to/fake/file.txt"));

  std::optional<FileDownloadScanningResources> resources;
  resources.emplace(PrepareCloudContentScanning(
      web_state_, GURL("https://example.com/download"), file_path,
      TriggerType::kSavePrompt, future.GetCallback()));

  EXPECT_NE(resources->content_analysis_info, nullptr);
  EXPECT_NE(resources->files_request_handler, nullptr);
  EXPECT_EQ(resources->content_analysis_info->settings().block_until_verdict,
            BlockUntilVerdict::kBlock);

  // The future callback should NOT be invoked yet.
  EXPECT_FALSE(future.IsReady());

  resources.reset();
  web_state_ = nullptr;
  browser_.reset();
  profile_.reset();
}

// Tests that PrepareCloudContentScanning handles non-blocking scans correctly.
TEST_F(CloudContentScanningHelperTest, PrepareCloudContentScanningNonBlocking) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableFileDownloadConnectorIOS);

  // Set up the policy pref to enable non-blocking scans.
  SetUpAnalysisConnectorPolicy(kNonBlockingAnalysisSettingsPref);

  base::test::TestFuture<bool> future;
  base::FilePath file_path(FILE_PATH_LITERAL("/path/to/fake/file.txt"));
  FileDownloadScanningResources resources = PrepareCloudContentScanning(
      web_state_, GURL("https://example.com/download"), file_path,
      TriggerType::kSavePrompt, future.GetCallback());

  EXPECT_NE(resources.content_analysis_info, nullptr);
  EXPECT_NE(resources.files_request_handler, nullptr);

  // Non-blocking mode should invoke the callback with `true` asynchronously.
  EXPECT_TRUE(future.Get());
}

// Tests that BackgroundCloudScannerManager is successfully created for the
// Profile, and that destroying the profile cleanly tears down the manager and
// all active scans.
TEST_F(CloudContentScanningHelperTest,
       PrepareCloudContentScanningNonBlockingLifetime) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableFileDownloadConnectorIOS);

  // Set up the policy pref to enable non-blocking scans.
  SetUpAnalysisConnectorPolicy(kNonBlockingAnalysisSettingsPref);

  base::test::TestFuture<bool> future;
  base::FilePath file_path(FILE_PATH_LITERAL("/path/to/fake/file.txt"));

  std::optional<FileDownloadScanningResources> resources;
  resources.emplace(PrepareCloudContentScanning(
      web_state_, GURL("https://example.com/download"), file_path,
      TriggerType::kSavePrompt, future.GetCallback()));

  // Confirm that a BackgroundCloudScannerManager has been created for our
  // profile.
  BackgroundCloudScannerManager* manager =
      BackgroundCloudScannerManagerFactory::GetForProfile(profile_.get());
  EXPECT_NE(manager, nullptr);

  // Destroy resources first to release raw_ptr<ProfileIOS> and prevent
  // dangling pointers.
  resources.reset();

  // Destroying the browser and the profile should run completely cleanly
  // without any dangling pointer/crashes or leaks of the pending scan.
  web_state_ = nullptr;
  browser_.reset();
  profile_.reset();
}

}  // namespace enterprise_connectors
