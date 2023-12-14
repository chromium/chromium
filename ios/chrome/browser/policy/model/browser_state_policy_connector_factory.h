// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_STATE_POLICY_CONNECTOR_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_STATE_POLICY_CONNECTOR_FACTORY_H_

#import <memory>

class BrowserPolicyConnectorIOS;
class BrowserStatePolicyConnector;

namespace policy {
class SchemaRegistry;
class UserCloudPolicyManager;
}  // namespace policy

std::unique_ptr<BrowserStatePolicyConnector> BuildBrowserStatePolicyConnector(
    policy::SchemaRegistry* schema_registry,
    BrowserPolicyConnectorIOS* browser_policy_connector,
    policy::UserCloudPolicyManager* user_policy_manager);

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_STATE_POLICY_CONNECTOR_FACTORY_H_
