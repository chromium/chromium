// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"

#import <PhotosUI/PhotosUI.h>
#import <UIKit/UIKit.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/contextual_search/input_state_model.h"
#import "components/contextual_search/pref_names.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#pragma mark - FakePresenterDriveFilePickerHandler

@interface FakePresenterDriveFilePickerHandler
    : NSObject <DriveFilePickerCommands>

@property(nonatomic, assign) BOOL drivePickerShown;
@property(nonatomic, assign) NSUInteger maxAttachmentCount;
@property(nonatomic, weak) ComposeboxSnackbarPresenter* snackbarPresenter;

@end

@implementation FakePresenterDriveFilePickerHandler

- (void)showDriveFilePicker {
}

- (void)hideDriveFilePicker {
}

- (void)setDriveFilePickerSelectedIdentity:
    (id<SystemIdentity>)selectedIdentity {
}

- (void)showDriveFilePickerWithComposeboxDelegate:
            (id<ComposeboxPickerPresenterDelegate>)composeboxDelegate
                               baseViewController:
                                   (UIViewController*)baseViewController
                               maxAttachmentCount:(NSUInteger)maxAttachmentCount
                                snackbarPresenter:(ComposeboxSnackbarPresenter*)
                                                      snackbarPresenter {
  self.drivePickerShown = YES;
  self.maxAttachmentCount = maxAttachmentCount;
  self.snackbarPresenter = snackbarPresenter;
}

@end

#pragma mark - FakePresenterDataSource

@interface FakePresenterDataSource
    : NSObject <ComposeboxPickerPresenterDataSource>

@property(nonatomic, assign) NSUInteger remainingCapacity;

@end

@implementation FakePresenterDataSource

- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContextForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return {};
}

- (NSUInteger)maxTabAttachmentCountForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return 10;
}

- (NSUInteger)maxDriveAttachmentCountForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return self.remainingCapacity;
}

- (NSArray<NSString*>*)attachedImageAssetIDsForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return @[];
}

@end

#pragma mark - ComposeboxPickerPresenterTest

// Test fixture for ComposeboxPickerPresenter.
class ComposeboxPickerPresenterTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    base_view_controller_ = [[UIViewController alloc] init];

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    handler_ = [[FakePresenterDriveFilePickerHandler alloc] init];
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:handler_
                             forProtocol:@protocol(DriveFilePickerCommands)];

    data_source_ = [[FakePresenterDataSource alloc] init];
    data_source_.remainingCapacity = 8;

    presenter_ = [[ComposeboxPickerPresenter alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    presenter_.dataSource = data_source_;
    metrics_recorder_ = [[ComposeboxMetricsRecorder alloc]
        initWithEntrypoint:ComposeboxEntrypoint::kNTPFakebox];
    presenter_.metricsRecorder = metrics_recorder_;
  }

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
  UIViewController* base_view_controller_ = nil;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakePresenterDriveFilePickerHandler* handler_ = nil;
  FakePresenterDataSource* data_source_ = nil;
  ComposeboxMetricsRecorder* metrics_recorder_ = nil;
  ComposeboxPickerPresenter* presenter_ = nil;
};

// Tests that when the disclaimer feature is disabled and user is signed in, the
// Drive picker is presented directly.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_DisclaimerDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  SignIn();

  [presenter_ presentDriveFilePicker];

  EXPECT_TRUE(handler_.drivePickerShown);
  EXPECT_EQ(handler_.maxAttachmentCount, 8u);
  EXPECT_NE(handler_.snackbarPresenter, nil);
}

// Tests that when the user has already consented, the Drive picker is
// presented directly even if the disclaimer feature is enabled.
TEST_F(ComposeboxPickerPresenterTest, TestPresentDriveFilePicker_PreConsented) {
  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kConsent));

  SignIn();

  [presenter_ presentDriveFilePicker];

  EXPECT_TRUE(handler_.drivePickerShown);
  EXPECT_EQ(handler_.maxAttachmentCount, 8u);
  EXPECT_NE(handler_.snackbarPresenter, nil);
}

// Tests that when kForceDriveDisclaimerAccepted is enabled, the Drive picker is
// presented directly without needing consent.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_ForceAccepted) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{omnibox::
                                kComposeboxDriveContextMenuOptionDisclaimer,
                            omnibox::kForceDriveDisclaimerAccepted},
      /*disabled_features=*/{});

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  SignIn();

  [presenter_ presentDriveFilePicker];

  EXPECT_TRUE(handler_.drivePickerShown);
  EXPECT_EQ(handler_.maxAttachmentCount, 8u);
  EXPECT_NE(handler_.snackbarPresenter, nil);
}

// Tests that attempting to present the Drive file picker when no identity is
// signed in causes a CHECK failure.
TEST_F(ComposeboxPickerPresenterTest, TestPresentDriveFilePicker_NoIdentity) {
  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  EXPECT_DEATH_IF_SUPPORTED([presenter_ presentDriveFilePicker], "");
}

// Tests that when consent is needed and the upstream provider stubs out
// ConsentKit (returns false), the Drive picker is not shown.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_NeedsConsent_UpstreamStub) {
  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  SignIn();

  [presenter_ presentDriveFilePicker];

  EXPECT_FALSE(handler_.drivePickerShown);
}

// Tests that picking an image records kAttachmentAdded for Camera.
TEST_F(ComposeboxPickerPresenterTest,
       TestCameraPicker_DidFinishWithImage_RecordsAttachmentAdded) {
  base::HistogramTester histogram_tester;
  UIImage* test_image = [[UIImage alloc] init];
  id<UIImagePickerControllerDelegate> camera_delegate =
      static_cast<id<UIImagePickerControllerDelegate>>(presenter_);

  [camera_delegate imagePickerController:[[UIImagePickerController alloc] init]
           didFinishPickingMediaWithInfo:@{
             UIImagePickerControllerOriginalImage : test_image
           }];

  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Camera",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
}

// Tests that picking with no image records kLocalError for Camera.
TEST_F(ComposeboxPickerPresenterTest,
       TestCameraPicker_DidFinishWithNoImage_RecordsLocalError) {
  base::HistogramTester histogram_tester;
  id<UIImagePickerControllerDelegate> camera_delegate =
      static_cast<id<UIImagePickerControllerDelegate>>(presenter_);

  [camera_delegate imagePickerController:[[UIImagePickerController alloc] init]
           didFinishPickingMediaWithInfo:@{}];

  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Camera",
      static_cast<int>(MobileFuseboxPickerOutcome::kLocalError), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kLocalError), 1);
}

// Tests that cancelling camera picker records kManualUserExit for Camera.
TEST_F(ComposeboxPickerPresenterTest,
       TestCameraPicker_DidCancel_RecordsManualUserExit) {
  base::HistogramTester histogram_tester;
  id<UIImagePickerControllerDelegate> camera_delegate =
      static_cast<id<UIImagePickerControllerDelegate>>(presenter_);

  [camera_delegate
      imagePickerControllerDidCancel:[[UIImagePickerController alloc] init]];

  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Camera",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
}

// Tests that picking items in the Gallery picker records kAttachmentAdded.
TEST_F(ComposeboxPickerPresenterTest,
       TestGalleryPicker_DidFinishWithResults_RecordsAttachmentAdded) {
  base::HistogramTester histogram_tester;
  id<PHPickerViewControllerDelegate> gallery_delegate =
      static_cast<id<PHPickerViewControllerDelegate>>(presenter_);

  id mock_result = OCMClassMock([PHPickerResult class]);
  id mock_item_provider = [[NSItemProvider alloc] init];
  OCMStub([mock_result itemProvider]).andReturn(mock_item_provider);
  OCMStub([mock_result assetIdentifier]).andReturn(@"test_asset_id");

  PHPickerConfiguration* config = [[PHPickerConfiguration alloc] init];
  PHPickerViewController* picker =
      [[PHPickerViewController alloc] initWithConfiguration:config];

  [gallery_delegate picker:picker didFinishPicking:@[ mock_result ]];

  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Gallery",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
}

// Tests that dismissing the Gallery picker with no items records
// kManualUserExit.
TEST_F(ComposeboxPickerPresenterTest,
       TestGalleryPicker_DidFinishEmpty_RecordsManualUserExit) {
  base::HistogramTester histogram_tester;
  id<PHPickerViewControllerDelegate> gallery_delegate =
      static_cast<id<PHPickerViewControllerDelegate>>(presenter_);

  PHPickerConfiguration* config = [[PHPickerConfiguration alloc] init];
  PHPickerViewController* picker =
      [[PHPickerViewController alloc] initWithConfiguration:config];

  [gallery_delegate picker:picker didFinishPicking:@[]];

  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Gallery",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
}

// Tests that selecting files in document picker records kAttachmentAdded.
TEST_F(ComposeboxPickerPresenterTest,
       TestDocumentPicker_DidPickDocuments_RecordsAttachmentAdded) {
  base::HistogramTester histogram_tester;
  id<UIDocumentPickerDelegate> document_delegate =
      static_cast<id<UIDocumentPickerDelegate>>(presenter_);

  UIDocumentPickerViewController* mock_controller =
      OCMClassMock([UIDocumentPickerViewController class]);
  NSURL* test_url = [NSURL URLWithString:@"file:///test.pdf"];
  [document_delegate documentPicker:mock_controller
             didPickDocumentsAtURLs:@[ test_url ]];

  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.File",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
}

// Tests that cancelling document picker records kManualUserExit.
TEST_F(ComposeboxPickerPresenterTest,
       TestDocumentPicker_DidCancel_RecordsManualUserExit) {
  base::HistogramTester histogram_tester;
  id<UIDocumentPickerDelegate> document_delegate =
      static_cast<id<UIDocumentPickerDelegate>>(presenter_);

  UIDocumentPickerViewController* mock_controller =
      OCMClassMock([UIDocumentPickerViewController class]);
  [document_delegate documentPickerWasCancelled:mock_controller];

  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.File",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
}
