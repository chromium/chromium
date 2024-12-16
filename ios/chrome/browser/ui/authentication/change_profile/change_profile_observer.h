// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_OBSERVER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/change_profile_commands.h"

@protocol ChangeProfileContinuation;

// This object is retained until -[<ChangeProfileObserving> operationFailed:] or
// -[<ChangeProfileObserving> operationDidComplete:withSceneState:] is called.
@interface ChangeProfileObserver : NSObject <ChangeProfileObserving>

- (instancetype)initWithContinuations:
    (NSArray<id<ChangeProfileContinuation>>*)continuations;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_OBSERVER_H_
