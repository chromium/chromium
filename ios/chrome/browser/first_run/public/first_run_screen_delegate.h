// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FIRST_RUN_SCREEN_DELEGATE_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FIRST_RUN_SCREEN_DELEGATE_H_

#import <UIKit/UIKit.h>

@class ChromeCoordinator;

// The delegate for transferring between screens.
@protocol FirstRunScreenDelegate <NSObject>

// Called by `coordinator` when it has finished presenting its view and requires
// its delegate to stop it synchronously.
- (void)firstRunScreenCoordinatorWantsToBeStopped:
    (ChromeCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FIRST_RUN_SCREEN_DELEGATE_H_
