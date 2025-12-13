// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/themes/ntp_background.pb.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/new_tab_page_app_interface.h"
#import "ios/chrome/browser/home_customization/model/home_customization_seed_colors.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_accessibility_identifiers.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "skia/ext/skia_utils_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace {

const char kTestImageRelativeURL[] = "/some_image.jpg";

const char kCollectionsRelativeURL[] =
    "/cast/chromecast/home/wallpaper/collections?rt=b";

const char kCollectionsImagesRelativeURL[] =
    "/cast/chromecast/home/wallpaper/collection-images?rt=b";

const char kCollectionTitle[] = "Shapes Title";

const char kImageAttribution[] = "attribution text";

// Provides responses containing a custom title for fake URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    net::EmbeddedTestServer* server,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);

  if (request.relative_url == kCollectionsRelativeURL) {
    ntp::background::Collection collection;
    collection.set_collection_id("shapes");
    collection.set_collection_name(kCollectionTitle);
    collection.add_preview()->set_image_url(
        server->GetURL(kTestImageRelativeURL).spec());
    ntp::background::GetCollectionsResponse collectionsResponse;
    *collectionsResponse.add_collections() = collection;
    std::string responseString;
    collectionsResponse.SerializeToString(&responseString);

    response->set_content(responseString);
    return std::move(response);
  }

  if (request.relative_url == kCollectionsImagesRelativeURL) {
    ntp::background::Image image;
    image.set_asset_id(12345);
    image.set_image_url(server->GetURL(kTestImageRelativeURL).spec());
    image.add_attribution()->set_text(kImageAttribution);
    image.set_action_url("/test");
    ntp::background::GetImagesInCollectionResponse imageResponse;
    *imageResponse.add_images() = image;
    std::string responseString;
    imageResponse.SerializeToString(&responseString);

    response->set_content(responseString);
    return std::move(response);
  }

  if (request.relative_url.starts_with(kTestImageRelativeURL)) {
    response->set_content_type("image/png");
    UIImage* image = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(10, 10), UIColor.greenColor);
    NSData* image_data = UIImagePNGRepresentation(image);
    response->set_content(std::string(
        static_cast<const char*>(image_data.bytes), image_data.length));
    return std::move(response);
  }

  return nullptr;
}

}  // namespace

@interface HomeBackgroundCustomizationTestCase : ChromeTestCase {
  GURL _baseURL;
}
@end

@implementation HomeBackgroundCustomizationTestCase

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse, self.testServer));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  _baseURL = self.testServer->base_url();

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("--") + "collections-base-url" +
                                   "=" + _baseURL.spec());

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kNTPBackgroundCustomization);

  return config;
}

// Tests that a custom color can be set.
- (void)testCustomizeColor {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBackgroundPickerCellAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertAction(l10n_util::GetNSStringWithFixup(
                     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_COLOR_TITLE))]
      performAction:grey_tap()];

  SeedColor chosenColor = kSeedColors[0];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   chosenColor.accessibilityNameId))]
      performAction:grey_tap()];

  NewTabPageColorPalette* palette =
      [NewTabPageAppInterface currentBackgroundColor];
  SkColor paletteSeedColor = skia::UIColorToSkColor(palette.seedColor);
  EXPECT_EQ(chosenColor.color, paletteSeedColor);

  // Tapping Done should dismiss the entire menu and keep the background color.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPickerViewDoneButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kHomeCustomizationMainViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  palette = [NewTabPageAppInterface currentBackgroundColor];
  paletteSeedColor = skia::UIColorToSkColor(palette.seedColor);
  EXPECT_EQ(chosenColor.color, paletteSeedColor);
}

// Tests that a custom gallery background can be set.
- (void)testCustomizeGalleryBackground {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBackgroundPickerCellAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::AlertAction(l10n_util::GetNSStringWithFixup(
              IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESET_GALLERY_TITLE))]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForMatcher:grey_text(base::SysUTF8ToNSString(kCollectionTitle))];

  id<GREYMatcher> cellMatcher = grey_allOf(
      grey_accessibilityLabel(base::SysUTF8ToNSString(kImageAttribution)),
      grey_ancestor(grey_accessibilityID(
          kHomeCustomizationGalleryPickerViewAccessibilityIdentifier)),
      nil);
  [[EarlGrey selectElementWithMatcher:cellMatcher] performAction:grey_tap()];

  EXPECT_TRUE([NewTabPageAppInterface hasBackgroundImage]);

  // Tapping Done should dismiss the entire menu and keep the background image.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPickerViewDoneButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kHomeCustomizationMainViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  EXPECT_TRUE([NewTabPageAppInterface hasBackgroundImage]);
}

// Tests that a custom camera roll image can be set.
- (void)testCustomizeCameraRollBackground {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBackgroundPickerCellAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::AlertAction(l10n_util::GetNSStringWithFixup(
              IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PHOTO_LIBRARY_TITLE))]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(
                         kPhotoFramingMainViewAccessibilityIdentifier)];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPhotoFramingViewSaveButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  EXPECT_TRUE([NewTabPageAppInterface hasBackgroundImage]);
}

// Tests that picking and then cancelling a color does not end up changing the
// NTP's background color.
- (void)testCancelCustomizeColor {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPCustomizationMenuButtonIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBackgroundPickerCellAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertAction(l10n_util::GetNSStringWithFixup(
                     IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_COLOR_TITLE))]
      performAction:grey_tap()];

  SeedColor chosenColor = kSeedColors[0];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   chosenColor.accessibilityNameId))]
      performAction:grey_tap()];

  NewTabPageColorPalette* p = [NewTabPageAppInterface currentBackgroundColor];
  SkColor paletteSeedColor = skia::UIColorToSkColor(p.seedColor);
  EXPECT_EQ(chosenColor.color, paletteSeedColor);

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPickerViewCancelButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  p = [NewTabPageAppInterface currentBackgroundColor];
  EXPECT_EQ(nil, p);

  // The main customization menu should still be visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kHomeCustomizationMainViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

@end
