// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/policy_app_interface.h"

#import <memory>
#import <optional>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/values.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/browser/url_blocklist_manager.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/device_management_service.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/configuration_policy_provider.h"
#import "components/policy/core/common/policy_bundle.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/policy_namespace.h"
#import "components/policy/core/common/policy_types.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/test_platform_policy_provider.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace {

// Directory where device management token is stored. This value is from
// "ios/chrome/browser/policy/model/browser_dm_token_storage_ios.mm"
const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment");

// Directory where cloud policy are stored. This value is from
// "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.cc"
const base::FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Policy");

// Returns a JSON-encoded string representing the given `base::Value`. If
// `value` is nullptr, returns a string representing a `base::Value` of type
// NONE.
NSString* SerializeValue(const base::Value* value) {
  if (!value) {
    // The representation for base::Value::Type::NONE, according to the
    // JSON spec at https://www.json.org/json-en.html
    return @"null";
  }

  const std::optional<std::string> json_string = base::WriteJson(*value);
  return base::SysUTF8ToNSString(json_string.value_or(std::string()));
}

// Takes a JSON-encoded string representing a `base::Value`, and deserializes
// into a `base::Value` pointer. If nullptr is given, returns a pointer to a
// `base::Value` of type NONE.
std::optional<base::Value> DeserializeValue(NSString* json_value) {
  if (!json_value.length) {
    return base::Value();
  }

  return base::JSONReader::Read(base::SysNSStringToUTF8(json_value));
}

}  // namespace

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
  // `GetValueUnsafe` is used due to multiple policy types being handled.
  return SerializeValue(policyMap.GetValueUnsafe(key));
}

+ (void)setPolicyValue:(NSString*)jsonValue forKey:(NSString*)policyKey {
  std::optional<base::Value> value = DeserializeValue(jsonValue);
  policy::PolicyMap values;
  values.Set(base::SysNSStringToUTF8(policyKey), policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
             std::move(value), /*external_data_fetcher=*/nullptr);
  GetTestPlatformPolicyProvider()->UpdateChromePolicy(values);
}

+ (void)mergePolicyValue:(NSString*)jsonValue forKey:(NSString*)policyKey {
  // Get the policy bundle.
  policy::MockConfigurationPolicyProvider* platformProvider =
      GetTestPlatformPolicyProvider();
  policy::PolicyBundle mutablePolicyBundle;
  mutablePolicyBundle.MergeFrom(platformProvider->policies());
  // Get the policy map.
  policy::PolicyNamespace chromePolicyNamespace(
      policy::PolicyDomain::POLICY_DOMAIN_CHROME, std::string());
  policy::PolicyMap& chromePolicyMap =
      mutablePolicyBundle.Get(chromePolicyNamespace);
  // Add the value.
  std::optional<base::Value> value = DeserializeValue(jsonValue);
  chromePolicyMap.Set(
      base::SysNSStringToUTF8(policyKey), policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
      std::move(value), /*external_data_fetcher=*/nullptr);
  // Update the policy.
  platformProvider->UpdatePolicy(std::move(mutablePolicyBundle));
}

+ (void)clearPolicies {
  policy::PolicyMap values;
  GetTestPlatformPolicyProvider()->UpdateChromePolicy(values);
}

+ (void)clearAllPoliciesInNSUserDefault {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

+ (BOOL)isURLBlocked:(NSString*)URL {
  GURL gurl = GURL(base::SysNSStringToUTF8(URL));
  PolicyBlocklistService* service =
      PolicyBlocklistServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  return service->GetURLBlocklistState(gurl) ==
         policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

+ (void)setBrowserCloudPolicyDataWithDomain:(NSString*)domain {
  policy::MachineLevelUserCloudPolicyManager* manager =
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->machine_level_user_cloud_policy_manager();
  DCHECK(manager);

  policy::CloudPolicyStore* store = manager->core()->store();
  DCHECK(store);

  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_managed_by(base::SysNSStringToUTF8(domain));
  store->set_policy_data_for_testing(std::move(policy_data));
}

+ (void)setUserCloudPolicyDataWithDomain:(NSString*)domain {
  policy::UserCloudPolicyManager* manager =
      chrome_test_util::GetOriginalProfile()->GetUserCloudPolicyManager();
  DCHECK(manager);

  policy::CloudPolicyStore* store = manager->core()->store();
  DCHECK(store);

  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_managed_by(base::SysNSStringToUTF8(domain));
  store->set_policy_data_for_testing(std::move(policy_data));
}

+ (BOOL)clearDMTokenDirectory {
  base::FilePath appDataDirPath;
  base::PathService::Get(base::DIR_APP_DATA, &appDataDirPath);
  base::FilePath dmTokenDirPath = appDataDirPath.Append(kDmTokenBaseDir);

  __block BOOL didComplete = NO;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        base::DeletePathRecursively(dmTokenDirPath);
      }),
      base::BindOnce(^{
        didComplete = YES;
      }));

  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^{
        return didComplete;
      });
}

+ (BOOL)isCloudPolicyClientRegistered {
  return GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->machine_level_user_cloud_policy_manager()
      ->core()
      ->client()
      ->is_registered();
}

+ (BOOL)clearCloudPolicyDirectory {
  base::FilePath userDataDir;
  base::PathService::Get(ios::DIR_USER_DATA, &userDataDir);
  base::FilePath policyDir = userDataDir.Append(kPolicyDir);

  __block BOOL didComplete = NO;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        base::DeletePathRecursively(policyDir);
      }),
      base::BindOnce(^{
        didComplete = YES;
      }));

  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^{
        return didComplete;
      });
}

+ (BOOL)hasUserPolicyDataInCurrentProfile {
  policy::UserCloudPolicyManager* manager =
      chrome_test_util::GetOriginalProfile()->GetUserCloudPolicyManager();
  DCHECK(manager);

  policy::CloudPolicyStore* store = manager->core()->store();
  DCHECK(store);

  return store->has_policy() && store->is_managed();
}

+ (BOOL)hasUserPolicyInCurrentProfile:(NSString*)policyName
                     withIntegerValue:(int)expectedValue {
  policy::UserCloudPolicyManager* manager =
      chrome_test_util::GetOriginalProfile()->GetUserCloudPolicyManager();
  DCHECK(manager);

  policy::CloudPolicyStore* store = manager->core()->store();
  DCHECK(store);

  const base::Value* value = store->policy_map().GetValue(
      base::SysNSStringToUTF8(policyName), base::Value::Type::INTEGER);

  return value && value->GetInt() == expectedValue;
}

@end
