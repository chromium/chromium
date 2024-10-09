// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/chrome_test_util.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_service.h"
#import "components/prefs/pref_member.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/previous_session_info/previous_session_info_private.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/metrics_mediator_testing.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller_testing.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/country_code_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state_observer.h"
#import "net/base/apple/url_conversions.h"

// A subclass to pass instances of UIOpenURLContext to scene delegate during
// testing. UIOpenURLContext has no init available, so this can only be
// allocated. It uses obscuring properties for URL and options.
// TODO(crbug.com/40711105) Explore improving this which can become brittle.
@interface FakeUIOpenURLContext : UIOpenURLContext
@property(nonatomic, copy) NSURL* URL;
@property(nonatomic, strong) UISceneOpenURLOptions* options;
@end

@implementation FakeUIOpenURLContext
@synthesize URL = _URL;
@synthesize options = _options;
@end

namespace {

// Returns the original ProfileIOS if `incognito` is false. If
// `incognito` is true, returns an off-the-record ProfileIOS.
ProfileIOS* GetProfile(bool incognito) {
  const std::vector<ProfileIOS*> loaded_profiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();
  DCHECK(!loaded_profiles.empty());

  ProfileIOS* profile = loaded_profiles.front();
  DCHECK(!profile->IsOffTheRecord());

  return incognito ? profile->GetOffTheRecordProfile() : profile;
}

}  // namespace

namespace chrome_test_util {

MainController* GetMainController() {
  return MainApplicationDelegate.sharedMainController;
}

SceneState* GetForegroundActiveScene() {
  return MainApplicationDelegate.sharedAppState.foregroundActiveScene;
}

SceneController* GetForegroundActiveSceneController() {
  return MainApplicationDelegate.sharedAppState.foregroundActiveScene
      .controller;
}

NSUInteger RegularBrowserCount() {
  return static_cast<NSUInteger>(
      BrowserListFactory::GetForProfile(GetOriginalProfile())
          ->BrowsersOfType(BrowserList::BrowserType::kRegularAndInactive)
          .size());
}

ProfileIOS* GetOriginalProfile() {
  return GetProfile(false);
}

ProfileIOS* GetCurrentIncognitoProfile() {
  return GetProfile(true);
}

Browser* GetMainBrowser() {
  return GetForegroundActiveScene()
      .browserProviderInterface.mainBrowserProvider.browser;
}

Browser* GetCurrentBrowser() {
  return GetForegroundActiveScene()
      .browserProviderInterface.currentBrowserProvider.browser;
}

UIViewController* GetActiveViewController() {
  UIWindow* main_window = GetAnyKeyWindow();
  DCHECK([main_window isKindOfClass:[ChromeOverlayWindow class]]);
  UIViewController* main_view_controller = main_window.rootViewController;

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
        base::apple::ObjCCastStrict<BVCContainerViewController>(
            active_view_controller)
            .currentBVC;
  }
  return active_view_controller;
}

id<ApplicationCommands,
   BrowserCommands,
   BrowserCoordinatorCommands,
   CountryCodePickerCommands,
   UnitConversionCommands,
   DriveFilePickerCommands>
HandlerForActiveBrowser() {
  return static_cast<id<ApplicationCommands, BrowserCommands,
                        BrowserCoordinatorCommands, UnitConversionCommands,
                        CountryCodePickerCommands, DriveFilePickerCommands>>(
      GetMainBrowser()->GetCommandDispatcher());
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

void ClearPresentedState(ProceduralBlock completion) {
  [GetForegroundActiveSceneController()
      dismissModalDialogsWithCompletion:completion
                         dismissOmnibox:YES];
}

void PresentSignInAccountsViewControllerIfNecessary() {
  [GetForegroundActiveSceneController()
      presentSignInAccountsViewControllerIfNecessary];
}

void SetBooleanLocalStatePref(const char* pref_name, bool value) {
  DCHECK(GetApplicationContext());
  DCHECK(GetApplicationContext()->GetLocalState());
  BooleanPrefMember pref;
  pref.Init(pref_name, GetApplicationContext()->GetLocalState());
  pref.SetValue(value);
}

void SetBooleanUserPref(ProfileIOS* profile,
                        const char* pref_name,
                        bool value) {
  DCHECK(profile);
  DCHECK(profile->GetPrefs());
  BooleanPrefMember pref;
  pref.Init(pref_name, profile->GetPrefs());
  pref.SetValue(value);
}

void SetIntegerUserPref(ProfileIOS* profile, const char* pref_name, int value) {
  DCHECK(profile);
  DCHECK(profile->GetPrefs());
  IntegerPrefMember pref;
  pref.Init(pref_name, profile->GetPrefs());
  pref.SetValue(value);
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

bool IsCrashpadEnabled() {
  return crash_reporter::IsCrashpadRunning();
}

bool IsCrashpadReportingEnabled() {
  return crash_helper::common::UserEnabledUploading();
}

void OpenChromeFromExternalApp(const GURL& url) {
  UIScene* scene =
      [[UIApplication sharedApplication].connectedScenes anyObject];
  [scene.delegate sceneWillResignActive:scene];

  // FakeUIOpenURLContext cannot be instanciated, but it is just needed
  // for carrying the properties over to the scene delegate.
  FakeUIOpenURLContext* context = [FakeUIOpenURLContext alloc];
  context.URL = net::NSURLWithGURL(url);

  NSSet<UIOpenURLContext*>* URLContexts =
      [[NSSet alloc] initWithArray:@[ context ]];

  [scene.delegate scene:scene openURLContexts:URLContexts];
  [scene.delegate sceneDidBecomeActive:scene];
}

bool PurgeCachedWebViewPages() {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  const GURL last_committed_url = web_state->GetLastCommittedURL();

  web_state->SetWebUsageEnabled(false);
  web_state->SetWebUsageEnabled(true);

  auto observer = std::make_unique<web::FakeWebStateObserver>(web_state);
  web::FakeWebStateObserver* observer_ptr = observer.get();

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
