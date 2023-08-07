// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_OPTIONS_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_OPTIONS_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@protocol AddressBarPreferenceServiceDelegate;

// The address bar setting item.
@interface AddressBarOptionsItem : TableViewItem

// The preference service delegate.
@property(nonatomic, weak) id<AddressBarPreferenceServiceDelegate>
    addressBarpreferenceServiceDelegate;
// The bottom address bar option
@property(nonatomic, assign) BOOL bottomAddressBarOptionSelected;

@end

// Cell class associated to AddressBarOptionsItem.
// It presents a view with two custom buttons, each present an address bar
// preference option.
@interface AddressBarOptionsCell : TableViewCell

// The preference service delegate.
@property(nonatomic, weak) id<AddressBarPreferenceServiceDelegate>
    addressBarpreferenceServiceDelegate;
// If set to YES ,bottom address bar option is selected, otherwise the top
// address bar is selected.
@property(nonatomic, assign) BOOL bottomAddressBarOptionSelected;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_OPTIONS_ITEM_H_
