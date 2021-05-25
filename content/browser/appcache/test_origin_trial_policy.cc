// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/test_origin_trial_policy.h"

#include "base/cxx17_backports.h"

namespace content {

const uint8_t kTestPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

TestOriginTrialPolicy::TestOriginTrialPolicy() {
  public_keys_.push_back(
      base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey),
                        base::size(kTestPublicKey)));
}

bool TestOriginTrialPolicy::IsOriginTrialsSupported() const {
  return true;
}

std::vector<base::StringPiece> TestOriginTrialPolicy::GetPublicKeys() const {
  return public_keys_;
}

bool TestOriginTrialPolicy::IsOriginSecure(const GURL& url) const {
  return true;
}

TestOriginTrialPolicy::~TestOriginTrialPolicy() = default;

}  // namespace content
