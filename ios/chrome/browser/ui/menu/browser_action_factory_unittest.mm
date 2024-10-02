// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/browser_action_factory.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/sessions/model/test_session_service.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "url/gurl.h"

namespace {
const MenuScenarioHistogram kTestMenuScenario =
    kMenuScenarioHistogramHistoryEntry;
}  // namespace

// Test fixture for the BrowserActionFactory.
class BrowserActionFactoryTest : public PlatformTest {
 protected:
  BrowserActionFactoryTest() : test_title_(@"SomeTitle") {}

  void SetUp() override {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    test_browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];

    mock_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_commands_handler_
                     forProtocol:@protocol(SettingsCommands)];

    mock_browser_coordinator_commands_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_coordinator_commands_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    mock_qr_scanner_commands_handler_ =
        OCMStrictProtocolMock(@protocol(QRScannerCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_qr_scanner_commands_handler_
                     forProtocol:@protocol(QRScannerCommands)];

    mock_load_query_commands_handler_ =
        OCMStrictProtocolMock(@protocol(LoadQueryCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_load_query_commands_handler_
                     forProtocol:@protocol(LoadQueryCommands)];

    mock_save_to_photos_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SaveToPhotosCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_save_to_photos_commands_handler_
                     forProtocol:@protocol(SaveToPhotosCommands)];
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  NSString* test_title_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> test_browser_;
  id mock_application_commands_handler_;
  id mock_settings_commands_handler_;
  id mock_browser_coordinator_commands_handler_;
  id mock_qr_scanner_commands_handler_;
  id mock_load_query_commands_handler_;
  id mock_save_to_photos_commands_handler_;
};

// Tests that the Open in New Tab actions have the right titles and images.
TEST_F(BrowserActionFactoryTest, OpenInNewTabAction_URL) {
  GURL testURL = GURL("https://example.com");

  // Using an action factory with a browser should return expected action.
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kNewTabActionSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);

  UIAction* actionWithURL = [factory actionToOpenInNewTabWithURL:testURL
                                                      completion:nil];
  EXPECT_NSEQ(expectedTitle, actionWithURL.title);
  EXPECT_EQ(expectedImage, actionWithURL.image);

  UIAction* actionWithBlock = [factory actionToOpenInNewTabWithBlock:nil];
  EXPECT_NSEQ(expectedTitle, actionWithBlock.title);
  EXPECT_EQ(expectedImage, actionWithBlock.image);
}

// Tests that the Open in New Incognito Tab actions have the right titles
// and images.
TEST_F(BrowserActionFactoryTest, OpenInNewIncognitoTabAction_URL) {
  GURL testURL = GURL("https://example.com");

  // Using an action factory with a browser should return expected action.
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage =
      CustomSymbolWithPointSize(kIncognitoSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE);

  UIAction* actionWithURL =
      [factory actionToOpenInNewIncognitoTabWithURL:testURL completion:nil];
  EXPECT_NSEQ(expectedTitle, actionWithURL.title);
  EXPECT_EQ(expectedImage, actionWithURL.image);

  UIAction* actionWithBlock =
      [factory actionToOpenInNewIncognitoTabWithBlock:nil];
  EXPECT_NSEQ(expectedTitle, actionWithBlock.title);
  EXPECT_EQ(expectedImage, actionWithBlock.image);
}

// Tests that the Open in New Window action has the right title and image.
TEST_F(BrowserActionFactoryTest, OpenInNewWindowAction) {
  GURL testURL = GURL("https://example.com");

  // Using an action factory with a browser should return expected action.
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kNewWindowActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW);

  // Test URL variant
  UIAction* action =
      [factory actionToOpenInNewWindowWithURL:testURL
                               activityOrigin:WindowActivityToolsOrigin];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);

  // Test user activity variant
  action = [factory
      actionToOpenInNewWindowWithActivity:ActivityToLoadURL(
                                              WindowActivityToolsOrigin,
                                              testURL)];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the open image action has the right title and image.
TEST_F(BrowserActionFactoryTest, OpenImageAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  GURL testURL = GURL("https://example.com/logo.png");

  UIImage* expectedImage = DefaultSymbolWithPointSize(kOpenImageActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);

  UIAction* action = [factory actionOpenImageWithURL:testURL
                                          completion:^{
                                          }];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the open image in new tab action has the right title and image.
TEST_F(BrowserActionFactoryTest, OpenImageInNewTabAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  GURL testURL = GURL("https://example.com/logo.png");
  UrlLoadParams testParams = UrlLoadParams::InNewTab(testURL);

  UIImage* expectedImage =
      CustomSymbolWithPointSize(kPhotoBadgePlusSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);

  UIAction* action =
      [factory actionOpenImageInNewTabWithUrlLoadParams:testParams
                                             completion:^{
                                             }];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the hide preview action has the right title and image.
TEST_F(BrowserActionFactoryTest, OpenNewTabAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kNewTabActionSymbol, kSymbolActionPointSize);
  NSString* expectedTitle = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_TAB);

  UIAction* action = [factory actionToOpenNewTab];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
  EXPECT_EQ(0U, action.attributes);

  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kForced)));

  UIAction* managed_action = [factory actionToOpenNewTab];
  EXPECT_EQ(UIMenuElementAttributesDisabled, managed_action.attributes);
}

// Tests that the action has the right title and image.
TEST_F(BrowserActionFactoryTest, OpenNewIncognitoTabAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage =
      CustomSymbolWithPointSize(kIncognitoSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);

  UIAction* action = [factory actionToOpenNewIncognitoTab];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
  EXPECT_EQ(0U, action.attributes);

  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kDisabled)));

  UIAction* managed_action = [factory actionToOpenNewIncognitoTab];
  EXPECT_EQ(UIMenuElementAttributesDisabled, managed_action.attributes);
}

// Tests that the action has the right title and image.
TEST_F(BrowserActionFactoryTest, CloseCurrentTabAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CLOSE_TAB);

  UIAction* action = [factory actionToCloseCurrentTab];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
  EXPECT_EQ(UIMenuElementAttributesDestructive, action.attributes);
}

// Tests that the action has the right title and image.
TEST_F(BrowserActionFactoryTest, ShowQRScannerAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kQRCodeFinderActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_QR_SCANNER);

  UIAction* action = [factory actionToShowQRScanner];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the action has the right title and image.
TEST_F(BrowserActionFactoryTest, StartVoiceSearchAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kMicrophoneSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_VOICE_SEARCH);

  UIAction* action = [factory actionToStartVoiceSearch];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the action has the right title, image and attributes.
TEST_F(BrowserActionFactoryTest, StartNewSearchAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage =
      DefaultSymbolWithPointSize(kSearchSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_SEARCH);

  UIAction* action = [factory actionToStartNewSearch];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
  EXPECT_EQ(0U, action.attributes);

  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kForced)));

  UIAction* managed_action = [factory actionToStartNewSearch];
  EXPECT_EQ(UIMenuElementAttributesDisabled, managed_action.attributes);
}

// Tests that the action has the right title, image and attributes.
TEST_F(BrowserActionFactoryTest, NewIncognitoSearchAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage =
      CustomSymbolWithPointSize(kIncognitoSymbol, kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_SEARCH);

  UIAction* action = [factory actionToStartNewIncognitoSearch];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
  EXPECT_EQ(0U, action.attributes);

  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kDisabled)));

  UIAction* managed_action = [factory actionToStartNewIncognitoSearch];
  EXPECT_EQ(UIMenuElementAttributesDisabled, managed_action.attributes);
}

// Tests that the action has the right title and image.
TEST_F(BrowserActionFactoryTest, SearchCopiedImageAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kClipboardActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SEARCH_COPIED_IMAGE);

  UIAction* action = [factory actionToSearchCopiedImage];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the action has the right title and image.
TEST_F(BrowserActionFactoryTest, SearchCopiedURLAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kClipboardActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_VISIT_COPIED_LINK);

  UIAction* action = [factory actionToSearchCopiedURL];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the action has the right title and image.
TEST_F(BrowserActionFactoryTest, SearchCopiedTextAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

  UIImage* expectedImage = DefaultSymbolWithPointSize(kClipboardActionSymbol,
                                                      kSymbolActionPointSize);
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SEARCH_COPIED_TEXT);

  UIAction* action = [factory actionToSearchCopiedText];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}

// Tests that the action has the right title and image.
TEST_F(BrowserActionFactoryTest, SaveImageInGooglePhotosAction) {
  BrowserActionFactory* factory =
      [[BrowserActionFactory alloc] initWithBrowser:test_browser_.get()
                                           scenario:kTestMenuScenario];

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* expectedImage =
      CustomSymbolWithPointSize(kGooglePhotosSymbol, kSymbolActionPointSize);
#else
  UIImage* expectedImage = DefaultSymbolWithPointSize(kSaveImageActionSymbol,
                                                      kSymbolActionPointSize);
#endif
  NSString* expectedTitle =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SAVE_IMAGE_TO_PHOTOS);

  GURL fakeImageURL("https://example.com/image.png");
  web::Referrer fakeImageReferrer;
  std::unique_ptr<web::WebState> fakeWebState =
      std::make_unique<web::FakeWebState>();
  UIAction* action =
      [factory actionToSaveToPhotosWithImageURL:fakeImageURL
                                       referrer:fakeImageReferrer
                                       webState:fakeWebState.get()
                                          block:nil];

  EXPECT_NSEQ(expectedTitle, action.title);
  EXPECT_EQ(expectedImage, action.image);
}
