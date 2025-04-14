// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_
#define IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/explain_with_gemini/coordinator/explain_with_gemini_delegate.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

@protocol ApplicationCommands;

class WebStateList;

// Mediator that mediates between the browser container views and Explain
// Gemini.
@interface ExplainWithGeminiMediator : NSObject <ExplainWithGeminiDelegate>

// The handler for ApplicationCommands commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandHandler;

// Initializer for a mediator. `webStateList` is the WebStateList for the
// BrowserContainer that owns this mediator.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                     identityManager:(signin::IdentityManager*)identityManager
                         authService:(AuthenticationService*)authService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_
