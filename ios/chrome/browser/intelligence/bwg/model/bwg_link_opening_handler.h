// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_delegate.h"

class UrlLoadingBrowserAgent;

// The handler for opening links for BWG.
@interface BWGLinkOpeningHandler : NSObject <BWGLinkOpeningDelegate>

// Initialize the handler with a URL loading browser agent.
- (instancetype)initWithURLLoader:(UrlLoadingBrowserAgent*)URLLoadingAgent
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_HANDLER_H_
