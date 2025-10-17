// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/bwg_settings_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_location_view_controller.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
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
  SectionIdentifierLocation = kSectionIdentifierEnumZero,
  SectionIdentifierPageContent,
  SectionIdentifierActivity,
  SectionIdentifierExtensions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLocation = kItemTypeEnumZero,
  ItemTypePageContentSharing,
  ItemTypeAppActivity,
  ItemTypeExtensions,
  ItemTypeLocationFooter,
  ItemTypePageContentSharingFooter,
  ItemTypeAppActivityFooter,
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

@interface BWGSettingsViewController () <TableViewLinkHeaderFooterItemDelegate>
@end

@implementation BWGSettingsViewController {
  // Precise location item.
  TableViewMultiDetailTextItem* _preciseLocationItem;
  // Switch item for toggling page content sharing.
  TableViewSwitchItem* _pageContentSharingItem;
  // Location view controller shown when precise location row is tapped.
  BWGLocationViewController* _locationViewController;
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
  RecordBWGSettingsOpened();
  [self loadModel];
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  _preciseLocationItem =
      [self detailItemWithType:ItemTypeLocation
                             text:l10n_util::GetNSString(
                                      IDS_IOS_BWG_SETTINGS_LOCATION_TITLE)
               trailingDetailText:[self preciseLocationTrailingDetailText]
          accessibilityIdentifier:kLocationCellId];
  _pageContentSharingItem = [self
           switchItemWithType:ItemTypePageContentSharing
                         text:
                             l10n_util::GetNSString(
                                 IDS_IOS_BWG_SETTINGS_PAGE_CONTENT_SHARING_TITLE)
                  switchValue:_pageContentSharingEnabled
      accessibilityIdentifier:kPageContentSharingCellId];
  _pageContentSharingItem.target = self;
  _pageContentSharingItem.selector = @selector(pageContentSharingSwitchTapped:);

  TableViewLinkHeaderFooterItem* locationFooterItem = [self
      headerFooterItemWithType:ItemTypeLocationFooter
                          text:l10n_util::GetNSString(
                                   IDS_IOS_BWG_SETTINGS_LOCATION_FOOTER_TEXT)
                       linkURL:GURL(kBWGPreciseLocationURL)];
  TableViewLinkHeaderFooterItem* pageContentSharingFooterItem = [self
      headerFooterItemWithType:ItemTypePageContentSharingFooter
                          text:
                              l10n_util::GetNSString(
                                  IDS_IOS_BWG_SETTINGS_PAGE_CONTENT_FOOTER_TEXT)
                       linkURL:GURL(kBWGPageContentSharingURL)];
  TableViewLinkHeaderFooterItem* BWGAppActivityFooterItem = [self
      headerFooterItemWithType:ItemTypeAppActivityFooter
                          text:
                              l10n_util::GetNSString(
                                  IDS_IOS_BWG_SETTINGS_APP_ACTIVITY_FOOTER_TEXT)
                       linkURL:GURL()];

  TableViewModel* model = self.tableViewModel;
  if (IsBWGPreciseLocationEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierLocation];
    [model addItem:_preciseLocationItem
        toSectionWithIdentifier:SectionIdentifierLocation];
    [model setFooter:locationFooterItem
        forSectionWithIdentifier:SectionIdentifierLocation];
  }

  [model addSectionWithIdentifier:SectionIdentifierPageContent];
  [model addItem:_pageContentSharingItem
      toSectionWithIdentifier:SectionIdentifierPageContent];
  [model setFooter:pageContentSharingFooterItem
      forSectionWithIdentifier:SectionIdentifierPageContent];

  [model addSectionWithIdentifier:SectionIdentifierActivity];
  [model addItem:[self BWGAppActivityItem]
      toSectionWithIdentifier:SectionIdentifierActivity];
  [model setFooter:BWGAppActivityFooterItem
      forSectionWithIdentifier:SectionIdentifierActivity];

  [model addSectionWithIdentifier:SectionIdentifierExtensions];
  [model addItem:[self BWGExtensionsItem]
      toSectionWithIdentifier:SectionIdentifierExtensions];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  RecordBWGSettingsClose();
}

- (void)reportBackUserAction {
  RecordBWGSettingsBack();
}

#pragma mark - Private

// Creates a multi detail item with multiple options.
- (TableViewMultiDetailTextItem*)detailItemWithType:(NSInteger)type
                                               text:(NSString*)text
                                 trailingDetailText:(NSString*)trailingText
                            accessibilityIdentifier:
                                (NSString*)accessibilityIdentifier {
  TableViewMultiDetailTextItem* detailItem =
      [[TableViewMultiDetailTextItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.trailingDetailText = trailingText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  return detailItem;
}

// Creates a switch item with multiple options.
- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                      text:(NSString*)title
                               switchValue:(BOOL)isOn
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.on = isOn;
  switchItem.accessibilityIdentifier = accessibilityIdentifier;

  return switchItem;
}
// Creates a footer item with an optional URL.
- (TableViewLinkHeaderFooterItem*)headerFooterItemWithType:(NSInteger)type
                                                      text:(NSString*)footerText
                                                   linkURL:(GURL)linkURL {
  TableViewLinkHeaderFooterItem* headerFooterItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:type];
  headerFooterItem.text = footerText;

  if (!linkURL.is_empty()) {
    NSMutableArray* urls = [[NSMutableArray alloc] init];
    [urls addObject:[[CrURL alloc] initWithGURL:linkURL]];
    headerFooterItem.urls = urls;
  }

  return headerFooterItem;
}

// Creates the BWG app activity item.
- (TableViewDetailTextItem*)BWGAppActivityItem {
  TableViewDetailTextItem* BWGAppActivityItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeAppActivity];
  BWGAppActivityItem.text =
      l10n_util::GetNSString(IDS_IOS_BWG_SETTINGS_APP_ACTIVITY_TITLE);
  BWGAppActivityItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  BWGAppActivityItem.accessibilityTraits = UIAccessibilityTraitLink;
  return BWGAppActivityItem;
}

// Creates the BWG extensions item.
- (TableViewDetailTextItem*)BWGExtensionsItem {
  TableViewDetailTextItem* BWGExtensionsItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeExtensions];
  BWGExtensionsItem.text =
      l10n_util::GetNSString(IDS_IOS_BWG_SETTINGS_EXTENSIONS_TITLE);
  BWGExtensionsItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  BWGExtensionsItem.accessibilityTraits = UIAccessibilityTraitLink;
  return BWGExtensionsItem;
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

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeLocation) {
    _locationViewController = [[BWGLocationViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
    _locationViewController.navigationItem.backButtonTitle =
        l10n_util::GetNSString(IDS_IOS_BWG_LOCATION_BACK_BUTTON_TITLE);
    _locationViewController.preciseLocationEnabled = _preciseLocationEnabled;
    _locationViewController.mutator = self.mutator;
    [self.navigationController pushViewController:_locationViewController
                                         animated:YES];
  }

  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeAppActivity) {
    RecordBWGSettingsAppActivity();
    [self.mutator openNewTabWithURL:GURL(kBWGAppActivityURL)];
  }

  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeExtensions) {
    RecordBWGSettingsExtensions();
    [self.mutator openNewTabWithURL:GURL(kBWGExtensionsURL)];
  }

  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  TableViewLinkHeaderFooterView* footer =
      base::apple::ObjCCast<TableViewLinkHeaderFooterView>(footerView);
  footer.delegate = self;
  return footerView;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.mutator openNewTabWithURL:URL.gurl];
}

#pragma mark - BWGSettingsConsumer

- (void)setPreciseLocationEnabled:(BOOL)enabled {
  _preciseLocationEnabled = enabled;

  // Propagate precise location pref changes to other views that may be opened
  // such as an alternate multi-window screen.
  if (_locationViewController) {
    _locationViewController.preciseLocationEnabled = enabled;
  }

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
