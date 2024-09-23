// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/widget_kit/model/widget_metrics_util.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/widget_kit/model/model_swift.h"

using base::UmaHistogramEnumeration;

namespace {

// Values of the UMA IOS.WidgetKit.Install, IOS.WidgetKit.Uninstall and
// IOS.WidgetKit.Current histograms. Must be kept up to date with
// IOSWidgetKitExtensionKind in enums.xml. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class WidgetKitExtensionKind {
  kDino = 0,
  kSearch = 1,
  kQuickActions = 2,
  kObsolete = 3,
  kLockscreenLauncherSearch = 4,
  kLockscreenLauncherIncognito = 5,
  kLockscreenLauncherVoiceSearch = 6,
  kLockscreenLauncherGame = 7,
  kShortcuts = 8,
  kSearchPasswords = 9,
  kMaxValue = kSearchPasswords,
};

WidgetKitExtensionKind UMAKindForWidgetKind(NSString* kind) {
  // TODO(crbug.com/40725610): Share this names in a constant file everywhere
  // they are used. Currently names matches the declared names in each widget
  // file in ios/c/widget_kit_extension.
  if ([kind isEqualToString:@"DinoGameWidget"]) {
    return WidgetKitExtensionKind::kDino;
  }
  if ([kind isEqualToString:@"SearchWidget"]) {
    return WidgetKitExtensionKind::kSearch;
  }
  if ([kind isEqualToString:@"QuickActionsWidget"]) {
    return WidgetKitExtensionKind::kQuickActions;
  }
  if ([kind isEqualToString:@"ShortcutsWidget"]) {
    return WidgetKitExtensionKind::kShortcuts;
  }
  if ([kind isEqualToString:@"SearchPasswordsWidget"]) {
    return WidgetKitExtensionKind::kSearchPasswords;
  }
  if ([kind isEqualToString:@"LockscreenLauncherSearchWidget"]) {
    return WidgetKitExtensionKind::kLockscreenLauncherSearch;
  }
  if ([kind isEqualToString:@"LockscreenLauncherIncognitoWidget"]) {
    return WidgetKitExtensionKind::kLockscreenLauncherIncognito;
  }
  if ([kind isEqualToString:@"LockscreenLauncherVoiceSearchWidget"]) {
    return WidgetKitExtensionKind::kLockscreenLauncherVoiceSearch;
  }
  if ([kind isEqualToString:@"LockscreenLauncherGameWidget"]) {
    return WidgetKitExtensionKind::kLockscreenLauncherGame;
  }
  if ([kind isEqualToString:@"SearchPasswordsWidget"]) {
    return WidgetKitExtensionKind::kSearchPasswords;
  }

  NOTREACHED_IN_MIGRATION() << base::SysNSStringToUTF8(kind);
  return WidgetKitExtensionKind::kObsolete;
}

}  // namespace

@implementation WidgetMetricsUtil

+ (void)logInstalledWidgets {
  WidgetsMetricLogger.widgetInstalledCallback = ^(NSString* kind) {
    UmaHistogramEnumeration("IOS.WidgetKit.Install",
                            UMAKindForWidgetKind(kind));
  };
  WidgetsMetricLogger.widgetUninstalledCallback = ^(NSString* kind) {
    UmaHistogramEnumeration("IOS.WidgetKit.Uninstall",
                            UMAKindForWidgetKind(kind));
  };
  WidgetsMetricLogger.widgetCurrentCallback = ^(NSString* kind) {
    UmaHistogramEnumeration("IOS.WidgetKit.Current",
                            UMAKindForWidgetKind(kind));
  };
  [WidgetsMetricLogger logInstalledWidgets];
}

@end
