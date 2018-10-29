// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings_collection_view_controller.h"

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
#include "ios/chrome/browser/mailto/features.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/block_popups_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_detail_item.h"
#import "ios/chrome/browser/ui/settings/compose_email_handler_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/translate_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"
#import "ios/chrome/browser/web/mailto_handler_manager.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
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

@interface ContentSettingsCollectionViewController ()<PrefObserverDelegate,
                                                      BooleanObserver> {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // The observable boolean that binds to the "Disable Popups" setting state.
  ContentSettingBackedBoolean* _disablePopupsSetting;

  // This object contains the list of available Mail client apps that can
  // handle mailto: URLs.
  MailtoHandlerManager* _mailtoHandlerManager;

  // Updatable Items
  LegacySettingsDetailItem* _blockPopupsDetailItem;
  LegacySettingsDetailItem* _translateDetailItem;
  LegacySettingsDetailItem* _composeEmailDetailItem;
}

// Returns the value for the default setting with ID |settingID|.
- (ContentSetting)getContentSetting:(ContentSettingsType)settingID;

// Helpers to create collection view items.
- (id)blockPopupsItem;
- (id)translateItem;
- (id)composeEmailItem;

@end

@implementation ContentSettingsCollectionViewController {
  ios::ChromeBrowserState* browserState_;  // weak
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
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
                             settingID:CONTENT_SETTINGS_TYPE_POPUPS
                              inverted:YES];
    [_disablePopupsSetting setObserver:self];

    if (!base::FeatureList::IsEnabled(kMailtoHandledWithGoogleUI)) {
      _mailtoHandlerManager =
          [MailtoHandlerManager mailtoHandlerManagerWithStandardHandlers];
      [_mailtoHandlerManager setObserver:self];
    }

    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

- (void)loadModel {
  [super loadModel];

  CollectionViewModel* model = self.collectionViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:[self blockPopupsItem]
      toSectionWithIdentifier:SectionIdentifierSettings];
  [model addItem:[self translateItem]
      toSectionWithIdentifier:SectionIdentifierSettings];
  // If Google mailto handling UI is available, display the relevant settings.
  if (base::FeatureList::IsEnabled(kMailtoHandledWithGoogleUI)) {
    MailtoHandlerProvider* provider =
        ios::GetChromeBrowserProvider()->GetMailtoHandlerProvider();
    NSString* settingsTitle = provider->MailtoHandlerSettingsTitle();
    if (settingsTitle) {
      [model addItem:[self composeEmailItem]
          toSectionWithIdentifier:SectionIdentifierSettings];
    }
  } else {
    [model addItem:[self composeEmailItem]
        toSectionWithIdentifier:SectionIdentifierSettings];
  }
}

- (CollectionViewItem*)blockPopupsItem {
  _blockPopupsDetailItem = [[LegacySettingsDetailItem alloc]
      initWithType:ItemTypeSettingsBlockPopups];
  NSString* subtitle = [_disablePopupsSetting value]
                           ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                           : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _blockPopupsDetailItem.text = l10n_util::GetNSString(IDS_IOS_BLOCK_POPUPS);
  _blockPopupsDetailItem.detailText = subtitle;
  _blockPopupsDetailItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  _blockPopupsDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return _blockPopupsDetailItem;
}

- (CollectionViewItem*)translateItem {
  _translateDetailItem =
      [[LegacySettingsDetailItem alloc] initWithType:ItemTypeSettingsTranslate];
  BOOL enabled =
      browserState_->GetPrefs()->GetBoolean(prefs::kOfferTranslateEnabled);
  NSString* subtitle = enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _translateDetailItem.text = l10n_util::GetNSString(IDS_IOS_TRANSLATE_SETTING);
  _translateDetailItem.detailText = subtitle;
  _translateDetailItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  _translateDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return _translateDetailItem;
}

- (CollectionViewItem*)composeEmailItem {
  _composeEmailDetailItem = [[LegacySettingsDetailItem alloc]
      initWithType:ItemTypeSettingsComposeEmail];
  if (base::FeatureList::IsEnabled(kMailtoHandledWithGoogleUI)) {
    // Use the handler's preferred title string for the compose email item.
    MailtoHandlerProvider* provider =
        ios::GetChromeBrowserProvider()->GetMailtoHandlerProvider();
    NSString* settingsTitle = provider->MailtoHandlerSettingsTitle();
    DCHECK([settingsTitle length]);
    _composeEmailDetailItem.text = settingsTitle;
  } else {
    // Use the default Chrome string when mailto handling with Google UI is not
    // available.
    _composeEmailDetailItem.text =
        l10n_util::GetNSString(IDS_IOS_COMPOSE_EMAIL_SETTING);
    // Displaying the selected app name is only supported in the Chrome
    // implementation of mailto content settings.
    // The Google UI version of mailto handling does not expose the name of the
    // user's preferred app.
    _composeEmailDetailItem.detailText =
        [_mailtoHandlerManager defaultHandlerName];
  }
  _composeEmailDetailItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  _composeEmailDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return _composeEmailDetailItem;
}

- (ContentSetting)getContentSetting:(ContentSettingsType)settingID {
  return ios::HostContentSettingsMapFactory::GetForBrowserState(browserState_)
      ->GetDefaultContentSetting(settingID, NULL);
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSettingsBlockPopups: {
      UIViewController* controller =
          [[BlockPopupsCollectionViewController alloc]
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
      if (base::FeatureList::IsEnabled(kMailtoHandledWithGoogleUI)) {
        MailtoHandlerProvider* provider =
            ios::GetChromeBrowserProvider()->GetMailtoHandlerProvider();
        UIViewController* controller =
            provider->MailtoHandlerSettingsController();
        if (controller) {
          [self.navigationController pushViewController:controller
                                               animated:YES];
        }
      } else {
        UIViewController* controller =
            [[ComposeEmailHandlerCollectionViewController alloc]
                initWithManager:_mailtoHandlerManager];
        [self.navigationController pushViewController:controller animated:YES];
      }
      break;
    }
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kOfferTranslateEnabled) {
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

#pragma mark - MailtoHandlerManagerObserver

- (void)handlerDidChangeForMailtoHandlerManager:(MailtoHandlerManager*)manager {
  if (manager != _mailtoHandlerManager)
    return;
  _composeEmailDetailItem.detailText = [manager defaultHandlerName];
  [self reconfigureCellsForItems:@[ _composeEmailDetailItem ]];
}

@end
