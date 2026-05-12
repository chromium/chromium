// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AI_BASE_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AI_BASE_MUTATOR_H_

#import <Foundation/Foundation.h>

@class TableViewItem;

// Mutator for actions in the Autofill AI base view.
@protocol AutofillAIBaseMutator <NSObject>

- (void)didSelectEntityItem:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AI_BASE_MUTATOR_H_
