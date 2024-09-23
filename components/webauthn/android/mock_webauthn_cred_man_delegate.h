// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_MOCK_WEBAUTHN_CRED_MAN_DELEGATE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_MOCK_WEBAUTHN_CRED_MAN_DELEGATE_H_

#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webauthn {

class MockWebAuthnCredManDelegate : public WebAuthnCredManDelegate {
 public:
  MockWebAuthnCredManDelegate();
  ~MockWebAuthnCredManDelegate() override;

  MOCK_METHOD(WebAuthnCredManDelegate::State,
              HasPasskeys,
              (),
              (const, override));

  MOCK_METHOD(void,
              TriggerCredManUi,
              (WebAuthnCredManDelegate::RequestPasswords),
              (override));

  MOCK_METHOD(void,
              SetRequestCompletionCallback,
              (base::RepeatingCallback<void(bool)>),
              (override));
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_MOCK_WEBAUTHN_CRED_MAN_DELEGATE_H_
