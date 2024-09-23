// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_BRIDGE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mutator.h"

class CardUnmaskAuthenticationSelectionMutatorBridgeTarget;

// This class implements the Objective-C protocol and forwards the messages to
// the C++ abstract class.
@interface CardUnmaskAuthenticationSelectionMutatorBridge
    : NSObject <CardUnmaskAuthenticationSelectionMutator>

// Create the bridge given the C++ target.
- (instancetype)initWithTarget:
    (base::WeakPtr<CardUnmaskAuthenticationSelectionMutatorBridgeTarget>)target;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MUTATOR_BRIDGE_H_
