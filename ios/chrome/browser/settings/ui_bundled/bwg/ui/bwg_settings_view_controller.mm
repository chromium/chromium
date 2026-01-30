// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_dynamic_settings_item.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_action.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_action_type.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_context.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_metadata.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_location_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_camera_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/utils/gemini_settings_metrics.h"
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
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Section identifiers in the Gemini settings table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLocation = kSectionIdentifierEnumZero,
  SectionIdentifierCamera,
  SectionIdentifierPageContent,
  SectionIdentifierActivity,
  SectionIdentifierExtensions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLocation = kItemTypeEnumZero,
  ItemTypeCamera,
  ItemTypePageContentSharing,
  ItemTypeAppActivity,
  ItemTypeExtensions,
  ItemTypeLocationFooter,
  ItemTypeCameraFooter,
  ItemTypePageContentSharingFooter,
  ItemTypeAppActivityFooter,
};

// Table identifier.
NSString* const kBWGSettingsViewTableIdentifier =
    @"BWGSettingsViewTableIdentifier";

// Row identifiers.
NSString* const kLocationCellId = @"LocationCellId";
NSString* const kCameraCellId = @"CameraCellId";
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
  // Camera item.
  TableViewMultiDetailTextItem* _cameraItem;
  // Switch item for toggling page content sharing.
  TableViewSwitchItem* _pageContentSharingItem;
  // Location view controller shown when precise location row is tapped.
  BWGLocationViewController* _locationViewController;
  // Camera view controller shown when camera row is tapped.
  GeminiCameraViewController* _cameraViewController;
  // Precise location preference value.
  BOOL _preciseLocationEnabled;
  // Camera preference value.
  BOOL _cameraEnabled;
  // Page content sharing preference value.
  BOOL _pageContentSharingEnabled;
  // Dynamic settings items.
  NSArray<GeminiDynamicSettingsItem*>* _dynamicSettingsItems;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kBWGSettingsViewTableIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_BWG_SETTINGS_TITLE);
  RecordGeminiSettingsOpened();
  [self loadModel];
  [self.mutator loadDynamicSettings];
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
  _cameraItem =
      [self detailItemWithType:ItemTypeCamera
                             text:l10n_util::GetNSString(
                                      IDS_IOS_GEMINI_SETTINGS_CAMERA_TITLE)
               trailingDetailText:[self cameraTrailingDetailText]
          accessibilityIdentifier:kCameraCellId];
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
  TableViewLinkHeaderFooterItem* cameraFooterItem = [self
      headerFooterItemWithType:ItemTypeCameraFooter
                          text:l10n_util::GetNSString(
                                   IDS_IOS_GEMINI_SETTINGS_CAMERA_FOOTER_TEXT)
                       linkURL:GURL()];
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

  if (IsGeminiImageRemixToolEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierCamera];
    [model addItem:_cameraItem toSectionWithIdentifier:SectionIdentifierCamera];
    [model setFooter:cameraFooterItem
        forSectionWithIdentifier:SectionIdentifierCamera];
  }

  [model addSectionWithIdentifier:SectionIdentifierPageContent];
  [model addItem:_pageContentSharingItem
      toSectionWithIdentifier:SectionIdentifierPageContent];
  [model setFooter:pageContentSharingFooterItem
      forSectionWithIdentifier:SectionIdentifierPageContent];

  if (!IsGeminiDynamicSettingsEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierActivity];
    [model addItem:[self BWGAppActivityItem]
        toSectionWithIdentifier:SectionIdentifierActivity];
    [model setFooter:BWGAppActivityFooterItem
        forSectionWithIdentifier:SectionIdentifierActivity];
    RecordGeminiSettingsItemShown(IOSGeminiSettingsItem::kGeminiAppsActivity);

    [model addSectionWithIdentifier:SectionIdentifierExtensions];
    [model addItem:[self BWGExtensionsItem]
        toSectionWithIdentifier:SectionIdentifierExtensions];
    RecordGeminiSettingsItemShown(IOSGeminiSettingsItem::kExtensions);
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  RecordGeminiSettingsClose();
}

- (void)reportBackUserAction {
  RecordGeminiSettingsBack();
}

#pragma mark - Private

- (void)recordItemShownForContext:(GeminiSettingsContext)context {
  switch (context) {
    case GeminiSettingsContextGeminiAppsActivity:
      RecordGeminiSettingsItemShown(IOSGeminiSettingsItem::kGeminiAppsActivity);
      break;
    case GeminiSettingsContextPersonalization:
      RecordGeminiSettingsItemShown(IOSGeminiSettingsItem::kPersonalization);
      break;
    case GeminiSettingsContextExtensions:
      RecordGeminiSettingsItemShown(IOSGeminiSettingsItem::kExtensions);
      break;
    default:
      RecordGeminiSettingsItemShown(IOSGeminiSettingsItem::kUnknown);
      break;
  }
}

- (void)recordItemUsedForContext:(GeminiSettingsContext)context {
  switch (context) {
    case GeminiSettingsContextGeminiAppsActivity:
      RecordGeminiSettingsItemUsed(IOSGeminiSettingsItem::kGeminiAppsActivity);
      RecordGeminiSettingsAppsActivity();
      break;
    case GeminiSettingsContextPersonalization:
      RecordGeminiSettingsItemUsed(IOSGeminiSettingsItem::kPersonalization);
      RecordGeminiSettingsPersonalization();
      break;
    case GeminiSettingsContextExtensions:
      RecordGeminiSettingsItemUsed(IOSGeminiSettingsItem::kExtensions);
      RecordGeminiSettingsExtensions();
      break;
    default:
      RecordGeminiSettingsItemUsed(IOSGeminiSettingsItem::kUnknown);
      break;
  }
}

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

// Creates the Gemini app activity item.
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

// Creates the Gemini extensions item.
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

// Returns camera trailing detail text which depends on the related pref value.
- (NSString*)cameraTrailingDetailText {
  if (_cameraEnabled) {
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

  if ([self.tableViewModel itemTypeForIndexPath:indexPath] == ItemTypeCamera) {
    _cameraViewController = [[GeminiCameraViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
    _cameraViewController.cameraEnabled = _cameraEnabled;
    _cameraViewController.mutator = self.mutator;
    [self.navigationController pushViewController:_cameraViewController
                                         animated:YES];
  }

  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeAppActivity) {
    RecordGeminiSettingsItemUsed(IOSGeminiSettingsItem::kGeminiAppsActivity);
    RecordGeminiSettingsAppsActivity();
    [self.mutator openNewTabWithURL:GURL(kBWGAppActivityURL)];
  }

  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeExtensions) {
    RecordGeminiSettingsItemUsed(IOSGeminiSettingsItem::kExtensions);
    RecordGeminiSettingsExtensions();
    [self.mutator openNewTabWithURL:GURL(kBWGExtensionsURL)];
  }

  if (IsGeminiDynamicSettingsEnabled()) {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    GeminiDynamicSettingsItem* dynamicSettingsItem =
        base::apple::ObjCCast<GeminiDynamicSettingsItem>(item);

    if (dynamicSettingsItem) {
      switch (dynamicSettingsItem.action.type) {
        case GeminiSettingsActionTypeViewController: {
          UIViewController* viewController =
              dynamicSettingsItem.action.viewController;
          if (viewController) {
            [self.navigationController pushViewController:viewController
                                                 animated:YES];
          }
          break;
        }

        case GeminiSettingsActionTypeURL: {
          GURL gURL = net::GURLWithNSURL(dynamicSettingsItem.action.URL);
          [self.mutator openNewTabWithURL:gURL];
          break;
        }

        case GeminiSettingsActionTypeUnknown:
          break;
      }

      [self recordItemUsedForContext:dynamicSettingsItem.metadata.context];
    }
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

#pragma mark - GeminiSettingsConsumer

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

- (void)setCameraPermissionEnabled:(BOOL)enabled {
  _cameraEnabled = enabled;

  if (_cameraViewController) {
    _cameraViewController.cameraEnabled = enabled;
  }

  if ([self isViewLoaded]) {
    _cameraItem.trailingDetailText = [self cameraTrailingDetailText];
    [self reconfigureCellsForItems:@[ _cameraItem ]];
  }
}

- (void)setPageContentSharingEnabled:(BOOL)enabled {
  _pageContentSharingEnabled = enabled;

  if ([self isViewLoaded]) {
    _pageContentSharingItem.on = _pageContentSharingEnabled;
    [self reconfigureCellsForItems:@[ _pageContentSharingItem ]];
  }
}

- (void)updateDynamicSettingsItems:
    (NSArray<GeminiDynamicSettingsItem*>*)newItems {
  // Remove previous dynamic settings sections.
  for (GeminiDynamicSettingsItem* oldItem in _dynamicSettingsItems) {
    [self.tableViewModel removeSectionWithIdentifier:oldItem.type];
  }

  _dynamicSettingsItems = newItems;

  // Add a new section, item and optional footer for each dynamic setting.
  for (GeminiDynamicSettingsItem* newItem in newItems) {
    [self recordItemShownForContext:newItem.metadata.context];

    NSInteger settingIdentifier = newItem.type;

    [self.tableViewModel addSectionWithIdentifier:settingIdentifier];
    [self.tableViewModel addItem:newItem
         toSectionWithIdentifier:settingIdentifier];

    if (newItem.metadata.subtitle) {
      TableViewLinkHeaderFooterItem* settingFooterItem =
          [self headerFooterItemWithType:ItemTypeAppActivityFooter
                                    text:newItem.metadata.subtitle
                                 linkURL:GURL()];
      [self.tableViewModel setFooter:settingFooterItem
            forSectionWithIdentifier:settingIdentifier];
    }
  }
  [self.tableView reloadData];
}

@end
