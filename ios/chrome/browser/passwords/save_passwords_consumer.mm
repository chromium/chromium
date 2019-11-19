// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/save_passwords_consumer.h"

#include <utility>

#include "components/autofill/core/common/password_form.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

SavePasswordsConsumer::SavePasswordsConsumer(
    id<SavePasswordsConsumerDelegate> delegate)
    : delegate_(delegate) {}

SavePasswordsConsumer::~SavePasswordsConsumer() = default;

void SavePasswordsConsumer::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  [delegate_ onGetPasswordStoreResults:std::move(results)];
}

}  // namespace ios
