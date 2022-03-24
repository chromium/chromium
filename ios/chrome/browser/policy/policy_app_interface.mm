// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/policy_app_interface.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/chrome/browser/policy/test_platform_policy_provider.h"
#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_service.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a JSON-encoded string representing the given |base::Value|. If
// |value| is nullptr, returns a string representing a |base::Value| of type
// NONE.
NSString* SerializeValue(const base::Value* value) {
  base::Value none_value(base::Value::Type::NONE);

  if (!value) {
    value = &none_value;
  }
  DCHECK(value);

  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(*value);
  return base::SysUTF8ToNSString(serialized_value);
}

// Takes a JSON-encoded string representing a |base::Value|, and deserializes
// into a |base::Value| pointer. If nullptr is given, returns a pointer to a
// |base::Value| of type NONE.
absl::optional<base::Value> DeserializeValue(NSString* json_value) {
  if (!json_value) {
    return base::Value(base::Value::Type::NONE);
  }

  std::string json = base::SysNSStringToUTF8(json_value);
  JSONStringValueDeserializer deserializer(json);
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(/*error_code=*/nullptr,
                               /*error_message=*/nullptr);
  return value ? absl::make_optional<base::Value>(std::move(*value))
               : absl::nullopt;
}
}

@implementation PolicyAppInterface

+ (NSString*)valueForPlatformPolicy:(NSString*)policyKey {
  const std::string key = base::SysNSStringToUTF8(policyKey);

  BrowserPolicyConnectorIOS* connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  if (!connector) {
    return SerializeValue(nullptr);
  }

  const policy::ConfigurationPolicyProvider* platformProvider =
      connector->GetPlatformProvider();
  if (!platformProvider) {
    return SerializeValue(nullptr);
  }

  const policy::PolicyBundle& policyBundle = platformProvider->policies();
  const policy::PolicyMap& policyMap = policyBundle.Get(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, ""));
  // |GetValueUnsafe| is used due to multiple policy types being handled.
  return SerializeValue(policyMap.GetValueUnsafe(key));
}

+ (void)setPolicyValue:(NSString*)jsonValue forKey:(NSString*)policyKey {
  absl::optional<base::Value> value = DeserializeValue(jsonValue);
  policy::PolicyMap values;
  values.Set(base::SysNSStringToUTF8(policyKey), policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
             std::move(value), /*external_data_fetcher=*/nullptr);
  GetTestPlatformPolicyProvider()->UpdateChromePolicy(values);
}

+ (void)clearPolicies {
  policy::PolicyMap values;
  GetTestPlatformPolicyProvider()->UpdateChromePolicy(values);
}

+ (BOOL)isURLBlocked:(NSString*)URL {
  GURL gurl = GURL(base::SysNSStringToUTF8(URL));
  PolicyBlocklistService* service =
      PolicyBlocklistServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  return service->GetURLBlocklistState(gurl) ==
         policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

@end
