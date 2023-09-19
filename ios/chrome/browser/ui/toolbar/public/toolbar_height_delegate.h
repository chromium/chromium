// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_HEIGHT_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_HEIGHT_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol ToolbarHeightDelegate <NSObject>

/// Primary and secondary toolbars collapsed and expanded size have changed.
/// This is NOT used for fullscreen.
- (void)toolbarsHeightChanged;

/// Secondary toolbar is moving above the keyboard, adjust the constraints to
/// allow this.
- (void)secondaryToolbarMovedAboveKeyboard;

/// Secondary toolbar is removed from the keyboard, reset to default
/// constraints.
- (void)secondaryToolbarRemovedFromKeyboard;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_TOOLBAR_HEIGHT_DELEGATE_H_
