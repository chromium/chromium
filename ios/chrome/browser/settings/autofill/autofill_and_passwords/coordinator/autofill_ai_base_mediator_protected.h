// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AI_BASE_MEDIATOR_PROTECTED_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AI_BASE_MEDIATOR_PROTECTED_H_

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/common/dense_set.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_ai_base_mediator.h"

@class TableViewItem;

// Protected methods for subclasses of AutofillAIBaseMediator.
@interface AutofillAIBaseMediator (Protected)

// Subclasses must override to define which entities to display.
- (autofill::DenseSet<autofill::EntityTypeName>)supportedEntityTypes;

// Subclasses must override to provide the items to the consumer.
- (void)pushItemsToConsumer:(NSArray<TableViewItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AI_BASE_MEDIATOR_PROTECTED_H_
