// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_UI_CONTEXT_MENU_PRESENTER_H_
#define IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_UI_CONTEXT_MENU_PRESENTER_H_

#import <UIKit/UIKit.h>

// Presents a context menu at a specified location.
API_AVAILABLE(ios(17.4))
@interface ContextMenuPresenter : NSObject

@property(nonatomic, weak) id<UIContextMenuInteractionDelegate>
    contextMenuInteractionDelegate;
@property(nonatomic, strong, readonly)
    UIContextMenuInteraction* contextMenuInteraction;

- (instancetype)initWithRootView:(UIView*)rootView NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Presents a context menu at the specified location.
- (void)presentAtLocationInRootView:(CGPoint)locationInRootView;
// Dismisses the context menu.
- (void)dismiss;

@end

#endif  // IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_UI_CONTEXT_MENU_PRESENTER_H_
