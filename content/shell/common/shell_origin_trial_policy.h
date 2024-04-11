// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_
#define CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_

#include <vector>

#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"

namespace content {

class ShellOriginTrialPolicy : public blink::OriginTrialPolicy {
 public:
  ShellOriginTrialPolicy();

  ShellOriginTrialPolicy(const ShellOriginTrialPolicy&) = delete;
  ShellOriginTrialPolicy& operator=(const ShellOriginTrialPolicy&) = delete;

  ~ShellOriginTrialPolicy() override;

  // blink::OriginTrialPolicy interface
  bool IsOriginTrialsSupported() const override;
  const std::vector<blink::OriginTrialPublicKey>& GetPublicKeys()
      const override;
  bool IsOriginSecure(const GURL& url) const override;

 private:
  std::vector<blink::OriginTrialPublicKey> public_keys_;
};

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_
