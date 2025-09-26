// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/script_blocking/script_blocking_view_controller.h"

#import "components/strings/grit/privacy_sandbox_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kScriptBlockingTableViewId = @"kScriptBlockingTableViewId";

}  // namespace

@implementation ScriptBlockingViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kScriptBlockingTableViewId;
  self.title = l10n_util::GetNSString(IDS_FINGERPRINTING_PROTECTION_PAGE_TITLE);
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate scriptBlockingViewControllerDidRemove:self];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/442799337): Record dismissal metric.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/442799337): Record back metric.
}

@end
