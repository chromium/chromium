// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/tailored_security/test/mock_tailored_security_service_infobar_delegate.h"

#import "base/memory/ptr_util.h"

namespace safe_browsing {

// static
std::unique_ptr<MockTailoredSecurityServiceInfobarDelegate>
MockTailoredSecurityServiceInfobarDelegate::Create(
    TailoredSecurityServiceMessageState message_state,
    web::WebState* web_state) {
  return std::make_unique<MockTailoredSecurityServiceInfobarDelegate>(
      message_state, web_state);
}

MockTailoredSecurityServiceInfobarDelegate::
    MockTailoredSecurityServiceInfobarDelegate(
        TailoredSecurityServiceMessageState message_state,
        web::WebState* web_state)
    : TailoredSecurityServiceInfobarDelegate(message_state, web_state) {}

MockTailoredSecurityServiceInfobarDelegate::
    ~MockTailoredSecurityServiceInfobarDelegate() = default;

}  // namespace safe_browsing
