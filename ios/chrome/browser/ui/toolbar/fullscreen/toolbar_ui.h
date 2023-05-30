// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_TOOLBAR_UI_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_TOOLBAR_UI_H_

#import <UIKit/UIKit.h>

// Protocol for the UI displaying the toolbar.
@protocol ToolbarUI<NSObject>

// The minimum height of the toolbar relative to the browser content area.
// This should be broadcast using `-broadcastCollapsedTopToolbarHeight:`.
@property(nonatomic, readonly) CGFloat collapsedTopToolbarHeight;

// The minimum height of the toolbar relative to the browser content area.
// This should be broadcast using `-broadcastExpandedTopToolbarHeight:`.
@property(nonatomic, readonly) CGFloat expandedTopToolbarHeight;

// The height of the bottom toolbar relative to the browser content area.
// This should be broadcast using `-broadcastExpandedBottomToolbarHeight:`.
@property(nonatomic, readonly) CGFloat expandedBottomToolbarHeight;

@property(nonatomic, readonly) CGFloat collapsedBottomToolbarHeight;

@end

// Simple implementation of ToolbarUI that allows readwrite access to broadcast
// properties.
@interface ToolbarUIState : NSObject<ToolbarUI>

// Redefine properties as readwrite.
@property(nonatomic, assign) CGFloat collapsedTopToolbarHeight;
@property(nonatomic, assign) CGFloat expandedTopToolbarHeight;
@property(nonatomic, assign) CGFloat expandedBottomToolbarHeight;
@property(nonatomic, assign) CGFloat collapsedBottomToolbarHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_TOOLBAR_UI_H_
