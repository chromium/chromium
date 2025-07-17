// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg//ui/bwg_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/bwg_settings_mutator.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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
  // Precise location item.
  TableViewMultiDetailTextItem* _preciseLocationItem;
  // Switch item for toggling page content sharing.
  TableViewSwitchItem* _pageContentSharingItem;
  // BWG Apps activity item. Uses `accessoryView` to create a tappable icon.
  TableViewDetailTextItem* _BWGAppsActivityItem;
  // Precise location preference value.
  BOOL _preciseLocationEnabled;
  // Page content sharing preference value.
  BOOL _pageContentSharingEnabled;
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
  [model addSectionWithIdentifier:SectionIdentifierActivity];

  _preciseLocationItem =
      [self detailItemWithType:ItemTypeLocation
                             text:l10n_util::GetNSString(
                                      IDS_IOS_BWG_SETTINGS_LOCATION_TITLE)
                       detailText:l10n_util::GetNSString(
                                      IDS_IOS_BWG_SETTINGS_LOCATION_DESCRIPTION)
               trailingDetailText:[self preciseLocationTrailingDetailText]
          accessibilityIdentifier:kLocationCellId];
  _pageContentSharingItem = [self
           switchItemWithType:ItemTypePageContentSharing
                         text:
                             l10n_util::GetNSString(
                                 IDS_IOS_BWG_SETTINGS_PAGE_CONTENT_SHARING_TITLE)
                   detailText:
                       l10n_util::GetNSString(
                           IDS_IOS_BWG_SETTINGS_PAGE_CONTENT_SHARING_DESCRIPTION)
                  switchValue:_pageContentSharingEnabled
      accessibilityIdentifier:kPageContentSharingCellId];
  [model addItem:_preciseLocationItem
      toSectionWithIdentifier:SectionIdentifierBrowsingData];
  [model addItem:_pageContentSharingItem
      toSectionWithIdentifier:SectionIdentifierBrowsingData];
  [model addItem:[self BWGAppActivityItem]
      toSectionWithIdentifier:SectionIdentifierActivity];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileBWGSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileBWGSettingsBack"));
}

#pragma mark - Private

// Creates a multi detail item with multiple options.
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
  detailItem.trailingDetailText = trailingText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  return detailItem;
}

// Creates a switch item with multiple options.
- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                      text:(NSString*)title
                                detailText:(NSString*)detailText
                               switchValue:(BOOL)isOn
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.detailText = detailText;
  switchItem.on = isOn;
  switchItem.accessibilityIdentifier = accessibilityIdentifier;

  return switchItem;
}

// Called from the PageContentSharing setting's UIControlEventTouchUpInside.
// Updates underlying page content sharing pref.
- (void)pageContentSharingSwitchTapped:(UISwitch*)switchView {
  [self.mutator setPageContentSharingPref:switchView.isOn];
}

// Returns precise Location trailing detail text which depends on the related
// pref value.
- (NSString*)preciseLocationTrailingDetailText {
  if (_preciseLocationEnabled) {
    return l10n_util::GetNSString(IDS_IOS_SETTING_ON);
  }

  return l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
}

// Creates the BWG app activity item.
- (TableViewDetailTextItem*)BWGAppActivityItem {
  TableViewDetailTextItem* BWGAppActivityItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeAppActivity];
  BWGAppActivityItem.text =
      l10n_util::GetNSString(IDS_IOS_BWG_SETTINGS_APP_ACTIVITY_TITLE);
  BWGAppActivityItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  return BWGAppActivityItem;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeAppActivity) {
    base::RecordAction(
        base::UserMetricsAction("Settings.BWGSettings.BWGAppActivity"));
    [self.mutator openNewTabWithURL:GURL(kBWGAppActivityURL)];
  }
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypePageContentSharing) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(pageContentSharingSwitchTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
}

#pragma mark - BWGSettingsConsumer

- (void)setPreciseLocationEnabled:(BOOL)enabled {
  _preciseLocationEnabled = enabled;

  if ([self isViewLoaded]) {
    _preciseLocationItem.trailingDetailText =
        [self preciseLocationTrailingDetailText];
    [self reconfigureCellsForItems:@[ _preciseLocationItem ]];
  }
}

- (void)setPageContentSharingEnabled:(BOOL)enabled {
  _pageContentSharingEnabled = enabled;

  if ([self isViewLoaded]) {
    _pageContentSharingItem.on = _pageContentSharingEnabled;
    [self reconfigureCellsForItems:@[ _pageContentSharingItem ]];
  }
}

@end
