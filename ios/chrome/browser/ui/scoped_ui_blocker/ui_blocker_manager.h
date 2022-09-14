// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_MANAGER_H_

#import <Foundation/Foundation.h>

@protocol UIBlockerTarget;

// Manager in charge to block and unblock all UI.
@protocol UIBlockerManager <NSObject>

// The current UI blocker, if any.
- (id<UIBlockerTarget>)currentUIBlocker;

// Call this when showing a new blocking UI in `target`.
// It is an error to call this for target A when target B is already showing one
// or more blocking UI.
// This method can be called multiple time with the same target, before calling
// `decrementBlockingUICounterForTarget:`.
- (void)incrementBlockingUICounterForTarget:(id<UIBlockerTarget>)target;
// Call this after dismissing a blocking UI.
// `target` has to be the same value when `incrementBlockingUICounterForTarget:`
// was called.
- (void)decrementBlockingUICounterForTarget:(id<UIBlockerTarget>)target;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_MANAGER_H_
