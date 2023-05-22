// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/format_macros.h"
#import "base/json/json_string_value_serializer.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/system_alert_handler.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::ActivityViewHeader;
using chrome_test_util::CopyLinkButton;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::OpenLinkInIncognitoButton;
using chrome_test_util::OpenLinkInNewWindowButton;
using chrome_test_util::ShareButton;

namespace {
NSString* const kWaitForPageToStartLoadingError = @"Page did not start to load";
NSString* const kWaitForPageToFinishLoadingError =
    @"Page did not finish loading";
NSString* const kHistoryError = @"Error occurred during history verification.";
NSString* const kWaitForRestoreSessionToFinishError =
    @"Session restoration did not finish";

// Helper class to allow EarlGrey to match elements with isAccessible=N.
class ScopedMatchNonAccessibilityElements {
 public:
  ScopedMatchNonAccessibilityElements() {
    original_value_ = GREY_CONFIG_BOOL(kGREYConfigKeyIgnoreIsAccessible);
    [[GREYConfiguration sharedConfiguration]
            setValue:@YES
        forConfigKey:kGREYConfigKeyIgnoreIsAccessible];
  }

  ~ScopedMatchNonAccessibilityElements() {
    [[GREYConfiguration sharedConfiguration]
            setValue:[NSNumber numberWithBool:original_value_]
        forConfigKey:kGREYConfigKeyIgnoreIsAccessible];
  }

 private:
  BOOL original_value_;
};

}  // namespace

namespace chrome_test_util {
UIWindow* GetAnyKeyWindow() {
  // Only one or zero foreground scene should be available if this is called.
  NSSet<UIScene*>* scenes =
      [GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication]
          .connectedScenes;
  int foregroundScenes = 0;
  for (UIScene* scene in scenes) {
    if (scene.activationState == UISceneActivationStateForegroundInactive ||
        scene.activationState == UISceneActivationStateForegroundActive) {
      foregroundScenes++;
    }
  }
  DCHECK(foregroundScenes <= 1);

  return [ChromeEarlGreyAppInterface keyWindow];
}
}  // namespace chrome_test_util

id<GREYAction> grey_longPressWithDuration(base::TimeDelta duration) {
  return grey_longPressWithDuration(duration.InSecondsF());
}

@interface ChromeEarlGreyImpl ()

// Waits for session restoration to finish within a timeout, or a GREYAssert is
// induced.
- (void)waitForRestoreSessionToFinish;

// Perform a tap with a timeout, or a GREYAssert is induced. Occasionally EG
// doesn't sync up properly to the animations of tab switcher, so it is
// necessary to poll.
- (void)waitForAndTapButton:(id<GREYMatcher>)button;

@end

@implementation ChromeEarlGreyImpl

#pragma mark - Test Utilities

- (void)waitForMatcher:(id<GREYMatcher>)matcher {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  NSString* errorString =
      [NSString stringWithFormat:@"Waiting for matcher %@ failed.", matcher];
  EG_TEST_HELPER_ASSERT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, condition),
      errorString);
}

#pragma mark - Device Utilities

- (BOOL)isIPadIdiom {
  UIUserInterfaceIdiom idiom =
      [[GREY_REMOTE_CLASS_IN_APP(UIDevice) currentDevice] userInterfaceIdiom];

  return idiom == UIUserInterfaceIdiomPad;
}

- (BOOL)isRTL {
  return [ChromeEarlGreyAppInterface isRTL];
}

- (BOOL)isCompactWidth {
  UIUserInterfaceSizeClass horizontalSpace =
      [[chrome_test_util::GetAnyKeyWindow() traitCollection]
          horizontalSizeClass];
  return horizontalSpace == UIUserInterfaceSizeClassCompact;
}

- (BOOL)isCompactHeight {
  UIUserInterfaceSizeClass verticalSpace =
      [[chrome_test_util::GetAnyKeyWindow() traitCollection] verticalSizeClass];
  return verticalSpace == UIUserInterfaceSizeClassCompact;
}

- (BOOL)isSplitToolbarMode {
  return [self isCompactWidth] && ![self isCompactHeight];
}

- (BOOL)isRegularXRegularSizeClass {
  UITraitCollection* traitCollection =
      [chrome_test_util::GetAnyKeyWindow() traitCollection];
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

#pragma mark - History Utilities (EG2)

- (void)clearBrowsingHistory {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface clearBrowsingHistory]);

  // After clearing browsing history via code, wait for the UI to be done
  // with any updates. This includes icons from the new tab page being removed.
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)killWebKitNetworkProcess {
  [ChromeEarlGreyAppInterface killWebKitNetworkProcess];
}

- (NSInteger)browsingHistoryEntryCount {
  NSError* error = nil;
  NSInteger result =
      [ChromeEarlGreyAppInterface browsingHistoryEntryCountWithError:&error];
  EG_TEST_HELPER_ASSERT_NO_ERROR(error);
  return result;
}

- (NSInteger)navigationBackListItemsCount {
  return [ChromeEarlGreyAppInterface navigationBackListItemsCount];
}

- (void)removeBrowsingCache {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface removeBrowsingCache]);
}

- (void)saveSessionImmediately {
  [ChromeEarlGreyAppInterface saveSessionImmediately];
}

#pragma mark - Navigation Utilities (EG2)

- (void)goBack {
  [ChromeEarlGreyAppInterface startGoingBack];

  [self waitForPageToFinishLoading];
}

- (void)goForward {
  [ChromeEarlGreyAppInterface startGoingForward];
  [self waitForPageToFinishLoading];
}

- (void)reload {
  [self reloadAndWaitForCompletion:YES];
}

- (void)reloadAndWaitForCompletion:(BOOL)wait {
  [ChromeEarlGreyAppInterface startReloading];
  if (wait) {
    [self waitForPageToFinishLoading];
  }
}

- (void)openURLFromExternalApp:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface openURLFromExternalApp:spec];
}

- (void)dismissSettings {
  [ChromeEarlGreyAppInterface dismissSettings];
}

#pragma mark - Tab Utilities (EG2)

- (void)selectTabAtIndex:(NSUInteger)index {
  [ChromeEarlGreyAppInterface selectTabAtIndex:index];
  // Tab changes are initiated through `WebStateList`. Need to wait its
  // obeservers to complete UI changes at app.
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (BOOL)isIncognitoMode {
  return [ChromeEarlGreyAppInterface isIncognitoMode];
}

- (void)closeTabAtIndex:(NSUInteger)index {
  [ChromeEarlGreyAppInterface closeTabAtIndex:index];
  // Tab changes are initiated through `WebStateList`. Need to wait its
  // obeservers to complete UI changes at app.
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (NSUInteger)mainTabCount {
  return [ChromeEarlGreyAppInterface mainTabCount];
}

- (NSUInteger)inactiveTabCount {
  return [ChromeEarlGreyAppInterface inactiveTabCount];
}

- (NSUInteger)incognitoTabCount {
  return [ChromeEarlGreyAppInterface incognitoTabCount];
}

- (NSUInteger)browserCount {
  return [ChromeEarlGreyAppInterface browserCount];
}

- (NSUInteger)evictedMainTabCount {
  return [ChromeEarlGreyAppInterface evictedMainTabCount];
}

- (void)evictOtherBrowserTabs {
  [ChromeEarlGreyAppInterface evictOtherBrowserTabs];
}

- (void)simulateTabsBackgrounding {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface simulateTabsBackgrounding]);
}

- (void)setCurrentTabsToBeColdStartTabs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface setCurrentTabsToBeColdStartTabs]);
}

- (void)resetTabUsageRecorder {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface resetTabUsageRecorder]);
}

- (void)openNewTab {
  [ChromeEarlGreyAppInterface openNewTab];
  [self waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:(GURL)url {
  [ChromeEarlGreyAppInterface
      simulateExternalAppURLOpeningWithURL:net::NSURLWithGURL(url)];
  // Wait until the navigation is finished.
  GREYCondition* finishedLoading = [GREYCondition
      conditionWithName:kWaitForPageToStartLoadingError
                  block:^{
                    return url == [ChromeEarlGrey webStateVisibleURL];
                  }];
  bool pageLoaded =
      [finishedLoading waitWithTimeout:kWaitForPageLoadTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, kWaitForPageToStartLoadingError);
  // Wait until the page is loaded.
  [self waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)simulateAddAccountFromWeb {
  [ChromeEarlGreyAppInterface simulateAddAccountFromWeb];
  [self waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeCurrentTab {
  [ChromeEarlGreyAppInterface closeCurrentTab];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)openNewIncognitoTab {
  [ChromeEarlGreyAppInterface openNewIncognitoTab];
  [self waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeAllTabsInCurrentMode {
  [ChromeEarlGreyAppInterface closeAllTabsInCurrentMode];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeAllNormalTabs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface closeAllNormalTabs]);
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeAllIncognitoTabs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface closeAllIncognitoTabs]);
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeAllTabs {
  [ChromeEarlGreyAppInterface closeAllTabs];
  // Tab changes are initiated through `WebStateList`. Need to wait its
  // obeservers to complete UI changes at app.
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)waitForPageToFinishLoading {
  GREYCondition* finishedLoading = [GREYCondition
      conditionWithName:kWaitForPageToFinishLoadingError
                  block:^{
                    return ![ChromeEarlGreyAppInterface isLoading];
                  }];

  BOOL pageLoaded =
      [finishedLoading waitWithTimeout:kWaitForPageLoadTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, kWaitForPageToFinishLoadingError);
}

- (void)sceneOpenURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface sceneOpenURL:spec];
}

- (void)loadURL:(const GURL&)URL waitForCompletion:(BOOL)wait {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface startLoadingURL:spec];
  if (wait) {
    [self waitForWebStateVisible];
    [self waitForPageToFinishLoading];
    // Loading URL (especially the first time) can trigger alerts.
    [SystemAlertHandler handleSystemAlertIfVisible];
  }
}

- (void)loadURL:(const GURL&)URL {
  return [self loadURL:URL waitForCompletion:YES];
}

- (BOOL)isLoading {
  return [ChromeEarlGreyAppInterface isLoading];
}

- (void)waitForSufficientlyVisibleElementWithMatcher:(id<GREYMatcher>)matcher {
  NSString* errorDescription = [NSString
      stringWithFormat:
          @"Failed waiting for element with matcher %@ to become visible",
          matcher];

  GREYCondition* waitForElement = [GREYCondition
      conditionWithName:errorDescription
                  block:^{
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:matcher]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }];

  bool matchedElement =
      [waitForElement waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(matchedElement, errorDescription);
}

- (void)waitForNotSufficientlyVisibleElementWithMatcher:
    (id<GREYMatcher>)matcher {
  NSString* errorDescription = [NSString
      stringWithFormat:
          @"Failed waiting for element with matcher %@ to become not visible",
          matcher];

  GREYCondition* waitForElement = [GREYCondition
      conditionWithName:errorDescription
                  block:^{
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:matcher]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error != nil;
                  }];

  bool matchedElement =
      [waitForElement waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(matchedElement, errorDescription);
}

- (void)waitForUIElementToAppearWithMatcher:(id<GREYMatcher>)matcher {
  [self waitForUIElementToAppearWithMatcher:matcher
                                    timeout:kWaitForUIElementTimeout];
}

- (BOOL)testUIElementAppearanceWithMatcher:(id<GREYMatcher>)matcher {
  return [self testUIElementAppearanceWithMatcher:matcher
                                          timeout:kWaitForUIElementTimeout];
}

- (void)waitForUIElementToAppearWithMatcher:(id<GREYMatcher>)matcher
                                    timeout:(base::TimeDelta)timeout {
  NSString* errorDescription = [NSString
      stringWithFormat:@"Failed waiting for element with matcher %@ to appear",
                       matcher];
  bool matched = [self testUIElementAppearanceWithMatcher:matcher
                                                  timeout:timeout];
  EG_TEST_HELPER_ASSERT_TRUE(matched, errorDescription);
}

- (BOOL)testUIElementAppearanceWithMatcher:(id<GREYMatcher>)matcher
                                   timeout:(base::TimeDelta)timeout {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };

  return WaitUntilConditionOrTimeout(timeout, condition);
}

- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher {
  [self waitForUIElementToDisappearWithMatcher:matcher
                                       timeout:kWaitForUIElementTimeout];
}

- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher
                                       timeout:(base::TimeDelta)timeout {
  NSString* errorDescription = [NSString
      stringWithFormat:
          @"Failed waiting for element with matcher %@ to disappear", matcher];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_nil()
                                                             error:&error];
    return error == nil;
  };

  bool matched = WaitUntilConditionOrTimeout(timeout, condition);
  EG_TEST_HELPER_ASSERT_TRUE(matched, errorDescription);
}

- (NSString*)currentTabTitle {
  return [ChromeEarlGreyAppInterface currentTabTitle];
}

- (NSString*)nextTabTitle {
  return [ChromeEarlGreyAppInterface nextTabTitle];
}

- (NSString*)currentTabID {
  return [ChromeEarlGreyAppInterface currentTabID];
}

- (NSString*)nextTabID {
  return [ChromeEarlGreyAppInterface nextTabID];
}

- (void)waitForAndTapButton:(id<GREYMatcher>)button {
  NSString* errorDescription =
      [NSString stringWithFormat:@"Waiting to tap on button %@", button];
  // Perform a tap with a timeout. Occasionally EG doesn't sync up properly to
  // the animations of tab switcher, so it is necessary to poll here.
  // TODO(crbug.com/1050052): Fix the underlying issue in EarlGrey and remove
  // this workaround.
  GREYCondition* tapButton =
      [GREYCondition conditionWithName:errorDescription
                                 block:^BOOL {
                                   NSError* error;
                                   [[EarlGrey selectElementWithMatcher:button]
                                       performAction:grey_tap()
                                               error:&error];
                                   return error == nil;
                                 }];
  // Wait for the tap.
  BOOL hasClicked =
      [tapButton waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(hasClicked, errorDescription);
}

- (void)showTabSwitcher {
  [ChromeEarlGrey waitForAndTapButton:chrome_test_util::ShowTabsButton()];
}

#pragma mark - Cookie Utilities (EG2)

- (NSDictionary*)cookies {
  NSString* const kGetCookiesScript =
      @"document.cookie ? document.cookie.split(/;\\s*/) : [];";
  base::Value result = [self evaluateJavaScript:kGetCookiesScript];

  EG_TEST_HELPER_ASSERT_TRUE(result.is_list(),
                             @"The script response is not iterable.");

  NSMutableDictionary* cookies = [NSMutableDictionary dictionary];
  for (const auto& option : result.GetList()) {
    if (option.is_string()) {
      NSString* nameValuePair = base::SysUTF8ToNSString(option.GetString());
      NSMutableArray* cookieNameValue =
          [[nameValuePair componentsSeparatedByString:@"="] mutableCopy];
      // For cookies with multiple parameters it may be valid to have multiple
      // occurrences of the delimiter.
      EG_TEST_HELPER_ASSERT_TRUE((2 <= cookieNameValue.count),
                                 @"Cookie has invalid format.");
      NSString* cookieName = cookieNameValue[0];
      [cookieNameValue removeObjectAtIndex:0];

      NSString* cookieValue = [cookieNameValue componentsJoinedByString:@"="];
      cookies[cookieName] = cookieValue;
    }
  }

  return cookies;
}

#pragma mark - WebState Utilities (EG2)

- (void)tapWebStateElementWithID:(NSString*)elementID {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface tapWebStateElementWithID:elementID]);
}

- (void)tapWebStateElementInIFrameWithID:(const std::string&)elementID {
  NSString* NSElementID = base::SysUTF8ToNSString(elementID);
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      tapWebStateElementInIFrameWithID:NSElementID]);
}

- (void)waitForWebStateContainingElement:(ElementSelector*)selector {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForWebStateContainingElement:selector]);
}

- (void)waitForWebStateNotContainingElement:(ElementSelector*)selector {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForWebStateNotContainingElement:selector]);
}

- (void)waitForMainTabCount:(NSUInteger)count {
  __block NSUInteger actualCount = [ChromeEarlGreyAppInterface mainTabCount];
  NSString* conditionName = [NSString
      stringWithFormat:@"Waiting for main tab count to become %" PRIuNS, count];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* tabCountCheck = [GREYCondition
      conditionWithName:conditionName
                  block:^{
                    actualCount = [ChromeEarlGreyAppInterface mainTabCount];
                    return actualCount == count;
                  }];
  bool tabCountEqual =
      [tabCountCheck waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];

  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for main tab count to become %" PRIuNS
                        "; actual count: %" PRIuNS,
                       count, actualCount];
  EG_TEST_HELPER_ASSERT_TRUE(tabCountEqual, errorString);
}

- (void)waitForIncognitoTabCount:(NSUInteger)count {
  NSString* errorString = [NSString
      stringWithFormat:
          @"Failed waiting for incognito tab count to become %" PRIuNS, count];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* tabCountCheck = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return
                        [ChromeEarlGreyAppInterface incognitoTabCount] == count;
                  }];
  bool tabCountEqual =
      [tabCountCheck waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(tabCountEqual, errorString);
}

- (NSUInteger)indexOfActiveNormalTab {
  return [ChromeEarlGreyAppInterface indexOfActiveNormalTab];
}

- (void)waitForRestoreSessionToFinish {
  GREYCondition* finishedRestoreSession = [GREYCondition
      conditionWithName:kWaitForRestoreSessionToFinishError
                  block:^{
                    return !
                        [ChromeEarlGreyAppInterface isRestoreSessionInProgress];
                  }];
  bool restoreSessionCompleted = [finishedRestoreSession
      waitWithTimeout:kWaitForPageLoadTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(restoreSessionCompleted,
                             kWaitForRestoreSessionToFinishError);
}

- (void)submitWebStateFormWithID:(const std::string&)UTF8FormID {
  NSString* formID = base::SysUTF8ToNSString(UTF8FormID);
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface submitWebStateFormWithID:formID]);
}

- (void)waitForWebStateVisible {
  NSString* errorString =
      [NSString stringWithFormat:@"Failed waiting for web state to be visible"];
  GREYCondition* waitForWebState = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    NSError* error;
                    [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                                            WebViewMatcher()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  bool containsWebState =
      [waitForWebState waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(containsWebState, errorString);
}

- (void)waitForWebStateContainingText:(const std::string&)UTF8Text {
  [self waitForWebStateContainingText:UTF8Text timeout:kWaitForPageLoadTimeout];
}

- (void)waitForWebStateFrameContainingText:(const std::string&)UTF8Text {
  NSString* text = base::SysUTF8ToNSString(UTF8Text);
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForWebStateContainingTextInIFrame:text]);
}

- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                              timeout:(base::TimeDelta)timeout {
  NSString* text = base::SysUTF8ToNSString(UTF8Text);
  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for web state containing %@", text];

  GREYCondition* waitForText = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return
                        [ChromeEarlGreyAppInterface webStateContainsText:text];
                  }];
  bool containsText = [waitForText waitWithTimeout:timeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(containsText, errorString);
}

- (void)waitForWebStateNotContainingText:(const std::string&)UTF8Text {
  NSString* text = base::SysUTF8ToNSString(UTF8Text);
  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for web state not containing %@", text];

  GREYCondition* waitForText = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return !
                        [ChromeEarlGreyAppInterface webStateContainsText:text];
                  }];
  bool containsText =
      [waitForText waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(containsText, errorString);
}

- (void)waitForWebStateContainingBlockedImageElementWithID:
    (const std::string&)UTF8ImageID {
  NSString* imageID = base::SysUTF8ToNSString(UTF8ImageID);
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForWebStateContainingBlockedImage:imageID]);
}

- (void)waitForWebStateContainingLoadedImageElementWithID:
    (const std::string&)UTF8ImageID {
  NSString* imageID = base::SysUTF8ToNSString(UTF8ImageID);
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForWebStateContainingLoadedImage:imageID]);
}

- (void)waitForWebStateZoomScale:(CGFloat)scale {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForWebStateZoomScale:scale]);
}

- (GURL)webStateVisibleURL {
  return GURL(
      base::SysNSStringToUTF8([ChromeEarlGreyAppInterface webStateVisibleURL]));
}

- (GURL)webStateLastCommittedURL {
  return GURL(base::SysNSStringToUTF8(
      [ChromeEarlGreyAppInterface webStateLastCommittedURL]));
}

- (void)purgeCachedWebViewPages {
  [ChromeEarlGreyAppInterface purgeCachedWebViewPages];
  [self waitForRestoreSessionToFinish];
  [self waitForPageToFinishLoading];
}

- (void)triggerRestoreViaTabGridRemoveAllUndo {
  [ChromeEarlGrey showTabSwitcher];
  GREYWaitForAppToIdle(@"App failed to idle");
  [ChromeEarlGrey
      waitForAndTapButton:grey_allOf(chrome_test_util::TabGridEditButton(),
                                     grey_sufficientlyVisible(), nil)];
  [ChromeEarlGrey
      waitForAndTapButton:chrome_test_util::TabGridEditMenuCloseAllButton()];
  [ChromeEarlGrey
      waitForAndTapButton:chrome_test_util::TabGridUndoCloseAllButton()];
  [ChromeEarlGrey waitForAndTapButton:chrome_test_util::TabGridDoneButton()];
  [self waitForRestoreSessionToFinish];
  [self waitForPageToFinishLoading];
}

- (BOOL)webStateWebViewUsesContentInset {
  return [ChromeEarlGreyAppInterface webStateWebViewUsesContentInset];
}

- (CGSize)webStateWebViewSize {
  return [ChromeEarlGreyAppInterface webStateWebViewSize];
}

- (void)stopAllWebStatesLoading {
  [ChromeEarlGreyAppInterface stopAllWebStatesLoading];
  // Wait for any UI change.
  GREYWaitForAppToIdle(
      @"Failed to wait app to idle after stopping all WebStates");
}

- (void)clearAllWebStateBrowsingData:(AppLaunchConfiguration)config {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface clearAllWebStateBrowsingData]);

  // The app must be relaunched to rebuild internal //ios/web state after
  // clearing browsing data with `[ChromeEarlGreyAppInterface
  // clearAllWebStateBrowsingData]`.
  config.relaunch_policy = ForceRelaunchByKilling;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

#pragma mark - Sync Utilities (EG2)

- (void)clearSyncServerData {
  [ChromeEarlGreyAppInterface clearSyncServerData];
}

- (void)signInWithoutSyncWithIdentity:(FakeSystemIdentity*)identity {
  [ChromeEarlGreyAppInterface signInWithoutSyncWithIdentity:identity];
}

- (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender {
  [ChromeEarlGreyAppInterface
      addUserDemographicsToSyncServerWithBirthYear:rawBirthYear
                                            gender:gender];
}

- (void)clearAutofillProfileWithGUID:(const std::string&)UTF8GUID {
  NSString* GUID = base::SysUTF8ToNSString(UTF8GUID);
  [ChromeEarlGreyAppInterface clearAutofillProfileWithGUID:GUID];
}

- (void)addAutofillProfileToFakeSyncServerWithGUID:(const std::string&)UTF8GUID
                               autofillProfileName:
                                   (const std::string&)UTF8FullName {
  NSString* GUID = base::SysUTF8ToNSString(UTF8GUID);
  NSString* fullName = base::SysUTF8ToNSString(UTF8FullName);
  [ChromeEarlGreyAppInterface
      addAutofillProfileToFakeSyncServerWithGUID:GUID
                             autofillProfileName:fullName];
}

- (BOOL)isAutofillProfilePresentWithGUID:(const std::string&)UTF8GUID
                     autofillProfileName:(const std::string&)UTF8FullName {
  NSString* GUID = base::SysUTF8ToNSString(UTF8GUID);
  NSString* fullName = base::SysUTF8ToNSString(UTF8FullName);
  return [ChromeEarlGreyAppInterface isAutofillProfilePresentWithGUID:GUID
                                                  autofillProfileName:fullName];
}

- (void)setUpFakeSyncServer {
  [ChromeEarlGreyAppInterface setUpFakeSyncServer];
}

- (void)tearDownFakeSyncServer {
  [ChromeEarlGreyAppInterface tearDownFakeSyncServer];
}

- (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type {
  return [ChromeEarlGreyAppInterface numberOfSyncEntitiesWithType:type];
}

- (void)addFakeSyncServerBookmarkWithURL:(const GURL&)URL
                                   title:(const std::string&)UTF8Title {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  NSString* title = base::SysUTF8ToNSString(UTF8Title);
  [ChromeEarlGreyAppInterface addFakeSyncServerBookmarkWithURL:spec
                                                         title:title];
}

- (void)addFakeSyncServerDeviceInfo:(NSString*)deviceName
               lastUpdatedTimestamp:(base::Time)lastUpdatedTimestamp {
  [ChromeEarlGreyAppInterface addFakeSyncServerDeviceInfo:deviceName
                                     lastUpdatedTimestamp:lastUpdatedTimestamp];
}

- (void)addFakeSyncServerLegacyBookmarkWithURL:(const GURL&)URL
                                         title:(const std::string&)UTF8Title
                     originator_client_item_id:
                         (const std::string&)UTF8OriginatorClientItemId {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  NSString* title = base::SysUTF8ToNSString(UTF8Title);
  NSString* originator_client_item_id =
      base::SysUTF8ToNSString(UTF8OriginatorClientItemId);
  [ChromeEarlGreyAppInterface
      addFakeSyncServerLegacyBookmarkWithURL:spec
                                       title:title
                   originator_client_item_id:originator_client_item_id];
}

- (void)addFakeSyncServerTypedURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface addFakeSyncServerTypedURL:spec];
}

- (void)addFakeSyncServerHistoryVisit:(const GURL&)URL {
  [ChromeEarlGreyAppInterface
      addFakeSyncServerHistoryVisit:net::NSURLWithGURL(URL)];
}

- (void)addHistoryServiceTypedURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface addHistoryServiceTypedURL:spec];
}

- (void)deleteHistoryServiceTypedURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface deleteHistoryServiceTypedURL:spec];
}

- (void)waitForHistoryURL:(const GURL&)URL
            expectPresent:(BOOL)expectPresent
                  timeout:(base::TimeDelta)timeout {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  GREYCondition* waitForURL = [GREYCondition
      conditionWithName:kHistoryError
                  block:^{
                    return [ChromeEarlGreyAppInterface isURL:spec
                                             presentOnClient:expectPresent];
                  }];

  bool success = [waitForURL waitWithTimeout:timeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(success, kHistoryError);
}

- (void)waitForSyncInvalidationFields {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForSyncInvalidationFields]);
}

- (void)triggerSyncCycleForType:(syncer::ModelType)type {
  [ChromeEarlGreyAppInterface triggerSyncCycleForType:type];
}

- (void)deleteAutofillProfileFromFakeSyncServerWithGUID:
    (const std::string&)UTF8GUID {
  NSString* GUID = base::SysUTF8ToNSString(UTF8GUID);
  [ChromeEarlGreyAppInterface
      deleteAutofillProfileFromFakeSyncServerWithGUID:GUID];
}

- (void)waitForSyncEngineInitialized:(BOOL)isInitialized
                         syncTimeout:(base::TimeDelta)timeout {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForSyncEngineInitialized:isInitialized
                       syncTimeout:timeout]);
}

- (void)waitForSyncFeatureEnabled:(BOOL)isEnabled
                      syncTimeout:(base::TimeDelta)timeout {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForSyncFeatureEnabled:isEnabled
                    syncTimeout:timeout]);
}

- (void)waitForSyncTransportStateActiveWithTimeout:(base::TimeDelta)timeout {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForSyncTransportStateActiveWithTimeout:timeout]);
}

- (const std::string)syncCacheGUID {
  NSString* cacheGUID = [ChromeEarlGreyAppInterface syncCacheGUID];
  return base::SysNSStringToUTF8(cacheGUID);
}

- (void)verifySyncServerSessionURLs:(NSArray<NSString*>*)URLs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface verifySessionsOnSyncServerWithSpecs:URLs]);
}

- (void)waitForSyncServerEntitiesWithType:(syncer::ModelType)type
                                     name:(const std::string&)UTF8Name
                                    count:(size_t)count
                                  timeout:(base::TimeDelta)timeout {
  NSString* errorString = [NSString
      stringWithFormat:@"Expected %zu entities of the %d type.", count, type];
  NSString* name = base::SysUTF8ToNSString(UTF8Name);
  GREYCondition* verifyEntities = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    NSError* error = [ChromeEarlGreyAppInterface
                        verifyNumberOfSyncEntitiesWithType:type
                                                      name:name
                                                     count:count];
                    return !error;
                  }];

  bool success = [verifyEntities waitWithTimeout:timeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(success, errorString);
}

- (void)waitForSyncServerHistoryURLs:(NSArray<NSURL*>*)URLs
                             timeout:(base::TimeDelta)timeout {
  NSString* errorString =
      [NSString stringWithFormat:@"Expected %zu URLs.", [URLs count]];
  __block NSError* blockError = nil;
  GREYCondition* verifyURLs =
      [GREYCondition conditionWithName:errorString
                                 block:^{
                                   blockError = [ChromeEarlGreyAppInterface
                                       verifyHistoryOnSyncServerWithURLs:URLs];
                                   return !blockError;
                                 }];

  bool success = [verifyURLs waitWithTimeout:timeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_NO_ERROR(blockError);
  EG_TEST_HELPER_ASSERT_TRUE(success, errorString);
}

#pragma mark - Window utilities (EG2)

- (CGRect)screenPositionOfScreenWithNumber:(int)windowNumber {
  return [ChromeEarlGreyAppInterface
      screenPositionOfScreenWithNumber:windowNumber];
}

- (NSUInteger)windowCount [[nodiscard]] {
  return [ChromeEarlGreyAppInterface windowCount];
}

- (NSUInteger)foregroundWindowCount [[nodiscard]] {
  return [ChromeEarlGreyAppInterface foregroundWindowCount];
}

- (void)closeAllExtraWindows {
  if ([self windowCount] <= 1) {
    return;
  }
  [ChromeEarlGreyAppInterface closeAllExtraWindows];

  // Tab changes are initiated through `WebStateList`. Need to wait its
  // observers to complete UI changes at app. Wait until window count is
  // officially 1 in the app, otherwise we may start a new test while the
  // removed window is still partly registered.
  [self waitForForegroundWindowCount:1];
}

- (void)waitForForegroundWindowCount:(NSUInteger)count {
  __block NSUInteger actualCount =
      [ChromeEarlGreyAppInterface foregroundWindowCount];
  NSString* conditionName = [NSString
      stringWithFormat:@"Waiting for window count to become %" PRIuNS, count];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* browserCountCheck = [GREYCondition
      conditionWithName:conditionName
                  block:^{
                    actualCount =
                        [ChromeEarlGreyAppInterface foregroundWindowCount];
                    return actualCount == count;
                  }];
  bool browserCountEqual =
      [browserCountCheck waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];

  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for window count to become %" PRIuNS
                        "; actual count: %" PRIuNS,
                       count, actualCount];
  EG_TEST_HELPER_ASSERT_TRUE(browserCountEqual, errorString);
}

- (void)openNewWindow {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface openNewWindow]);
}

- (void)openNewTabInWindowWithNumber:(int)windowNumber {
  [ChromeEarlGreyAppInterface openNewTabInWindowWithNumber:windowNumber];
  [self waitForPageToFinishLoadingInWindowWithNumber:windowNumber];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeWindowWithNumber:(int)windowNumber {
  [ChromeEarlGreyAppInterface closeWindowWithNumber:windowNumber];
}

- (void)changeWindowWithNumber:(int)windowNumber
                   toNewNumber:(int)newWindowNumber {
  [ChromeEarlGreyAppInterface changeWindowWithNumber:windowNumber
                                         toNewNumber:newWindowNumber];
}

- (void)waitForPageToFinishLoadingInWindowWithNumber:(int)windowNumber {
  GREYCondition* finishedLoading = [GREYCondition
      conditionWithName:kWaitForPageToFinishLoadingError
                  block:^{
                    return ![ChromeEarlGreyAppInterface
                        isLoadingInWindowWithNumber:windowNumber];
                  }];

  BOOL pageLoaded =
      [finishedLoading waitWithTimeout:kWaitForPageLoadTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, kWaitForPageToFinishLoadingError);
}

- (void)loadURL:(const GURL&)URL
    inWindowWithNumber:(int)windowNumber
     waitForCompletion:(BOOL)wait {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface startLoadingURL:spec
                           inWindowWithNumber:windowNumber];
  if (wait) {
    [self waitForPageToFinishLoadingInWindowWithNumber:windowNumber];
    // Loading URL (especially the first time) can trigger alerts.
    [SystemAlertHandler handleSystemAlertIfVisible];
  }
}

- (void)loadURL:(const GURL&)URL inWindowWithNumber:(int)windowNumber {
  return [self loadURL:URL
      inWindowWithNumber:windowNumber
       waitForCompletion:YES];
}

- (BOOL)isLoadingInWindowWithNumber:(int)windowNumber {
  return [ChromeEarlGreyAppInterface isLoadingInWindowWithNumber:windowNumber];
}

- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                   inWindowWithNumber:(int)windowNumber {
  [self waitForWebStateContainingText:UTF8Text
                              timeout:kWaitForPageLoadTimeout
                   inWindowWithNumber:windowNumber];
}

- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                              timeout:(base::TimeDelta)timeout
                   inWindowWithNumber:(int)windowNumber {
  NSString* text = base::SysUTF8ToNSString(UTF8Text);
  NSString* errorString =
      [NSString stringWithFormat:@"Failed waiting for web state containing %@ "
                                 @"in window with number %d",
                                 text, windowNumber];

  GREYCondition* waitForText =
      [GREYCondition conditionWithName:errorString
                                 block:^{
                                   return [ChromeEarlGreyAppInterface
                                       webStateContainsText:text
                                         inWindowWithNumber:windowNumber];
                                 }];
  bool containsText = [waitForText waitWithTimeout:timeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(containsText, errorString);
}

- (void)waitForMainTabCount:(NSUInteger)count
         inWindowWithNumber:(int)windowNumber {
  __block NSUInteger actualCount =
      [ChromeEarlGreyAppInterface mainTabCountInWindowWithNumber:windowNumber];
  NSString* conditionName = [NSString
      stringWithFormat:@"Waiting for main tab count to become %" PRIuNS
                        " from %" PRIuNS " in window with number %d",
                       count, actualCount, windowNumber];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* tabCountCheck = [GREYCondition
      conditionWithName:conditionName
                  block:^{
                    actualCount = [ChromeEarlGreyAppInterface
                        mainTabCountInWindowWithNumber:windowNumber];
                    return actualCount == count;
                  }];
  bool tabCountEqual =
      [tabCountCheck waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];

  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for main tab count to become %" PRIuNS
                        " in window with number %d"
                        "; actual count: %" PRIuNS,
                       count, windowNumber, actualCount];
  EG_TEST_HELPER_ASSERT_TRUE(tabCountEqual, errorString);
}

- (void)waitForIncognitoTabCount:(NSUInteger)count
              inWindowWithNumber:(int)windowNumber {
  __block NSUInteger actualCount = [ChromeEarlGreyAppInterface
      incognitoTabCountInWindowWithNumber:windowNumber];
  NSString* conditionName =
      [NSString stringWithFormat:
                    @"Failed waiting for incognito tab count to become %" PRIuNS
                     " from %" PRIuNS " in window with number %d",
                    count, actualCount, windowNumber];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* tabCountCheck = [GREYCondition
      conditionWithName:conditionName
                  block:^{
                    actualCount = [ChromeEarlGreyAppInterface
                        incognitoTabCountInWindowWithNumber:windowNumber];
                    return actualCount == count;
                  }];
  bool tabCountEqual =
      [tabCountCheck waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()];

  NSString* errorString =
      [NSString stringWithFormat:
                    @"Failed waiting for incognito tab count to become %" PRIuNS
                     " in window with number %d"
                     "; actual count: %" PRIuNS,
                    count, windowNumber, actualCount];
  EG_TEST_HELPER_ASSERT_TRUE(tabCountEqual, errorString);
}

- (void)waitForJavaScriptCondition:(NSString*)javaScriptCondition {
  auto verifyBlock = ^BOOL {
    base::Value value = [ChromeEarlGrey evaluateJavaScript:javaScriptCondition];
    DCHECK(value.is_bool());
    return value.GetBool();
  };
  base::TimeDelta timeout = base::test::ios::kWaitForActionTimeout;
  NSString* conditionName = [NSString
      stringWithFormat:@"Wait for JS condition: %@", javaScriptCondition];
  GREYCondition* condition = [GREYCondition conditionWithName:conditionName
                                                        block:verifyBlock];

  NSString* errorString =
      [NSString stringWithFormat:@"Failed waiting for condition '%@'",
                                 javaScriptCondition];
  EG_TEST_HELPER_ASSERT_TRUE([condition waitWithTimeout:timeout.InSecondsF()],
                             errorString);
}

- (void)waitUntilReadyWindowWithNumber:(int)windowNumber {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::MatchInWindowWithNumber(
                          windowNumber, chrome_test_util::FakeOmnibox())];
}

#pragma mark - SignIn Utilities (EG2)

- (void)signOutAndClearIdentities {
  [ChromeEarlGreyAppInterface signOutAndClearIdentities];

  GREYCondition* allIdentitiesCleared = [GREYCondition
      conditionWithName:@"All Chrome identities were cleared"
                  block:^{
                    return ![ChromeEarlGreyAppInterface hasIdentities];
                  }];
  bool success =
      [allIdentitiesCleared waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(success,
                             @"Failed waiting for identities to be cleared");
}

#pragma mark - Bookmarks Utilities (EG2)

- (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase {
  [ChromeEarlGreyAppInterface addBookmarkWithSyncPassphrase:syncPassphrase];
}

- (void)waitForBookmarksToFinishLoading {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForBookmarksToFinishinLoading]);
}

- (void)clearBookmarks {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface clearBookmarks]);
}

- (base::Value)evaluateJavaScript:(NSString*)javaScript {
  JavaScriptExecutionResult* result =
      [ChromeEarlGreyAppInterface executeJavaScript:javaScript];
  EG_TEST_HELPER_ASSERT_TRUE(
      result.success, @"An error was produced during the script's execution");

  std::string jsonRepresentation = base::SysNSStringToUTF8(result.result);
  JSONStringValueDeserializer deserializer(jsonRepresentation);

  int errorCode;
  std::string errorMessage;
  auto jsonValue = deserializer.Deserialize(&errorCode, &errorMessage);
  NSString* message = [NSString
      stringWithFormat:@"JSON parsing failed: code=%d, message=%@", errorCode,
                       base::SysUTF8ToNSString(errorMessage)];
  EG_TEST_HELPER_ASSERT_TRUE(jsonValue, message);

  return jsonValue ? std::move(*jsonValue) : base::Value();
}

- (void)evaluateJavaScriptForSideEffect:(NSString*)javaScript {
  JavaScriptExecutionResult* result =
      [ChromeEarlGreyAppInterface executeJavaScript:javaScript];
  EG_TEST_HELPER_ASSERT_TRUE(
      result.success, @"An error was produced during the script's execution");
}

- (NSString*)mobileUserAgentString {
  return [ChromeEarlGreyAppInterface mobileUserAgentString];
}

#pragma mark - URL Utilities (EG2)

- (NSString*)displayTitleForURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  return [ChromeEarlGreyAppInterface displayTitleForURL:spec];
}

#pragma mark - Accessibility Utilities (EG2)

- (void)verifyAccessibilityForCurrentScreen {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface verifyAccessibilityForCurrentScreen]);
}

#pragma mark - Check features (EG2)

- (BOOL)isVariationEnabled:(int)variationID {
  return [ChromeEarlGreyAppInterface isVariationEnabled:variationID];
}

- (BOOL)isTriggerVariationEnabled:(int)variationID {
  return [ChromeEarlGreyAppInterface isTriggerVariationEnabled:variationID];
}

- (BOOL)isUKMEnabled {
  return [ChromeEarlGreyAppInterface isUKMEnabled];
}

- (BOOL)isSynthesizedRestoreSessionEnabled {
  return [ChromeEarlGreyAppInterface isSynthesizedRestoreSessionEnabled];
}

- (BOOL)isTestFeatureEnabled {
  return [ChromeEarlGreyAppInterface isTestFeatureEnabled];
}

- (BOOL)isDemographicMetricsReportingEnabled {
  return [ChromeEarlGreyAppInterface isDemographicMetricsReportingEnabled];
}

- (BOOL)isSyncHistoryDataTypeEnabled {
  return [ChromeEarlGreyAppInterface isSyncHistoryDataTypeEnabled];
}

- (BOOL)appHasLaunchSwitch:(const std::string&)launchSwitch {
  return [ChromeEarlGreyAppInterface
      appHasLaunchSwitch:base::SysUTF8ToNSString(launchSwitch)];
}

- (BOOL)isCustomWebKitLoadedIfRequested {
  return [ChromeEarlGreyAppInterface isCustomWebKitLoadedIfRequested];
}

- (BOOL)isLoadSimulatedRequestAPIEnabled {
  return [ChromeEarlGreyAppInterface isLoadSimulatedRequestAPIEnabled];
}

- (BOOL)isMobileModeByDefault {
  return [ChromeEarlGreyAppInterface isMobileModeByDefault];
}

- (BOOL)areMultipleWindowsSupported {
  return [ChromeEarlGreyAppInterface areMultipleWindowsSupported];
}

- (BOOL)isNewOverflowMenuEnabled {
  return [ChromeEarlGreyAppInterface isNewOverflowMenuEnabled];
}

// Returns whether the UseLensToSearchForImage feature is enabled;
- (BOOL)isUseLensToSearchForImageEnabled {
  return [ChromeEarlGreyAppInterface isUseLensToSearchForImageEnabled];
}

- (BOOL)isThumbstripEnabledForWindowWithNumber:(int)windowNumber {
  return [ChromeEarlGreyAppInterface
      isThumbstripEnabledForWindowWithNumber:windowNumber];
}

- (BOOL)isWebChannelsEnabled {
  return [ChromeEarlGreyAppInterface isWebChannelsEnabled];
}

- (BOOL)isUIButtonConfigurationEnabled {
  return [ChromeEarlGreyAppInterface isUIButtonConfigurationEnabled];
}

- (BOOL)isSortingTabsByRecency {
  return [ChromeEarlGreyAppInterface isSortingTabsByRecency];
}

#pragma mark - ContentSettings

- (ContentSetting)popupPrefValue {
  return [ChromeEarlGreyAppInterface popupPrefValue];
}

- (void)setPopupPrefValue:(ContentSetting)value {
  return [ChromeEarlGreyAppInterface setPopupPrefValue:value];
}

- (void)resetDesktopContentSetting {
  [ChromeEarlGreyAppInterface resetDesktopContentSetting];
}

#pragma mark - Keyboard utilities

- (NSInteger)registeredKeyCommandCount {
  return [ChromeEarlGreyAppInterface registeredKeyCommandCount];
}

- (void)simulatePhysicalKeyboardEvent:(NSString*)input
                                flags:(UIKeyModifierFlags)flags {
  [ChromeEarlGreyAppInterface simulatePhysicalKeyboardEvent:input flags:flags];
}

#pragma mark - Default Utilities (EG2)

- (void)setUserDefaultObject:(id)value forKey:(NSString*)defaultName {
  [ChromeEarlGreyAppInterface setUserDefaultObject:value forKey:defaultName];
}

- (void)removeUserDefaultObjectForKey:(NSString*)key {
  [ChromeEarlGreyAppInterface removeUserDefaultObjectForKey:key];
}

#pragma mark - Pref Utilities (EG2)

// Returns a base::Value representation of the requested pref.
- (std::unique_ptr<base::Value>)localStatePrefValue:
    (const std::string&)prefName {
  std::string jsonRepresentation =
      base::SysNSStringToUTF8([ChromeEarlGreyAppInterface
          localStatePrefValue:base::SysUTF8ToNSString(prefName)]);
  JSONStringValueDeserializer deserializer(jsonRepresentation);
  return deserializer.Deserialize(/*error_code=*/nullptr,
                                  /*error_message=*/nullptr);
}

- (bool)localStateBooleanPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self localStatePrefValue:prefName];
  BOOL success = value && value->is_bool();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected bool");
  return success ? value->GetBool() : false;
}

- (int)localStateIntegerPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self localStatePrefValue:prefName];
  BOOL success = value && value->is_int();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected int");
  return success ? value->GetInt() : 0;
}

- (std::string)localStateStringPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self localStatePrefValue:prefName];
  BOOL success = value && value->is_string();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected string");
  return success ? value->GetString() : "";
}

- (void)setIntegerValue:(int)value
      forLocalStatePref:(const std::string&)prefName {
  [ChromeEarlGreyAppInterface
        setIntegerValue:value
      forLocalStatePref:base::SysUTF8ToNSString(prefName)];
}

// Returns a base::Value representation of the requested pref.
- (std::unique_ptr<base::Value>)userPrefValue:(const std::string&)prefName {
  std::string jsonRepresentation =
      base::SysNSStringToUTF8([ChromeEarlGreyAppInterface
          userPrefValue:base::SysUTF8ToNSString(prefName)]);
  JSONStringValueDeserializer deserializer(jsonRepresentation);
  return deserializer.Deserialize(/*error_code=*/nullptr,
                                  /*error_message=*/nullptr);
}

- (bool)userBooleanPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self userPrefValue:prefName];
  BOOL success = value && value->is_bool();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected bool");
  return success ? value->GetBool() : false;
}

- (int)userIntegerPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self userPrefValue:prefName];
  BOOL success = value && value->is_int();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected int");
  return success ? value->GetInt() : 0;
}

- (std::string)userStringPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self userPrefValue:prefName];
  BOOL success = value && value->is_string();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected string");
  return success ? value->GetString() : "";
}

- (void)setBoolValue:(BOOL)value forUserPref:(const std::string&)UTF8PrefName {
  NSString* prefName = base::SysUTF8ToNSString(UTF8PrefName);
  return [ChromeEarlGreyAppInterface setBoolValue:value forUserPref:prefName];
}

- (void)setIntegerValue:(int)value
            forUserPref:(const std::string&)UTF8PrefName {
  NSString* prefName = base::SysUTF8ToNSString(UTF8PrefName);
  return [ChromeEarlGreyAppInterface setIntegerValue:value
                                         forUserPref:prefName];
}

- (void)resetBrowsingDataPrefs {
  return [ChromeEarlGreyAppInterface resetBrowsingDataPrefs];
}

- (void)resetDataForLocalStatePref:(const std::string&)prefName {
  return [ChromeEarlGreyAppInterface
      resetDataForLocalStatePref:base::SysUTF8ToNSString(prefName)];
}

#pragma mark - Pasteboard Utilities (EG2)

- (void)verifyStringCopied:(NSString*)text {
  ConditionBlock condition = ^{
    NSArray<NSString*>* pasteboardStrings =
        [ChromeEarlGreyAppInterface pasteboardStrings];
    for (NSString* paste in pasteboardStrings) {
      if ([paste containsString:text]) {
        return true;
      }
    }

    return false;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                                          condition),
             @"Waiting for '%@' to be copied to pasteboard.", text);
}

- (BOOL)pasteboardHasImages {
  return [ChromeEarlGreyAppInterface pasteboardHasImages];
}

- (GURL)pasteboardURL {
  NSString* absoluteString = [ChromeEarlGreyAppInterface pasteboardURLSpec];
  return absoluteString ? GURL(base::SysNSStringToUTF8(absoluteString))
                        : GURL::EmptyGURL();
}

- (void)clearPasteboard {
  [ChromeEarlGreyAppInterface clearPasteboard];
}

- (void)copyTextToPasteboard:(NSString*)text {
  [ChromeEarlGreyAppInterface copyTextToPasteboard:text];
}

#pragma mark - Context Menus Utilities (EG2)

- (void)verifyCopyLinkActionWithText:(NSString*)text {
  [ChromeEarlGreyAppInterface clearPasteboardURLs];
  [[EarlGrey selectElementWithMatcher:CopyLinkButton()]
      performAction:grey_tap()];
  [self verifyStringCopied:text];
}

- (void)verifyOpenInNewTabActionWithURL:(const std::string&)URL {
  // Check tab count prior to execution.
  NSUInteger oldRegularTabCount = [ChromeEarlGreyAppInterface mainTabCount];
  NSUInteger oldIncognitoTabCount =
      [ChromeEarlGreyAppInterface incognitoTabCount];

  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];

  [self waitForMainTabCount:oldRegularTabCount + 1];
  [self waitForIncognitoTabCount:oldIncognitoTabCount];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(URL)]
      assertWithMatcher:grey_notNil()];
}

- (void)verifyOpenInNewWindowActionWithContent:(const std::string&)content {
  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewWindowButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForWebStateContainingText:content inWindowWithNumber:1];
}

- (void)verifyOpenInIncognitoActionWithURL:(const std::string&)URL {
  // Check tab count prior to execution.
  NSUInteger oldRegularTabCount = [ChromeEarlGreyAppInterface mainTabCount];
  NSUInteger oldIncognitoTabCount =
      [ChromeEarlGreyAppInterface incognitoTabCount];

  [[EarlGrey selectElementWithMatcher:OpenLinkInIncognitoButton()]
      performAction:grey_tap()];

  [self waitForIncognitoTabCount:oldIncognitoTabCount + 1];
  [self waitForMainTabCount:oldRegularTabCount];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(URL)]
      assertWithMatcher:grey_notNil()];
}

- (void)verifyShareActionWithURL:(const GURL&)URL
                       pageTitle:(NSString*)pageTitle {
  [[EarlGrey selectElementWithMatcher:ShareButton()] performAction:grey_tap()];

  {
#if TARGET_IPHONE_SIMULATOR
    // The activity view share sheet blocks EarlGrey's synchronization on
    // the simulators. Ref:
    // github.com/google/EarlGrey/blob/master/docs/features.md#visibility-checks
    ScopedSynchronizationDisabler disabler;
#endif

    // On iOS 16, LPLinkView and LPTextView are marked isAccessible=N.
    ScopedMatchNonAccessibilityElements enabler;

    // Page title is added asynchronously, so wait for its appearance.
    NSString* hostString = base::SysUTF8ToNSString(URL.host());
    [self waitForMatcher:grey_allOf(ActivityViewHeader(hostString, pageTitle),
                                    grey_sufficientlyVisible(), nil)];
  }

  // Dismiss the Activity View by tapping outside its bounds.
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tap()];
}

#pragma mark - Unified consent utilities

- (void)setURLKeyedAnonymizedDataCollectionEnabled:(BOOL)enabled {
  return [ChromeEarlGreyAppInterface
      setURLKeyedAnonymizedDataCollectionEnabled:enabled];
}

#pragma mark - Watcher utilities

- (void)watchForButtonsWithLabels:(NSArray<NSString*>*)labels
                          timeout:(base::TimeDelta)timeout {
  [ChromeEarlGreyAppInterface watchForButtonsWithLabels:labels timeout:timeout];
}

- (BOOL)watcherDetectedButtonWithLabel:(NSString*)label {
  return [ChromeEarlGreyAppInterface watcherDetectedButtonWithLabel:label];
}

- (void)stopWatcher {
  [ChromeEarlGreyAppInterface stopWatcher];
}

@end
