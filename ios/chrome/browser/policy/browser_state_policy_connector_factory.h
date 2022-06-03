// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_BROWSER_STATE_POLICY_CONNECTOR_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_BROWSER_STATE_POLICY_CONNECTOR_FACTORY_H_

#include <memory>

class BrowserPolicyConnectorIOS;
class BrowserStatePolicyConnector;

namespace policy {
class SchemaRegistry;
}  // namespace policy

std::unique_ptr<BrowserStatePolicyConnector> BuildBrowserStatePolicyConnector(
    policy::SchemaRegistry* schema_registry,
    BrowserPolicyConnectorIOS* browser_policy_connector);

#endif  // IOS_CHROME_BROWSER_POLICY_BROWSER_STATE_POLICY_CONNECTOR_FACTORY_H_
