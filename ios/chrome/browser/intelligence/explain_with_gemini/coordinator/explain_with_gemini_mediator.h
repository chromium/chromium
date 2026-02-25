// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_content/model/edit_menu_builder.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

@protocol SceneCommands;

// Mediator that mediates between the browser container views and Explain
// Gemini.
@interface ExplainWithGeminiMediator : NSObject <EditMenuBuilder>

// The handler for SceneCommands commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// The handler for Gemini commands.
@property(nonatomic, weak) id<BWGCommands> BWGHandler;

// Initializer for a mediator.
- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                            authService:(AuthenticationService*)authService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_MEDIATOR_H_
