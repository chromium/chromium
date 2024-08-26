// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_STATE_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_STATE_H_

#import <optional>
#import <string>

// Everything you need to know when displaying messages about device management,
// such as "Your browser is managed by acme.com"
struct ManagementState {
  ManagementState();
  ~ManagementState();

  ManagementState(const ManagementState&);
  ManagementState(ManagementState&&);
  ManagementState& operator=(const ManagementState&);
  ManagementState& operator=(ManagementState&&);

  // True if (a) there are policies set, or (b) the browser or profile is
  // enrolled to a domain that *could* send Chrome policies, even if it doesn't.
  bool is_managed() const {
    return is_browser_managed() || is_profile_managed();
  }

  bool is_browser_managed() const {
    return has_machine_level_policy || machine_level_domain.has_value();
  }

  bool is_profile_managed() const { return user_level_domain.has_value(); }

  // True if one or more policies are set in this browser, *specifically* at the
  // machine level.
  bool has_machine_level_policy;

  // Domain name for policies that affect the entire browser. Based on Chrome
  // Browser Cloud Management (CBCM) enrollment.
  std::optional<std::string> machine_level_domain;

  // Domain name for policies that affect this profile, based on sign-in state.
  // e.g. if you sign in as johndoe@acme.com, this is acme.com.
  std::optional<std::string> user_level_domain;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_STATE_H_
