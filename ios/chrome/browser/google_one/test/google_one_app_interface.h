// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_ONE_TEST_GOOGLE_ONE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_GOOGLE_ONE_TEST_GOOGLE_ONE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@protocol GoogleOneControllerFactory;

// EG test app interface managing the Google One feature.
@interface GoogleOneAppInterface : NSObject

// Overrides the GoogleOneController factory to return a
// TestGoogleOneController.
+ (void)overrideGoogleOneController;

// Reset GoogleOneController to the default behavior.
+ (void)restoreGoogleOneController;

@end

#endif  // IOS_CHROME_BROWSER_GOOGLE_ONE_TEST_GOOGLE_ONE_APP_INTERFACE_H_
