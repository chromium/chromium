// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/compose_email_handler_collection_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/web/mailto_handler.h"
#import "ios/chrome/browser/web/mailto_handler_manager.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MDCPalettes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMailtoHandlers = kSectionIdentifierEnumZero,
  SectionIdentifierAlwaysAsk,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMailtoHandlers = kItemTypeEnumZero,
  ItemTypeAlwaysAskSwitch,
};

}  // namespace

@interface ComposeEmailHandlerCollectionViewController () {
  // Object that manages a set of MailtoHandler objects which can handle
  // mailto:// URLs.
  MailtoHandlerManager* _manager;
  // When this switch is ON, the user wants to be prompted for which Mail
  // client app to use, so the list of available Mail client apps should be
  // disabled (grayed out).
  LegacySettingsSwitchItem* _alwaysAskItem;
}

// Returns the MailtoHandler at |indexPath|. Returns nil if |indexPath| falls
// in a different section or outside of the range of available mail client apps.
- (MailtoHandler*)handlerAtIndexPath:(NSIndexPath*)indexPath;

// Callback function when the value of UISwitch |sender| in
// ItemTypeAlwaysAskSwitch item is changed by the user.
- (void)didToggleAlwaysAskSwitch:(id)sender;

// Sets the text display state for |item| representing the mail client
// |handler|. Sets the checkmark if |isSelected| is true.
- (void)setTextItemState:(SettingsTextItem*)item
                 handler:(MailtoHandler*)handler
                selected:(BOOL)isSelected;
@end

@implementation ComposeEmailHandlerCollectionViewController

- (instancetype)initWithManager:(MailtoHandlerManager*)manager {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _manager = manager;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_COMPOSE_EMAIL_SETTING);
  self.collectionViewAccessibilityIdentifier =
      @"compose_email_handler_view_controller";
  // TODO(crbug.com/764578): -loadModel should not be called from
  // initializer. A possible fix is to move this call to -viewDidLoad.
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  // Populates the first section with all the available Mail client apps.
  [model addSectionWithIdentifier:SectionIdentifierMailtoHandlers];

  // Finds the default Mail client app.
  NSArray<MailtoHandler*>* handlers = [_manager defaultHandlers];
  NSString* currentHandlerID = [_manager defaultHandlerID];

  // Populates the toggle "Always Ask" toggle switch row first because the
  // state of of the Mail client apps selection list is dependent on the value
  // of the toggle switch. The second section is the toggle switch to always
  // prompt for selection of Mail client app.
  [model addSectionWithIdentifier:SectionIdentifierAlwaysAsk];
  _alwaysAskItem =
      [[LegacySettingsSwitchItem alloc] initWithType:ItemTypeAlwaysAskSwitch];
  _alwaysAskItem.text = l10n_util::GetNSString(IDS_IOS_CHOOSE_EMAIL_ASK_TOGGLE);
  _alwaysAskItem.on = currentHandlerID == nil;
  [model addItem:_alwaysAskItem
      toSectionWithIdentifier:SectionIdentifierAlwaysAsk];

  // Lists all the Mail client apps known.
  for (MailtoHandler* handler in handlers) {
    SettingsTextItem* item =
        [[SettingsTextItem alloc] initWithType:ItemTypeMailtoHandlers];
    [item setText:[handler appName]];
    BOOL isSelected = [currentHandlerID isEqualToString:[handler appStoreID]];
    [self setTextItemState:item handler:handler selected:isSelected];
    [model addItem:item
        toSectionWithIdentifier:SectionIdentifierMailtoHandlers];
  }
}

#pragma mark - UICollectionViewDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  // Superclass implementation must be called. See NS_REQUIRES_SUPER annotation
  // in MDCCollectionViewController.h.
  BOOL result = [super collectionView:collectionView
          shouldSelectItemAtIndexPath:indexPath];
  // Disallows selection if the handler for the tapped row is not available.
  return !_alwaysAskItem.isOn &&
         [[self handlerAtIndexPath:indexPath] isAvailable] && result;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  CollectionViewModel* model = self.collectionViewModel;

  SettingsTextItem* selectedItem = base::mac::ObjCCastStrict<SettingsTextItem>(
      [model itemAtIndexPath:indexPath]);
  // Selection in rows other than mailto handlers should be prevented, so
  // DCHECK here is correct.
  DCHECK_EQ(ItemTypeMailtoHandlers, selectedItem.type);

  // Do nothing if the tapped row is already chosen as the default.
  if (selectedItem.accessoryType == MDCCollectionViewCellAccessoryCheckmark)
    return;

  // Iterate through the rows and remove the checkmark from any that has it.
  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (id item in
       [model itemsInSectionWithIdentifier:SectionIdentifierMailtoHandlers]) {
    SettingsTextItem* textItem =
        base::mac::ObjCCastStrict<SettingsTextItem>(item);
    DCHECK_EQ(ItemTypeMailtoHandlers, textItem.type);
    if (textItem == selectedItem) {
      // Shows the checkmark on the new default mailto: URL handler.
      textItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
      [modifiedItems addObject:textItem];
    } else if (textItem.accessoryType ==
               MDCCollectionViewCellAccessoryCheckmark) {
      // Unchecks any currently checked selection.
      textItem.accessoryType = MDCCollectionViewCellAccessoryNone;
      [modifiedItems addObject:textItem];
    }
  }

  // Sets the Mail client app that will handle mailto:// URLs.
  MailtoHandler* handler = [self handlerAtIndexPath:indexPath];
  DCHECK([handler isAvailable]);
  [_manager setDefaultHandlerID:[handler appStoreID]];

  [self reconfigureCellsForItems:modifiedItems];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeAlwaysAskSwitch) {
    LegacySettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(didToggleAlwaysAskSwitch:)
                    forControlEvents:UIControlEventValueChanged];
  }

  return cell;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  // Disallow highlight (ripple effect) if the handler for the tapped row is not
  // an available mailto:// handler.
  return _alwaysAskItem.isOn ||
         ![[self handlerAtIndexPath:indexPath] isAvailable];
}

#pragma mark - Private

- (void)didToggleAlwaysAskSwitch:(id)sender {
  BOOL isOn = [sender isOn];
  [_alwaysAskItem setOn:isOn];
  [_manager setDefaultHandlerID:nil];

  // Clear all sections by iterating through the rows. Text color of each
  // row is changed to reflect whether selection is enabled.
  NSMutableArray* modifiedItems = [NSMutableArray array];
  NSArray<CollectionViewItem*>* itemsInSection = [self.collectionViewModel
      itemsInSectionWithIdentifier:SectionIdentifierMailtoHandlers];
  NSArray<MailtoHandler*>* handlers = [_manager defaultHandlers];
  for (NSUInteger index = 0; index < [itemsInSection count]; ++index) {
    SettingsTextItem* textItem =
        base::mac::ObjCCastStrict<SettingsTextItem>(itemsInSection[index]);
    // Nothing is selected after this clear all operation.
    [self setTextItemState:textItem handler:handlers[index] selected:NO];
    [modifiedItems addObject:textItem];
  }
  [self reconfigureCellsForItems:modifiedItems];
}

- (void)setTextItemState:(SettingsTextItem*)item
                 handler:(MailtoHandler*)handler
                selected:(BOOL)isSelected {
  DCHECK_EQ(ItemTypeMailtoHandlers, item.type);
  if (_alwaysAskItem.isOn || ![handler isAvailable]) {
    [item setTextColor:[[MDCPalette greyPalette] tint500]];
    [item setAccessibilityTraits:UIAccessibilityTraitNotEnabled];
  } else {
    [item setTextColor:[[MDCPalette greyPalette] tint900]];
    [item setAccessibilityTraits:UIAccessibilityTraitButton];
  }
  item.accessoryType = isSelected ? MDCCollectionViewCellAccessoryCheckmark
                                  : MDCCollectionViewCellAccessoryNone;
}

- (MailtoHandler*)handlerAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewModel* model = self.collectionViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  if (itemType != ItemTypeMailtoHandlers)
    return nil;
  NSUInteger handlerIndex = [model indexInItemTypeForIndexPath:indexPath];
  return [[_manager defaultHandlers] objectAtIndex:handlerIndex];
}

@end
