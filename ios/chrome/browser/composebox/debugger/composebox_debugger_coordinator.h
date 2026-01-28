// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_COORDINATOR_H_

#import "ios/chrome/browser/composebox/debugger/composebox_debugger_logger.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ComposeboxDebuggerCoordinatorDelegate <NSObject>

- (void)composeboxDebuggerDidRequestOmniboxDebugging;

@end

@interface ComposeboxDebuggerCoordinator
    : ChromeCoordinator <ComposeboxDebuggerLogger>

@property(nonatomic, weak) id<ComposeboxDebuggerCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_COORDINATOR_H_
