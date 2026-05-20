// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AI_BASE_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AI_BASE_MUTATOR_H_

#import <Foundation/Foundation.h>

@class TableViewItem;

namespace autofill {
class EntityType;
}  // namespace autofill

// Mutator for actions in the Autofill AI base view.
@protocol AutofillAIBaseMutator <NSObject>

// Notifies the mutator that the user selected an existing entity `item`.
- (void)didSelectEntityItem:(TableViewItem*)item;

// Notifies the mutator that the user selected to add a new entity of the
// specified `type`.
- (void)didSelectAddEntityWithType:(autofill::EntityType)type;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AI_BASE_MUTATOR_H_
