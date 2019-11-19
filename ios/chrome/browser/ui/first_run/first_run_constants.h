// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace first_run {

// The accessibility identifier for the sign in button shown in first run.
extern NSString* const kSignInButtonAccessibilityIdentifier;
// The accessibility identifier for the skip sign in button shown in first run.
extern NSString* const kSignInSkipButtonAccessibilityIdentifier;
// The accessibility identifier for the UMA collection checkbox shown in first
// run.
extern NSString* const kUMAMetricsButtonAccessibilityIdentifier;

}  // first_run

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_
