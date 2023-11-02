// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_FORM_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_FORM_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// The different form types used in autofilling.
typedef NS_OPTIONS(NSInteger, CWVAutofillFormType) {
  // The type of form is unknown.
  CWVAutofillFormTypeUnknown = 0,
  // Address forms that can be autofilled with saved profiles.
  CWVAutofillFormTypeAddresses = 1 << 0,
  // Credit card forms that can be autofilled with saved credit cards.
  CWVAutofillFormTypeCreditCards = 1 << 1,
  // Log in forms that can be autofilled with saved credentials.
  CWVAutofillFormTypePasswords = 1 << 2
};

// Contains information on a HTML <form> that may be autofilled.
CWV_EXPORT
@interface CWVAutofillForm : NSObject

// The name attribute of the form.
@property(nullable, nonatomic, copy, readonly) NSString* name;

// Indicates the types of form this may be. Note that a form may contain
// addresses, credit cards, and passwords.
@property(nonatomic, readonly) CWVAutofillFormType type;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_FORM_H_
