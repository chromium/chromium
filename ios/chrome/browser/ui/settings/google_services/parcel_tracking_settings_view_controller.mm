// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_view_controller.h"

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_model_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation ParcelTrackingSettingsViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE);
  [self loadModel];
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  [self.modelDelegate loadModel];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.modelDelegate tableViewDidSelectItemAtIndexPath:indexPath];
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

@end
