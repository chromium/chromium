// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AGENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AGENT_DELEGATE_H_

#import "components/autofill/ios/browser/autofill_agent.h"

@protocol AtMemoryCommands;
@protocol SnackbarCommands;

// Implementation of AutofillAgentDelegate protocol.
@interface AutofillAgentDelegate : NSObject <AutofillAgentDelegate>

// The designated initializer. `snackbarHandler` is used to display snackbars
// triggered by the Autofill flow. `atMemoryCommands` is used to show the
// AtMemory UI when the user interacts with the AtMemory button, but can be nil
// if the feature flag is disabled.
- (instancetype)initWithSnackbarHandler:(id<SnackbarCommands>)snackbarHandler
                        atMemoryHandler:(id<AtMemoryCommands>)atMemoryCommands
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AGENT_DELEGATE_H_
