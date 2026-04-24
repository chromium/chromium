// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_MUTATOR_H_

#import <UIKit/UIKit.h>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/common/dense_set.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

@class AutofillAIEntityEditDateItem;

// Mutator for Autofill AI entities.
@protocol AutofillAIEntityEditMutator

// Saves the entity instance that is being edited.
- (void)saveEntityInstance;

// Notifies the mutator that the date for `item` has changed to `date`.
- (void)didChangeDate:(NSDate*)date forItem:(AutofillAIEntityEditDateItem*)item;

// Returns the list of fields that are missing based on the present attributes.
- (autofill::DenseSet<autofill::AttributeType>)getMissingImportConstraintsFor:
    (const autofill::DenseSet<autofill::AttributeType>&)presentAttributes;

// Requests authentication before entering edit mode.
// `completion` is called with the result of the authentication attempt.
- (void)requestEditingWithCompletion:(ReauthenticationResultBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_MUTATOR_H_
