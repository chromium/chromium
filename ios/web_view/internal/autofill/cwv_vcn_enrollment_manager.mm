// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/autofill/cwv_autofill_util.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_vcn_enrollment_manager_internal.h"
#import "net/base/apple/url_conversions.h"
#import "ui/gfx/range/range.h"

NS_ASSUME_NONNULL_BEGIN

@implementation CWVVCNEnrollmentManager {
  // The callback to be executed when the user accepts VCN enrollment.
  // This callback is move-only and will be run exactly once.
  base::OnceClosure _acceptCallback;

  // The callback to be executed when the user declines VCN enrollment.
  // This is also invoked if this object is deallocated before a decision is
  // made. This callback is move-only and will be run exactly once.
  base::OnceClosure _declineCallback;

  // The Objective-C block to be invoked with the result of the enrollment
  // process. This is typically provided by the UI layer (e.g., from an alert
  // handler) and is called after the enrollment network request completes. The
  // BOOL parameter indicates whether the enrollment was successful.
  void (^_Nullable _enrollmentCompletionHandler)(BOOL);

  // A flag to ensure that the user's decision can only be handled once.
  // It is set to YES after either `enrollWithCompletionHandler:` or `decline`
  // is called to prevent subsequent calls.
  BOOL _decisionMade;
}

@synthesize creditCard = _creditCard;
@synthesize legalMessages = _legalMessages;

- (instancetype)initWithCreditCard:(const autofill::CreditCard&)creditCard
                 legalMessageLines:
                     (autofill::LegalMessageLines)legalMessageLines
                    enrollCallback:(base::OnceClosure)acceptCallback
                   declineCallback:(base::OnceClosure)declineCallback {
  self = [super init];
  if (self) {
    _creditCard = [[CWVCreditCard alloc] initWithCreditCard:creditCard];

    _legalMessages = CWVLegalMessagesFromLegalMessageLines(legalMessageLines);

    _acceptCallback = std::move(acceptCallback);
    _declineCallback = std::move(declineCallback);
    _decisionMade = NO;
  }
  return self;
}

- (void)dealloc {
  if (!_decisionMade) {
    std::move(_declineCallback).Run();
  }
}

#pragma mark - Public Methods

- (void)enrollWithCompletionHandler:(void (^)(BOOL))completionHandler {
  CHECK(!_decisionMade);
  CHECK(_acceptCallback);
  _enrollmentCompletionHandler = completionHandler;
  std::move(_acceptCallback).Run();
  _decisionMade = YES;
}

- (void)decline {
  CHECK(!_decisionMade);
  CHECK(_declineCallback);
  std::move(_declineCallback).Run();
  _decisionMade = YES;
}

#pragma mark - Internal Methods

- (void)handleCreditCardVCNEnrollmentCompleted:(BOOL)cardEnrolled {
  if (_enrollmentCompletionHandler) {
    _enrollmentCompletionHandler(cardEnrolled);
    _enrollmentCompletionHandler = nil;
  }
}

@end

NS_ASSUME_NONNULL_END
