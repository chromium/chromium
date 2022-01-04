// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/policy_features.h"

#include <string>

#include "base/command_line.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/common/channel_info.h"

namespace {

bool HasSwitch(const std::string& switch_name) {
  // Most policy features must be controlled via the command line because policy
  // infrastructure must be initialized before about:flags or field trials.
  // Using a command line flag is the only way to control these features at
  // runtime.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switch_name);
}

// Returns true if the current command line contains the
// |kDisableEnterprisePolicy| switch.
bool IsDisableEnterprisePolicySwitchPresent() {
  return HasSwitch(switches::kDisableEnterprisePolicy);
}

}  // namespace

bool IsEnterprisePolicyEnabled() {
  return !IsDisableEnterprisePolicySwitchPresent();
}

bool ShouldInstallEnterprisePolicyHandlers() {
  return IsEnterprisePolicyEnabled();
}

bool ShouldInstallURLBlocklistPolicyHandlers() {
  return HasSwitch(switches::kInstallURLBlocklistHandlers);
}

bool IsURLBlocklistEnabled() {
  return ShouldInstallURLBlocklistPolicyHandlers();
}
