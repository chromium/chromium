// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCOPED_IPHONE_PORTRAIT_ONLY_IPHONE_PORTRAIT_ONLY_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_SCOPED_IPHONE_PORTRAIT_ONLY_IPHONE_PORTRAIT_ONLY_MANAGER_H_

#import <Foundation/Foundation.h>

// Manager in charge to block and unblock to portrait mode only.
@protocol IphonePortraitOnlyManager <NSObject>

// Call this when the UI should be blocked in portrait mode.
- (void)incrementIphonePortraitOnlyCounter;
// Call this when the UI can be unblocked from portrait mode.
- (void)decrementIphonePortraitOnlyCounter;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCOPED_IPHONE_PORTRAIT_ONLY_IPHONE_PORTRAIT_ONLY_MANAGER_H_
