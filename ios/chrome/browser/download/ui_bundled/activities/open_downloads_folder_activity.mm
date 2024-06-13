// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/activities/open_downloads_folder_activity.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kOpenDownloadsFolderActivityType =
    @"com.google.chrome.OpenDownloadsFolderActivity";

}  // namespace

@implementation OpenDownloadsFolderActivity

#pragma mark - UIActivity

- (NSString*)activityType {
  return kOpenDownloadsFolderActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_OPEN_IN_DOWNLOADS);
}

- (UIImage*)activityImage {
  return DefaultSymbolTemplateWithPointSize(kOpenInDownloadsSymbol,
                                            kSymbolDownloadInfobarPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return YES;
}

- (void)prepareWithActivityItems:(NSArray*)activityItems {
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  base::RecordAction(base::UserMetricsAction(
      "MobileDownloadFolderUIShownFromDownloadManager"));
  [self.browserHandler showDownloadsFolder];
  [self activityDidFinish:YES];
}

@end
