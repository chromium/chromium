// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_FACTORY_H_

#include <memory>

class ChromeBrowserState;

namespace policy {
class CombinedSchemaRegistry;
class Schema;
class SchemaRegistry;
}  // namespace policy

std::unique_ptr<policy::SchemaRegistry> BuildSchemaRegistryForBrowserState(
    ChromeBrowserState* browser_state,
    const policy::Schema& chrome_schema,
    policy::CombinedSchemaRegistry* global_registry);

#endif  // IOS_CHROME_BROWSER_POLICY_SCHEMA_REGISTRY_FACTORY_H_
