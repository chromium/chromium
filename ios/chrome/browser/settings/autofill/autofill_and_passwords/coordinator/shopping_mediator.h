// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_SHOPPING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_SHOPPING_MEDIATOR_H_

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_mutator.h"

@protocol ShoppingConsumer;

// Mediator for the Shopping settings page.
@interface ShoppingMediator : AutofillAIBaseMediator <ShoppingMutator>

// Consumer for this mediator.
@property(nonatomic, weak) id<ShoppingConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_SHOPPING_MEDIATOR_H_
