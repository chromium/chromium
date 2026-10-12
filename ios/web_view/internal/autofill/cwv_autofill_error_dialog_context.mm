// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <optional>
#import <string>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_error_dialog_context_internal.h"

namespace {

CWVAutofillErrorDialogType CWVTypeFromAutofillType(
    autofill::AutofillErrorDialogType type) {
  switch (type) {
    case autofill::AutofillErrorDialogType::kVirtualCardTemporaryError:
      return CWVAutofillErrorDialogTypeVirtualCardTemporaryError;
    case autofill::AutofillErrorDialogType::kVirtualCardPermanentError:
      return CWVAutofillErrorDialogTypeVirtualCardPermanentError;
    case autofill::AutofillErrorDialogType::kVirtualCardNotEligibleError:
      return CWVAutofillErrorDialogTypeVirtualCardNotEligibleError;
    case autofill::AutofillErrorDialogType::
        kMaskedServerCardRiskBasedUnmaskingPermanentError:
      return CWVAutofillErrorDialogTypeMaskedServerCardRiskBasedUnmaskingPermanentError;
    case autofill::AutofillErrorDialogType::
        kMaskedServerCardRiskBasedUnmaskingNetworkError:
      return CWVAutofillErrorDialogTypeMaskedServerCardRiskBasedUnmaskingNetworkError;
    case autofill::AutofillErrorDialogType::kCardInfoRetrievalTemporaryError:
      return CWVAutofillErrorDialogTypeCardInfoRetrievalTemporaryError;
    case autofill::AutofillErrorDialogType::kCardInfoRetrievalPermanentError:
      return CWVAutofillErrorDialogTypeCardInfoRetrievalPermanentError;
    // The remaining types are either unreachable in ChromeWebView (IBAN, BNPL)
    // or not shown through `ShowAutofillErrorDialog()`.
    case autofill::AutofillErrorDialogType::kTypeUnknown:
    case autofill::AutofillErrorDialogType::
        kMaskedServerIbanUnmaskingTemporaryError:
    case autofill::AutofillErrorDialogType::kCreditCardUploadError:
    case autofill::AutofillErrorDialogType::
        kVirtualCardEnrollmentTemporaryError:
    case autofill::AutofillErrorDialogType::kBnplTemporaryError:
    case autofill::AutofillErrorDialogType::kBnplPermanentError:
    case autofill::AutofillErrorDialogType::kBnplUnsupportedCurrencyError:
      return CWVAutofillErrorDialogTypeUnknown;
  }
  return CWVAutofillErrorDialogTypeUnknown;
}

// Returns nil when `value` is unset or empty, so embedders can treat any
// non-nil string as displayable.
NSString* NSStringFromOptional(const std::optional<std::string>& value) {
  return (value && !value->empty()) ? base::SysUTF8ToNSString(*value) : nil;
}

}  // namespace

@implementation CWVAutofillErrorDialogContext

- (instancetype)initWithContext:
    (const autofill::AutofillErrorDialogContext&)context {
  self = [super init];
  if (self) {
    _type = CWVTypeFromAutofillType(context.type);
    _serverReturnedTitle = NSStringFromOptional(context.server_returned_title);
    _serverReturnedDescription =
        NSStringFromOptional(context.server_returned_description);
  }
  return self;
}

@end
