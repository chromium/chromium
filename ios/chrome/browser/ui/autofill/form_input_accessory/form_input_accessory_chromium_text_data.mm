// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_chromium_text_data.h"

#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FormInputAccessoryViewTextData* ChromiumAccessoryViewTextData() {
  return [[FormInputAccessoryViewTextData alloc]
              initWithCloseButtonTitle:l10n_util::GetNSString(
                                           IDS_IOS_AUTOFILL_INPUT_BAR_DONE)
         closeButtonAccessibilityLabel:
             l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD)
          nextButtonAccessibilityLabel:l10n_util::GetNSString(
                                           IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD)
      previousButtonAccessibilityLabel:
          l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD)];
}
