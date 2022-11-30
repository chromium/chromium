// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_FORM_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_FORM_INTERNAL_H_

#import "ios/web_view/public/cwv_autofill_form.h"

NS_ASSUME_NONNULL_BEGIN

namespace autofill {
class FormStructure;
}

@interface CWVAutofillForm ()

// |formStructure| is only used to populate properties of this class and will
// not be used by this class past initialization.
- (instancetype)initWithFormStructure:
    (const autofill::FormStructure&)formStructure NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_FORM_INTERNAL_H_
