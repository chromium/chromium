// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/shell_origin_trial_policy.h"

#include "base/feature_list.h"
#include "base/stl_util.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_util.h"

namespace content {

namespace {

// This is the public key which the content shell will use to enable origin
// trial features. Trial tokens for use in web tests can be created with the
// tool in /tools/origin_trials/generate_token.py, using the private key
// contained in /tools/origin_trials/eftest.key.
static const uint8_t kOriginTrialPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

}  // namespace

ShellOriginTrialPolicy::ShellOriginTrialPolicy()
    : public_key_(base::StringPiece(
          reinterpret_cast<const char*>(kOriginTrialPublicKey),
          base::size(kOriginTrialPublicKey))) {}

ShellOriginTrialPolicy::~ShellOriginTrialPolicy() {}

bool ShellOriginTrialPolicy::IsOriginTrialsSupported() const {
  return true;
}

base::StringPiece ShellOriginTrialPolicy::GetPublicKey() const {
  return public_key_;
}

bool ShellOriginTrialPolicy::IsOriginSecure(const GURL& url) const {
  return content::IsOriginSecure(url);
}

}  // namespace content
