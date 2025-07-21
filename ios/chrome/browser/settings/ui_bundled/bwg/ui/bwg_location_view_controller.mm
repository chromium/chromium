// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_location_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Table identifier.
NSString* const kBWGLocationViewTableIdentifier =
    @"BWGLocationViewTableIdentifier";

}  // namespace

@implementation BWGLocationViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kBWGLocationViewTableIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_BWG_LOCATION_TITLE);
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiLocationSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiLocationSettingsBack"));
}

@end
