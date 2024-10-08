// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"

#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider+Testing.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Menu scenario to create a BrowserActionFactory.
const MenuScenarioHistogram kTestMenuScenario =
    kMenuScenarioHistogramHistoryEntry;

// Email for the primary account when signing-in.
const char kPrimaryAccountEmail[] = "peter.parker@gmail.com";

// Image source URL for the context menu params.
const char kImageUrl[] = "https://www.example.com/image.jpg";

// Link URL for the context menu params.
const char kLinkUrl[] = "https://www.example.com";

// Returns context menu params with `src_url` set to `image_url`.
web::ContextMenuParams GetContextMenuParamsWithImageUrl(const char* image_url) {
  web::ContextMenuParams params;
  params.src_url = GURL(image_url);
  return params;
}

// Returns context menu params with `link_url` set to `kLinkUrl` and the given
// location.
web::ContextMenuParams GetContextMenuParamsWithLinkURL(const char* link_url,
                                                       CGPoint location) {
  web::ContextMenuParams params;
  params.link_url = GURL(kLinkUrl);
  params.location = location;
  return params;
}

}  // namespace

// Unit tests for the ContextMenuConfigurationProvider.
class ContextMenuConfigurationProviderTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());

    base_view_controller_ = [[UIViewController alloc] init];
    configuration_provider_ = [[ContextMenuConfigurationProvider alloc]
           initWithBrowser:browser_.get()
        baseViewController:base_view_controller_];

    mock_application_command_handler =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_command_handler
                     forProtocol:@protocol(ApplicationCommands)];
    mock_mini_map_commands_handler =
        OCMStrictProtocolMock(@protocol(MiniMapCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_mini_map_commands_handler
                     forProtocol:@protocol(MiniMapCommands)];
    mock_unit_conversion_handler =
        OCMStrictProtocolMock(@protocol(UnitConversionCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_unit_conversion_handler
                     forProtocol:@protocol(UnitConversionCommands)];
    mock_save_to_photos_commands_handler =
        OCMStrictProtocolMock(@protocol(SaveToPhotosCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_save_to_photos_commands_handler
                     forProtocol:@protocol(SaveToPhotosCommands)];
    mock_activity_service_commands_handler =
        OCMStrictProtocolMock(@protocol(ActivityServiceCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_activity_service_commands_handler
                     forProtocol:@protocol(ActivityServiceCommands)];
  }

  void TearDown() final {
    [configuration_provider_ stop];
    PlatformTest::TearDown();
  }

  // Sign-in with a fake account.
  void SignIn() {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        kPrimaryAccountEmail, signin::ConsentLevel::kSignin);
  }

  // Returns a BrowserActionFactory.
  BrowserActionFactory* GetBrowserActionFactory() {
    return [[BrowserActionFactory alloc] initWithBrowser:browser_.get()
                                                scenario:kTestMenuScenario];
  }

  // The UIMenu returned by the action provider from the
  // ContextMenuConfigurationProvider.
  UIMenu* GetContextMenuForParams(const web::ContextMenuParams& params) {
    UIContextMenuActionProvider actionProvider = [configuration_provider_
        contextMenuActionProviderForWebState:browser_->GetWebStateList()
                                                 ->GetActiveWebState()
                                      params:params];
    return actionProvider(@[]);
  }

  // Returns the browser's active web state.
  web::FakeWebState* GetActiveWebState() {
    return static_cast<web::FakeWebState*>(
        browser_->GetWebStateList()->GetActiveWebState());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  ContextMenuConfigurationProvider* configuration_provider_;

  id mock_mini_map_commands_handler;
  id mock_unit_conversion_handler;
  id mock_save_to_photos_commands_handler;
  id mock_activity_service_commands_handler;
  id mock_application_command_handler;
};

// Test that the "Save Image in Google Photos" action is added to the context
// menu if enough conditions are met.
TEST_F(ContextMenuConfigurationProviderTest, HasSaveImageToPhotosMenuElement) {
  // Enable the Save to Photos feature flag.
  base::test::ScopedFeatureList feature_list(kIOSSaveToPhotos);

  // The action is only available if the user is signed-in.
  SignIn();

  // Get menu with params containing image source URL.
  web::ContextMenuParams paramsWithImage =
      GetContextMenuParamsWithImageUrl(kImageUrl);
  UIMenu* menu = GetContextMenuForParams(paramsWithImage);

  BrowserActionFactory* actionFactory = GetBrowserActionFactory();
  UIMenuElement* expectedMenuElement =
      [actionFactory actionToSaveToPhotosWithImageURL:GURL(kImageUrl)
                                             referrer:web::Referrer()
                                             webState:GetActiveWebState()
                                                block:nil];

  // Test that there is an element with the expected title in the menu.
  NSUInteger indexOfFoundMenuElement =
      [menu.children indexOfObjectPassingTest:^BOOL(UIMenuElement* menuElement,
                                                    NSUInteger, BOOL*) {
        return [menuElement.title isEqualToString:expectedMenuElement.title];
      }];
  ASSERT_TRUE(indexOfFoundMenuElement != NSNotFound);

  UIMenuElement* foundMenuElement = menu.children[indexOfFoundMenuElement];
  // Test that the element has the expected subtitle.
  EXPECT_EQ(foundMenuElement.subtitle, expectedMenuElement.subtitle);
  // Test that the element has the expected image.
  EXPECT_TRUE([foundMenuElement.image isEqual:expectedMenuElement.image]);
}

// Test that the "Share" action is added to the context
// menu if enough conditions are met.
TEST_F(ContextMenuConfigurationProviderTest, HasShareInWebContextMenuElement) {
  // Enable the Share in web context menu flag.
  base::test::ScopedFeatureList feature_list(kShareInWebContextMenuIOS);

  // Get menu with params containing url.
  web::ContextMenuParams params_with_link =
      GetContextMenuParamsWithLinkURL(kLinkUrl, CGPointMake(0, 0));

  UIMenu* menu = GetContextMenuForParams(params_with_link);

  BrowserActionFactory* action_factory = GetBrowserActionFactory();
  UIMenuElement* expected_menu_element =
      [action_factory actionToShareWithBlock:nil];

  ASSERT_NE(expected_menu_element, nil);

  // Test that there is an element with the expected title in the menu.
  NSUInteger index_of_found_menu_element =
      [menu.children indexOfObjectPassingTest:^BOOL(UIMenuElement* menu_element,
                                                    NSUInteger, BOOL*) {
        return [menu_element.title isEqualToString:expected_menu_element.title];
      }];

  ASSERT_TRUE(index_of_found_menu_element != NSNotFound);

  UIMenuElement* found_menu_element =
      menu.children[index_of_found_menu_element];

  // Test that the element has the expected subtitle.
  EXPECT_NSEQ(found_menu_element.subtitle, expected_menu_element.subtitle);
  // Test that the element has the expected image.
  EXPECT_NSEQ(found_menu_element.image, expected_menu_element.image);
}
