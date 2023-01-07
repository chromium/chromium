// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_LONG_PRESS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_LONG_PRESS_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate for the object creating popup menu with a long press gesture.
@protocol PopupMenuLongPressDelegate

// Notifies the popup menu that the focus `point` of the long press gesture has
// changed. `point` should be in window base coordinates.
- (void)longPressFocusPointChangedTo:(CGPoint)point;
// Notifies the popup menu that the long press gesture has ended at `point`.
// `point` should be in window base coordinates.
- (void)longPressEndedAtPoint:(CGPoint)point;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_LONG_PRESS_DELEGATE_H_
