// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_LIST_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_LIST_FACTORY_H_

#include <memory>

namespace policy {
class ConfigurationPolicyHandlerList;
class Schema;
}  // namespace policy

// Builds a policy handler list.
// All un-released policies will be ignored by default unless
// `allow_future_policies` is True.
std::unique_ptr<policy::ConfigurationPolicyHandlerList> BuildPolicyHandlerList(
    bool allow_future_policies,
    const policy::Schema& chrome_schema);

#endif  // IOS_CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_LIST_FACTORY_H_
