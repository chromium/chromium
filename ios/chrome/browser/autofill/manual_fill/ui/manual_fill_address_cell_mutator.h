// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ADDRESS_CELL_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ADDRESS_CELL_MUTATOR_H_

#import <Foundation/Foundation.h>

@class ManualFillAddress;

// Address field types that can be selected in `ManualFillAddressCell`.
enum class ManualFillAddressFieldType {
  kFirstName,
  kMiddleName,
  kLastName,
  kCompany,
  kLine1,
  kLine2,
  kZip,
  kCity,
  kState,
  kCountry,
  kPhoneNumber,
  kEmailAddress,
};

// Mutator for `ManualFillAddressCell` user actions.
@protocol ManualFillAddressCellMutator <NSObject>

// Notifies the mutator that the user tapped a field chip of `fieldType` for
// `address`.
- (void)didTapChipForAddress:(ManualFillAddress*)address
                   fieldType:(ManualFillAddressFieldType)fieldType;

// Notifies the mutator that the user tapped the "Autofill Form" button for
// `address` at `cellIndex`.
- (void)didTapAutofillFormButtonForAddress:(ManualFillAddress*)address
                                 cellIndex:(NSInteger)cellIndex;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ADDRESS_CELL_MUTATOR_H_
