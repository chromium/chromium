// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_table_view_controller.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface TravelInfoTableViewController ()
@end

@implementation TravelInfoTableViewController {
  BOOL _settingsAreDismissed;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate travelInfoTableViewControllerDidRemove:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_AUTOFILL_TRAVEL_TITLE);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  // For now, empty page.
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/500341282): Add missing metric.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/500341282): Add missing metric.
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);
  _settingsAreDismissed = YES;
}

@end
