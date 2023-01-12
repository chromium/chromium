// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"

#include "base/functional/bind.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace blink {

// This is the public key which the test below will use to enable origin
// trial features. Trial tokens for use in tests can be created with the
// tool in /tools/origin_trials/generate_token.py, using the private key
// contained in /tools/origin_trials/eftest.key.
//
// Private key:
//  0x83, 0x67, 0xf4, 0xcd, 0x2a, 0x1f, 0x0e, 0x04, 0x0d, 0x43, 0x13,
//  0x4c, 0x67, 0xc4, 0xf4, 0x28, 0xc9, 0x90, 0x15, 0x02, 0xe2, 0xba,
//  0xfd, 0xbb, 0xfa, 0xbc, 0x92, 0x76, 0x8a, 0x2c, 0x4b, 0xc7, 0x75,
//  0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2, 0x9a,
//  0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f, 0x64,
//  0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0
const blink::OriginTrialPublicKey kTestPublicKey = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

bool ScopedTestOriginTrialPolicy::IsOriginTrialsSupported() const {
  return true;
}

const std::vector<blink::OriginTrialPublicKey>&
ScopedTestOriginTrialPolicy::GetPublicKeys() const {
  return public_keys_;
}

bool ScopedTestOriginTrialPolicy::IsOriginSecure(const GURL& url) const {
  return true;
}

ScopedTestOriginTrialPolicy::ScopedTestOriginTrialPolicy()
    : public_keys_({kTestPublicKey}) {
  TrialTokenValidator::SetOriginTrialPolicyGetter(base::BindRepeating(
      [](OriginTrialPolicy* self) { return self; }, base::Unretained(this)));
}

ScopedTestOriginTrialPolicy::~ScopedTestOriginTrialPolicy() {
  TrialTokenValidator::ResetOriginTrialPolicyGetter();
}

}  // namespace blink
