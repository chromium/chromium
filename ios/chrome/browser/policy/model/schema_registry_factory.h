// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_SCHEMA_REGISTRY_FACTORY_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_SCHEMA_REGISTRY_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace policy {
class CombinedSchemaRegistry;
class Schema;
class SchemaRegistry;
}  // namespace policy

std::unique_ptr<policy::SchemaRegistry> BuildSchemaRegistryForProfile(
    ProfileIOS* profile,
    const policy::Schema& chrome_schema,
    policy::CombinedSchemaRegistry* global_registry);

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_SCHEMA_REGISTRY_FACTORY_H_
