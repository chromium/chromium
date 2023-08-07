// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/address_bar_preference/address_bar_preference_view_controller.h"

#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_options_item.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeOptionsContent = kItemTypeEnumZero,
};

}  // namespace

@implementation AddressBarPreferenceViewController {
  BOOL _bottomOmniboxPreference;
  AddressBarOptionsItem* _addressBarOptionsItem;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Address Bar";
  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierContent];

  _addressBarOptionsItem =
      [[AddressBarOptionsItem alloc] initWithType:ItemTypeOptionsContent];

  _addressBarOptionsItem.bottomAddressBarOptionSelected =
      _bottomOmniboxPreference;
  _addressBarOptionsItem.addressBarpreferenceServiceDelegate =
      _prefServiceDelegate;

  [self.tableViewModel addItem:_addressBarOptionsItem
       toSectionWithIdentifier:SectionIdentifierContent];

  TableViewLinkHeaderFooterItem* addressBarTip =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeOptionsContent];
  addressBarTip.text =
      @"You can also touch & hold the address bar to change the position.";
  [self.tableViewModel setFooter:addressBarTip
        forSectionWithIdentifier:SectionIdentifierContent];
}

#pragma mark - AddressBarPreferenceConsumer

- (void)setPreferenceForOmniboxAtBottom:(BOOL)omniboxAtBottom {
  _bottomOmniboxPreference = omniboxAtBottom;
  if (_addressBarOptionsItem) {
    _addressBarOptionsItem.bottomAddressBarOptionSelected = omniboxAtBottom;
    [self reconfigureCellsForItems:@[ _addressBarOptionsItem ]];
  }
}

@end
