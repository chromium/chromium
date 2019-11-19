// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/autofill/manual_fill/full_card_request_result_delegate_bridge.h"

#include "base/containers/adapters.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"

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
    const base::string16& /* cvc */) {
  [delegate_ onFullCardRequestSucceeded:card];
}

void FullCardRequestResultDelegateBridge::OnFullCardRequestFailed() {
  [delegate_ onFullCardRequestFailed];
}
