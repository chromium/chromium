// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#import "base/ios/ios_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/metrics/demographics/demographic_metrics_provider.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#import "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/browsing_data_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"
#import "ios/chrome/test/app/sync_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/window_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/hardware_keyboard_util.h"
#import "ios/testing/nserror_util.h"
#import "ios/testing/open_url_context.h"
#include "ios/testing/verify_custom_webkit.h"
#import "ios/web/common/features.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#import "ios/web/public/test/element_selector.h"
#include "ios/web/public/test/url_test_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#include "net/base/mac/url_conversions.h"
#import "services/metrics/public/cpp/ukm_recorder.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Returns a JSON-encoded string representing the given |pref|. If |pref| is
// nullptr, returns a string representing a base::Value of type NONE.
NSString* SerializedPref(const PrefService::Preference* pref) {
  base::Value none_value(base::Value::Type::NONE);

  const base::Value* value = pref ? pref->GetValue() : &none_value;
  DCHECK(value);

  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(*value);
  return base::SysUTF8ToNSString(serialized_value);
}
// Returns a JSON-encoded string representing the given |value|. If |value| is
// nullptr, returns a string representing a base::Value of type NONE.
NSString* SerializedValue(const base::Value* value) {
  base::Value none_value(base::Value::Type::NONE);
  const base::Value* result = value ? value : &none_value;
  DCHECK(result);

  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(*result);
  return base::SysUTF8ToNSString(serialized_value);
}
}

@implementation JavaScriptExecutionResult

- (instancetype)initWithResult:(NSString*)result
           successfulExecution:(BOOL)outcome {
  self = [super init];
  if (self) {
    _result = result;
    _success = outcome;
  }
  return self;
}
@end

@implementation ChromeEarlGreyAppInterface

+ (BOOL)isRTL {
  return UseRTLLayout();
}

+ (NSError*)clearBrowsingHistory {
  if (chrome_test_util::ClearBrowsingHistory()) {
    return nil;
  }

  return testing::NSErrorWithLocalizedDescription(
      @"Clearing browser history timed out");
}

+ (NSInteger)browsingHistoryEntryCountWithError:
    (NSError* __autoreleasing*)error {
  return chrome_test_util::GetBrowsingHistoryEntryCount(error);
}

+ (NSInteger)navigationBackListItemsCount {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();

  if (!webState)
    return -1;

  return webState->GetNavigationManager()->GetBackwardItems().size();
}

+ (NSError*)removeBrowsingCache {
  if (chrome_test_util::RemoveBrowsingCache()) {
    return nil;
  }

  return testing::NSErrorWithLocalizedDescription(
      @"Clearing browser cache for main tabs timed out");
}

+ (NSError*)clearAllWebStateBrowsingData {
  if (chrome_test_util::ClearAllWebStateBrowsingData()) {
    return nil;
  }

  return testing::NSErrorWithLocalizedDescription(
      @"Clearing web state browsing data for main tabs timed out");
}

+ (void)sceneOpenURL:(NSString*)spec {
  NSURL* url = [NSURL URLWithString:spec];
  TestOpenURLContext* context = [[TestOpenURLContext alloc] init];
  context.URL = url;

  UIApplication* application = UIApplication.sharedApplication;
  UIScene* scene = application.connectedScenes.anyObject;

  [scene.delegate scene:scene openURLContexts:[NSSet setWithObject:context]];
}

+ (void)startLoadingURL:(NSString*)spec {
  chrome_test_util::LoadUrl(GURL(base::SysNSStringToUTF8(spec)));
}

+ (BOOL)waitForWindowIDInjectionIfNeeded {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();

  if (webState->ContentIsHTML()) {
    return web::WaitUntilWindowIdInjected(webState);
  }

  return YES;
}

+ (bool)isLoading {
  return chrome_test_util::IsLoading();
}

+ (void)startReloading {
  WebNavigationBrowserAgent::FromBrowser(chrome_test_util::GetMainBrowser())
      ->Reload();
}

+ (NamedGuide*)guideWithName:(GuideName*)name view:(UIView*)view {
  return [NamedGuide guideWithName:name view:view];
}

+ (void)openURLFromExternalApp:(NSString*)URL {
  chrome_test_util::OpenChromeFromExternalApp(
      GURL(base::SysNSStringToUTF8(URL)));
}

+ (void)dismissSettings {
  [chrome_test_util::HandlerForActiveBrowser() closeSettingsUI];
}

#pragma mark - Tab Utilities (EG2)

+ (void)selectTabAtIndex:(NSUInteger)index {
  chrome_test_util::SelectTabAtIndexInCurrentMode(index);
}

+ (BOOL)isIncognitoMode {
  return chrome_test_util::IsIncognitoMode();
}

+ (void)closeTabAtIndex:(NSUInteger)index {
  chrome_test_util::CloseTabAtIndex(index);
}

+ (NSUInteger)mainTabCount {
  return chrome_test_util::GetMainTabCount();
}

+ (NSUInteger)incognitoTabCount {
  return chrome_test_util::GetIncognitoTabCount();
}

+ (NSUInteger)browserCount {
  return chrome_test_util::RegularBrowserCount();
}

+ (NSUInteger)evictedMainTabCount {
  return chrome_test_util::GetEvictedMainTabCount();
}

+ (void)evictOtherBrowserTabs {
  chrome_test_util::EvictOtherBrowserTabs();
}

+ (NSError*)simulateTabsBackgrounding {
  if (!chrome_test_util::SimulateTabsBackgrounding()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to simulate tab backgrounding.");
  }
  return nil;
}

+ (void)saveSessionImmediately {
  chrome_test_util::SaveSessionImmediately();
}

+ (NSError*)setCurrentTabsToBeColdStartTabs {
  if (!chrome_test_util::SetCurrentTabsToBeColdStartTabs()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to state tabs as cold start tabs");
  }
  return nil;
}

+ (NSError*)resetTabUsageRecorder {
  if (!chrome_test_util::ResetTabUsageRecorder()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to reset the TabUsageRecorder");
  }
  return nil;
}

+ (void)openNewTab {
  chrome_test_util::OpenNewTab();
}

+ (NSURL*)simulateExternalAppURLOpening {
  return chrome_test_util::SimulateExternalAppURLOpening();
}

+ (void)simulateExternalAppURLOpeningWithURL:(NSURL*)URL {
  chrome_test_util::SimulateExternalAppURLOpeningWithURL(URL);
}

+ (void)simulateAddAccountFromWeb {
  chrome_test_util::SimulateAddAccountFromWeb();
}

+ (void)closeCurrentTab {
  chrome_test_util::CloseCurrentTab();
}

+ (void)openNewIncognitoTab {
  chrome_test_util::OpenNewIncognitoTab();
}

+ (NSString*)currentTabTitle {
  return chrome_test_util::GetCurrentTabTitle();
}

+ (NSString*)nextTabTitle {
  return chrome_test_util::GetNextTabTitle();
}

+ (void)closeAllTabsInCurrentMode {
  chrome_test_util::CloseAllTabsInCurrentMode();
}

+ (NSError*)closeAllNormalTabs {
  bool success = chrome_test_util::CloseAllNormalTabs();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Could not close all normal tabs");
  }
  return nil;
}

+ (NSError*)closeAllIncognitoTabs {
  bool success = chrome_test_util::CloseAllIncognitoTabs();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Could not close all Incognito tabs");
  }
  return nil;
}

+ (void)closeAllTabs {
  chrome_test_util::CloseAllTabs();
}

+ (void)startGoingBack {
  WebNavigationBrowserAgent::FromBrowser(chrome_test_util::GetMainBrowser())
      ->GoBack();
}

+ (void)startGoingForward {
  WebNavigationBrowserAgent::FromBrowser(chrome_test_util::GetMainBrowser())
      ->GoForward();
}

+ (NSString*)currentTabID {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return web_state->GetStableIdentifier();
}

+ (NSString*)nextTabID {
  web::WebState* web_state = chrome_test_util::GetNextWebState();
  return web_state->GetStableIdentifier();
}

+ (NSUInteger)indexOfActiveNormalTab {
  return chrome_test_util::GetIndexOfActiveNormalTab();
}

#pragma mark - Window utilities (EG2)

+ (UIWindow*)windowWithNumber:(int)windowNumber {
  NSArray<SceneState*>* connectedScenes =
      chrome_test_util::GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  for (SceneState* state in connectedScenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      return state.window;
    }
  }
  return nil;
}

// Returns screen position of the given |windowNumber|
+ (CGRect)screenPositionOfScreenWithNumber:(int)windowNumber {
  NSArray<SceneState*>* connectedScenes =
      chrome_test_util::GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  for (SceneState* state in connectedScenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      return
          [state.window convertRect:state.window.frame
                  toCoordinateSpace:state.window.screen.fixedCoordinateSpace];
    }
  }
  return CGRectZero;
}

+ (NSUInteger)windowCount [[nodiscard]] {
  // If the scene API is in use, return the count of open sessions.
  return UIApplication.sharedApplication.openSessions.count;
}

+ (NSUInteger)foregroundWindowCount [[nodiscard]] {
  // If the scene API is in use, look at all the connected scenes and count
  // those in the foreground.
  NSUInteger count = 0;
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if (scene.activationState == UISceneActivationStateForegroundActive ||
        scene.activationState == UISceneActivationStateForegroundInactive) {
      count++;
    }
  }
  return count;
}

+ (NSError*)openNewWindow {
  if (!base::ios::IsMultipleScenesSupported()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Multiwindow not supported");
  }

  // Always disable default browser promo in new window, to avoid
  // messages to be closed too early.
  [self disableDefaultBrowserPromo];

  NSUserActivity* activity =
      [[NSUserActivity alloc] initWithActivityType:@"EG2NewWindow"];
  UISceneActivationRequestOptions* options =
      [[UISceneActivationRequestOptions alloc] init];
  [UIApplication.sharedApplication
      requestSceneSessionActivation:nil /* make a new scene */
                       userActivity:activity
                            options:options
                       errorHandler:nil];
  return nil;
}

+ (void)openNewTabInWindowWithNumber:(int)windowNumber {
  chrome_test_util::OpenNewTabInWindowWithNumber(windowNumber);
}

+ (void)changeWindowWithNumber:(int)windowNumber
                   toNewNumber:(int)newWindowNumber {
  NSArray<SceneState*>* connectedScenes =
      chrome_test_util::GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  NSString* newAccessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)newWindowNumber];
  for (SceneState* state in connectedScenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      state.window.accessibilityIdentifier = newAccessibilityIdentifier;
      break;
    }
  }
}

+ (void)closeWindowWithNumber:(int)windowNumber {
  NSArray<SceneState*>* connectedScenes =
      chrome_test_util::GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  for (SceneState* state in connectedScenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      UIWindowSceneDestructionRequestOptions* options =
          [[UIWindowSceneDestructionRequestOptions alloc] init];
      options.windowDismissalAnimation =
          UIWindowSceneDismissalAnimationStandard;
      [UIApplication.sharedApplication
          requestSceneSessionDestruction:state.scene.session
                                 options:options
                            errorHandler:nil];
    }
  }
}

+ (void)closeAllExtraWindows {
  if (!base::ios::IsMultipleScenesSupported())
    return;
  SceneState* foreground_scene_state =
      chrome_test_util::GetMainController().appState.foregroundActiveScene;
  // New windows get an accessibilityIdentifier equal to the number of windows
  // when they are created.
  // Renumber the remaining window to avoid conflicts with future windows.
  foreground_scene_state.window.accessibilityIdentifier = @"0";
  NSSet<UISceneSession*>* sessions =
      UIApplication.sharedApplication.openSessions;
  if (sessions.count <= 1)
    return;
  for (UISceneSession* session in sessions) {
    if (foreground_scene_state.scene == session.scene) {
      continue;
    }
    UIWindowSceneDestructionRequestOptions* options =
        [[UIWindowSceneDestructionRequestOptions alloc] init];
    options.windowDismissalAnimation = UIWindowSceneDismissalAnimationStandard;
    [UIApplication.sharedApplication requestSceneSessionDestruction:session
                                                            options:options
                                                       errorHandler:nil];
  }
}

+ (void)startLoadingURL:(NSString*)spec inWindowWithNumber:(int)windowNumber {
  chrome_test_util::LoadUrlInWindowWithNumber(
      GURL(base::SysNSStringToUTF8(spec)), windowNumber);
}

+ (BOOL)isLoadingInWindowWithNumber:(int)windowNumber {
  return chrome_test_util::IsLoadingInWindowWithNumber(windowNumber);
}

+ (BOOL)waitForWindowIDInjectionIfNeededInWindowWithNumber:(int)windowNumber {
  web::WebState* webState =
      chrome_test_util::GetCurrentWebStateForWindowWithNumber(windowNumber);

  if (webState->ContentIsHTML()) {
    return web::WaitUntilWindowIdInjected(webState);
  }

  return YES;
}

+ (BOOL)webStateContainsText:(NSString*)text
          inWindowWithNumber:(int)windowNumber {
  web::WebState* webState =
      chrome_test_util::GetCurrentWebStateForWindowWithNumber(windowNumber);
  return webState ? web::test::IsWebViewContainingText(
                        webState, base::SysNSStringToUTF8(text))
                  : NO;
}

+ (NSUInteger)mainTabCountInWindowWithNumber:(int)windowNumber {
  return chrome_test_util::GetMainTabCountForWindowWithNumber(windowNumber);
}

+ (NSUInteger)incognitoTabCountInWindowWithNumber:(int)windowNumber {
  return chrome_test_util::GetIncognitoTabCountForWindowWithNumber(
      windowNumber);
}

#pragma mark - WebState Utilities (EG2)

+ (NSError*)tapWebStateElementInIFrameWithID:(NSString*)elementID {
  bool success = web::test::TapWebViewElementWithIdInIframe(
      chrome_test_util::GetCurrentWebState(),
      base::SysNSStringToUTF8(elementID));
  if (!success) {
    return testing::NSErrorWithLocalizedDescription([NSString
        stringWithFormat:@"Failed to tap element with ID=%@", elementID]);
  }

  return nil;
}

+ (NSError*)tapWebStateElementWithID:(NSString*)elementID {
  NSError* error = nil;
  bool success = web::test::TapWebViewElementWithId(
      chrome_test_util::GetCurrentWebState(),
      base::SysNSStringToUTF8(elementID), &error);
  if (!success || error) {
    NSString* errorDescription =
        [NSString stringWithFormat:
                      @"Failed to tap web state element with ID=%@! Error: %@",
                      elementID, error];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

+ (NSError*)waitForWebStateContainingElement:(ElementSelector*)selector {
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return web::test::IsWebViewContainingElement(
        chrome_test_util::GetCurrentWebState(), selector);
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:@"Failed waiting for web state containing element %@",
                         selector.selectorDescription];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (NSError*)waitForWebStateNotContainingElement:(ElementSelector*)selector {
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return !web::test::IsWebViewContainingElement(
        chrome_test_util::GetCurrentWebState(), selector);
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:@"Failed waiting for web state without element %@",
                         selector.selectorDescription];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (NSError*)waitForWebStateContainingTextInIFrame:(NSString*)text {
  std::string stringText = base::SysNSStringToUTF8(text);
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return web::test::IsWebViewContainingTextInFrame(
        chrome_test_util::GetCurrentWebState(), stringText);
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:
            @"Failed waiting for web state's iframes containing text %@", text];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (NSError*)submitWebStateFormWithID:(NSString*)formID {
  bool success = web::test::SubmitWebViewFormWithId(
      chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(formID));

  if (!success) {
    NSString* errorString =
        [NSString stringWithFormat:@"Failed to submit form with ID=%@", formID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (BOOL)webStateContainsElement:(ElementSelector*)selector {
  return web::test::IsWebViewContainingElement(
      chrome_test_util::GetCurrentWebState(), selector);
}

+ (BOOL)webStateContainsText:(NSString*)text {
  return web::test::IsWebViewContainingText(
      chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(text));
}

+ (NSError*)waitForWebStateContainingLoadedImage:(NSString*)imageID {
  bool success = web::test::WaitForWebViewContainingImage(
      base::SysNSStringToUTF8(imageID), chrome_test_util::GetCurrentWebState(),
      web::test::IMAGE_STATE_LOADED);

  if (!success) {
    NSString* errorString = [NSString
        stringWithFormat:@"Failed waiting for web view loaded image %@",
                         imageID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (NSError*)waitForWebStateContainingBlockedImage:(NSString*)imageID {
  bool success = web::test::WaitForWebViewContainingImage(
      base::SysNSStringToUTF8(imageID), chrome_test_util::GetCurrentWebState(),
      web::test::IMAGE_STATE_BLOCKED);

  if (!success) {
    NSString* errorString = [NSString
        stringWithFormat:@"Failed waiting for web view blocked image %@",
                         imageID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (NSError*)waitForWebStateZoomScale:(CGFloat)scale {
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    web::WebState* web_state = chrome_test_util::GetCurrentWebState();
    if (!web_state) {
      return false;
    }

    CGFloat current_scale =
        [[web_state->GetWebViewProxy() scrollViewProxy] zoomScale];
    return (current_scale > (scale - 0.05)) && (current_scale < (scale + 0.05));
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:@"Failed waiting for web state zoom scale %f", scale];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (void)signOutAndClearIdentities {
  chrome_test_util::SignOutAndClearIdentities();
}

+ (BOOL)hasIdentities {
  return chrome_test_util::HasIdentities();
}

+ (NSString*)webStateVisibleURL {
  return base::SysUTF8ToNSString(
      chrome_test_util::GetCurrentWebState()->GetVisibleURL().spec());
}

+ (NSString*)webStateLastCommittedURL {
  return base::SysUTF8ToNSString(
      chrome_test_util::GetCurrentWebState()->GetLastCommittedURL().spec());
}

+ (NSError*)purgeCachedWebViewPages {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  web_state->SetWebUsageEnabled(false);
  if (!chrome_test_util::RemoveBrowsingCache()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to purge cached web view pages.");
  }

  // Attempt to deflake WebKit sometimes still holding on to the browser cache
  // with a larger hammer.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath library_dir = base::mac::GetUserLibraryPath();
  base::FilePath webkit_cache_dir = library_dir.Append("WebKit");
  DeletePathRecursively(webkit_cache_dir);

  web_state->SetWebUsageEnabled(true);
  web_state->GetNavigationManager()->LoadIfNecessary();

  return nil;
}

+ (BOOL)isRestoreSessionInProgress {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return web_state->GetNavigationManager()->IsRestoreSessionInProgress();
}

+ (BOOL)webStateWebViewUsesContentInset {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return web_state->GetWebViewProxy().shouldUseViewContentInset;
}

+ (CGSize)webStateWebViewSize {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return [web_state->GetWebViewProxy() bounds].size;
}

+ (void)stopAllWebStatesLoading {
  WebStateList* web_state_list =
      chrome_test_util::GetMainController()
          .interfaceProvider.currentInterface.browser->GetWebStateList();
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    if (web_state->IsLoading()) {
      web_state->Stop();
    }
  }
}

#pragma mark - Bookmarks Utilities (EG2)

+ (NSError*)waitForBookmarksToFinishinLoading {
  bool success = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return chrome_test_util::BookmarksLoaded();
  });
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Bookmark model did not load");
  }
  return nil;
}

+ (NSError*)clearBookmarks {
  bool success = chrome_test_util::ClearBookmarks();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Not all bookmarks were removed.");
  }
  return nil;
}

#pragma mark - URL Utilities (EG2)

+ (NSString*)displayTitleForURL:(NSString*)URL {
  return base::SysUTF16ToNSString(
      web::GetDisplayTitleForUrl(GURL(base::SysNSStringToUTF8(URL))));
}

#pragma mark - Sync Utilities (EG2)

+ (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type {
  return chrome_test_util::GetNumberOfSyncEntities(type);
}

+ (void)addFakeSyncServerBookmarkWithURL:(NSString*)URL title:(NSString*)title {
  chrome_test_util::AddBookmarkToFakeSyncServer(base::SysNSStringToUTF8(URL),
                                                base::SysNSStringToUTF8(title));
}

+ (void)addFakeSyncServerLegacyBookmarkWithURL:(NSString*)URL
                                         title:(NSString*)title
                     originator_client_item_id:
                         (NSString*)originator_client_item_id {
  chrome_test_util::AddLegacyBookmarkToFakeSyncServer(
      base::SysNSStringToUTF8(URL), base::SysNSStringToUTF8(title),
      base::SysNSStringToUTF8(originator_client_item_id));
}

+ (void)addFakeSyncServerTypedURL:(NSString*)URL {
  chrome_test_util::AddTypedURLToFakeSyncServer(base::SysNSStringToUTF8(URL));
}

+ (void)addFakeSyncServerDeviceInfo:(NSString*)deviceName
               lastUpdatedTimestamp:(base::Time)lastUpdatedTimestamp {
  chrome_test_util::AddDeviceInfoToFakeSyncServer(
      base::SysNSStringToUTF8(deviceName), lastUpdatedTimestamp);
}

+ (void)addHistoryServiceTypedURL:(NSString*)URL {
  chrome_test_util::AddTypedURLToClient(GURL(base::SysNSStringToUTF8(URL)));
}

+ (void)deleteHistoryServiceTypedURL:(NSString*)URL {
  chrome_test_util::DeleteTypedUrlFromClient(
      GURL(base::SysNSStringToUTF8(URL)));
}

+ (BOOL)isTypedURL:(NSString*)spec presentOnClient:(BOOL)expectPresent {
  NSError* error = nil;
  GURL URL(base::SysNSStringToUTF8(spec));
  BOOL success =
      chrome_test_util::IsTypedUrlPresentOnClient(URL, expectPresent, &error);
  return success && !error;
}

+ (void)triggerSyncCycleForType:(syncer::ModelType)type {
  chrome_test_util::TriggerSyncCycle(type);
}

+ (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender {
  chrome_test_util::AddUserDemographicsToSyncServer(rawBirthYear, gender);
}

+ (void)clearAutofillProfileWithGUID:(NSString*)GUID {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  chrome_test_util::ClearAutofillProfile(utfGUID);
}

+ (void)addAutofillProfileToFakeSyncServerWithGUID:(NSString*)GUID
                               autofillProfileName:(NSString*)fullName {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  std::string utfFullName = base::SysNSStringToUTF8(fullName);
  chrome_test_util::AddAutofillProfileToFakeSyncServer(utfGUID, utfFullName);
}

+ (BOOL)isAutofillProfilePresentWithGUID:(NSString*)GUID
                     autofillProfileName:(NSString*)fullName {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  std::string utfFullName = base::SysNSStringToUTF8(fullName);
  return chrome_test_util::IsAutofillProfilePresent(utfGUID, utfFullName);
}

+ (void)deleteAutofillProfileFromFakeSyncServerWithGUID:(NSString*)GUID {
  chrome_test_util::DeleteAutofillProfileFromFakeSyncServer(
      base::SysNSStringToUTF8(GUID));
}

+ (void)signInWithoutSyncWithIdentity:(FakeChromeIdentity*)identity {
  chrome_test_util::SignInWithoutSync(identity);
}

+ (void)clearSyncServerData {
  chrome_test_util::ClearSyncServerData();
}

+ (void)startSync {
  chrome_test_util::StartSync();
}

+ (void)stopSync {
  chrome_test_util::StopSync();
}

+ (NSError*)waitForSyncInitialized:(BOOL)isInitialized
                       syncTimeout:(NSTimeInterval)timeout {
  bool success = WaitUntilConditionOrTimeout(timeout, ^{
    return chrome_test_util::IsSyncInitialized() == isInitialized;
  });
  if (!success) {
    NSString* errorDescription =
        [NSString stringWithFormat:@"Sync must be initialized: %@",
                                   isInitialized ? @"YES" : @"NO"];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

+ (NSString*)syncCacheGUID {
  return base::SysUTF8ToNSString(chrome_test_util::GetSyncCacheGuid());
}

+ (NSError*)waitForSyncInvalidationFields {
  const bool success = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return chrome_test_util::VerifySyncInvalidationFieldsPopulated();
  });
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"The local DeviceInfo doesn't have invalidation fields");
  }
  return nil;
}

+ (BOOL)isFakeSyncServerSetUp {
  return chrome_test_util::IsFakeSyncServerSetUp();
}

+ (void)setUpFakeSyncServer {
  chrome_test_util::SetUpFakeSyncServer();
}

+ (void)tearDownFakeSyncServer {
  chrome_test_util::TearDownFakeSyncServer();
}

+ (NSError*)verifyNumberOfSyncEntitiesWithType:(NSUInteger)type
                                          name:(NSString*)name
                                         count:(NSUInteger)count {
  std::string UTF8Name = base::SysNSStringToUTF8(name);
  NSError* __autoreleasing tempError = nil;
  bool success = chrome_test_util::VerifyNumberOfSyncEntitiesWithName(
      (syncer::ModelType)type, UTF8Name, count, &tempError);
  NSError* error = tempError;

  if (!success and !error) {
    NSString* errorString =
        [NSString stringWithFormat:@"Expected %zu entities of the %d type.",
                                   count, (syncer::ModelType)type];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return error;
}

+ (NSError*)verifySessionsOnSyncServerWithSpecs:(NSArray<NSString*>*)specs {
  std::multiset<std::string> multisetSpecs;
  for (NSString* spec in specs) {
    multisetSpecs.insert(base::SysNSStringToUTF8(spec));
  }

  NSError* __autoreleasing tempError = nil;
  bool success =
      chrome_test_util::VerifySessionsOnSyncServer(multisetSpecs, &tempError);
  NSError* error = tempError;
  if (!success && !error) {
    error = testing::NSErrorWithLocalizedDescription(
        @"Error occurred during verification sessions.");
  }
  return error;
}

+ (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase {
  chrome_test_util::AddBookmarkWithSyncPassphrase(
      base::SysNSStringToUTF8(syncPassphrase));
}

#pragma mark - JavaScript Utilities (EG2)

+ (id)executeJavaScript:(NSString*)javaScript error:(NSError**)outError {
  __block bool handlerCalled = false;
  __block id blockResult = nil;
  __block NSError* blockError = nil;
  [chrome_test_util::GetCurrentWebState()->GetJSInjectionReceiver()
      executeJavaScript:javaScript
      completionHandler:^(id result, NSError* error) {
        handlerCalled = true;
        blockResult = [result copy];
        blockError = [error copy];
      }];

  bool completed = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return handlerCalled;
  });

  if (completed) {
    NSError* __autoreleasing autoreleasedError = blockError;
    *outError = autoreleasedError;
  } else {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Did not complete execution of JavaScript: %@",
                         javaScript];
    NSError* __autoreleasing autoreleasedError =
        testing::NSErrorWithLocalizedDescription(errorDescription);
    *outError = autoreleasedError;
  }
  return blockResult;
}

+ (JavaScriptExecutionResult*)executeJavaScript:(NSString*)javaScript {
  __block bool handlerCalled = false;
  __block NSString* blockResult = nil;
  __block bool blockError = false;

  web::WebFrame* web_frame =
      web::GetMainFrame(chrome_test_util::GetCurrentWebState());

  if (web_frame) {
    std::u16string script = base::SysNSStringToUTF16(javaScript);
    web_frame->ExecuteJavaScript(
        script, base::BindOnce(^(const base::Value* value, bool error) {
          handlerCalled = true;
          blockError = error;
          blockResult = SerializedValue(value);
        }));

    bool completed = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return handlerCalled;
    });

    BOOL success = completed && !blockError;

    JavaScriptExecutionResult* result =
        [[JavaScriptExecutionResult alloc] initWithResult:blockResult
                                      successfulExecution:success];
    return result;
  }

  NSError* error = nil;
  id output = [self executeJavaScript:javaScript error:&error];
  std::unique_ptr<base::Value> value = web::ValueResultFromWKResult(output);

  NSString* callbackResult = SerializedValue(value.get());
  BOOL success = error ? false : true;

  JavaScriptExecutionResult* result =
      [[JavaScriptExecutionResult alloc] initWithResult:callbackResult
                                    successfulExecution:success];
  return result;
}

+ (NSString*)mobileUserAgentString {
  return base::SysUTF8ToNSString(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));
}

#pragma mark - Accessibility Utilities (EG2)

+ (NSError*)verifyAccessibilityForCurrentScreen {
  NSError* error = nil;
  bool success = chrome_test_util::VerifyAccessibilityForCurrentScreen(error);
  if (!success || error) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Accessibility checks failed! Error: %@", error];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

#pragma mark - Check features (EG2)

+ (BOOL)isBlockNewTabPagePendingLoadEnabled {
  return base::FeatureList::IsEnabled(kBlockNewTabPagePendingLoad);
}

+ (BOOL)isVariationEnabled:(int)variationID {
  variations::VariationsIdsProvider* provider =
      variations::VariationsIdsProvider::GetInstance();
  std::vector<variations::VariationID> ids = provider->GetVariationsVector(
      {variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
       variations::GOOGLE_WEB_PROPERTIES_FIRST_PARTY});
  return std::find(ids.begin(), ids.end(), variationID) != ids.end();
}

+ (BOOL)isTriggerVariationEnabled:(int)variationID {
  variations::VariationsIdsProvider* provider =
      variations::VariationsIdsProvider::GetInstance();
  std::vector<variations::VariationID> ids = provider->GetVariationsVector(
      {variations::GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
       variations::GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY});
  return std::find(ids.begin(), ids.end(), variationID) != ids.end();
}

+ (BOOL)isUKMEnabled {
  return base::FeatureList::IsEnabled(ukm::kUkmFeature);
}

+ (BOOL)isSynthesizedRestoreSessionEnabled {
  return base::FeatureList::IsEnabled(
      web::features::kSynthesizedRestoreSession);
}

+ (BOOL)isTestFeatureEnabled {
  return base::FeatureList::IsEnabled(kTestFeature);
}

+ (BOOL)isDemographicMetricsReportingEnabled {
  return base::FeatureList::IsEnabled(
      metrics::DemographicMetricsProvider::kDemographicMetricsReporting);
}

+ (BOOL)appHasLaunchSwitch:(NSString*)launchSwitch {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      base::SysNSStringToUTF8(launchSwitch));
}

+ (BOOL)isCustomWebKitLoadedIfRequested {
  return IsCustomWebKitLoadedIfRequested();
}

+ (BOOL)isLoadSimulatedRequestAPIEnabled {
  return web::features::IsLoadSimulatedRequestAPIEnabled();
}

+ (BOOL)isMobileModeByDefault {
  web::UserAgentType webClientUserAgent =
      web::GetWebClient()->GetDefaultUserAgent(
          chrome_test_util::GetCurrentWebState(), GURL());

  return webClientUserAgent == web::UserAgentType::MOBILE;
}

+ (BOOL)areMultipleWindowsSupported {
  return base::ios::IsMultipleScenesSupported();
}

+ (BOOL)isNewOverflowMenuEnabled {
  return IsNewOverflowMenuEnabled();
}

+ (BOOL)isNewOmniboxPopupEnabled {
  return base::FeatureList::IsEnabled(kIOSOmniboxUpdatedPopupUI);
}

+ (BOOL)isExperimentalOmniboxEnabled {
  return base::FeatureList::IsEnabled(kIOSNewOmniboxImplementation);
}

+ (BOOL)isUseLensToSearchForImageEnabled {
  return base::FeatureList::IsEnabled(kUseLensToSearchForImage) &&
         ios::provider::IsLensSupported();
}

+ (BOOL)isThumbstripEnabledForWindowWithNumber:(int)windowNumber {
  return ShowThumbStripInTraitCollection(
      [self windowWithNumber:windowNumber].traitCollection);
}

+ (BOOL)isWebChannelsEnabled {
  return base::FeatureList::IsEnabled(kEnableWebChannels);
}

#pragma mark - ContentSettings

+ (ContentSetting)popupPrefValue {
  return ios::HostContentSettingsMapFactory::GetForBrowserState(
             chrome_test_util::GetOriginalBrowserState())
      ->GetDefaultContentSetting(ContentSettingsType::POPUPS, NULL);
}

+ (void)setPopupPrefValue:(ContentSetting)value {
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS, value);
}

+ (void)resetDesktopContentSetting {
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetDefaultContentSetting(ContentSettingsType::REQUEST_DESKTOP_SITE,
                                 CONTENT_SETTING_BLOCK);
}

#pragma mark - Pref Utilities (EG2)

+ (NSString*)localStatePrefValue:(NSString*)prefName {
  std::string path = base::SysNSStringToUTF8(prefName);
  const PrefService::Preference* pref =
      GetApplicationContext()->GetLocalState()->FindPreference(path);
  return SerializedPref(pref);
}

+ (void)setIntegerValue:(int)value forLocalStatePref:(NSString*)prefName {
  std::string path = base::SysNSStringToUTF8(prefName);
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  prefService->SetInteger(path, value);
}

+ (NSString*)userPrefValue:(NSString*)prefName {
  std::string path = base::SysNSStringToUTF8(prefName);
  const PrefService::Preference* pref =
      chrome_test_util::GetOriginalBrowserState()->GetPrefs()->FindPreference(
          path);
  return SerializedPref(pref);
}

+ (void)setBoolValue:(BOOL)value forUserPref:(NSString*)prefName {
  chrome_test_util::SetBooleanUserPref(
      chrome_test_util::GetOriginalBrowserState(),
      base::SysNSStringToUTF8(prefName).c_str(), value);
}

+ (void)setIntegerValue:(int)value forUserPref:(NSString*)prefName {
  chrome_test_util::SetIntegerUserPref(
      chrome_test_util::GetOriginalBrowserState(),
      base::SysNSStringToUTF8(prefName).c_str(), value);
}

+ (void)resetBrowsingDataPrefs {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->ClearPref(browsing_data::prefs::kDeleteBrowsingHistory);
  prefs->ClearPref(browsing_data::prefs::kDeleteCookies);
  prefs->ClearPref(browsing_data::prefs::kDeleteCache);
  prefs->ClearPref(browsing_data::prefs::kDeletePasswords);
  prefs->ClearPref(browsing_data::prefs::kDeleteFormData);
}

+ (void)resetDataForLocalStatePref:(NSString*)prefName {
  std::string path = base::SysNSStringToUTF8(prefName);
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  prefService->ClearPref(path);
}

#pragma mark - Unified Consent utilities

+ (void)setURLKeyedAnonymizedDataCollectionEnabled:(BOOL)enabled {
  UnifiedConsentServiceFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetUrlKeyedAnonymizedDataCollectionEnabled(enabled);
}

#pragma mark - Keyboard Command Utilities

+ (NSInteger)registeredKeyCommandCount {
  UIViewController* mainViewController =
      chrome_test_util::GetMainController()
          .interfaceProvider.mainInterface.viewController;
  return mainViewController.keyCommands.count;
}

+ (void)simulatePhysicalKeyboardEvent:(NSString*)input
                                flags:(UIKeyModifierFlags)flags {
  chrome_test_util::SimulatePhysicalKeyboardEvent(flags, input);
}

#pragma mark - Pasteboard utilities

+ (void)clearPasteboardURLs {
  [[UIPasteboard generalPasteboard] setURLs:nil];
}

+ (NSArray<NSString*>*)pasteboardStrings {
  return [UIPasteboard generalPasteboard].strings;
}

+ (NSString*)pasteboardURLSpec {
  return [UIPasteboard generalPasteboard].URL.absoluteString;
}

#pragma mark - Watcher utilities

// Delay between two watch cycles.
const NSTimeInterval kWatcherCycleDelay = 0.2;

// Set of buttons being watched for.
NSMutableSet* watchingButtons;
// Set of buttons that were actually watched.
NSMutableSet* watchedButtons;

// Current watch number, to allow terminating older scheduled runs.
int watchRunNumber = 0;

+ (void)watchForButtonsWithLabels:(NSArray<NSString*>*)labels
                          timeout:(NSTimeInterval)timeout {
  watchRunNumber++;
  watchedButtons = [NSMutableSet set];
  watchingButtons = [NSMutableSet set];
  for (NSString* label in labels) {
    [watchingButtons addObject:label];
  }
  [self scheduleNextWatchForButtonsWithTimeout:timeout
                                     runNumber:watchRunNumber];
}

+ (BOOL)watcherDetectedButtonWithLabel:(NSString*)label {
  return [watchedButtons containsObject:label];
}

+ (void)stopWatcher {
  [watchingButtons removeAllObjects];
  [watchedButtons removeAllObjects];
}

// Schedule the next watch cycles from a background thread, that will dispatch
// the actual check, async, on the main thread, since UI objects can only
// be accessed from there. Scheduling directly on the main
// thread would let EG to try to drain the main thread queue without
// success.
+ (void)scheduleNextWatchForButtonsWithTimeout:(NSTimeInterval)timeout
                                     runNumber:(int)runNumber {
  dispatch_queue_t background_queue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    (int64_t)(kWatcherCycleDelay * NSEC_PER_SEC)),
      background_queue, ^{
        dispatch_async(dispatch_get_main_queue(), ^{
          if (!watchingButtons.count || runNumber != watchRunNumber)
            return;

          [self findButtonsWithLabelsInViews:[UIApplication sharedApplication]
                                                 .windows];

          if (watchingButtons.count && timeout > 0.0) {
            [self scheduleNextWatchForButtonsWithTimeout:timeout -
                                                         kWatcherCycleDelay
                                               runNumber:runNumber];
          } else {
            [watchingButtons removeAllObjects];
          }
        });
      });
}

// Looks for a button (based on traits) with the given |label|,
// recursively in the given |views|.
+ (void)findButtonsWithLabelsInViews:(NSArray<UIView*>*)views {
  if (!watchingButtons.count)
    return;

  for (UIView* view in views) {
    [self buttonsWithLabelsMatchView:view];
    [self findButtonsWithLabelsInViews:view.subviews];
  }
}

// Checks if the given |view| is a button (based on traits) with the
// given accessibility label.
+ (void)buttonsWithLabelsMatchView:(UIView*)view {
  if (![view respondsToSelector:@selector(accessibilityLabel)])
    return;
  if (([view accessibilityTraits] & UIAccessibilityTraitButton) == 0)
    return;
  if ([watchingButtons containsObject:view.accessibilityLabel]) {
    [watchedButtons addObject:view.accessibilityLabel];
    [watchingButtons removeObject:view.accessibilityLabel];
  }
}

#pragma mark - Default Browser Promo Utilities

+ (void)clearDefaultBrowserPromoData {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSArray<NSString*>* keys = @[
    @"lastTimeUserInteractedWithFullscreenPromo",
    @"userHasInteractedWithFullscreenPromo",
    @"userHasInteractedWithTailoredFullscreenPromo",
    @"userInteractedWithNonModalPromoCount",
    @"remindMeLaterPromoActionInteraction",
  ];
  for (NSString* key in keys) {
    [defaults removeObjectForKey:key];
  }
}

+ (void)copyURLToPasteBoard {
  UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
  pasteboard.URL = [NSURL URLWithString:@"chrome://version"];
}

+ (void)disableDefaultBrowserPromo {
  chrome_test_util::GetMainController().appState.shouldShowDefaultBrowserPromo =
      NO;
  LogUserInteractionWithFullscreenPromo();
}
#pragma mark - Url Param Classification utilities

+ (void)setUrlParamClassifications:(NSString*)contents {
  std::string file_contents = base::SysNSStringToUTF8(contents);
  url_param_filter::ClassificationsLoader::GetInstance()->ReadClassifications(
      file_contents);
}

+ (void)resetUrlParamClassifications {
  url_param_filter::ClassificationsLoader::GetInstance()
      ->ResetListsForTesting();
}

@end
