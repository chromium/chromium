// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/widget_kit/widget_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/widget_kit/widget_kit_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  kMaxValue = kObsolete,
};

WidgetKitExtensionKind UMAKindForWidgetKind(NSString* kind) {
  // TODO(crbug.com/1138721): Share this names in a constant file everywhere
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
  if ([kind isEqualToString:@"LockscreenLauncherSearchWidget"]) {
    // TODO(crbug.com/1347565): Add an enum for this case.
    return WidgetKitExtensionKind::kObsolete;
  }
  if ([kind isEqualToString:@"LockscreenLauncherIncognitoWidget"]) {
    // TODO(crbug.com/1347565): Add an enum for this case.
    return WidgetKitExtensionKind::kObsolete;
  }
  if ([kind isEqualToString:@"LockscreenLauncherVoiceSearchWidget"]) {
    // TODO(crbug.com/1347565): Add an enum for this case.
    return WidgetKitExtensionKind::kObsolete;
  }
  if ([kind isEqualToString:@"LockscreenLauncherGameWidget"]) {
    // TODO(crbug.com/1347565): Add an enum for this case.
    return WidgetKitExtensionKind::kObsolete;
  }

  NOTREACHED() << base::SysNSStringToUTF8(kind);
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
