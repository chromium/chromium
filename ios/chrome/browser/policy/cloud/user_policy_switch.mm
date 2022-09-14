// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"

#import "base/command_line.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace policy {

const char kEnableUserPolicy[] = "enable-user-policy-for-ios";

void EnableUserPolicy() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(kEnableUserPolicy);
}

bool IsUserPolicyEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kEnableUserPolicy);
}

}  // namespace policy
