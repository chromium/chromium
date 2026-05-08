// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
enum SectionIdentifier {
  SectionIdentifierTravelInfo = kSectionIdentifierEnumZero,
};

}  // namespace

@interface TravelInfoTableViewController ()
@end

@implementation TravelInfoTableViewController {
  NSArray<TableViewItem*>* _travelInfoItems;
  BOOL _settingsAreDismissed;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_AUTOFILL_TRAVEL_TITLE);
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate travelInfoTableViewControllerDidRemove:self];
  }
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  if (_travelInfoItems.count > 0) {
    [model addSectionWithIdentifier:SectionIdentifierTravelInfo];
    for (TableViewItem* item in _travelInfoItems) {
      [model addItem:item toSectionWithIdentifier:SectionIdentifierTravelInfo];
    }
  }
}

#pragma mark - TravelInfoConsumer

- (void)setTravelInfoItems:(NSArray<TableViewItem*>*)travelInfoItems {
  _travelInfoItems = travelInfoItems;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  [self.mutator didSelectTravelInfoItem:item];
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
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
