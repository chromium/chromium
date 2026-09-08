// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/ui/site_settings_table_view_controller.h"

#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Section identifiers for Site Settings table view.
enum SectionIdentifier {
  kSettings = kSectionIdentifierEnumZero,
};

// Item types for Site Settings table view.
enum ItemType {
  kPermissionsHeader = kItemTypeEnumZero,
  kMicrophone,
  kCamera,
  kLocation,
};

// Returns the detail text representing the given ContentSetting.
NSString* DetailTextForSetting(ContentSetting setting) {
  // TODO(crbug.com/553098545): Use localized strings.
  switch (setting) {
    case CONTENT_SETTING_ASK:
      return @"Ask first";
    case CONTENT_SETTING_BLOCK:
      return @"Not allowed";
    case CONTENT_SETTING_ALLOW:
      return @"Allowed";
    default:
      return nil;
  }
}

}  // namespace

@implementation SiteSettingsTableViewController {
  BOOL _locationCategoryEnabled;
  ContentSetting _microphoneSetting;
  ContentSetting _cameraSetting;
  ContentSetting _locationSetting;
  TableViewDetailIconItem* _microphoneItem;
  TableViewDetailIconItem* _cameraItem;
  TableViewDetailIconItem* _locationItem;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _microphoneSetting = CONTENT_SETTING_DEFAULT;
    _cameraSetting = CONTENT_SETTING_DEFAULT;
    _locationSetting = CONTENT_SETTING_DEFAULT;
  }
  return self;
}

#pragma mark - ChromeTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/553098545): Use localized string.
  self.title = @"Site settings";
  self.tableView.accessibilityIdentifier = kSiteSettingsTableViewId;

  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate siteSettingsTableViewControllerWasRemoved:self];
  }
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kSettings];

  TableViewTextHeaderFooterItem* permissionsHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:kPermissionsHeader];
  // TODO(crbug.com/553098545): Use localized string.
  permissionsHeader.text = @"Permissions";
  [model setHeader:permissionsHeader forSectionWithIdentifier:kSettings];

  _microphoneItem =
      [self detailItemWithType:kMicrophone
                             text:l10n_util::GetNSString(
                                      IDS_IOS_PERMISSIONS_MICROPHONE)
                       detailText:DetailTextForSetting(_microphoneSetting)
                           symbol:SymbolMicrophone
          accessibilityIdentifier:kSiteSettingsMicrophoneCellId];
  [model addItem:_microphoneItem toSectionWithIdentifier:kSettings];

  _cameraItem = [self
           detailItemWithType:kCamera
                         text:l10n_util::GetNSString(IDS_IOS_PERMISSIONS_CAMERA)
                   detailText:DetailTextForSetting(_cameraSetting)
                       symbol:SymbolCamera
      accessibilityIdentifier:kSiteSettingsCameraCellId];
  [model addItem:_cameraItem toSectionWithIdentifier:kSettings];

  if (_locationCategoryEnabled) {
    _locationItem = [self locationItem];
    [model addItem:_locationItem toSectionWithIdentifier:kSettings];
  }
}

#pragma mark - SiteSettingsConsumer

- (void)setLocationCategoryEnabled:(BOOL)enabled {
  if (_locationCategoryEnabled == enabled) {
    return;
  }
  _locationCategoryEnabled = enabled;
  if (!self.tableViewModel) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  if (enabled) {
    if (![model hasItemForItemType:kLocation sectionIdentifier:kSettings]) {
      _locationItem = [self locationItem];
      [model addItem:_locationItem toSectionWithIdentifier:kSettings];
      NSIndexPath* indexPath = [model indexPathForItemType:kLocation
                                         sectionIdentifier:kSettings];
      [self.tableView insertRowsAtIndexPaths:@[ indexPath ]
                            withRowAnimation:UITableViewRowAnimationAutomatic];
    }
  } else {
    if ([model hasItemForItemType:kLocation sectionIdentifier:kSettings]) {
      NSIndexPath* indexPath = [model indexPathForItemType:kLocation
                                         sectionIdentifier:kSettings];
      [model removeItemWithType:kLocation fromSectionWithIdentifier:kSettings];
      [self.tableView deleteRowsAtIndexPaths:@[ indexPath ]
                            withRowAnimation:UITableViewRowAnimationAutomatic];
      _locationItem = nil;
    }
  }
}

- (void)setDefaultSetting:(ContentSetting)setting
                  forType:(ContentSettingsType)type {
  TableViewDetailIconItem* item = nil;
  switch (type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
      _microphoneSetting = setting;
      item = _microphoneItem;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      _cameraSetting = setting;
      item = _cameraItem;
      break;
    case ContentSettingsType::GEOLOCATION:
      _locationSetting = setting;
      item = _locationItem;
      break;
    default:
      return;
  }

  if (item && [self.tableViewModel hasItem:item]) {
    item.detailText = DetailTextForSetting(setting);
    [self reconfigureCellsForItems:@[ item ]];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case kPermissionsHeader:
      break;
    case kMicrophone:
      [self.delegate
          siteSettingsTableViewController:self
                     didSelectSettingType:ContentSettingsType::MEDIASTREAM_MIC];
      break;
    case kCamera:
      [self.delegate siteSettingsTableViewController:self
                                didSelectSettingType:ContentSettingsType::
                                                         MEDIASTREAM_CAMERA];
      break;
    case kLocation:
      [self.delegate
          siteSettingsTableViewController:self
                     didSelectSettingType:ContentSettingsType::GEOLOCATION];
      break;
  }
}

#pragma mark - Private

// Creates and returns a configured TableViewDetailIconItem for a site setting
// category.
- (TableViewDetailIconItem*)detailItemWithType:(ItemType)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                                        symbol:(Symbol)symbol
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewDetailIconItem* item =
      [[TableViewDetailIconItem alloc] initWithType:type];
  item.text = text;
  item.detailText = detailText;
  item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  item.iconImage =
      SymbolWithPointSize(symbol, kSettingsRootSymbolImagePointSize);
  item.iconTintColor = [UIColor colorNamed:kGrey600Color];
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  item.accessibilityIdentifier = accessibilityIdentifier;
  return item;
}

// Creates and returns the location TableViewDetailIconItem.
- (TableViewDetailIconItem*)locationItem {
  // TODO(crbug.com/553098545): Use localized string.
  return [self detailItemWithType:kLocation
                             text:@"Location"
                       detailText:DetailTextForSetting(_locationSetting)
                           symbol:SymbolLocation
          accessibilityIdentifier:kSiteSettingsLocationCellId];
}

@end
