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

// Possible shortcut item types, including invalid ones.
enum class ShortcutItemType {
  kInvalid,
  kNewSearch,
  kNewIncognitoSearch,
  kVoiceSearch,
  kQRScanner,
  kLens,
  kChangeWidgetToAppIcon,
};

// Maps shortcut item type string to shortcut items.
struct ShortcutItemMapping {
  NSString* shortcut_item_type_string;
  ShortcutItemType shortcut_item_type;
};

// Returns the shortcut item type of `shortcut_item`.
ShortcutItemType ShortcutItemTypeOf(UIApplicationShortcutItem* shortcut_item) {
  const ShortcutItemMapping kShortcutItemMap[] = {
      {kShortcutNewSearch, ShortcutItemType::kNewSearch},
      {kShortcutNewIncognitoSearch, ShortcutItemType::kNewIncognitoSearch},
      {kShortcutVoiceSearch, ShortcutItemType::kVoiceSearch},
      {kShortcutQRScanner, ShortcutItemType::kQRScanner},
      {kShortcutLensFromAppIconLongPress, ShortcutItemType::kLens},
      {kShortcutChangeWidgetToAppIcon,
       ShortcutItemType::kChangeWidgetToAppIcon},
  };

  NSString* shortcut_item_type = shortcut_item.type;
  for (const auto& item : kShortcutItemMap) {
    if ([shortcut_item_type isEqualToString:item.shortcut_item_type_string]) {
      return item.shortcut_item_type;
    }
  }

  return ShortcutItemType::kInvalid;
}

// Record metric for handle a shortcut of `shortcut_item_type`.
void RecordMetrics(ShortcutItemType shortcut_item_type,
                   NSString* shortcut_item_type_string) {
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::LONG_PRESS_ON_APP_ICON);
  switch (shortcut_item_type) {
    case ShortcutItemType::kNewSearch:
      base::RecordAction(
          base::UserMetricsAction("ApplicationShortcut.NewSearchPressed"));
      break;
    case ShortcutItemType::kNewIncognitoSearch:
      base::RecordAction(base::UserMetricsAction(
          "ApplicationShortcut.NewIncognitoSearchPressed"));
      break;
    case ShortcutItemType::kVoiceSearch:
      base::RecordAction(
          base::UserMetricsAction("ApplicationShortcut.VoiceSearchPressed"));
      break;
    case ShortcutItemType::kQRScanner:
      base::RecordAction(
          base::UserMetricsAction("ApplicationShortcut.ScanQRCodePressed"));
      break;
    case ShortcutItemType::kLens:
      base::RecordAction(base::UserMetricsAction(
          "ApplicationShortcut.LensPressedFromAppIconLongPress"));
      break;
    case ShortcutItemType::kChangeWidgetToAppIcon:
      // This intent is already handled by the OS, the default action for this
      // intent is to open the app, no additional handling is needed. Check
      // crbug.com/384806920 for additional info.
      break;
    case ShortcutItemType::kInvalid:
      // Use 32 as the maximum length of the reported value for this key (31
      // characters + '\0'). Expected values are UIApplicationShortcutItemType
      // entries in Info.plist.
      static crash_reporter::CrashKeyString<32> key("shortcut-item");
      crash_reporter::ScopedCrashKeyString crash_key(
          &key, base::SysNSStringToUTF8(shortcut_item_type_string));
      base::debug::DumpWithoutCrashing();
      break;
  }
}

// Returns whether the shortcut item is recognized.
bool IsValidShortcutItem(ShortcutItemType shortcut_item_type) {
  switch (shortcut_item_type) {
    case ShortcutItemType::kNewSearch:
    case ShortcutItemType::kNewIncognitoSearch:
    case ShortcutItemType::kVoiceSearch:
    case ShortcutItemType::kQRScanner:
    case ShortcutItemType::kLens:
      return true;
    case ShortcutItemType::kChangeWidgetToAppIcon:
    case ShortcutItemType::kInvalid:
      return false;
  }
}

}  // namespace

@implementation TaskRequestForShortcutItem {
  UIApplicationShortcutItem* _shortcutItem;
  ShortcutCompletionHandler _shortcutHandler;
  ShortcutItemType _shortcutItemType;
}

- (instancetype)initWithShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                          sceneState:(SceneState*)sceneState
                             handler:(ShortcutCompletionHandler)handler
                         isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithSceneState:sceneState isColdStart:isColdStart])) {
    _shortcutItem = shortcutItem;
    _shortcutHandler = handler;
    _shortcutItemType = ShortcutItemTypeOf(shortcutItem);
    RecordMetrics(_shortcutItemType, shortcutItem.type);
  }
  return self;
}

- (void)execute {
  // Don't handle the intent if it's not recognised.
  if (!IsValidShortcutItem(_shortcutItemType)) {
    if (_shortcutHandler) {
      _shortcutHandler(NO);
    }
    return;
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

  switch (_shortcutItemType) {
    case ShortcutItemType::kNewSearch:
      action = FOCUS_OMNIBOX;
      break;
    case ShortcutItemType::kNewIncognitoSearch:
      action = FOCUS_OMNIBOX;
      targetMode = ApplicationModeForTabOpening::INCOGNITO;
      break;
    case ShortcutItemType::kVoiceSearch:
      action = START_VOICE_SEARCH;
      break;
    case ShortcutItemType::kQRScanner:
      action = START_QR_CODE_SCANNER;
      break;
    case ShortcutItemType::kLens:
      action = START_LENS_FROM_APP_ICON_LONG_PRESS;
      url = GURL();
      break;
    case ShortcutItemType::kChangeWidgetToAppIcon:
    case ShortcutItemType::kInvalid:
      NOTREACHED();
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
  ProceduralBlock completion = ^{
    ProceduralBlock triggerBlock =
        [weakTabOpener completionBlockForTriggeringAction:action];
    if (triggerBlock) {
      triggerBlock();
    }
  };

  [tabOpener
      dismissModalsAndMaybeOpenSelectedTabInMode:targetMode
                               withUrlLoadParams:params
                                  dismissOmnibox:(action != FOCUS_OMNIBOX)
                                      completion:completion];

  if (isUnexpectedMode) {
    ShowToastWhenOpenInUnexpectedMode(sceneState, targetMode);
  }

  if (_shortcutHandler) {
    _shortcutHandler(YES);
  }
}

@end
