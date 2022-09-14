// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/tailored_security/test/mock_tailored_security_service_infobar_delegate.h"

#import "base/memory/ptr_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace safe_browsing {

// static
std::unique_ptr<MockTailoredSecurityServiceInfobarDelegate>
MockTailoredSecurityServiceInfobarDelegate::Create(bool consent_status) {
  return base::WrapUnique(
      new MockTailoredSecurityServiceInfobarDelegate(consent_status));
}

MockTailoredSecurityServiceInfobarDelegate::
    MockTailoredSecurityServiceInfobarDelegate(bool consent_status)
    : TailoredSecurityServiceInfobarDelegate(consent_status) {}

MockTailoredSecurityServiceInfobarDelegate::
    ~MockTailoredSecurityServiceInfobarDelegate() = default;

}  // namespace safe_browsing
