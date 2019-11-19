// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings_table_view_controller.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/settings/block_popups_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/translate_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSettings = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSettingsBlockPopups = kItemTypeEnumZero,
  ItemTypeSettingsTranslate,
  ItemTypeSettingsComposeEmail,
};

}  // namespace

@interface ContentSettingsTableViewController ()<PrefObserverDelegate,
                                                 BooleanObserver> {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // The observable boolean that binds to the "Disable Popups" setting state.
  ContentSettingBackedBoolean* _disablePopupsSetting;

  // Updatable Items
  TableViewDetailIconItem* _blockPopupsDetailItem;
  TableViewDetailIconItem* _translateDetailItem;
  TableViewDetailIconItem* _composeEmailDetailItem;
}

// Returns the value for the default setting with ID |settingID|.
- (ContentSetting)getContentSetting:(ContentSettingsType)settingID;

// Helpers to create collection view items.
- (id)blockPopupsItem;
- (id)translateItem;
- (id)composeEmailItem;

@end

@implementation ContentSettingsTableViewController {
  ios::ChromeBrowserState* browserState_;  // weak
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    browserState_ = browserState;
    self.title = l10n_util::GetNSString(IDS_IOS_CONTENT_SETTINGS_TITLE);

    _prefChangeRegistrar.Init(browserState->GetPrefs());
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kOfferTranslateEnabled, &_prefChangeRegistrar);

    HostContentSettingsMap* settingsMap =
        ios::HostContentSettingsMapFactory::GetForBrowserState(browserState);
    _disablePopupsSetting = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:ContentSettingsType::POPUPS
                              inverted:YES];
    [_disablePopupsSetting setObserver:self];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.rowHeight = UITableViewAutomaticDimension;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:[self blockPopupsItem]
      toSectionWithIdentifier:SectionIdentifierSettings];
  if (!base::FeatureList::IsEnabled(kLanguageSettings)) {
    [model addItem:[self translateItem]
        toSectionWithIdentifier:SectionIdentifierSettings];
  }
  MailtoHandlerProvider* provider =
      ios::GetChromeBrowserProvider()->GetMailtoHandlerProvider();
  NSString* settingsTitle = provider->MailtoHandlerSettingsTitle();
  if (settingsTitle) {
    [model addItem:[self composeEmailItem]
        toSectionWithIdentifier:SectionIdentifierSettings];
  }
}

#pragma mark - ContentSettingsTableViewController

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
  return _blockPopupsDetailItem;
}

- (TableViewItem*)translateItem {
  _translateDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeSettingsTranslate];
  BOOL enabled =
      browserState_->GetPrefs()->GetBoolean(prefs::kOfferTranslateEnabled);
  NSString* subtitle = enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _translateDetailItem.text = l10n_util::GetNSString(IDS_IOS_TRANSLATE_SETTING);
  _translateDetailItem.detailText = subtitle;
  _translateDetailItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _translateDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return _translateDetailItem;
}

- (TableViewItem*)composeEmailItem {
  _composeEmailDetailItem = [[TableViewDetailIconItem alloc]
      initWithType:ItemTypeSettingsComposeEmail];
  // Use the handler's preferred title string for the compose email item.
  MailtoHandlerProvider* provider =
      ios::GetChromeBrowserProvider()->GetMailtoHandlerProvider();
  NSString* settingsTitle = provider->MailtoHandlerSettingsTitle();
  DCHECK([settingsTitle length]);
  // .detailText can display the selected mailto handling app, but the current
  // MailtoHandlerProvider does not expose this through its API.
  _composeEmailDetailItem.text = settingsTitle;
  _composeEmailDetailItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _composeEmailDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return _composeEmailDetailItem;
}

- (ContentSetting)getContentSetting:(ContentSettingsType)settingID {
  return ios::HostContentSettingsMapFactory::GetForBrowserState(browserState_)
      ->GetDefaultContentSetting(settingID, NULL);
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSettingsBlockPopups: {
      UIViewController* controller = [[BlockPopupsTableViewController alloc]
          initWithBrowserState:browserState_];
      [self.navigationController pushViewController:controller animated:YES];
      break;
    }
    case ItemTypeSettingsTranslate: {
      TranslateTableViewController* controller =
          [[TranslateTableViewController alloc]
              initWithPrefs:browserState_->GetPrefs()];
      controller.dispatcher = self.dispatcher;
      [self.navigationController pushViewController:controller animated:YES];
      break;
    }
    case ItemTypeSettingsComposeEmail: {
      MailtoHandlerProvider* provider =
          ios::GetChromeBrowserProvider()->GetMailtoHandlerProvider();
      UIViewController* controller =
          provider->MailtoHandlerSettingsController();
      if (controller)
        [self.navigationController pushViewController:controller animated:YES];
      break;
    }
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // _translateDetailItem is lazily initialized when -translateItem is called.
  // TODO(crbug.com/1008433): If kLanguageSettings feature is enabled,
  // -translateItem is not called, leaving _translateDetailItem uninitialized.
  // This logic can be simplified when kLanguageSettings feature flag is
  // removed (when feature is fully enabled).
  if (_translateDetailItem && preferenceName == prefs::kOfferTranslateEnabled) {
    BOOL enabled = browserState_->GetPrefs()->GetBoolean(preferenceName);
    NSString* subtitle = enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                 : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _translateDetailItem.detailText = subtitle;
    [self reconfigureCellsForItems:@[ _translateDetailItem ]];
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _disablePopupsSetting);

  NSString* subtitle = [_disablePopupsSetting value]
                           ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                           : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  // Update the item.
  _blockPopupsDetailItem.detailText = subtitle;

  // Update the cell.
  [self reconfigureCellsForItems:@[ _blockPopupsDetailItem ]];
}

@end
