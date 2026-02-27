// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_MUTATOR_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_MUTATOR_H_

#import <UIKit/UIKit.h>

// The mutator protocol for the AppBar.
@protocol AppBarMutator <NSObject>

// Creates a new tab for the current mode.
- (void)createNewTabFromView:(UIView*)sender;

// Creates a new tab group for the current mode.
- (void)createNewTabGroupFromView:(UIView*)sender;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_MUTATOR_H_
