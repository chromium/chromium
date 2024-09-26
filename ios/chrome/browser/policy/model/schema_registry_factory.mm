// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/schema_registry_factory.h"

#import "base/check.h"
#import "components/policy/core/common/policy_namespace.h"
#import "components/policy/core/common/schema.h"
#import "components/policy/core/common/schema_registry.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

std::unique_ptr<policy::SchemaRegistry> BuildSchemaRegistryForProfile(
    ProfileIOS* profile,
    const policy::Schema& chrome_schema,
    policy::CombinedSchemaRegistry* global_registry) {
  DCHECK(!profile->IsOffTheRecord());

  auto registry = std::make_unique<policy::SchemaRegistry>();

  if (chrome_schema.valid()) {
    registry->RegisterComponent(
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, ""),
        chrome_schema);
  }
  registry->SetDomainReady(policy::POLICY_DOMAIN_CHROME);

  if (global_registry) {
    global_registry->Track(registry.get());
  }

  return registry;
}
