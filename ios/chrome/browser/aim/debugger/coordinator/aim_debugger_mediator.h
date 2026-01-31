// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_DEBUGGER_COORDINATOR_AIM_DEBUGGER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AIM_DEBUGGER_COORDINATOR_AIM_DEBUGGER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/aim/debugger/ui/aim_debugger_mutator.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"

class AimEligibilityService;
@protocol AimDebuggerConsumer;
class PrefService;

// Mediator for AIM functionality.
@interface AimDebuggerMediator : NSObject <AimDebuggerMutator>

@property(nonatomic, weak) id<AimDebuggerConsumer> consumer;
@property(nonatomic, weak) id<SnackbarCommands> snackbarHandler;

- (instancetype)initWithService:(AimEligibilityService*)service
                    prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects from the service and cleans up observers.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AIM_DEBUGGER_COORDINATOR_AIM_DEBUGGER_MEDIATOR_H_
