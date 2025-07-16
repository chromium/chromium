// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg//ui/bwg_settings_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_settings_mutator.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Section identifiers in the BWG settings table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierBrowsingData = kSectionIdentifierEnumZero,
  SectionIdentifierActivity,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLocation = kItemTypeEnumZero,
  ItemTypePageContentSharing,
  ItemTypeAppActivity,
};

// Table identifier.
NSString* const kBWGSettingsViewTableIdentifier =
    @"BWGSettingsViewTableIdentifier";

// Row identifiers.
NSString* const kLocationCellId = @"LocationCellId";
NSString* const kPageContentSharingCellId = @"PageContentSharingCellId";

// Action identifier on a tap on links.
NSString* const kLocationLinkAction = @"LocationLinkAction";
NSString* const kPageContentSharingAction = @"PageContentSharingAction";

}  // namespace

@implementation BWGSettingsViewController {
  // Location item.
  TableViewMultiDetailTextItem* _locationDetailItem;
  // Switch item for toggling page content sharing.
  TableViewSwitchItem* _pageContentSharingItem;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kBWGSettingsViewTableIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_BWG_SETTINGS_TITLE);
  [self loadModel];
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierBrowsingData];

  _locationDetailItem =
      [self detailItemWithType:ItemTypeLocation
                             text:l10n_util::GetNSString(
                                      IDS_IOS_BWG_SETTINGS_LOCATION_TITLE)
                       detailText:l10n_util::GetNSString(
                                      IDS_IOS_BWG_SETTINGS_LOCATION_DESCRIPTION)
               trailingDetailText:l10n_util::GetNSString(IDS_IOS_SETTING_OFF)
          accessibilityIdentifier:kLocationCellId];
  _pageContentSharingItem = [self
           switchItemWithType:ItemTypePageContentSharing
                         text:
                             l10n_util::GetNSString(
                                 IDS_IOS_BWG_SETTINGS_PAGE_CONTENT_SHARING_TITLE)
                   detailText:
                       l10n_util::GetNSString(
                           IDS_IOS_BWG_SETTINGS_PAGE_CONTENT_SHARING_DESCRIPTION)
      accessibilityIdentifier:kPageContentSharingCellId];
  [model addItem:_locationDetailItem
      toSectionWithIdentifier:SectionIdentifierBrowsingData];
  [model addItem:_pageContentSharingItem
      toSectionWithIdentifier:SectionIdentifierBrowsingData];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileBWGSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileBWGSettingsBack"));
}

#pragma mark - Private

// TODO(crbug.com/427226904): Convert to different TableViewItem that uses
// attributed text.
- (TableViewMultiDetailTextItem*)detailItemWithType:(NSInteger)type
                                               text:(NSString*)text
                                         detailText:(NSString*)detailText
                                 trailingDetailText:(NSString*)trailingText
                            accessibilityIdentifier:
                                (NSString*)accessibilityIdentifier {
  TableViewMultiDetailTextItem* detailItem =
      [[TableViewMultiDetailTextItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.leadingDetailText = detailText;
  // TODO(crbug.com/427226904): Update text based on a pref.
  detailItem.trailingDetailText = trailingText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  return detailItem;
}

// TODO(crbug.com/427226904): Convert to different TableViewItem that uses
// attributed text.
- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                      text:(NSString*)title
                                detailText:(NSString*)detailText
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.detailText = detailText;
  switchItem.accessibilityIdentifier = accessibilityIdentifier;

  return switchItem;
}

@end
