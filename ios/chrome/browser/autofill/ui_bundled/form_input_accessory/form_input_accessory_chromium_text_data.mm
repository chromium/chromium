// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_chromium_text_data.h"

#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

FormInputAccessoryViewTextData* ChromiumAccessoryViewTextData() {
  return [[FormInputAccessoryViewTextData alloc]
                          initWithCloseButtonTitle:
                              GetNSString(IDS_IOS_AUTOFILL_INPUT_BAR_DONE)
                     closeButtonAccessibilityLabel:
                         GetNSString(IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD)
                      nextButtonAccessibilityLabel:
                          GetNSString(IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD)
                  previousButtonAccessibilityLabel:
                      GetNSString(IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD)
                manualFillButtonAccessibilityLabel:
                    GetNSString(IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA)
        passwordManualFillButtonAccessibilityLabel:
            GetNSString(IDS_IOS_AUTOFILL_PASSWORD_AUTOFILL_DATA)
      creditCardManualFillButtonAccessibilityLabel:
          GetNSString(IDS_IOS_AUTOFILL_CREDIT_CARD_AUTOFILL_DATA)
         addressManualFillButtonAccessibilityLabel:
             GetNSString(IDS_IOS_AUTOFILL_ADDRESS_AUTOFILL_DATA)];
}
