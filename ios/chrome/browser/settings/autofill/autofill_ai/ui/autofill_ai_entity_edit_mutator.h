// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_MUTATOR_H_

#import <UIKit/UIKit.h>

@class AutofillAIEntityEditDateItem;

// Mutator for Autofill AI entities.
@protocol AutofillAIEntityEditMutator

// Saves the entity instance that is being edited.
- (void)saveEntityInstance;

// Notifies the mutator that the date for `item` has changed to `date`.
- (void)didChangeDate:(NSDate*)date forItem:(AutofillAIEntityEditDateItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_MUTATOR_H_
