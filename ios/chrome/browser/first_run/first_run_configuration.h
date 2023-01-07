// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/first_run/first_run_metrics.h"

// This class holds the state of the first run flow.
@interface FirstRunConfiguration : NSObject

@property(nonatomic, assign) first_run::SignInAttemptStatus signInAttemptStatus;
@property(nonatomic, assign) BOOL hasSSOAccount;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_CONFIGURATION_H_
