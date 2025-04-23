// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_STOP_ANIMATED_CHROME_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_STOP_ANIMATED_CHROME_COORDINATOR_H_

#import <Foundation/Foundation.h>

// Interface for a ChromeCoordinator that can be stopped with or without
// animated. It is expected that the `stop` method of classes implementing this
// protocol is defined as `[self stopAnimated:NO]`.
@protocol StopAnimatedChromeCoordinator <NSObject>

// Same as `ChromeCoordinator`’s stop, but can be animated or not. Must call
// `[super stop]`.
- (void)stopAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_STOP_ANIMATED_CHROME_COORDINATOR_H_
