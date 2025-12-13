// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_VCN_ENROLLMENT_MANAGER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_VCN_ENROLLMENT_MANAGER_INTERNAL_H_

#include "base/functional/callback.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#import "ios/web_view/public/cwv_vcn_enrollment_manager.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

@interface CWVVCNEnrollmentManager ()

// Designated Initializer.
// |creditCard| The card that needs to be enrolled.
// |legalMessageLines| Contains messaging that must be displayed to the user.
// |enrollCallback| The callback to run when enrolling the card.
// |declineCallback| The callback to run when declining to enroll the card.
- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
                 legalMessageLines:
                     (autofill::LegalMessageLines)legalMessageLines
                    enrollCallback:(base::OnceClosure)acceptCallback
                   declineCallback:(base::OnceClosure)declineCallback
    NS_DESIGNATED_INITIALIZER;

// Called to notify when enrollment was completed.
- (void)handleCreditCardVCNEnrollmentCompleted:(BOOL)cardEnrolled;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_VCN_ENROLLMENT_MANAGER_INTERNAL_H_
