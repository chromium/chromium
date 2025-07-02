// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_delegate.h"

class WebStateList;

// Handler for the BWG sessions.
@interface BWGSessionHandler : NSObject <BWGSessionDelegate>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_HANDLER_H_
