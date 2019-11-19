// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/chrome_test_util.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/metrics_mediator_testing.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/metrics/previous_session_info_private.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/view_controller_swapping.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/test/fakes/test_web_state_observer.h"
#import "ios/web/public/test/native_controller_test_util.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Methods to access private members for testing.
@interface BreakpadController (Testing)
- (BOOL)isEnabled;
- (BOOL)isUploadingEnabled;
- (dispatch_queue_t)queue;
@end
@implementation BreakpadController (Testing)
- (BOOL)isEnabled {
  return started_;
}
- (BOOL)isUploadingEnabled {
  return enableUploads_;
}
- (dispatch_queue_t)queue {
  return queue_;
}
@end

namespace {
// Returns the original ChromeBrowserState if |incognito| is false. If
// |ingonito| is true, returns an off-the-record ChromeBrowserState.
ios::ChromeBrowserState* GetBrowserState(bool incognito) {
  std::vector<ios::ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  DCHECK(!browser_states.empty());

  ios::ChromeBrowserState* browser_state = browser_states.front();
  DCHECK(!browser_state->IsOffTheRecord());

  return incognito ? browser_state->GetOffTheRecordChromeBrowserState()
                   : browser_state;
}

}  // namespace

namespace chrome_test_util {

MainController* GetMainController() {
  return [MainApplicationDelegate sharedMainController];
}

DeviceSharingManager* GetDeviceSharingManager() {
  return [GetMainController() deviceSharingManager];
}

ios::ChromeBrowserState* GetOriginalBrowserState() {
  return GetBrowserState(false);
}

ios::ChromeBrowserState* GetCurrentIncognitoBrowserState() {
  return GetBrowserState(true);
}

id<BrowserCommands> BrowserCommandDispatcherForMainBVC() {
  BrowserViewController* mainBVC =
      GetMainController().interfaceProvider.mainInterface.bvc;
  return mainBVC.dispatcher;
}

UIViewController* GetActiveViewController() {
  UIWindow* main_window = [[UIApplication sharedApplication] keyWindow];
  DCHECK([main_window isKindOfClass:[ChromeOverlayWindow class]]);
  UIViewController* main_view_controller = main_window.rootViewController;
  if ([main_view_controller
          conformsToProtocol:@protocol(ViewControllerSwapping)]) {
    // This is either the stack_view or the iPad tab_switcher, in which case it
    // is best to call |-activeViewController|.
    return [static_cast<id<ViewControllerSwapping>>(main_view_controller)
        activeViewController];
  }

  // The active view controller is either the TabGridViewController or its
  // presented BVC. The BVC is itself contained inside of a
  // BVCContainerViewController.
  UIViewController* active_view_controller =
      main_view_controller.presentedViewController
          ? main_view_controller.presentedViewController
          : main_view_controller;
  if ([active_view_controller
          isKindOfClass:[BVCContainerViewController class]]) {
    active_view_controller =
        base::mac::ObjCCastStrict<BVCContainerViewController>(
            active_view_controller)
            .currentBVC;
  }
  return active_view_controller;
}

id<ApplicationCommands, BrowserCommands>
DispatcherForActiveBrowserViewController() {
  UIViewController* vc = GetActiveViewController();
  BrowserViewController* bvc = base::mac::ObjCCast<BrowserViewController>(vc);
  return bvc.dispatcher;
}

void RemoveAllInfoBars() {
  web::WebState* webState = GetCurrentWebState();
  if (webState) {
    infobars::InfoBarManager* info_bar_manager =
        InfoBarManagerImpl::FromWebState(webState);
    if (info_bar_manager) {
      info_bar_manager->RemoveAllInfoBars(false /* animate */);
    }
  }
}

void ClearPresentedState() {
  [GetMainController() dismissModalDialogsWithCompletion:nil
                                          dismissOmnibox:YES];
}

void SetBooleanLocalStatePref(const char* pref_name, bool value) {
  DCHECK(GetApplicationContext());
  DCHECK(GetApplicationContext()->GetLocalState());
  BooleanPrefMember pref;
  pref.Init(pref_name, GetApplicationContext()->GetLocalState());
  pref.SetValue(value);
}

void SetBooleanUserPref(ios::ChromeBrowserState* browser_state,
                        const char* pref_name,
                        bool value) {
  DCHECK(browser_state);
  DCHECK(browser_state->GetPrefs());
  BooleanPrefMember pref;
  pref.Init(pref_name, browser_state->GetPrefs());
  pref.SetValue(value);
}

void SetWWANStateTo(bool value) {
  MainController* mainController = chrome_test_util::GetMainController();
  net::NetworkChangeNotifier::ConnectionType connectionType =
      value ? net::NetworkChangeNotifier::CONNECTION_4G
            : net::NetworkChangeNotifier::CONNECTION_WIFI;
  [mainController.metricsMediator connectionTypeChanged:connectionType];
}

void SetFirstLaunchStateTo(bool value) {
  [[PreviousSessionInfo sharedInstance] setIsFirstSessionAfterUpgrade:value];
}

bool IsMetricsRecordingEnabled() {
  DCHECK(GetApplicationContext());
  DCHECK(GetApplicationContext()->GetMetricsService());
  return GetApplicationContext()->GetMetricsService()->recording_active();
}

bool IsMetricsReportingEnabled() {
  DCHECK(GetApplicationContext());
  DCHECK(GetApplicationContext()->GetMetricsService());
  return GetApplicationContext()->GetMetricsService()->reporting_active();
}

bool IsBreakpadEnabled() {
  return [[BreakpadController sharedInstance] isEnabled];
}

bool IsBreakpadReportingEnabled() {
  return [[BreakpadController sharedInstance] isUploadingEnabled];
}

bool IsFirstLaunchAfterUpgrade() {
  return [chrome_test_util::GetMainController() isFirstLaunchAfterUpgrade];
}

void WaitForBreakpadQueue() {
  dispatch_queue_t queue = [[BreakpadController sharedInstance] queue];
  dispatch_barrier_sync(queue, ^{
                        });
}

void OpenChromeFromExternalApp(const GURL& url) {
  [[[UIApplication sharedApplication] delegate]
      applicationWillResignActive:[UIApplication sharedApplication]];
  [GetMainController() setStartupParametersWithURL:url];

  [[[UIApplication sharedApplication] delegate]
      applicationDidBecomeActive:[UIApplication sharedApplication]];
}

bool PurgeCachedWebViewPages() {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  const GURL last_committed_url = web_state->GetLastCommittedURL();

  web_state->SetWebUsageEnabled(false);
  web_state->SetWebUsageEnabled(true);

  auto observer = std::make_unique<web::TestWebStateObserver>(web_state);
  web::TestWebStateObserver* observer_ptr = observer.get();

  web_state->GetNavigationManager()->LoadIfNecessary();

  // The navigation triggered by LoadIfNecessary() may only start loading in the
  // next run loop, if it is for a web URL. The most reliable way to detect that
  // this navigation has finished is via the WebStateObserver.
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        return observer_ptr->did_finish_navigation_info() &&
               observer_ptr->did_finish_navigation_info()->context &&
               observer_ptr->did_finish_navigation_info()
                       ->context->GetWebState()
                       ->GetVisibleURL() == last_committed_url;
      });
}

}  // namespace chrome_test_util
