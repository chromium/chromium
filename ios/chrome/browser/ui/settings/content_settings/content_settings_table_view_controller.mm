// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/content_settings_table_view_controller.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/mailto_handler/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/mailto_handler_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/settings/content_settings/block_popups_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_coordinator.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/features.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Is YES when one window has mailTo controller opened.
BOOL openedMailTo = NO;

// Notification name of changes to openedMailTo state.
NSString* kMailToInstanceChanged = @"MailToInstanceChanged";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSettings = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSettingsBlockPopups = kItemTypeEnumZero,
  ItemTypeSettingsComposeEmail,
  ItemTypeSettingsShowLinkPreview,
  ItemTypeSettingsDefaultSiteMode,
};

}  // namespace

@interface ContentSettingsTableViewController () <BooleanObserver> {
  // The observable boolean that binds to the "Disable Popups" setting state.
  ContentSettingBackedBoolean* _disablePopupsSetting;

  // Updatable Items
  TableViewDetailIconItem* _blockPopupsDetailItem;
  TableViewDetailIconItem* _composeEmailDetailItem;
  TableViewMultiDetailTextItem* _openedInAnotherWindowItem;
  TableViewDetailIconItem* _defaultSiteMode;
}

// PrefBackedBoolean for "Show Link Preview" setting state.
@property(nonatomic, strong, readonly) PrefBackedBoolean* linkPreviewEnabled;

// The item related to the switch for the "Show Link Preview" setting.
@property(nonatomic, strong) TableViewSwitchItem* linkPreviewItem;

// The item related to the default mode used to load the pages.
@property(nonatomic, strong) TableViewDetailIconItem* defaultModeItem;

// The coordinator showing the view to choose the defaultMode.
@property(nonatomic, strong)
    DefaultPageModeCoordinator* defaultModeViewCoordinator;

// The setting used to store the default mode.
@property(nonatomic, strong) ContentSettingBackedBoolean* requestDesktopSetting;

// Helpers to create collection view items.
- (id)blockPopupsItem;
- (id)composeEmailItem;

@end

@implementation ContentSettingsTableViewController {
  Browser* _browser;  // weak
}

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    self.title = l10n_util::GetNSString(IDS_IOS_CONTENT_SETTINGS_TITLE);

    ChromeBrowserState* browserState = browser->GetBrowserState();

    HostContentSettingsMap* settingsMap =
        ios::HostContentSettingsMapFactory::GetForBrowserState(browserState);
    _disablePopupsSetting = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:ContentSettingsType::POPUPS
                              inverted:YES];
    [_disablePopupsSetting setObserver:self];

    _linkPreviewEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:browserState->GetPrefs()
                   prefName:prefs::kLinkPreviewEnabled];
    [_linkPreviewEnabled setObserver:self];

    _requestDesktopSetting = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:ContentSettingsType::REQUEST_DESKTOP_SITE
                              inverted:NO];
    [_requestDesktopSetting setObserver:self];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  [self loadModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(mailToControllerChanged)
             name:kMailToInstanceChanged
           object:nil];
  [self checkMailToOwnership];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self checkMailToOwnership];
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                                  name:kMailToInstanceChanged
                                                object:nil];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  if (!_browser)
    return;

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:[self blockPopupsItem]
      toSectionWithIdentifier:SectionIdentifierSettings];
  NSString* settingsTitle = MailtoHandlerServiceFactory::GetForBrowserState(
                                _browser->GetBrowserState())
                                ->SettingsTitle();
  // Display email settings only on one window at a time, by checking
  // if this is the current owner.
  _openedInAnotherWindowItem = nil;
  _composeEmailDetailItem = nil;
  if (settingsTitle) {
    if (!openedMailTo) {
      [model addItem:[self composeEmailItem]
          toSectionWithIdentifier:SectionIdentifierSettings];
    } else {
      [model addItem:[self openedInAnotherWindowItem]
          toSectionWithIdentifier:SectionIdentifierSettings];
    }
  }

  if (IsDiscoverFeedPreviewEnabled()) {
    [model addItem:[self linkPreviewItem]
        toSectionWithIdentifier:SectionIdentifierSettings];
  }

  self.defaultModeItem = [self defaultSiteMode];
  [model addItem:self.defaultModeItem
      toSectionWithIdentifier:SectionIdentifierSettings];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileContentSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileContentSettingsBack"));
}

#pragma mark - ContentSettingsTableViewController

- (TableViewDetailIconItem*)defaultSiteMode {
  _defaultSiteMode = [[TableViewDetailIconItem alloc]
      initWithType:ItemTypeSettingsDefaultSiteMode];
  _defaultSiteMode.text =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_LABEL);
  _defaultSiteMode.detailText = [self defaultModeDescription];
  _defaultSiteMode.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  _defaultSiteMode.accessibilityIdentifier = kSettingsDefaultSiteModeCellId;
  return _defaultSiteMode;
}

- (TableViewItem*)blockPopupsItem {
  _blockPopupsDetailItem = [[TableViewDetailIconItem alloc]
      initWithType:ItemTypeSettingsBlockPopups];
  NSString* subtitle = [_disablePopupsSetting value]
                           ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                           : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _blockPopupsDetailItem.text = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
  _blockPopupsDetailItem.detailText = subtitle;
  _blockPopupsDetailItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _blockPopupsDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  _blockPopupsDetailItem.accessibilityIdentifier = kSettingsBlockPopupsCellId;
  return _blockPopupsDetailItem;
}

- (TableViewItem*)composeEmailItem {
  if (!_browser)
    return nil;

  _composeEmailDetailItem = [[TableViewDetailIconItem alloc]
      initWithType:ItemTypeSettingsComposeEmail];
  // Use the handler's preferred title string for the compose email item.
  NSString* settingsTitle = MailtoHandlerServiceFactory::GetForBrowserState(
                                _browser->GetBrowserState())
                                ->SettingsTitle();
  DCHECK([settingsTitle length]);
  // .detailText can display the selected mailto handling app, but the current
  // MailtoHandlerService does not expose this through its API.
  _composeEmailDetailItem.text = settingsTitle;
  _composeEmailDetailItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _composeEmailDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  _composeEmailDetailItem.accessibilityIdentifier = kSettingsDefaultAppsCellId;
  return _composeEmailDetailItem;
}

- (TableViewItem*)openedInAnotherWindowItem {
  if (!_browser)
    return nil;

  _openedInAnotherWindowItem = [[TableViewMultiDetailTextItem alloc]
      initWithType:ItemTypeSettingsComposeEmail];
  // Use the handler's preferred title string for the compose email item.
  NSString* settingsTitle = MailtoHandlerServiceFactory::GetForBrowserState(
                                _browser->GetBrowserState())
                                ->SettingsTitle();
  DCHECK([settingsTitle length]);
  // .detailText can display the selected mailto handling app, but the current
  // MailtoHandlerService does not expose this through its API.
  _openedInAnotherWindowItem.text = settingsTitle;

  _openedInAnotherWindowItem.trailingDetailText =
      l10n_util::GetNSString(IDS_IOS_SETTING_OPENED_IN_ANOTHER_WINDOW);
  _openedInAnotherWindowItem.accessibilityTraits |=
      UIAccessibilityTraitButton | UIAccessibilityTraitNotEnabled;
  _openedInAnotherWindowItem.accessibilityIdentifier =
      kSettingsDefaultAppsCellId;
  return _openedInAnotherWindowItem;
}

- (TableViewSwitchItem*)linkPreviewItem {
  if (!_linkPreviewItem) {
    _linkPreviewItem = [[TableViewSwitchItem alloc]
        initWithType:ItemTypeSettingsShowLinkPreview];

    _linkPreviewItem.text = l10n_util::GetNSString(IDS_IOS_SHOW_LINK_PREVIEWS);
    _linkPreviewItem.on = [self.linkPreviewEnabled value];
    _linkPreviewItem.accessibilityIdentifier = kSettingsShowLinkPreviewCellId;
  }
  return _linkPreviewItem;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeSettingsShowLinkPreview) {
    TableViewSwitchCell* switchCell =
        base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(showLinkPreviewSwitchToggled:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if (!_browser)
    return;

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSettingsBlockPopups: {
      BlockPopupsTableViewController* controller =
          [[BlockPopupsTableViewController alloc]
              initWithBrowserState:_browser->GetBrowserState()];
      controller.dispatcher = self.dispatcher;
      [self.navigationController pushViewController:controller animated:YES];
      break;
    }
    case ItemTypeSettingsComposeEmail: {
      if (openedMailTo)
        break;

      UIViewController* controller =
          MailtoHandlerServiceFactory::GetForBrowserState(
              _browser->GetBrowserState())
              ->CreateSettingsController();
      if (controller) {
        [self.navigationController pushViewController:controller animated:YES];
        openedMailTo = YES;
        [[NSNotificationCenter defaultCenter]
            postNotificationName:kMailToInstanceChanged
                          object:nil];
      }
      break;
    }
    case ItemTypeSettingsDefaultSiteMode: {
      self.defaultModeViewCoordinator = [[DefaultPageModeCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:_browser];
      [self.defaultModeViewCoordinator start];
    }
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _disablePopupsSetting) {
    NSString* subtitle = [_disablePopupsSetting value]
                             ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    // Update the item.
    _blockPopupsDetailItem.detailText = subtitle;

    // Update the cell.
    [self reconfigureCellsForItems:@[ _blockPopupsDetailItem ]];
  } else if (observableBoolean == self.linkPreviewEnabled) {
    self.linkPreviewItem.on = [self.linkPreviewEnabled value];
    [self reconfigureCellsForItems:@[ self.linkPreviewItem ]];
  } else if (observableBoolean == self.requestDesktopSetting &&
             self.defaultModeItem) {
    self.defaultModeItem.detailText = [self defaultModeDescription];
    [self reconfigureCellsForItems:@[ self.defaultModeItem ]];
  } else {
    NOTREACHED();
  }
}

#pragma mark - Switch Actions

- (void)showLinkPreviewSwitchToggled:(UISwitch*)sender {
  BOOL newSwitchValue = sender.isOn;
  self.linkPreviewItem.on = newSwitchValue;
  [self.linkPreviewEnabled setValue:newSwitchValue];
}

#pragma mark - Private

// Called to reload data when another window has mailTo settings opened.
- (void)mailToControllerChanged {
  [self reloadData];
}

// Verifies using the navigation stack if this is a return from mailTo settings
// and this instance should reset `openedMailTo`.
- (void)checkMailToOwnership {
  if (!_browser)
    return;

  // Since this doesn't know or have access to the mailTo controller code,
  // it detects if the flow is coming back from it, based on the navigation
  // bar stack items.
  NSString* top = self.navigationController.navigationBar.topItem.title;
  NSString* mailToTitle = MailtoHandlerServiceFactory::GetForBrowserState(
                              _browser->GetBrowserState())
                              ->SettingsTitle();
  if ([top isEqualToString:mailToTitle]) {
    openedMailTo = NO;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kMailToInstanceChanged
                      object:nil];
  }
}

// Returns the string for the default mode.
- (NSString*)defaultModeDescription {
  return self.requestDesktopSetting.value
             ? l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_DESKTOP)
             : l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_MOBILE);
}

- (void)settingsWillBeDismissed {
  [_disablePopupsSetting stop];
  [_requestDesktopSetting stop];
  [_linkPreviewEnabled stop];
  _browser = nullptr;
}

@end
