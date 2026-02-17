// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_delegate.h"

class UrlLoadingBrowserAgent;

@class CommandDispatcher;
@protocol GeminiViewStateDelegate;

// The handler for opening links for BWG.
@interface BWGLinkOpeningHandler : NSObject <BWGLinkOpeningDelegate>

// Delegate for view state changes.
@property(nonatomic, weak) id<GeminiViewStateDelegate> geminiViewStateDelegate;

// Initialize the handler with a URL loading browser agent.
// In order to prevent a crash, we pass the 'CommandDispatcher' directly instead
// of a command handler. This is because command handlers fail the protocol
// conformance test at startup time, which is when this initializer get called.
- (instancetype)initWithURLLoader:(UrlLoadingBrowserAgent*)URLLoadingAgent
                       dispatcher:(CommandDispatcher*)dispatcher
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_HANDLER_H_
