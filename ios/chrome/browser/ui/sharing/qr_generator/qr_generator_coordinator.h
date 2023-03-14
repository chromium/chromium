// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_QR_GENERATOR_QR_GENERATOR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SHARING_QR_GENERATOR_QR_GENERATOR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#include "url/gurl.h"

@protocol QRGenerationCommands;

// QRGeneratorCoordinator presents the public interface for the QR code
// generation feature.
@interface QRGeneratorCoordinator : ChromeCoordinator

// Initializes an instance with a base `viewController`, the current `browser`,
// the `title` and `URL` of a webpage to generate a QR code for, and a `handler`
// to handle commands execution.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                       URL:(const GURL&)URL
                                   handler:(id<QRGenerationCommands>)handler
    NS_DESIGNATED_INITIALIZER;

// Unavailable, use -initWithBaseViewController:browser:title:URL:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_QR_GENERATOR_QR_GENERATOR_COORDINATOR_H_
