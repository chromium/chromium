// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "components/prefs/pref_service.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsDoneButton;

namespace {

// Blocks the media content to avoid starting background playing.
class Decider : public web::WebStatePolicyDecider {
 public:
  Decider(web::WebState* web_state) : web::WebStatePolicyDecider(web_state) {}

  bool ShouldAllowRequest(NSURLRequest* request,
                          const RequestInfo& request_info) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Decider);
};

TabModel* GetNormalTabModel() {
  return chrome_test_util::GetMainController()
      .interfaceProvider.mainInterface.tabModel;
}

}  // namespace

// Integration tests using the Google services settings screen.
@interface GoogleServicesSettingsTestCase : ChromeTestCase

@property(nonatomic, strong) id<GREYMatcher> scrollViewMatcher;

@end

@implementation GoogleServicesSettingsTestCase

// Opens the Google services settings view, and closes it.
- (void)testOpenGoogleServicesSettings {
  [self openGoogleServicesSettings];

  // Assert title and accessibility.
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests the Google Services settings.
- (void)testOpeningServices {
  [self openGoogleServicesSettings];
  [self assertNonPersonalizedServices];
}

// Tests the following steps:
//  + Opens sign-in from Google services
//  + Taps on the settings link to open the advanced sign-in settings
//  + Opens "Data from Chromium sync" to interrupt sign-in
- (void)testInterruptSigninFromGoogleServicesSettings {
  // Policy decider to avoid loading "Data from Chrome Sync". Since Chrome is
  // not really signed (just using a fake identity), Google server would answer
  // to start a sign-in workflow by loading this URL.
  std::unique_ptr<Decider> _webStatePolicyDecider(
      new Decider(GetNormalTabModel().webStateList->GetActiveWebState()));
  // Adds default identity.
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      [SigninEarlGreyUtils fakeIdentity1]);
  // Open "Google Services" settings.
  [self openGoogleServicesSettings];
  // Open sign-in.
  id<GREYMatcher> signinCellMatcher =
      [self cellMatcherWithTitleID:IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE
                      detailTextID:
                          IDS_IOS_GOOGLE_SERVICES_SETTINGS_SIGN_IN_DETAIL_TEXT];
  [[EarlGrey selectElementWithMatcher:signinCellMatcher]
      performAction:grey_tap()];
  // Open Settings link.
  [SigninEarlGreyUI tapSettingsLink];
  // Open "Manage Sync" settings.
  id<GREYMatcher> manageSyncMatcher =
      [self cellMatcherWithTitleID:IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE
                      detailTextID:0];
  [[EarlGrey selectElementWithMatcher:manageSyncMatcher]
      performAction:grey_tap()];
  // Open "Data from Chrome sync".
  id<GREYMatcher> manageSyncScrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  id<GREYMatcher> dataFromChromeSyncMatcher = [self
      cellMatcherWithTitleID:IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_TITLE
                detailTextID:
                    IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_DESCRIPTION];
  [[self elementInteractionWithGreyMatcher:dataFromChromeSyncMatcher
                         scrollViewMatcher:manageSyncScrollViewMatcher]
      performAction:grey_tap()];
  [self openGoogleServicesSettings];
  // Verify the sync is not confirmed yet.
  [self assertCellWithTitleID:IDS_IOS_SYNC_SETUP_NOT_CONFIRMED_TITLE
                 detailTextID:IDS_IOS_SYNC_SETTINGS_NOT_CONFIRMED_DESCRIPTION];
}

#pragma mark - Helpers

// Opens the Google services settings.
- (void)openGoogleServicesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  self.scrollViewMatcher =
      grey_accessibilityID(kGoogleServicesSettingsViewIdentifier);
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
}

// Scrolls Google services settings to the top.
- (void)scrollUp {
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      performAction:grey_scrollToContentEdgeWithStartPoint(kGREYContentEdgeTop,
                                                           0.1f, 0.1f)];
}

// Returns grey matcher for a cell with |titleID| and |detailTextID|.
- (id<GREYMatcher>)cellMatcherWithTitleID:(int)titleID
                             detailTextID:(int)detailTextID {
  NSString* accessibilityLabel = GetNSString(titleID);
  if (detailTextID) {
    accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                   GetNSString(detailTextID)];
  }
  return grey_allOf(grey_accessibilityLabel(accessibilityLabel),
                    grey_kindOfClass([UITableViewCell class]),
                    grey_sufficientlyVisible(), nil);
}

// Returns GREYElementInteraction for |matcher|, using |scrollViewMatcher| to
// scroll.
- (GREYElementInteraction*)
    elementInteractionWithGreyMatcher:(id<GREYMatcher>)matcher
                    scrollViewMatcher:(id<GREYMatcher>)scrollViewMatcher {
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:searchAction
      onElementWithMatcher:scrollViewMatcher];
}

// Returns GREYElementInteraction for |matcher|, with |self.scrollViewMatcher|
// to scroll.
- (GREYElementInteraction*)elementInteractionWithGreyMatcher:
    (id<GREYMatcher>)matcher {
  return [self elementInteractionWithGreyMatcher:matcher
                               scrollViewMatcher:self.scrollViewMatcher];
}

// Returns GREYElementInteraction for a cell based on the title string ID and
// the detail text string ID. |detailTextID| should be set to 0 if it doesn't
// exist in the cell.
- (GREYElementInteraction*)cellElementInteractionWithTitleID:(int)titleID
                                                detailTextID:(int)detailTextID {
  id<GREYMatcher> cellMatcher = [self cellMatcherWithTitleID:titleID
                                                detailTextID:detailTextID];
  return [self elementInteractionWithGreyMatcher:cellMatcher];
}

// Asserts that a cell exists, based on its title string ID and its detail text
// string ID. |detailTextID| should be set to 0 if it doesn't exist in the cell.
- (void)assertCellWithTitleID:(int)titleID detailTextID:(int)detailTextID {
  [[self cellElementInteractionWithTitleID:titleID detailTextID:detailTextID]
      assertWithMatcher:grey_notNil()];
}

// Asserts that the non-personalized service section is visible.
- (void)assertNonPersonalizedServices {
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL];
  [self
      assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL];
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL];
}

@end
