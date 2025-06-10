// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_MUTATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_MUTATOR_H_

#import <UIKit/UIKit.h>

@protocol OmniboxDebuggerMutator <NSObject>

/// Hardcode suggest response with `response`.
- (void)hardcodeSuggestResponse:(NSString*)response;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_MUTATOR_H_
