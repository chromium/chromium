// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/otp_credential.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
constexpr char kOtpCredentialType[] = "otp";
}

OTPCredential::OTPCredential(const String& code)
    : Credential(String(), kOtpCredentialType), code_(code) {}

bool OTPCredential::IsOTPCredential() const {
  return true;
}

}  // namespace blink
