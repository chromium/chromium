// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_MANAGER_H_

#import <Foundation/Foundation.h>

@protocol UIBlockerTarget;

// Observer of UIBlockerManager.
@protocol UIBlockerManagerObserver <NSObject>

@optional

// Called when the current UI blocker releases the screen. It is possible that
// the screen is blocked again by the time this method is called.
- (void)currentUIBlockerRemoved;

@end

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

- (void)addUIBlockerManagerObserver:(id<UIBlockerManagerObserver>)observer;
- (void)removeUIBlockerManagerObserver:(id<UIBlockerManagerObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_MANAGER_H_
