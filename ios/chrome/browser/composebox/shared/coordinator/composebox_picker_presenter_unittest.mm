// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"

#import <PhotosUI/PhotosUI.h>
#import <UIKit/UIKit.h>

#import <optional>

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
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/providers/privacy_primitive/test_privacy_primitive.h"
#import "ios/public/provider/chrome/browser/privacy_primitive/privacy_primitive_configuration.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

#pragma mark - FakePrivacyPrimitiveServiceWithCallback

@interface FakePrivacyPrimitiveServiceWithCallback
    : NSObject <PrivacyPrimitiveServiceFactory, PrivacyPrimitiveService>

@property(nonatomic, copy) void (^openURLCallback)(NSURL* URL);
@property(nonatomic, assign) BOOL showFlowCalled;

@end

@implementation FakePrivacyPrimitiveServiceWithCallback

- (id<PrivacyPrimitiveService>)createPrivacyPrimitiveService:
    (PrivacyPrimitiveConfiguration*)configuration {
  self.openURLCallback = configuration.openURLCallback;
  return self;
}

- (void)showFlowWithPresentingViewController:(UIViewController*)viewController
                           completionHandler:
                               (void (^)(BOOL success))completionHandler {
  self.showFlowCalled = YES;
}

@end

#pragma mark - FakePresenterDriveFilePickerHandler

@interface FakePresenterDriveFilePickerHandler
    : NSObject <DriveFilePickerCommands>

@property(nonatomic, assign) BOOL drivePickerShown;
@property(nonatomic, weak) id<DriveFilePickerResponseCommands> responseHandler;
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

- (void)
    showDriveFilePickerWithResponseHandler:
        (id<DriveFilePickerResponseCommands>)responseHandler
                        baseViewController:(UIViewController*)baseViewController
                        maxAttachmentCount:(NSUInteger)maxAttachmentCount
                         snackbarPresenter:
                             (ComposeboxSnackbarPresenter*)snackbarPresenter {
  self.drivePickerShown = YES;
  self.responseHandler = responseHandler;
  self.maxAttachmentCount = maxAttachmentCount;
  self.snackbarPresenter = snackbarPresenter;
}

@end

#pragma mark - FakePresenterTabPickerHandler

@interface FakePresenterTabPickerHandler : NSObject <TabPickerCommands>

@property(nonatomic, assign) BOOL tabPickerShown;
@property(nonatomic, strong) TabPickerParams* lastParams;
@property(nonatomic, copy) TabPickerCompletionBlock lastCompletionBlock;

@end

@implementation FakePresenterTabPickerHandler

- (void)showTabPickerWithParams:(TabPickerParams*)params
                     completion:(TabPickerCompletionBlock)completion {
  self.tabPickerShown = YES;
  self.lastParams = params;
  self.lastCompletionBlock = completion;
}

- (void)hideTabPicker {
}

@end

#pragma mark - FakePresenterDataSource

@interface FakePresenterDataSource
    : NSObject <ComposeboxPickerPresenterDataSource>

@property(nonatomic, assign) std::set<web::WebStateID> attachedWebStateIDs;
@property(nonatomic, assign) NSUInteger remainingCapacity;

@end

@implementation FakePresenterDataSource

- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContextForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return self.attachedWebStateIDs;
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

#pragma mark - FakePresenterDelegate

@interface FakePresenterDelegate : NSObject <ComposeboxPickerPresenterDelegate>

@property(nonatomic, assign) BOOL didPickTabsCalled;
@property(nonatomic, assign) std::set<web::WebStateID> selectedWebStateIDs;
@property(nonatomic, assign) std::set<web::WebStateID> cachedWebStateIDs;
@property(nonatomic, assign) BOOL didCancelTabPickerCalled;

@end

@implementation FakePresenterDelegate

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
    handleSelectedTabsWithWebStateIDs:
        (std::set<web::WebStateID>)selectedWebStateIDs
                    cachedWebStateIDs:
                        (std::set<web::WebStateID>)cachedWebStateIDs {
  self.didPickTabsCalled = YES;
  self.selectedWebStateIDs = selectedWebStateIDs;
  self.cachedWebStateIDs = cachedWebStateIDs;
}

- (void)composeboxPickerPresenterDidCancelTabPicker:
    (ComposeboxPickerPresenter*)presenter {
  self.didCancelTabPickerCalled = YES;
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                didPickDriveItems:
                    (NSArray<ComposeboxPickerDriveResult*>*)items {
}

- (void)composeboxPickerPresenterDidCancelDrivePicker:
    (ComposeboxPickerPresenter*)presenter {
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                    didPickImages:
                        (NSArray<ComposeboxPickerImageResult*>*)results {
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
             didPickFilesWithURLs:(NSArray<NSURL*>*)urls {
}

- (void)composeboxPickerPresenterDidDissmissCamera:
    (ComposeboxPickerPresenter*)presenter {
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
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_);

    handler_ = [[FakePresenterDriveFilePickerHandler alloc] init];
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:handler_
                             forProtocol:@protocol(DriveFilePickerCommands)];

    tab_picker_handler_ = [[FakePresenterTabPickerHandler alloc] init];
    [dispatcher startDispatchingToTarget:tab_picker_handler_
                             forProtocol:@protocol(TabPickerCommands)];

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

  void TearDown() override {
    ios::provider::test::SetPrivacyPrimitiveServiceFactory(nil);
    presenter_ = nil;
    metrics_recorder_ = nil;
    data_source_ = nil;
    handler_ = nil;
    browser_.reset();
    base_view_controller_ = nil;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);

    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_);
    auth_service->SignIn(fake_identity,
                         signin_metrics::AccessPoint::kStartPage);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  UIViewController* base_view_controller_ = nil;
  std::unique_ptr<TestBrowser> browser_;
  FakePresenterDriveFilePickerHandler* handler_ = nil;
  FakePresenterTabPickerHandler* tab_picker_handler_ = nil;
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

// Tests that when ConsentKit triggers a delegated action to open a URL, the
// composebox is hidden and the URL is opened in a new tab.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_DelegatedActionOpensURLInNewTab) {
  FakePrivacyPrimitiveServiceWithCallback* fake_service =
      [[FakePrivacyPrimitiveServiceWithCallback alloc] init];
  ios::provider::test::SetPrivacyPrimitiveServiceFactory(fake_service);

  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  SignIn();

  id mock_browser_coordinator_commands =
      OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_browser_coordinator_commands hideComposebox]);
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_browser_coordinator_commands
                   forProtocol:@protocol(BrowserCoordinatorCommands)];

  id mock_scene_commands = OCMStrictProtocolMock(@protocol(SceneCommands));
  OCMExpect([mock_scene_commands
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        return command.URL == GURL("https://example.com/privacy");
      }]]);
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_scene_commands
                   forProtocol:@protocol(SceneCommands)];

  base::HistogramTester histogram_tester;

  [presenter_ presentDriveFilePicker];

  EXPECT_TRUE(fake_service.showFlowCalled);
  ASSERT_NE(fake_service.openURLCallback, nil);

  fake_service.openURLCallback(
      [NSURL URLWithString:@"https://example.com/privacy"]);

  EXPECT_OCMOCK_VERIFY(mock_browser_coordinator_commands);
  EXPECT_OCMOCK_VERIFY(mock_scene_commands);

  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Drive",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
}

// Tests that when a modal view controller is presented by the privacy
// primitive, the delegated action dismisses the modal before hiding the
// composebox and opening the URL.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_DelegatedActionDismissesPresentedModal) {
  id mock_base_view_controller = OCMClassMock([UIViewController class]);
  id mock_modal = OCMClassMock([UIViewController class]);
  OCMStub([mock_base_view_controller presentedViewController])
      .andReturn(mock_modal);

  __block void (^dismissalCompletion)(void) = nil;
  OCMExpect([mock_modal dismissViewControllerAnimated:YES
                                           completion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        void (^completion)(void);
        [invocation getArgument:&completion atIndex:3];
        dismissalCompletion = [completion copy];
      });

  ComposeboxPickerPresenter* presenter = [[ComposeboxPickerPresenter alloc]
      initWithBaseViewController:mock_base_view_controller
                         browser:browser_.get()];
  presenter.dataSource = data_source_;
  presenter.metricsRecorder = metrics_recorder_;

  FakePrivacyPrimitiveServiceWithCallback* fake_service =
      [[FakePrivacyPrimitiveServiceWithCallback alloc] init];
  ios::provider::test::SetPrivacyPrimitiveServiceFactory(fake_service);

  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  SignIn();

  id mock_browser_coordinator_commands =
      OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_browser_coordinator_commands hideComposebox]);
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_browser_coordinator_commands
                   forProtocol:@protocol(BrowserCoordinatorCommands)];

  id mock_scene_commands = OCMStrictProtocolMock(@protocol(SceneCommands));
  OCMExpect([mock_scene_commands
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        return command.URL == GURL("https://example.com/privacy");
      }]]);
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_scene_commands
                   forProtocol:@protocol(SceneCommands)];

  [presenter presentDriveFilePicker];

  EXPECT_TRUE(fake_service.showFlowCalled);
  ASSERT_NE(fake_service.openURLCallback, nil);

  fake_service.openURLCallback(
      [NSURL URLWithString:@"https://example.com/privacy"]);

  EXPECT_OCMOCK_VERIFY(mock_modal);
  ASSERT_NE(dismissalCompletion, nil);

  dismissalCompletion();

  EXPECT_OCMOCK_VERIFY(mock_browser_coordinator_commands);
  EXPECT_OCMOCK_VERIFY(mock_scene_commands);
}

// Tests that a delegated action with an invalid or non-HTTP(S) URL is ignored
// without hiding the composebox or opening a tab.
TEST_F(ComposeboxPickerPresenterTest,
       TestPresentDriveFilePicker_DelegatedActionRejectsInvalidURL) {
  FakePrivacyPrimitiveServiceWithCallback* fake_service =
      [[FakePrivacyPrimitiveServiceWithCallback alloc] init];
  ios::provider::test::SetPrivacyPrimitiveServiceFactory(fake_service);

  scoped_feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOptionDisclaimer);

  profile_->GetPrefs()->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kNotConsent));

  SignIn();

  id mock_browser_coordinator_commands =
      OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_browser_coordinator_commands
                   forProtocol:@protocol(BrowserCoordinatorCommands)];

  id mock_scene_commands = OCMStrictProtocolMock(@protocol(SceneCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_scene_commands
                   forProtocol:@protocol(SceneCommands)];

  base::HistogramTester histogram_tester;

  [presenter_ presentDriveFilePicker];

  EXPECT_TRUE(fake_service.showFlowCalled);
  ASSERT_NE(fake_service.openURLCallback, nil);

  // Invoke callback with non-HTTP(S) scheme.
  fake_service.openURLCallback([NSURL URLWithString:@"javascript:void(0)"]);

  // No commands should be dispatched to hide composebox or open tab.
  EXPECT_OCMOCK_VERIFY(mock_browser_coordinator_commands);
  EXPECT_OCMOCK_VERIFY(mock_scene_commands);

  histogram_tester.ExpectTotalCount("Omnibox.MobileFusebox.PickerOutcome.Drive",
                                    0);
  histogram_tester.ExpectTotalCount("Omnibox.MobileFusebox.PickerOutcome", 0);
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

// Tests that picking Drive items records kAttachmentAdded and notifies the
// delegate.
TEST_F(ComposeboxPickerPresenterTest,
       TestDriveFilePicker_DidPickItems_RecordsAttachmentAdded) {
  base::HistogramTester histogram_tester;
  id mock_delegate =
      OCMStrictProtocolMock(@protocol(ComposeboxPickerPresenterDelegate));
  presenter_.delegate = mock_delegate;

  id<DriveFilePickerResponseCommands> response_handler =
      static_cast<id<DriveFilePickerResponseCommands>>(presenter_);

  ComposeboxPickerDriveResult* result =
      [[ComposeboxPickerDriveResult alloc] init];
  result.identifier = @"drive_file_id";
  NSArray<ComposeboxPickerDriveResult*>* items = @[ result ];

  OCMExpect([mock_delegate composeboxPickerPresenter:presenter_
                                   didPickDriveItems:items]);

  [response_handler driveFilePickerDidPickItems:items];

  EXPECT_OCMOCK_VERIFY(mock_delegate);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Drive",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
}

// Tests that cancelling Drive file picker records kManualUserExit and notifies
// the delegate.
TEST_F(ComposeboxPickerPresenterTest,
       TestDriveFilePicker_DidCancel_RecordsManualUserExit) {
  base::HistogramTester histogram_tester;
  id mock_delegate =
      OCMStrictProtocolMock(@protocol(ComposeboxPickerPresenterDelegate));
  presenter_.delegate = mock_delegate;

  id<DriveFilePickerResponseCommands> response_handler =
      static_cast<id<DriveFilePickerResponseCommands>>(presenter_);

  OCMExpect(
      [mock_delegate composeboxPickerPresenterDidCancelDrivePicker:presenter_]);

  [response_handler driveFilePickerDidCancel];

  EXPECT_OCMOCK_VERIFY(mock_delegate);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Drive",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
}

// Tests that picking tabs records kAttachmentAdded and notifies the delegate.
TEST_F(ComposeboxPickerPresenterTest,
       TestTabPicker_DidPickTabs_RecordsAttachmentAdded) {
  base::HistogramTester histogram_tester;
  FakePresenterDelegate* delegate = [[FakePresenterDelegate alloc] init];
  presenter_.delegate = delegate;

  [presenter_ presentTabPicker];

  EXPECT_TRUE(tab_picker_handler_.tabPickerShown);
  ASSERT_NE(tab_picker_handler_.lastCompletionBlock, nil);

  std::set<web::WebStateID> selected_ids = {
      web::WebStateID::FromSerializedValue(1)};
  std::set<web::WebStateID> cached_ids = {};

  tab_picker_handler_.lastCompletionBlock(TabPickerSelection{
      .selected_ids = selected_ids,
      .cached_ids = cached_ids,
  });

  EXPECT_TRUE(delegate.didPickTabsCalled);
  EXPECT_EQ(delegate.selectedWebStateIDs, selected_ids);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Tabs",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kAttachmentAdded), 1);
}

// Tests that unselecting tabs in tab picker records no outcome metric.
// TODO(crbug.com/558537506): This is not working as intended and may need
// fixing; this doesn't address the case where tabs are unselected, and an
// outcome case is likely missing in the enum.
TEST_F(ComposeboxPickerPresenterTest,
       TestTabPicker_DidPickTabs_UnselectingTabs_RecordsNoOutcome) {
  base::HistogramTester histogram_tester;
  FakePresenterDelegate* delegate = [[FakePresenterDelegate alloc] init];
  presenter_.delegate = delegate;
  data_source_.attachedWebStateIDs = {web::WebStateID::FromSerializedValue(1)};

  [presenter_ presentTabPicker];

  EXPECT_TRUE(tab_picker_handler_.tabPickerShown);
  ASSERT_NE(tab_picker_handler_.lastCompletionBlock, nil);

  std::set<web::WebStateID> selected_ids = {};
  std::set<web::WebStateID> cached_ids = {};

  tab_picker_handler_.lastCompletionBlock(TabPickerSelection{
      .selected_ids = selected_ids,
      .cached_ids = cached_ids,
  });

  EXPECT_TRUE(delegate.didPickTabsCalled);
  EXPECT_EQ(delegate.selectedWebStateIDs, selected_ids);
  histogram_tester.ExpectTotalCount("Omnibox.MobileFusebox.PickerOutcome.Tabs",
                                    0);
  histogram_tester.ExpectTotalCount("Omnibox.MobileFusebox.PickerOutcome", 0);
}

// Tests that keeping the same tabs in tab picker records no outcome metric.
TEST_F(ComposeboxPickerPresenterTest,
       TestTabPicker_DidPickTabs_KeepingSameTabs_RecordsNoOutcome) {
  base::HistogramTester histogram_tester;
  FakePresenterDelegate* delegate = [[FakePresenterDelegate alloc] init];
  presenter_.delegate = delegate;
  data_source_.attachedWebStateIDs = {web::WebStateID::FromSerializedValue(1)};

  [presenter_ presentTabPicker];

  EXPECT_TRUE(tab_picker_handler_.tabPickerShown);
  ASSERT_NE(tab_picker_handler_.lastCompletionBlock, nil);

  std::set<web::WebStateID> selected_ids = {
      web::WebStateID::FromSerializedValue(1)};
  std::set<web::WebStateID> cached_ids = {};

  tab_picker_handler_.lastCompletionBlock(TabPickerSelection{
      .selected_ids = selected_ids,
      .cached_ids = cached_ids,
  });

  EXPECT_TRUE(delegate.didPickTabsCalled);
  EXPECT_EQ(delegate.selectedWebStateIDs, selected_ids);
  histogram_tester.ExpectTotalCount("Omnibox.MobileFusebox.PickerOutcome.Tabs",
                                    0);
  histogram_tester.ExpectTotalCount("Omnibox.MobileFusebox.PickerOutcome", 0);
}

// Tests that cancelling tab picker records kManualUserExit and notifies the
// delegate.
TEST_F(ComposeboxPickerPresenterTest,
       TestTabPicker_DidCancel_RecordsManualUserExit) {
  base::HistogramTester histogram_tester;
  FakePresenterDelegate* delegate = [[FakePresenterDelegate alloc] init];
  presenter_.delegate = delegate;

  [presenter_ presentTabPicker];

  EXPECT_TRUE(tab_picker_handler_.tabPickerShown);
  ASSERT_NE(tab_picker_handler_.lastCompletionBlock, nil);

  tab_picker_handler_.lastCompletionBlock(std::nullopt);

  EXPECT_TRUE(delegate.didCancelTabPickerCalled);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome.Tabs",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.MobileFusebox.PickerOutcome",
      static_cast<int>(MobileFuseboxPickerOutcome::kManualUserExit), 1);
}
