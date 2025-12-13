// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PUBLIC_TOOLBAR_HEIGHT_DELEGATE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PUBLIC_TOOLBAR_HEIGHT_DELEGATE_H_

#import <Foundation/Foundation.h>

enum class ToolbarType;

@protocol ToolbarHeightDelegate <NSObject>

/// Primary and secondary toolbars collapsed and expanded size have changed.
/// This is NOT used for fullscreen.
- (void)toolbarsHeightChanged;

/// Layout toolbar height change.
- (void)layoutToolbarHeightChangeWithAnimation:(BOOL)animated;

/// Secondary toolbar is moving above the keyboard, adjust the constraints to
/// allow this.
- (void)secondaryToolbarMovedAboveKeyboard;

/// Secondary toolbar is removed from the keyboard, reset to default
/// constraints.
- (void)secondaryToolbarRemovedFromKeyboard;

/// Adjust the secondary toolbar when the keyboard is shown.
- (void)adjustSecondaryToolbarForKeyboardHeight:(CGFloat)keyboardHeight
                                    isCollapsed:(BOOL)isCollapsed
                                       duration:(NSTimeInterval)duration
                                          curve:(UIViewAnimationCurve)curve;

/// Called when the toolbar type changed.
/// TODO(crbug.com/429955447): Remove when diamond prototype is cleaned.
- (void)diamondToolbarTypeChanged:(ToolbarType)type;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PUBLIC_TOOLBAR_HEIGHT_DELEGATE_H_
