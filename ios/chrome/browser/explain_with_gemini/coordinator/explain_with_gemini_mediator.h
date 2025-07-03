// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_
#define IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_container/model/edit_menu_builder.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

@protocol ApplicationCommands;

// Mediator that mediates between the browser container views and Explain
// Gemini.
@interface ExplainWithGeminiMediator : NSObject <EditMenuBuilder>

// The handler for ApplicationCommands commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandHandler;

// Initializer for a mediator.
- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                            authService:(AuthenticationService*)authService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_
