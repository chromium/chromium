// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_LEGACY_TOOLBAR_UI_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_LEGACY_TOOLBAR_UI_UPDATER_H_

#import <UIKit/UIKit.h>

@class ToolbarUIState;
class WebStateList;

@protocol ToolbarHeightProviderForFullscreen
// The minimum and maximum amount by which the top toolbar overlaps the browser
// content area.
- (CGFloat)collapsedTopToolbarHeight;
- (CGFloat)expandedTopToolbarHeight;
// Height of the bottom toolbar.
- (CGFloat)bottomToolbarHeight;
@end

// Helper object that uses navigation events to update a ToolbarUIState.
@interface LegacyToolbarUIUpdater : NSObject

// The toolbar UI being updated by this object.
@property(nonatomic, strong, readonly, nonnull) ToolbarUIState* toolbarUI;

// Designated initializer that uses navigation events from |webStateList| and
// the height provided by |owner| to update |state|'s broadcast value.
- (nullable instancetype)
initWithToolbarUI:(nonnull ToolbarUIState*)toolbarUI
     toolbarOwner:(nonnull id<ToolbarHeightProviderForFullscreen>)owner
     webStateList:(nonnull WebStateList*)webStateList NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// Starts updating |state|.
- (void)startUpdating;

// Stops updating |state|.
- (void)stopUpdating;

// Forces an update of |state|.
- (void)updateState;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_LEGACY_TOOLBAR_UI_UPDATER_H_
