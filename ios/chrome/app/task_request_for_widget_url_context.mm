// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_widget_url_context.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/common/app_group/widget_constants.h"

namespace {

// Histogram helper to log the UMA IOS.WidgetKit.Action histogram.
void LogWidgetKitAction(WidgetKitExtensionAction action) {
  base::UmaHistogramEnumeration(kWidgetKitActionHistogram, action);
}

struct WidgetActionMapping {
  NSString* host;
  NSString* path;  // `nil` matches any path for `host`.
  WidgetKitExtensionAction action;
};

// Looks up the WidgetKitExtensionAction for a given widget URL.
std::optional<WidgetKitExtensionAction> ActionForWidgetURL(NSURL* url) {
  NSString* host = url.host;
  NSString* path = url.path;

  const WidgetActionMapping kWidgetActionMappings[] = {
      {kWidgetKitHostSearchWidget, nil,
       WidgetKitExtensionAction::ACTION_SEARCH_WIDGET_SEARCH},
      {kWidgetKitHostQuickActionsWidget, kWidgetKitActionSearch,
       WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_SEARCH},
      {kWidgetKitHostQuickActionsWidget, kWidgetKitActionIncognito,
       WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_INCOGNITO},
      {kWidgetKitHostQuickActionsWidget, kWidgetKitActionVoiceSearch,
       WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_VOICE_SEARCH},
      {kWidgetKitHostQuickActionsWidget, kWidgetKitActionQRReader,
       WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_QR_READER},
      {kWidgetKitHostQuickActionsWidget, kWidgetKitActionLens,
       WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_LENS},
      {kWidgetKitHostLockscreenLauncherWidget, kWidgetKitActionSearch,
       WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_SEARCH},
      {kWidgetKitHostLockscreenLauncherWidget, kWidgetKitActionIncognito,
       WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_INCOGNITO},
      {kWidgetKitHostLockscreenLauncherWidget, kWidgetKitActionVoiceSearch,
       WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_VOICE_SEARCH},
      {kWidgetKitHostLockscreenLauncherWidget, kWidgetKitActionGame,
       WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_GAME},
      {kWidgetKitHostShortcutsWidget, kWidgetKitActionSearch,
       WidgetKitExtensionAction::ACTION_SHORTCUTS_SEARCH},
      {kWidgetKitHostShortcutsWidget, kWidgetKitActionOpenURL,
       WidgetKitExtensionAction::ACTION_SHORTCUTS_OPEN},
      {kWidgetKitHostSearchPasswordsWidget, nil,
       WidgetKitExtensionAction::
           ACTION_SEARCH_PASSWORDS_WIDGET_SEARCH_PASSWORDS},
      {kWidgetKitHostDinoGameWidget, kWidgetKitActionGame,
       WidgetKitExtensionAction::ACTION_DINO_WIDGET_GAME},
  };

  for (const auto& mapping : kWidgetActionMappings) {
    if ([host isEqualToString:mapping.host]) {
      if (!mapping.path || [path isEqualToString:mapping.path]) {
        return mapping.action;
      }
    }
  }
  return std::nullopt;
}

}  // namespace

@implementation TaskRequestForWidgetURLContext

- (std::optional<WidgetKitExtensionAction>)action {
  return ActionForWidgetURL(self.URLContext.URL);
}

- (void)recordStartupMetrics {
  [super recordStartupMetrics];

  UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                            START_ACTION_WIDGET_KIT_COMMAND,
                            MOBILE_SESSION_START_ACTION_COUNT);

  base::UmaHistogramEnumeration(kAppLaunchSource, AppLaunchSource::WIDGET);

  const std::optional<WidgetKitExtensionAction> action = self.action;
  if (action.has_value()) {
    LogWidgetKitAction(action.value());
    if (action.value() == WidgetKitExtensionAction::
                              ACTION_SEARCH_PASSWORDS_WIDGET_SEARCH_PASSWORDS) {
      base::UmaHistogramEnumeration(
          "PasswordManager.ManagePasswordsReferrer",
          password_manager::ManagePasswordsReferrer::kSearchPasswordsWidget);
      base::RecordAction(base::UserMetricsAction(
          "MobileSearchPasswordsWidgetOpenPasswordManager"));
    }
  }
}

@end
