// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_coordinator.h"

#import <StoreKit/StoreKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/photos/photos_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_configuration.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_coordinator.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator_delegate.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Fake image URL to create the coordinator.
const char kFakeImageUrl[] = "http://example.com/image.png";

}  // namespace

// Unit tests for the SaveToPhotosCoordinator.
class SaveToPhotosCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    browser_->GetWebStateList()->InsertWebState(0, std::move(web_state),
                                                WebStateList::INSERT_ACTIVATE,
                                                WebStateOpener());

    base_view_controller_ = [[FakeUIViewController alloc] init];

    mock_save_to_photos_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SaveToPhotosCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_save_to_photos_commands_handler_
                     forProtocol:@protocol(SaveToPhotosCommands)];
    mock_snackbar_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_commands_handler_
                     forProtocol:@protocol(SnackbarCommands)];
    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    mock_application_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationSettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_settings_commands_handler_
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    mock_save_to_photos_mediator_ = OCMClassMock([SaveToPhotosMediator class]);
  }

  // Set up the mediator stub to ensure the coordinator creates a fake mediator.
  void SetUpMediatorStub() {
    OCMStub([mock_save_to_photos_mediator_ alloc])
        .andReturn(mock_save_to_photos_mediator_);
    OCMStub(
        [mock_save_to_photos_mediator_
            initWithPhotosService:reinterpret_cast<PhotosService*>(
                                      [OCMArg anyPointer])
                      prefService:reinterpret_cast<PrefService*>(
                                      [OCMArg anyPointer])
            accountManagerService:reinterpret_cast<
                                      ChromeAccountManagerService*>(
                                      [OCMArg anyPointer])
                  identityManager:reinterpret_cast<signin::IdentityManager*>(
                                      [OCMArg anyPointer])])
        .andReturn(mock_save_to_photos_mediator_);
  }

  void TearDown() final { [mock_save_to_photos_mediator_ stopMocking]; }

  // Create a SaveToPhotosCoordinator to test.
  SaveToPhotosCoordinator* CreateSaveToPhotosCoordinator() {
    return [[SaveToPhotosCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                          imageURL:GURL(kFakeImageUrl)
                          referrer:web::Referrer()
                          webState:GetActiveWebState()];
  }

  // Returns the browser's active web state.
  web::FakeWebState* GetActiveWebState() {
    return static_cast<web::FakeWebState*>(
        browser_->GetWebStateList()->GetActiveWebState());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;

  id mock_save_to_photos_mediator_;
  id mock_save_to_photos_commands_handler_;
  id mock_snackbar_commands_handler_;
  id mock_application_commands_handler_;
  id mock_application_settings_commands_handler_;
};

// Tests that the SaveToPhotosCoordinator creates the mediator when started and
// disconnects it when stopped.
TEST_F(SaveToPhotosCoordinatorTest, StartsAndDisconnectsMediator) {
  SaveToPhotosCoordinator* coordinator = CreateSaveToPhotosCoordinator();

  PhotosService* photosService =
      PhotosServiceFactory::GetForBrowserState(browser_state_.get());
  PrefService* prefService = browser_state_->GetPrefs();
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          browser_state_.get());
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browser_state_.get());

  OCMExpect([mock_save_to_photos_mediator_ alloc])
      .andReturn(mock_save_to_photos_mediator_);
  OCMExpect([mock_save_to_photos_mediator_
                initWithPhotosService:photosService
                          prefService:prefService
                accountManagerService:accountManagerService
                      identityManager:identityManager])
      .andReturn(mock_save_to_photos_mediator_);
  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(SaveToPhotosMediatorDelegate)]);
  OCMExpect([mock_save_to_photos_mediator_
      setDelegate:static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)]);
  OCMExpect([[mock_save_to_photos_mediator_ ignoringNonObjectArgs]
      startWithImageURL:GURL()
               referrer:web::Referrer()
               webState:GetActiveWebState()]);
  [coordinator start];
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_);

  OCMExpect([mock_save_to_photos_mediator_ disconnect]);
  [coordinator stop];
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_);
}

// Tests that the SaveToPhotosCoordinator creates/destroys an AlertCoordinator
// with the expected content when the mediator asks to show/hide it.
TEST_F(SaveToPhotosCoordinatorTest, ShowsAndHidesTryAgainOrCancelAlert) {
  SetUpMediatorStub();

  SaveToPhotosCoordinator* coordinator = CreateSaveToPhotosCoordinator();
  [coordinator start];

  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(SaveToPhotosMediatorDelegate)]);

  NSString* alertTitle = @"Alert Title";
  NSString* alertMessage = @"Alert message.";
  NSString* tryAgainTitle = @"Try Again";
  ProceduralBlock tryAgainAction = ^{
  };
  NSString* cancelTitle = @"Cancel";
  ProceduralBlock cancelAction = ^{
  };

  id mock_alert_coordinator = OCMClassMock([AlertCoordinator class]);
  OCMExpect([mock_alert_coordinator alloc]).andReturn(mock_alert_coordinator);
  OCMExpect([mock_alert_coordinator
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                                     title:alertTitle
                                   message:alertMessage])
      .andReturn(mock_alert_coordinator);
  OCMExpect([mock_alert_coordinator addItemWithTitle:tryAgainTitle
                                              action:tryAgainAction
                                               style:UIAlertActionStyleDefault
                                           preferred:YES
                                             enabled:YES]);
  OCMExpect([mock_alert_coordinator addItemWithTitle:cancelTitle
                                              action:cancelAction
                                               style:UIAlertActionStyleCancel
                                           preferred:NO
                                             enabled:YES]);
  OCMExpect(
      [base::apple::ObjCCast<AlertCoordinator>(mock_alert_coordinator) start]);

  [static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)
      showTryAgainOrCancelAlertWithTitle:alertTitle
                                 message:alertMessage
                           tryAgainTitle:tryAgainTitle
                          tryAgainAction:tryAgainAction
                             cancelTitle:cancelTitle
                            cancelAction:cancelAction];
  EXPECT_OCMOCK_VERIFY(mock_alert_coordinator);

  OCMExpect(
      [base::apple::ObjCCast<AlertCoordinator>(mock_alert_coordinator) stop]);
  [static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)
      hideTryAgainOrCancelAlert];
  EXPECT_OCMOCK_VERIFY(mock_alert_coordinator);

  [coordinator stop];
}

// Tests that the SaveToPhotosCoordinator creates/destroys an
// StoreKitCoordinator with the expected content when the mediator asks to
// show/hide it.
TEST_F(SaveToPhotosCoordinatorTest, ShowsAndHidesStoreKit) {
  SetUpMediatorStub();

  SaveToPhotosCoordinator* coordinator = CreateSaveToPhotosCoordinator();
  [coordinator start];

  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(SaveToPhotosMediatorDelegate)]);

  NSString* productIdentifier = @"product_identifier";
  NSString* campaignToken = @"campaign_token";

  id mock_store_kit_coordinator = OCMClassMock([StoreKitCoordinator class]);
  OCMExpect([mock_store_kit_coordinator alloc])
      .andReturn(mock_store_kit_coordinator);
  OCMExpect([mock_store_kit_coordinator
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mock_store_kit_coordinator);
  OCMExpect([mock_store_kit_coordinator
      setDelegate:static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)]);
  NSDictionary* expectedITunesProductParameters = @{
    SKStoreProductParameterITunesItemIdentifier : productIdentifier,
    SKStoreProductParameterCampaignToken : campaignToken
  };
  OCMExpect([mock_store_kit_coordinator
      setITunesProductParameters:expectedITunesProductParameters]);
  OCMExpect([base::apple::ObjCCast<StoreKitCoordinator>(
      mock_store_kit_coordinator) start]);

  [static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)
      showStoreKitWithProductIdentifier:productIdentifier
                          campaignToken:campaignToken];
  EXPECT_OCMOCK_VERIFY(mock_store_kit_coordinator);

  OCMExpect([base::apple::ObjCCast<StoreKitCoordinator>(
      mock_store_kit_coordinator) stop]);
  [static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator) hideStoreKit];
  EXPECT_OCMOCK_VERIFY(mock_store_kit_coordinator);

  [coordinator stop];
}

// Tests that the SaveToPhotosCoordinator presents a snackbar with the expected
// content when the mediator asks to show it.
TEST_F(SaveToPhotosCoordinatorTest, ShowsSnackbar) {
  SetUpMediatorStub();

  SaveToPhotosCoordinator* coordinator = CreateSaveToPhotosCoordinator();
  [coordinator start];

  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(SaveToPhotosMediatorDelegate)]);

  NSString* message = @"Snackbar message";
  NSString* buttonText = @"Button text";
  ProceduralBlock messageAction = ^{
  };
  void (^completionAction)(BOOL) = ^(BOOL) {
  };

  OCMExpect([mock_snackbar_commands_handler_
      showSnackbarWithMessage:message
                   buttonText:buttonText
                messageAction:messageAction
             completionAction:completionAction]);
  [static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)
      showSnackbarWithMessage:message
                   buttonText:buttonText
                messageAction:messageAction
             completionAction:completionAction];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_handler_);

  [coordinator stop];
}

// Tests that the SaveToPhotosCoordinator uses the -hideSaveToPhotos command
// when the mediator asks.
TEST_F(SaveToPhotosCoordinatorTest, HideSaveToPhotosStopsSaveToPhotos) {
  SetUpMediatorStub();

  SaveToPhotosCoordinator* coordinator = CreateSaveToPhotosCoordinator();
  [coordinator start];

  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(SaveToPhotosMediatorDelegate)]);

  OCMExpect([mock_save_to_photos_commands_handler_ stopSaveToPhotos]);
  [static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator) hideSaveToPhotos];
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_commands_handler_);

  [coordinator stop];
}

// Tests that the SaveToPhotosCoordinator creates/destroys an
// AccountPickerCoordinator with the expected configuration when the mediator
// asks to show/hide it.
TEST_F(SaveToPhotosCoordinatorTest, ShowsAndHidesAccountPicker) {
  SetUpMediatorStub();

  SaveToPhotosCoordinator* coordinator = CreateSaveToPhotosCoordinator();
  [coordinator start];

  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(SaveToPhotosMediatorDelegate)]);

  id mock_account_picker_coordinator =
      OCMClassMock([AccountPickerCoordinator class]);
  OCMExpect([mock_account_picker_coordinator alloc])
      .andReturn(mock_account_picker_coordinator);
  AccountPickerConfiguration* expected_configuration =
      [[AccountPickerConfiguration alloc] init];
  OCMExpect([mock_account_picker_coordinator
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                             configuration:expected_configuration])
      .andReturn(mock_account_picker_coordinator);
  OCMExpect([mock_account_picker_coordinator
      setDelegate:static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)]);
  OCMExpect([base::apple::ObjCCast<AccountPickerCoordinator>(
      mock_account_picker_coordinator) start]);

  [static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)
      showAccountPickerWithConfiguration:expected_configuration
                        selectedIdentity:nil];
  EXPECT_OCMOCK_VERIFY(mock_account_picker_coordinator);

  OCMExpect([base::apple::ObjCCast<AccountPickerCoordinator>(
      mock_account_picker_coordinator) stopAnimated:YES]);
  [static_cast<id<SaveToPhotosMediatorDelegate>>(coordinator)
      hideAccountPicker];
  EXPECT_OCMOCK_VERIFY(mock_account_picker_coordinator);

  [coordinator stop];
}

// Tests that the SaveToPhotosCoordinator shows the Add account view when the
// Account picker requires it.
TEST_F(SaveToPhotosCoordinatorTest, ShowsAddAccount) {
  SetUpMediatorStub();

  SaveToPhotosCoordinator* coordinator = CreateSaveToPhotosCoordinator();
  [coordinator start];

  ASSERT_TRUE([coordinator
      conformsToProtocol:@protocol(AccountPickerCoordinatorDelegate)]);

  // Create a mock account picker with a mocked view controller.
  id mock_account_picker_coordinator =
      OCMClassMock([AccountPickerCoordinator class]);
  FakeUIViewController* mock_account_picker_coordinator_view_controller =
      [[FakeUIViewController alloc] init];
  OCMStub([mock_account_picker_coordinator viewController])
      .andReturn(mock_account_picker_coordinator_view_controller);

  // Expect that a ShowSigninCommand will be dispatched to present the Add
  // account view on top of the account picker view.
  id<SystemIdentity> added_identity = [FakeSystemIdentity fakeIdentity1];
  OCMExpect([mock_application_commands_handler_
              showSignin:[OCMArg checkWithBlock:^BOOL(
                                     ShowSigninCommand* command) {
                if (command) {
                  command.callback(
                      SigninCoordinatorResultSuccess,
                      [SigninCompletionInfo
                          signinCompletionInfoWithIdentity:added_identity]);
                }
                EXPECT_EQ(AuthenticationOperation::kAddAccount,
                          command.operation);
                EXPECT_FALSE(command.identity);
                EXPECT_EQ(signin_metrics::AccessPoint::
                              ACCESS_POINT_SAVE_TO_PHOTOS_IOS,
                          command.accessPoint);
                EXPECT_EQ(
                    signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
                    command.promoAction);
                return YES;
              }]
      baseViewController:mock_account_picker_coordinator_view_controller]);

  // Ask the SaveToPhotosCoordinator to open the Add account view and verify the
  // ShowSigninCommand was dispatched.
  [static_cast<id<AccountPickerCoordinatorDelegate>>(coordinator)
          accountPickerCoordinator:mock_account_picker_coordinator
      openAddAccountWithCompletion:^(id<SystemIdentity> identity) {
        EXPECT_EQ(added_identity, identity);
      }];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);

  [coordinator stop];
}
