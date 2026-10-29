// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_url_context.h"

#import <optional>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/app/task_request_for_standard_url_context.h"
#import "ios/chrome/app/task_request_for_widget_url_context.h"
#import "ios/chrome/app/task_request_for_xcallback_url_context.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/app_group/widget_constants.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Returns the launch source for first run metrics.
first_run::ExternalLaunch GetLaunchSource(MobileSessionCallerApp caller_app,
                                          NSURL* url) {
  if (caller_app != CALLER_APP_APPLE_MOBILESAFARI) {
    return first_run::LAUNCH_BY_OTHERS;
  }

  NSString* query = url.query;
  if (![query length]) {
    return first_run::LAUNCH_BY_MOBILESAFARI;
  }

  // Look for `safarisab` (Smart App Banner key) anywhere in the query string.
  NSRange found = [query rangeOfString:@"safarisab"];
  if (found.location == NSNotFound) {
    return first_run::LAUNCH_BY_MOBILESAFARI;
  }

  if (found.location + found.length < [query length]) {
    unichar char_after =
        [query characterAtIndex:(found.location + found.length)];
    if (char_after != '&' && char_after != '=') {
      return first_run::LAUNCH_BY_MOBILESAFARI;
    }
  }
  if (found.location > 0) {
    unichar char_before = [query characterAtIndex:(found.location - 1)];
    if (char_before != '&') {
      return first_run::LAUNCH_BY_MOBILESAFARI;
    }
  }
  return first_run::LAUNCH_BY_SMARTAPPBANNER;
}

// Returns whether the URL specifies opening default browser settings.
bool IsShowDefaultBrowserSettings(NSURL* url) {
  NSURLComponents* components = [NSURLComponents componentsWithURL:url
                                           resolvingAgainstBaseURL:NO];
  NSString* postOpeningActionParameter =
      base::SysUTF8ToNSString(kPostOpeningActionParameter);
  NSString* defaultBrowserSettingsAction =
      base::SysUTF8ToNSString(kPostOpeningActionDefaultBrowserSettings);
  for (NSURLQueryItem* item in components.queryItems) {
    if ([item.name isEqualToString:postOpeningActionParameter] &&
        [item.value isEqualToString:defaultBrowserSettingsAction]) {
      return true;
    }
  }
  return false;
}

// Triggers `post_opening_action` on `tab_opener`, if any.
void TriggerPostOpeningAction(id<TabOpening> tab_opener,
                              TabOpeningPostOpeningAction post_opening_action) {
  if (ProceduralBlock trigger_block =
          [tab_opener completionBlockForTriggeringAction:post_opening_action]) {
    trigger_block();
  }
}

}  // namespace

@interface TaskRequestForURLContext ()

// Lazily computed caller application for `_URLContext`. Must not be accessed
// before `ChromeMain` has initialized URL scheme registries.
@property(nonatomic, assign, readonly) MobileSessionCallerApp callerApp;

@end

@implementation TaskRequestForURLContext {
  std::optional<MobileSessionCallerApp> _callerApp;
}

+ (instancetype)taskRequestWithURLContext:(UIOpenURLContext*)URLContext
                               sceneState:(SceneState*)sceneState
                              isColdStart:(BOOL)isColdStart {
  NSURL* url = URLContext.URL;
  if ([url.scheme isEqualToString:kWidgetKitSchemeChrome]) {
    return
        [[TaskRequestForWidgetURLContext alloc] initWithURLContext:URLContext
                                                        sceneState:sceneState
                                                       isColdStart:isColdStart];
  }
  if ([url.host isEqualToString:@"x-callback-url"]) {
    return [[TaskRequestForXCallbackURLContext alloc]
        initWithURLContext:URLContext
                sceneState:sceneState
               isColdStart:isColdStart];
  }
  return
      [[TaskRequestForStandardURLContext alloc] initWithURLContext:URLContext
                                                        sceneState:sceneState
                                                       isColdStart:isColdStart];
}

- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithSceneState:sceneState isColdStart:isColdStart])) {
    _URLContext = URLContext;
    [self extractGaiaID];
  }
  return self;
}

- (MobileSessionCallerApp)callerApp {
  if (!_callerApp.has_value()) {
    _callerApp = GetCallerApp(_URLContext.options.sourceApplication,
                              /*secure_source_app_id=*/nil,
                              net::GURLWithNSURL(_URLContext.URL));
  }
  return _callerApp.value();
}

- (void)recordMetrics {
  MobileSessionCallerApp callerApp = self.callerApp;
  base::UmaHistogramEnumeration("Startup.MobileSessionStartFromApps", callerApp,
                                MOBILE_SESSION_CALLER_APP_COUNT);

  if (IsShowDefaultBrowserSettings(_URLContext.URL)) {
    base::UmaHistogramEnumeration("Startup.ShowDefaultPromoFromApps", callerApp,
                                  MOBILE_SESSION_CALLER_APP_COUNT);
  }

  if (self.sceneState.profileState.appState.startupInformation.isFirstRun) {
    base::UmaHistogramEnumeration("FirstRun.LaunchSource",
                                  GetLaunchSource(callerApp, _URLContext.URL),
                                  first_run::LAUNCH_SIZE);
  }
}

- (void)handleCommandWithSceneState:(SceneState*)sceneState {
  NOTREACHED()
      << "-handleCommandWithSceneState: must be implemented by sub-classes";
}

- (ApplicationModeForTabOpening)
    targetModeForSceneState:(SceneState*)sceneState
                defaultMode:(ApplicationModeForTabOpening)defaultMode {
  PrefService* prefService = sceneState.profileState.profile->GetPrefs();
  if (IsIncognitoModeDisabled(prefService)) {
    return ApplicationModeForTabOpening::NORMAL;
  }
  if (IsIncognitoModeForced(prefService)) {
    return ApplicationModeForTabOpening::INCOGNITO;
  }
  if (self.callerApp == CALLER_APP_GOOGLE_CHROME &&
      defaultMode != ApplicationModeForTabOpening::INCOGNITO) {
    return ApplicationModeForTabOpening::CURRENT;
  }
  return defaultMode;
}

- (void)openTabWithSceneState:(SceneState*)sceneState
                  externalURL:(const GURL&)externalURL
                   virtualURL:(const GURL&)virtualURL
                   targetMode:(ApplicationModeForTabOpening)targetMode
            postOpeningAction:(TabOpeningPostOpeningAction)postOpeningAction
        postOpeningCompletion:(ProceduralBlock)postOpeningCompletion
             fromWidgetOrSiri:(BOOL)fromWidgetOrSiri {
  CHECK(!postOpeningAction || !postOpeningCompletion)
      << "Can't provide both postOpeningAction and postOpeningCompletion";

  ApplicationModeForTabOpening resolvedMode =
      [self targetModeForSceneState:sceneState defaultMode:targetMode];

  UrlLoadParams urlLoadParams =
      UrlLoadParams::InNewTab(externalURL, virtualURL);
  urlLoadParams.from_widget_or_siri = fromWidgetOrSiri;

  const BOOL dismissOmnibox =
      postOpeningAction != TabOpeningPostOpeningAction::FOCUS_OMNIBOX;

  id<TabOpening> tabOpener = sceneState.controller;
  ProceduralBlock completion = postOpeningCompletion;
  if (!completion) {
    __weak id<TabOpening> weakTabOpener = tabOpener;
    completion = base::CallbackToBlock(base::BindOnce(
        &TriggerPostOpeningAction, weakTabOpener, postOpeningAction));
  }
  CHECK(completion);

  [tabOpener dismissModalsAndMaybeOpenSelectedTabInMode:resolvedMode
                                      withUrlLoadParams:urlLoadParams
                                         dismissOmnibox:dismissOmnibox
                                             completion:completion];
}

- (NSURL*)URL {
  return _URLContext.URL;
}

- (void)execute {
  SceneState* sceneState = self.sceneState;
  CHECK(sceneState);

  [self recordMetrics];

  if (!self.isColdStart) {
    NSSet* URLContextSet = [NSSet setWithObject:_URLContext];
    // If the SystemIdentityManager handles the URL context, return early to
    // avoid opening the URL twice.
    if (GetApplicationContext()
            ->GetSystemIdentityManager()
            ->HandleSessionOpenURLContexts(sceneState.scene, URLContextSet)) {
      return;
    }
  }
  [self handleCommandWithSceneState:sceneState];
}

#pragma mark - Private

- (void)extractGaiaID {
  NSURL* URL = _URLContext.URL;

  // Only widgets and share extension support profile/account switching when
  // handling an intent.
  bool isWidget = [URL.scheme isEqualToString:@"chromewidgetkit"];
  bool isShareExtension = app_group::IsShareExtensionCommandURL(URL);

  if (!isWidget && !isShareExtension) {
    return;
  }

  // TODO(crbug.com/493826640): Investigate possible ways to check the gaia_id
  // when the task is executed (to be able to use GURL).
  NSURLComponents* URLComponents = [NSURLComponents componentsWithURL:URL
                                              resolvingAgainstBaseURL:NO];
  NSArray<NSURLQueryItem*>* queryItems = URLComponents.queryItems;
  NSString* gaiaIDKey =
      base::SysUTF8ToNSString(app_group::kGaiaIDQueryItemName);
  for (NSURLQueryItem* item in queryItems) {
    if ([item.name isEqualToString:gaiaIDKey]) {
      self.gaiaID = item.value;
      break;
    }
  }
}

@end
