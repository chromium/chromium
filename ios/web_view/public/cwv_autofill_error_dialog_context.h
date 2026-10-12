// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_ERROR_DIALOG_CONTEXT_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_ERROR_DIALOG_CONTEXT_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// The kind of error an autofill error dialog is reporting.
// This is kept in sync with autofill::AutofillErrorDialogType in
// components/autofill/core/browser/payments/autofill_error_dialog_context.h.
// Types that are not reachable in ChromeWebView are mapped to
// `CWVAutofillErrorDialogTypeUnknown`.
typedef NS_ENUM(NSInteger, CWVAutofillErrorDialogType) {
  // An unknown or unsupported error.
  CWVAutofillErrorDialogTypeUnknown = 0,
  // The server returned a temporary error while unmasking a virtual card.
  CWVAutofillErrorDialogTypeVirtualCardTemporaryError,
  // The server returned a permanent error while unmasking a virtual card.
  CWVAutofillErrorDialogTypeVirtualCardPermanentError,
  // The virtual card being unmasked is not eligible for the virtual card
  // feature.
  CWVAutofillErrorDialogTypeVirtualCardNotEligibleError,
  // The server returned a permanent error while unmasking a server card.
  CWVAutofillErrorDialogTypeMaskedServerCardRiskBasedUnmaskingPermanentError,
  // The server card could not be verified because the payments server could
  // not be reached.
  CWVAutofillErrorDialogTypeMaskedServerCardRiskBasedUnmaskingNetworkError,
  // The server returned a temporary error while unmasking a card enrolled in
  // card info retrieval.
  CWVAutofillErrorDialogTypeCardInfoRetrievalTemporaryError,
  // The server returned a permanent error while unmasking a card enrolled in
  // card info retrieval.
  CWVAutofillErrorDialogTypeCardInfoRetrievalPermanentError,
};

// Describes an error that should be shown to the user during a payments
// autofill flow. Instances are created internally by the framework.
CWV_EXPORT
@interface CWVAutofillErrorDialogContext : NSObject

// The kind of error being reported. The embedder should use this to pick a
// localized title and message when the server did not provide them.
@property(nonatomic, readonly, assign) CWVAutofillErrorDialogType type;

// Localized title provided by the server, if any. When non-nil, this should be
// preferred over a title derived from `type`.
@property(nonatomic, readonly, copy, nullable) NSString* serverReturnedTitle;

// Localized description provided by the server, if any. When non-nil, this
// should be preferred over a message derived from `type`.
@property(nonatomic, readonly, copy, nullable)
    NSString* serverReturnedDescription;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_ERROR_DIALOG_CONTEXT_H_
