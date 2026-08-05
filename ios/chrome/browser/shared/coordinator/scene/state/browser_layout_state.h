// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_BROWSER_LAYOUT_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_BROWSER_LAYOUT_STATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_passkey.h"

@class BrowserLayoutState;

// The position of the main toolbar (omnibox).
enum class ToolbarPosition {
  kTop,
  kBottom,
};

// Protocol for observers of the browser layout state.
@protocol BrowserLayoutStateObserver <NSObject>

@optional

// Called when the toolbar position changes for this browser.
- (void)browserLayoutState:(BrowserLayoutState*)layoutState
    didChangeToolbarPosition:(ToolbarPosition)toolbarPosition;

@end

// Object containing layout state specific to a single Browser instance.
@interface BrowserLayoutState : NSObject

// Position of the toolbar.
@property(nonatomic, readonly) ToolbarPosition toolbarPosition;

// Sets the position of the toolbar. Secured by passkey.
- (void)setToolbarPosition:(ToolbarPosition)toolbarPosition
                   passKey:(LayoutStateToolbarPassKey)passKey;

// Adds an observer to be notified of browser layout state changes.
- (void)addObserver:(id<BrowserLayoutStateObserver>)observer;

// Removes a previously added observer.
- (void)removeObserver:(id<BrowserLayoutStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_BROWSER_LAYOUT_STATE_H_
