// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_OPTION_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_OPTION_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

// A custom view Button that displays an address bar preference option.
// It displays a label, an image and a checkbox aligned vertically.
@interface AddressBarOptionView : UIButton

// Initialize the view with a symbol name for the image and a label text for the
// label.
- (instancetype)initWithSymbolName:(NSString*)symbolName
                         labelText:(NSString*)labelText;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_OPTION_ITEM_VIEW_H_
