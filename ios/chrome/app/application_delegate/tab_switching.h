// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_SWITCHING_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_SWITCHING_H_

#import <UIKit/UIKit.h>

// Handles opening tabs from the tab switcher.
@protocol TabSwitching<NSObject>

// Opens a new tab with animation if presenting the tab switcher.
// Returns whether it opened a new tab.
- (BOOL)openNewTabFromTabSwitcher;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_SWITCHING_H_
