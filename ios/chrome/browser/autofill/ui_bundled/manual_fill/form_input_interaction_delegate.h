// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FORM_INPUT_INTERACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FORM_INPUT_INTERACTION_DELEGATE_H_

#import "components/autofill/core/browser/filling_product.h"

// Delegate informing the manual fallback classes about any user interactions
// with the form.
@protocol FormInputInteractionDelegate

// Indicates that the focus has been changed to a field with the filling product
// to `fillingProduct`.
- (void)focusDidChangedWithFillingProduct:
    (autofill::FillingProduct)fillingProduct;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FORM_INPUT_INTERACTION_DELEGATE_H_
