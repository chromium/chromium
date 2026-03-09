// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_shortcut_item.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/app/unexpected_mode_toast_util.h"
#import "ios/chrome/browser/intents/model/intents_constants.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"

namespace {
// Validates whether the shortcut item is recognized, and record
// user action metrics.
bool ValidateShortcutItemAndRecordMetrics(NSString* shortcut_item) {
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::LONG_PRESS_ON_APP_ICON);
  if ([shortcut_item isEqualToString:kShortcutNewSearch]) {
    base::RecordAction(
        base::UserMetricsAction("ApplicationShortcut.NewSearchPressed"));
    return true;
  } else if ([shortcut_item isEqualToString:kShortcutNewIncognitoSearch]) {
    base::RecordAction(base::UserMetricsAction(
        "ApplicationShortcut.NewIncognitoSearchPressed"));
    return true;
  } else if ([shortcut_item isEqualToString:kShortcutVoiceSearch]) {
    base::RecordAction(
        base::UserMetricsAction("ApplicationShortcut.VoiceSearchPressed"));
    return true;
  } else if ([shortcut_item isEqualToString:kShortcutQRScanner]) {
    base::RecordAction(
        base::UserMetricsAction("ApplicationShortcut.ScanQRCodePressed"));
    return true;
  } else if ([shortcut_item
                 isEqualToString:kShortcutLensFromAppIconLongPress]) {
    base::RecordAction(base::UserMetricsAction(
        "ApplicationShortcut.LensPressedFromAppIconLongPress"));
    return true;
  } else if ([shortcut_item isEqualToString:kShortcutChangeWidgetToAppIcon]) {
    // This intent is already handled by the OS, the default action for this
    // intent is to open the app, no additional handling is needed. Check
    // crbug.com/384806920 for additional info.
    return false;
  }

  // Use 32 as the maximum length of the reported value for this key (31
  // characters + '\0'). Expected values are UIApplicationShortcutItemType
  // entries in Info.plist.
  static crash_reporter::CrashKeyString<32> key("shortcut-item");
  crash_reporter::ScopedCrashKeyString crash_key(
      &key, base::SysNSStringToUTF8(shortcut_item));
  base::debug::DumpWithoutCrashing();
  return false;
}
}  // namespace

@implementation TaskRequestForShortcutItem {
  UIApplicationShortcutItem* _shortcutItem;
  ShortcutCompletionHandler _shortcutHandler;
  // Shortcut item should not be handled if the type is not recognised.
  BOOL _isValidShortcutItem;
}

- (instancetype)initWithShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                          sceneState:(SceneState*)sceneState
                             handler:(ShortcutCompletionHandler)handler
                         isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithSceneState:sceneState isColdStart:isColdStart])) {
    _shortcutItem = shortcutItem;
    _shortcutHandler = handler;
    _isValidShortcutItem =
        ValidateShortcutItemAndRecordMetrics(shortcutItem.type);
  }
  return self;
}

- (void)execute {
  // Don't handle the intent if it's not recognised.
  if (!_isValidShortcutItem) {
    if (_shortcutHandler) {
      _shortcutHandler(NO);
    }
  }

  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  PrefService* prefService = browser->GetProfile()->GetPrefs();

  ApplicationModeForTabOpening targetMode =
      ApplicationModeForTabOpening::NORMAL;
  TabOpeningPostOpeningAction action = NO_ACTION;
  GURL url = GURL(kChromeUINewTabURL);

  if ([_shortcutItem.type isEqualToString:kShortcutNewSearch]) {
    action = FOCUS_OMNIBOX;
  } else if ([_shortcutItem.type isEqualToString:kShortcutNewIncognitoSearch]) {
    action = FOCUS_OMNIBOX;
    targetMode = ApplicationModeForTabOpening::INCOGNITO;
  } else if ([_shortcutItem.type isEqualToString:kShortcutVoiceSearch]) {
    action = START_VOICE_SEARCH;
  } else if ([_shortcutItem.type isEqualToString:kShortcutQRScanner]) {
    action = START_QR_CODE_SCANNER;
  } else if ([_shortcutItem.type
                 isEqualToString:kShortcutLensFromAppIconLongPress]) {
    action = START_LENS_FROM_APP_ICON_LONG_PRESS;
    url = GURL();
  }

  BOOL isUnexpectedMode = NO;
  if (targetMode == ApplicationModeForTabOpening::INCOGNITO &&
      IsIncognitoModeDisabled(prefService)) {
    isUnexpectedMode = YES;
  } else if (targetMode == ApplicationModeForTabOpening::NORMAL &&
             IsIncognitoModeForced(prefService)) {
    isUnexpectedMode = YES;
  }

  if (IsIncognitoModeForced(prefService)) {
    targetMode = ApplicationModeForTabOpening::INCOGNITO;
  } else if (IsIncognitoModeDisabled(prefService)) {
    targetMode = ApplicationModeForTabOpening::NORMAL;
  }

  UrlLoadParams params = UrlLoadParams::InNewTab(url);
  params.from_external = YES;

  id<TabOpening> tabOpener = sceneState.controller;
  __weak id<TabOpening> weakTabOpener = tabOpener;
  ProceduralBlock completion =
      [weakTabOpener completionBlockForTriggeringAction:action];

  [tabOpener
      dismissModalsAndMaybeOpenSelectedTabInMode:targetMode
                               withUrlLoadParams:params
                                  dismissOmnibox:(action != FOCUS_OMNIBOX)
                                      completion:completion];

  if (isUnexpectedMode) {
    ShowToastWhenOpenInUnexpectedMode(sceneState, targetMode);
  }

  if (_shortcutHandler) {
    _shortcutHandler(_isValidShortcutItem);
  }
}

@end
