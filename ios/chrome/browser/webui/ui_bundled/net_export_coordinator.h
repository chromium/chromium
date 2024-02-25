// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_NET_EXPORT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_NET_EXPORT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ShowMailComposerContext;

// Coordinator for the Net export.
@interface NetExportCoordinator : ChromeCoordinator

// Creates a coordinator that uses `viewController`, `browser` and `context`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       mailComposerContext:(ShowMailComposerContext*)context;

@end

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_NET_EXPORT_COORDINATOR_H_
