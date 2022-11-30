// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_MENUS_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_MENUS_PROVIDER_H_

#import <UIKit/UIKit.h>

// The types of button for which a menu can be requested.
typedef NS_ENUM(NSUInteger, AdaptiveToolbarButtonType) {
  AdaptiveToolbarButtonTypeBack,
  AdaptiveToolbarButtonTypeForward,
  AdaptiveToolbarButtonTypeNewTab,
  AdaptiveToolbarButtonTypeTabGrid,
};

// Provider of menus for the toolbar.
@protocol AdaptiveToolbarMenusProvider

// Returns a menu for the button of type `buttonType`.
- (UIMenu*)menuForButtonOfType:(AdaptiveToolbarButtonType)buttonType;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_MENUS_PROVIDER_H_
