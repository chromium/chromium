// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_CUSTOM_SYMBOL_H_
#define IOS_CHROME_BROWSER_UI_ICONS_CUSTOM_SYMBOL_H_

#import <UIKit/UIKit.h>

// Returns an UIView used in table views as a leading image icon.
// It contains a symbol with an elevated effect over the given
// `backgroundColor`.
UIView* ElevatedTableViewSymbolWithBackground(UIImage* symbol,
                                              UIColor* background_color);

#endif  // IOS_CHROME_BROWSER_UI_ICONS_CUSTOM_SYMBOL_H_
