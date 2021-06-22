// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_TEST_ORIGIN_TRIAL_POLICY_H_
#define CONTENT_BROWSER_APPCACHE_TEST_ORIGIN_TRIAL_POLICY_H_

#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"

namespace content {

class TestOriginTrialPolicy : public blink::OriginTrialPolicy {
 public:
  TestOriginTrialPolicy();
  ~TestOriginTrialPolicy() override;

  bool IsOriginTrialsSupported() const override;
  const std::vector<blink::OriginTrialPublicKey>& GetPublicKeys()
      const override;
  bool IsOriginSecure(const GURL& url) const override;

 private:
  std::vector<blink::OriginTrialPublicKey> public_keys_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_TEST_ORIGIN_TRIAL_POLICY_H_
