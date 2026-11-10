// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_PASSWORD_CELL_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_PASSWORD_CELL_MUTATOR_H_

#import <Foundation/Foundation.h>

@class ManualFillCredential;

// Mutator protocol for handling user actions in `ManualFillPasswordCell`.
@protocol ManualFillPasswordCellMutator <NSObject>

// Called when the user taps the username chip for `credential`.
- (void)didTapUsernameChipForCredential:(ManualFillCredential*)credential;

// Called when the user taps the password chip for `credential`.
- (void)didTapPasswordChipForCredential:(ManualFillCredential*)credential;

// Called when the user taps the "Autofill form" button for `credential` at
// `cellIndex` in the displayed list.
- (void)didTapAutofillFormButtonForCredential:(ManualFillCredential*)credential
                                    cellIndex:(NSInteger)cellIndex;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_PASSWORD_CELL_MUTATOR_H_
