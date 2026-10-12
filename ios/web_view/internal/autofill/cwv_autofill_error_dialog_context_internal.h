// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_ERROR_DIALOG_CONTEXT_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_ERROR_DIALOG_CONTEXT_INTERNAL_H_

#import "ios/web_view/public/cwv_autofill_error_dialog_context.h"

namespace autofill {
struct AutofillErrorDialogContext;
}  // namespace autofill

NS_ASSUME_NONNULL_BEGIN

@interface CWVAutofillErrorDialogContext (Internal)

// Initializes the Objective-C wrapper from the C++ struct.
- (instancetype)initWithContext:
    (const autofill::AutofillErrorDialogContext&)context;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_ERROR_DIALOG_CONTEXT_INTERNAL_H_
