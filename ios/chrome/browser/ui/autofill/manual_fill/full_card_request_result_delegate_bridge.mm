// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_request_result_delegate_bridge.h"

#import <string>

#import "base/containers/adapters.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/form_structure.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullCardRequestResultDelegateBridge::FullCardRequestResultDelegateBridge(
    id<FullCardRequestResultDelegateObserving> delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {}

FullCardRequestResultDelegateBridge::~FullCardRequestResultDelegateBridge() {}

base::WeakPtr<FullCardRequestResultDelegateBridge>
FullCardRequestResultDelegateBridge::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FullCardRequestResultDelegateBridge::OnFullCardRequestSucceeded(
    const autofill::payments::FullCardRequest& /* full_card_request */,
    const autofill::CreditCard& card,
    const std::u16string& /* cvc */) {
  [delegate_ onFullCardRequestSucceeded:card];
}

void FullCardRequestResultDelegateBridge::OnFullCardRequestFailed(
    autofill::CreditCard::RecordType /* card_type */,
    autofill::payments::FullCardRequest::FailureType /* failure_type */) {
  [delegate_ onFullCardRequestFailed];
}
