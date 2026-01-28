// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_LOGGER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_LOGGER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/composebox/debugger/composebox_debugger_event.h"

// Interface for logging composebox related events.
@protocol ComposeboxDebuggerLogger <NSObject>

// Logs the given event.
- (void)logEvent:(ComposeboxDebuggerEvent*)event;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_DEBUGGER_COMPOSEBOX_DEBUGGER_LOGGER_H_
