// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/mock_webauthn_cred_man_delegate.h"

namespace webauthn {

MockWebAuthnCredManDelegate::MockWebAuthnCredManDelegate()
    : WebAuthnCredManDelegate(/*web_contents=*/nullptr) {}

MockWebAuthnCredManDelegate::~MockWebAuthnCredManDelegate() = default;

}  // namespace webauthn
