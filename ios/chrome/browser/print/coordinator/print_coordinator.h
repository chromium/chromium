// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRINT_COORDINATOR_PRINT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PRINT_COORDINATOR_PRINT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/web/model/print/print_handler.h"

// Interface for printing.
NS_SWIFT_UI_ACTOR
@interface PrintCoordinator : ChromeCoordinator<PrintHandler>

// Dismisses the print dialog with animation if `animated`.
- (void)dismissAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_PRINT_COORDINATOR_PRINT_COORDINATOR_H_
