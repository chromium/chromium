// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_DELEGATE_H_

#import <UIKit/UIKit.h>

// The delegate for transferring between screens.
@protocol FirstRunScreenDelegate <NSObject>

// Called when one screen finished presenting.
- (void)screenWillFinishPresenting;

// Called when user want to skip all screens after.
- (void)skipAllScreens;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_DELEGATE_H_
