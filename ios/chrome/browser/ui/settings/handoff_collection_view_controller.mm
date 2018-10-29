// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/handoff_collection_view_controller.h"

#import "base/mac/foundation_util.h"
#include "components/handoff/pref_names_ios.h"
#include "components/prefs/pref_member.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierFooter,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface HandoffCollectionViewController () {
  // Pref for whether Handoff is enabled.
  BooleanPrefMember _handoffEnabled;
}

- (void)switchChanged:(UISwitch*)switchView;

@end

@implementation HandoffCollectionViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_OPTIONS_CONTINUITY_LABEL);
    _handoffEnabled.Init(prefs::kIosHandoffToOtherDevices,
                         browserState->GetPrefs());
    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitch];
  LegacySettingsSwitchItem* switchItem =
      [[LegacySettingsSwitchItem alloc] initWithType:ItemTypeSwitch];
  switchItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES);
  switchItem.on = _handoffEnabled.GetValue();
  [model addItem:switchItem toSectionWithIdentifier:SectionIdentifierSwitch];

  // The footer item must currently go into a separate section, to work around a
  // drawing bug in MDC.
  // TODO(crbug.com/650424) Use setFooter:forSectionWithIdentifier:.
  [model addSectionWithIdentifier:SectionIdentifierFooter];
  CollectionViewFooterItem* footer =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  footer.cellStyle = CollectionViewCellStyle::kUIKit;
  footer.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES_DETAILS);
  [model addItem:footer toSectionWithIdentifier:SectionIdentifierFooter];
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.collectionViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypeSwitch) {
    LegacySettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if (item.type == ItemTypeFooter)
    return [MDCCollectionViewCell
        cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                           forItem:item];
  return MDCCellDefaultOneLineHeight;
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierFooter:
      // Display the Learn More footer in the default style with no "card" UI
      // and no section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierFooter:
      // Display the Learn More footer without any background image or
      // shadowing.
      return YES;
    default:
      return NO;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeSwitch:
      return YES;
    default:
      return NO;
  }
}

#pragma mark - Private

- (void)switchChanged:(UISwitch*)switchView {
  _handoffEnabled.SetValue(switchView.isOn);
}

@end
