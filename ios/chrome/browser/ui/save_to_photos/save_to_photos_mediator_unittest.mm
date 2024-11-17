// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback_forward.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_configuration.h"
#import "ios/chrome/browser/photos/model/photos_metrics.h"
#import "ios/chrome/browser/photos/model/photos_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/manage_storage_alert_commands.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator_delegate.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/providers/photos/test_photos_service.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Fake image URL.
const char kFakeImageUrl[] = "http://example.com/image.png";

// To-be-encoded fake image data.
const NSString* kFakeImageData = @"kFakeImageData";

// Returns fake image data.
NSData* GetFakeImageData() {
  return [kFakeImageData dataUsingEncoding:NSUTF8StringEncoding];
}

// Returns formatted size string.
NSString* GetFakeImageSize() {
  return [NSByteCountFormatter
      stringFromByteCount:GetFakeImageData().length
               countStyle:NSByteCountFormatterCountStyleFile];
}

NSString* GetFakeImageName() {
  return base::SysUTF8ToNSString(GURL(kFakeImageUrl).ExtractFileName());
}

// Returns the URL to test whether the Google Photos app is installed.
NSURL* GetGooglePhotosAppURL() {
  NSURLComponents* photosAppURLComponents = [[NSURLComponents alloc] init];
  photosAppURLComponents.scheme = kGooglePhotosAppURLScheme;
  return photosAppURLComponents.URL;
}

}  // namespace

// Fake implementation of ImageFetchTabHelper which return fake image data.
class FakeImageFetchTabHelper : public ImageFetchTabHelper {
 public:
  static void CreateForWebState(web::WebState* web_state) {
    web_state->SetUserData(
        UserDataKey(), std::make_unique<FakeImageFetchTabHelper>(web_state));
  }

  FakeImageFetchTabHelper(web::WebState* web_state)
      : ImageFetchTabHelper(web_state),
        get_image_data_called_(false),
        quit_closure_(base::DoNothing()) {}
  FakeImageFetchTabHelper(const ImageFetchTabHelper&) = delete;
  FakeImageFetchTabHelper& operator=(const ImageFetchTabHelper&) = delete;
  ~FakeImageFetchTabHelper() override = default;

  void SetQuitClosure(base::RepeatingClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  bool GetImageDataCalled() const { return get_image_data_called_; }

  const GURL& GetImageUrl() const { return image_url_; }

  // ImageFetchTabHelper
  void GetImageData(const GURL& url,
                    const web::Referrer& referrer,
                    ImageDataCallback callback) override {
    get_image_data_called_ = true;
    image_url_ = url;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(callback, GetFakeImageData()).Then(quit_closure_));
  }

 private:
  bool get_image_data_called_;
  GURL image_url_;
  base::RepeatingClosure quit_closure_;
};

// SaveToPhotosMediator unit tests.
class SaveToPhotosMediatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_ = std::make_unique<web::FakeWebState>();
    FakeImageFetchTabHelper::CreateForWebState(web_state_.get());
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity_);
    mock_application_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    mock_manage_storage_alert_handler_ =
        OCMStrictProtocolMock(@protocol(ManageStorageAlertCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_manage_storage_alert_handler_
                     forProtocol:@protocol(ManageStorageAlertCommands)];

    mock_application_ = OCMClassMock([UIApplication class]);
    OCMStub([mock_application_ sharedApplication]).andReturn(mock_application_);
  }

  // Set-up the FakeImageFetchTabHelper so it quits the run loop when calling
  // back.
  void SetUpImageFetchTabHelperQuitClosure() {
    GetFakeImageFetchTabHelper()->SetQuitClosure(
        task_environment_.QuitClosure());
  }

  // Set-up the TestPhotosService so it quits the run loop when calling back.
  void SetUpPhotosServiceQuitClosure() {
    GetTestPhotosService()->SetQuitClosure(task_environment_.QuitClosure());
  }

  void TearDown() final { [mock_application_ stopMocking]; }

  // Create a SaveToPhotosMediator with services from the test browser state.
  SaveToPhotosMediator* CreateSaveToPhotosMediator() {
    PhotosService* photos_service =
        PhotosServiceFactory::GetForProfile(profile_.get());
    PrefService* pref_service = profile_->GetPrefs();
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    return [[SaveToPhotosMediator alloc]
            initWithPhotosService:photos_service
                      prefService:pref_service
            accountManagerService:account_manager_service
                  identityManager:identity_manager
        manageStorageAlertHandler:mock_manage_storage_alert_handler_
               applicationHandler:mock_application_handler_];
  }

  // Sign-in with a fake account.
  void SignIn() {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        base::SysNSStringToUTF8(fake_identity_.userEmail),
        signin::ConsentLevel::kSignin);
  }

  // Returns the TestPhotosService tied to the browser state.
  TestPhotosService* GetTestPhotosService() {
    return static_cast<TestPhotosService*>(
        PhotosServiceFactory::GetForProfile(profile_.get()));
  }

  // Returns the FakeImageFetchTabHelper tied to the WebState.
  FakeImageFetchTabHelper* GetFakeImageFetchTabHelper() {
    return static_cast<FakeImageFetchTabHelper*>(
        ImageFetchTabHelper::FromWebState(web_state_.get()));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::FakeWebState> web_state_;
  id mock_application_;
  id<SystemIdentity> fake_identity_;
  base::HistogramTester histogram_tester_;
  id mock_application_handler_;
  id mock_manage_storage_alert_handler_;
};

// Tests that the mediator attempts to fetch the image data when started.
TEST_F(SaveToPhotosMediatorTest, StartGetsImageData) {
  // Create and start mediator.
  SaveToPhotosMediator* mediator = CreateSaveToPhotosMediator();
  [mediator startWithImageURL:GURL(kFakeImageUrl)
                     referrer:web::Referrer()
                     webState:web_state_.get()];

  // Test that the image fetch tab helper was called with the given image URL.
  FakeImageFetchTabHelper* image_fetch_tab_helper =
      GetFakeImageFetchTabHelper();
  EXPECT_TRUE(image_fetch_tab_helper->GetImageDataCalled());
  EXPECT_EQ(image_fetch_tab_helper->GetImageUrl(), GURL(kFakeImageUrl));
}

// Tests that the mediator shows the account picker if preferences do not
// contain a default account choice for Save to Photos.
TEST_F(SaveToPhotosMediatorTest, ShowsAccountPickerIfNoDefaultAccountInPrefs) {
  // The feature requires the user being signed-in.
  SignIn();

  // This test assumes there is no default account memorized for Save to Photos.
  profile_->GetPrefs()->ClearPref(prefs::kIosSaveToPhotosDefaultGaiaId);
  profile_->GetPrefs()->ClearPref(prefs::kIosSaveToPhotosSkipAccountPicker);

  // Create a mediator and set up with mock delegate.
  SaveToPhotosMediator* mediator = CreateSaveToPhotosMediator();
  id mock_save_to_photos_mediator_delegate =
      OCMStrictProtocolMock(@protocol(SaveToPhotosMediatorDelegate));
  mediator.delegate = static_cast<id<SaveToPhotosMediatorDelegate>>(
      mock_save_to_photos_mediator_delegate);

  // Expect that the mediator will show the account picker with this
  // configuration.
  NSString* expected_title_text =
      l10n_util::GetNSString(IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_TITLE);
  NSString* expected_body_text =
      l10n_util::GetNSStringF(IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_BODY,
                              base::SysNSStringToUTF16(GetFakeImageName()),
                              base::SysNSStringToUTF16(GetFakeImageSize()));
  NSString* expected_submit_button_title =
      l10n_util::GetNSString(IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_SUBMIT);
  NSString* expected_ask_every_time_switch_label_text = l10n_util::GetNSString(
      IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_ASK_EVERY_TIME);
  OCMExpect([mock_save_to_photos_mediator_delegate
      showAccountPickerWithConfiguration:[OCMArg checkWithBlock:^(
                                                     AccountPickerConfiguration*
                                                         configuration) {
        EXPECT_NSEQ(expected_title_text, configuration.titleText);
        EXPECT_NSEQ(expected_body_text, configuration.bodyText);
        EXPECT_NSEQ(expected_submit_button_title,
                    configuration.submitButtonTitle);
        EXPECT_NSEQ(expected_ask_every_time_switch_label_text,
                    configuration.askEveryTimeSwitchLabelText);
        return YES;
      }]
                        selectedIdentity:nil]);

  // Start the mediator and run until the image has been fetched and
  // processed by the mediator.
  SetUpImageFetchTabHelperQuitClosure();
  [mediator startWithImageURL:GURL(kFakeImageUrl)
                     referrer:web::Referrer()
                     webState:web_state_.get()];
  task_environment_.RunUntilQuit();

  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);
}

// Tests that the mediator skips the account picker if preferences do contain a
// default account choice for Save to Photos.
TEST_F(SaveToPhotosMediatorTest,
       DoesNotShowAccountPickerIfDefaultAccountInPrefs) {
  // The feature requires the user being signed-in.
  SignIn();

  // This test assumes there is a default account memorized for Save to Photos
  // and that the user opted-in skipping the account picker.
  profile_->GetPrefs()->SetString(
      prefs::kIosSaveToPhotosDefaultGaiaId,
      base::SysNSStringToUTF8(fake_identity_.gaiaID).c_str());
  profile_->GetPrefs()->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                                   true);

  // Create a mediator and set up with mock delegate.
  SaveToPhotosMediator* mediator = CreateSaveToPhotosMediator();
  id mock_save_to_photos_mediator_delegate =
      OCMProtocolMock(@protocol(SaveToPhotosMediatorDelegate));
  mediator.delegate = static_cast<id<SaveToPhotosMediatorDelegate>>(
      mock_save_to_photos_mediator_delegate);

  // Check that the Photos service has not been called yet.
  EXPECT_TRUE(GetTestPhotosService()->IsAvailable());
  EXPECT_EQ(GetTestPhotosService()->GetIdentity(), nil);

  // Start the mediator and run until the image has been fetched and
  // processed by the mediator.
  SetUpImageFetchTabHelperQuitClosure();
  [mediator startWithImageURL:GURL(kFakeImageUrl)
                     referrer:web::Referrer()
                     webState:web_state_.get()];
  task_environment_.RunUntilQuit();

  histogram_tester_.ExpectUniqueSample(
      kSaveToPhotosAccountPickerActionsHistogram,
      SaveToPhotosAccountPickerActions::kSkipped, 1);

  // Test that the Photos service has already been called with the identity from
  // the prefs.
  EXPECT_FALSE(GetTestPhotosService()->IsAvailable());
  EXPECT_EQ(GetTestPhotosService()->GetIdentity(), fake_identity_);

  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);
}

// Tests that upon identity selection, the SaveToPhotosMediator starts the
// validation spinner in the account picker, it uploads the image it has fetched
// from the image fetch tab helper, hides the account picker and shows a
// snackbar message when the PhotosService reports upload completion.
TEST_F(SaveToPhotosMediatorTest,
       DidSelectIdentityUploadsImageAndShowsSnackbarMessage) {
  // The feature requires the user being signed-in.
  SignIn();

  // This test assumes there is no default account memorized for Save to Photos.
  profile_->GetPrefs()->ClearPref(prefs::kIosSaveToPhotosDefaultGaiaId);
  profile_->GetPrefs()->ClearPref(prefs::kIosSaveToPhotosSkipAccountPicker);

  // Create a mediator and set up with mock delegate.
  SaveToPhotosMediator* mediator = CreateSaveToPhotosMediator();
  id mock_save_to_photos_mediator_delegate =
      OCMProtocolMock(@protocol(SaveToPhotosMediatorDelegate));
  mediator.delegate = static_cast<id<SaveToPhotosMediatorDelegate>>(
      mock_save_to_photos_mediator_delegate);

  // Start the mediator and run until the image has been fetched and processed
  // by the mediator.
  SetUpImageFetchTabHelperQuitClosure();
  [mediator startWithImageURL:GURL(kFakeImageUrl)
                     referrer:web::Referrer()
                     webState:web_state_.get()];
  task_environment_.RunUntilQuit();

  // Test that the PhotosService has not been used to upload an image yet.
  EXPECT_TRUE(GetTestPhotosService()->IsAvailable());
  EXPECT_EQ(GetTestPhotosService()->GetIdentity(), nil);

  // Expect that the mediator start the validation spinner in the account picker
  // when it knows what identity has been selected.
  OCMExpect([mock_save_to_photos_mediator_delegate
      startValidationSpinnerForAccountPicker]);

  // Run until the mediator calls the Photos service.
  SetUpPhotosServiceQuitClosure();

  // Give the selected identity to the mediator and verify that the mediator
  // asked to start the validation spinner in the account picker.
  [mediator accountPickerDidSelectIdentity:fake_identity_ askEveryTime:YES];
  histogram_tester_.ExpectUniqueSample(
      kSaveToPhotosAccountPickerActionsHistogram,
      SaveToPhotosAccountPickerActions::kSelectedIdentity, 1);
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);

  // Test that the PhotosService is now unavailable and has been given an image
  // to upload.
  EXPECT_FALSE(GetTestPhotosService()->IsAvailable());
  EXPECT_NSEQ(GetTestPhotosService()->GetImageName(),
              base::SysUTF8ToNSString(GURL(kFakeImageUrl).ExtractFileName()));
  EXPECT_NSEQ(GetTestPhotosService()->GetImageData(), GetFakeImageData());
  EXPECT_EQ(GetTestPhotosService()->GetIdentity(), fake_identity_);

  // Expect that the success snackbar is shown once image is uploaded.
  NSString* expected_message = l10n_util::GetNSStringF(
      IDS_IOS_SAVE_TO_PHOTOS_SNACKBAR_IMAGE_SAVED_MESSAGE,
      base::SysNSStringToUTF16(fake_identity_.userEmail));
  NSString* expected_button_text = l10n_util::GetNSString(
      IDS_IOS_SAVE_TO_PHOTOS_SNACKBAR_IMAGE_SAVED_OPEN_BUTTON);
  OCMExpect([mock_save_to_photos_mediator_delegate
      showSnackbarWithMessage:expected_message
                   buttonText:expected_button_text
                messageAction:[OCMArg isNotNil]
             completionAction:[OCMArg isNotNil]]);

  // Run until the PhotosService finishes to upload the image.
  task_environment_.RunUntilQuit();

  // Verify that the success snackbar has been shown.
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);
}

// Tests after the account picker has been displayed, the user can dismiss it
// using the "Cancel" button.
TEST_F(SaveToPhotosMediatorTest, DidCancelBeforeUploadDismissesAccountPicker) {
  // The feature requires the user being signed-in.
  SignIn();

  // This test assumes there is no default account memorized for Save to Photos.
  profile_->GetPrefs()->ClearPref(prefs::kIosSaveToPhotosDefaultGaiaId);
  profile_->GetPrefs()->ClearPref(prefs::kIosSaveToPhotosSkipAccountPicker);

  // Create a mediator and set up with mock delegate.
  SaveToPhotosMediator* mediator = CreateSaveToPhotosMediator();
  id mock_save_to_photos_mediator_delegate =
      OCMProtocolMock(@protocol(SaveToPhotosMediatorDelegate));
  mediator.delegate = static_cast<id<SaveToPhotosMediatorDelegate>>(
      mock_save_to_photos_mediator_delegate);

  // Start the mediator and run until the image has been fetched and processed
  // by the mediator.
  SetUpImageFetchTabHelperQuitClosure();
  [mediator startWithImageURL:GURL(kFakeImageUrl)
                     referrer:web::Referrer()
                     webState:web_state_.get()];
  task_environment_.RunUntilQuit();

  // Test that the PhotosService has not been used to upload an image yet.
  EXPECT_TRUE(GetTestPhotosService()->IsAvailable());
  EXPECT_EQ(GetTestPhotosService()->GetIdentity(), nil);

  // Expect that the mediator hides the account picker when "Cancel" has been
  // tapped.
  OCMExpect([mock_save_to_photos_mediator_delegate hideAccountPicker]);

  // Give the selected identity to the mediator and verify that the mediator
  // asked to start the validation spinner in the account picker.
  [mediator accountPickerDidCancel];
  histogram_tester_.ExpectUniqueSample(
      kSaveToPhotosAccountPickerActionsHistogram,
      SaveToPhotosAccountPickerActions::kCancelled, 1);
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);

  // Expect that the mediator hides Save to Photos the account picker is hidden.
  OCMExpect([mock_save_to_photos_mediator_delegate hideSaveToPhotos]);
  // Let the mediator know that the account picker was hidden.
  [mediator accountPickerWasHidden];
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);
  histogram_tester_.ExpectUniqueSample(
      kSaveToPhotosActionsHistogram,
      SaveToPhotosActions::kFailureUserCancelledWithAccountPicker, 1);
}

// Tests that the SaveToPhotosMediator tries to open the Google Photos app if it
// detects that it is installed and the user taps "Open" in the success
// snackbar.
TEST_F(SaveToPhotosMediatorTest, SnackbarOpenButtonOpensPhotosAppIfInstalled) {
  // The feature requires the user being signed-in.
  SignIn();

  // Create a mediator and set up with mock delegate.
  SaveToPhotosMediator* mediator = CreateSaveToPhotosMediator();
  id mock_save_to_photos_mediator_delegate =
      OCMProtocolMock(@protocol(SaveToPhotosMediatorDelegate));
  mediator.delegate = static_cast<id<SaveToPhotosMediatorDelegate>>(
      mock_save_to_photos_mediator_delegate);

  // Start the mediator and run until the image has been fetched and processed
  // by the mediator.
  SetUpImageFetchTabHelperQuitClosure();
  [mediator startWithImageURL:GURL(kFakeImageUrl)
                     referrer:web::Referrer()
                     webState:web_state_.get()];
  task_environment_.RunUntilQuit();

  // Expect the success snackbar (with a non-nil completion) is shown and save
  // the message and completion actions to later simulate the snackbar being
  // dismissed by the user tapping the "Open" button.
  __block ProceduralBlock savedMessageAction = nil;
  __block void (^savedCompletionAction)(BOOL) = nil;
  OCMExpect([mock_save_to_photos_mediator_delegate
      showSnackbarWithMessage:[OCMArg any]
                   buttonText:[OCMArg any]
                messageAction:[OCMArg checkWithBlock:^(
                                          ProceduralBlock messageAction) {
                  savedMessageAction = messageAction;
                  return messageAction != nil;
                }]
             completionAction:[OCMArg checkWithBlock:^(
                                          void (^completionAction)(BOOL)) {
               savedCompletionAction = completionAction;
               return completionAction != nil;
             }]]);

  // Run until the PhotosService is done uploading, at which point the success
  // snackbar is presented to the user.
  SetUpPhotosServiceQuitClosure();
  [mediator accountPickerDidSelectIdentity:fake_identity_ askEveryTime:YES];
  [mediator accountPickerWasHidden];
  task_environment_.RunUntilQuit();

  // Verify that the success snackbar was shown.
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);
  ASSERT_TRUE(savedMessageAction);
  ASSERT_TRUE(savedCompletionAction);

  // Expect that the mediator will detect that the app is installed when the
  // user taps "Open".
  OCMExpect([mock_application_ canOpenURL:GetGooglePhotosAppURL()])
      .andReturn(YES);

  // Expect that the mediator tries to open the Photos app and switch to the
  // Photos account associated with `fake_identity_`.
  NSString* recently_added_url_string = [kGooglePhotosRecentlyAddedURLString
      stringByAppendingString:fake_identity_.gaiaID];
  NSURL* photos_url = [NSURL URLWithString:recently_added_url_string];
  OCMExpect([mock_application_
                openURL:photos_url
                options:@{
                  UIApplicationOpenURLOptionUniversalLinksOnly : @YES
                }
      completionHandler:nil]);

  // Expect that the mediator hides Save to Photos when the user has tapped
  // "Open".
  OCMExpect([mock_save_to_photos_mediator_delegate hideSaveToPhotos]);

  // Simulate the user tapped the "Open" button.
  savedMessageAction();
  // Simulate the snackbar was dismissed because of user interaction.
  savedCompletionAction(/* user_triggered= */ YES);

  histogram_tester_.ExpectUniqueSample(
      kSaveToPhotosActionsHistogram,
      SaveToPhotosActions::kSuccessAndOpenPhotosApp, 1);

  [mediator disconnect];

  // Verify that the mediator detected that the app is installed and tried to
  // open it.
  EXPECT_OCMOCK_VERIFY(mock_application_);
}

// Tests that the SaveToPhotosMediator tries to show the StoreKit if it detects
// that the Google Photos app is not installed and the user taps "Open" in the
// success snackbar.
TEST_F(SaveToPhotosMediatorTest,
       SnackbarOpenButtonOpensStoreKitIfAppNotInstalled) {
  // The feature requires the user being signed-in.
  SignIn();

  // Create a mediator and set up with mock delegate.
  SaveToPhotosMediator* mediator = CreateSaveToPhotosMediator();
  id mock_save_to_photos_mediator_delegate =
      OCMProtocolMock(@protocol(SaveToPhotosMediatorDelegate));
  mediator.delegate = static_cast<id<SaveToPhotosMediatorDelegate>>(
      mock_save_to_photos_mediator_delegate);

  // Start the mediator and run until the image has been fetched and processed
  // by the mediator.
  SetUpImageFetchTabHelperQuitClosure();
  [mediator startWithImageURL:GURL(kFakeImageUrl)
                     referrer:web::Referrer()
                     webState:web_state_.get()];
  task_environment_.RunUntilQuit();

  // Expect the success snackbar (with a non-nil completion) is shown and save
  // the message and completion actions to later simulate the snackbar being
  // dismissed by the user tapping the "Open" button.
  __block ProceduralBlock savedMessageAction = nil;
  __block void (^savedCompletionAction)(BOOL) = nil;
  OCMExpect([mock_save_to_photos_mediator_delegate
      showSnackbarWithMessage:[OCMArg any]
                   buttonText:[OCMArg any]
                messageAction:[OCMArg checkWithBlock:^(
                                          ProceduralBlock messageAction) {
                  savedMessageAction = messageAction;
                  return messageAction != nil;
                }]
             completionAction:[OCMArg checkWithBlock:^(
                                          void (^completionAction)(BOOL)) {
               savedCompletionAction = completionAction;
               return completionAction != nil;
             }]]);

  // Run until the PhotosService is done uploading, at which point the success
  // snackbar is presented to the user.
  SetUpPhotosServiceQuitClosure();
  [mediator accountPickerDidSelectIdentity:fake_identity_ askEveryTime:YES];
  [mediator accountPickerWasHidden];
  task_environment_.RunUntilQuit();

  // Verify that the success snackbar was shown.
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);
  ASSERT_TRUE(savedMessageAction);
  ASSERT_TRUE(savedCompletionAction);

  // Expect that the mediator will detect that the app is not installed when the
  // user taps "Open".
  OCMExpect([mock_application_ canOpenURL:GetGooglePhotosAppURL()])
      .andReturn(NO);

  // Expect that the mediator tries to show StoreKit with the expected product
  // identifier.
  OCMExpect([mock_save_to_photos_mediator_delegate
      showStoreKitWithProductIdentifier:kGooglePhotosAppProductIdentifier
                          providerToken:kGooglePhotosStoreKitProviderToken
                          campaignToken:kGooglePhotosStoreKitCampaignToken]);

  // Simulate the user tapped the "Open" button.
  savedMessageAction();
  // Simulate the snackbar was dismissed because of user interaction.
  savedCompletionAction(/* user_triggered= */ YES);

  // Verify that the mediator detected that the app is not installed and tried
  // to open show the app in StoreKit.
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);

  // Test that the mediator asks to hide Save to Photos when StoreKit wants to
  // hide.
  OCMExpect([mock_save_to_photos_mediator_delegate hideSaveToPhotos]);
  [mediator storeKitWantsToHide];
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);
  histogram_tester_.ExpectUniqueSample(
      kSaveToPhotosActionsHistogram,
      SaveToPhotosActions::kSuccessAndOpenStoreKitAndAppNotInstalled, 1);

  [mediator disconnect];
}

// Tests that the SaveToPhotosMediator shows an alert with Try Again and Cancel
// options if the PhotosService fails to upload the image.
TEST_F(SaveToPhotosMediatorTest, ShowsTryAgainOrCancelAlertIfUploadFails) {
  // The feature requires the user being signed-in.
  SignIn();

  // Create a mediator and set up with mock delegate.
  SaveToPhotosMediator* mediator = CreateSaveToPhotosMediator();
  id mock_save_to_photos_mediator_delegate =
      OCMProtocolMock(@protocol(SaveToPhotosMediatorDelegate));
  mediator.delegate = static_cast<id<SaveToPhotosMediatorDelegate>>(
      mock_save_to_photos_mediator_delegate);

  // Start the mediator and run until the image has been fetched and processed
  // by the mediator.
  SetUpImageFetchTabHelperQuitClosure();
  [mediator startWithImageURL:GURL(kFakeImageUrl)
                     referrer:web::Referrer()
                     webState:web_state_.get()];
  task_environment_.RunUntilQuit();

  // Set up the PhotosService to simulate upload failure.
  GetTestPhotosService()->SetUploadResult({.successful = false});

  // Expect that the failure alert is shown by the mediator upon upload failure.
  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAVE_TO_PHOTOS_THIS_FILE_COULD_NOT_BE_UPLOADED_TITLE);
  NSString* expected_message = l10n_util::GetNSStringF(
      IDS_IOS_SAVE_TO_PHOTOS_THIS_FILE_COULD_NOT_BE_UPLOADED_MESSAGE,
      base::SysNSStringToUTF16(GetFakeImageName()),
      base::SysNSStringToUTF16(GetFakeImageSize()));
  NSString* expected_cancel_title = l10n_util::GetNSString(IDS_CANCEL);
  NSString* expected_try_again_title = l10n_util::GetNSString(
      IDS_IOS_SAVE_TO_PHOTOS_THIS_FILE_COULD_NOT_BE_UPLOADED_TRY_AGAIN);
  OCMExpect([mock_save_to_photos_mediator_delegate
      showTryAgainOrCancelAlertWithTitle:expected_title
                                 message:expected_message
                           tryAgainTitle:expected_try_again_title
                          tryAgainAction:[OCMArg isNotNil]
                             cancelTitle:expected_cancel_title
                            cancelAction:[OCMArg isNotNil]]);

  // Run until the PhotosService fails to upload.
  SetUpPhotosServiceQuitClosure();
  [mediator accountPickerDidSelectIdentity:fake_identity_ askEveryTime:YES];
  [mediator accountPickerWasHidden];
  task_environment_.RunUntilQuit();

  // Verify that the failure alert has been presented.
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_mediator_delegate);
}
