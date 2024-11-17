// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_TARGET_H_
#define IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_TARGET_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

@protocol UIBlockerManager;
@class UIScene;

// Target to block all UI.
@protocol UIBlockerTarget <NSObject>

@property(nonatomic, readonly, getter=isUIBlocked) BOOL uiBlocked;

// Returns UI blocker manager.
- (id<UIBlockerManager>)uiBlockerManagerForExtent:(UIBlockerExtent)extent;

// Force the blocking UI to appear. Specifically, bring the blocking UI window
// forward.
- (void)bringBlockerToFront:(UIScene*)requestingScene;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_UI_BLOCKER_TARGET_H_
