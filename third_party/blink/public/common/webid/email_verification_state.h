// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBID_EMAIL_VERIFICATION_STATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBID_EMAIL_VERIFICATION_STATE_H_

namespace blink {

enum class EmailVerificationState {
  kNone,
  kLoading,
  kVerified,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBID_EMAIL_VERIFICATION_STATE_H_
