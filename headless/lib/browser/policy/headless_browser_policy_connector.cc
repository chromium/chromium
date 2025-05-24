// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/policy/headless_browser_policy_connector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/headless/policy/headless_mode_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler.h"  // nogncheck http://crbug.com/1227148
#include "components/policy/core/browser/url_blocklist_policy_handler.h"  // nogncheck http://crbug.com/1227148
#include "components/policy/core/common/async_policy_provider.h"  // nogncheck http://crbug.com/1227148
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_paths.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "headless/lib/browser/policy/headless_prefs.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#include "components/policy/core/common/policy_loader_win.h"
#elif BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"  // nogncheck http://crbug.com/1227148
#endif

namespace policy {

namespace {

void PopulatePolicyHandlerParameters(PolicyHandlerParameters* parameters) {}

std::unique_ptr<ConfigurationPolicyHandlerList> BuildHandlerList(
    const Schema& chrome_schema) {
  auto handlers = std::make_unique<ConfigurationPolicyHandlerList>(
      base::BindRepeating(&PopulatePolicyHandlerParameters),
      base::BindRepeating(&GetChromePolicyDetails),
      /*are_future_policies_allowed_by_default=*/false);

// TODO(kvitekp): remove #ifdef when ChromeOS is supported by //headless.
#if !BUILDFLAG(IS_CHROMEOS)
  handlers->AddHandler(std::make_unique<headless::HeadlessModePolicyHandler>());
#endif

  handlers->AddHandler(
      std::make_unique<URLBlocklistPolicyHandler>(key::kURLBlocklist));
  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kURLAllowlist, policy_prefs::kUrlAllowlist,
      base::Value::Type::LIST));

  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kRemoteDebuggingAllowed,
      headless::prefs::kDevToolsRemoteDebuggingAllowed,
      base::Value::Type::BOOLEAN));

  return handlers;
}

}  // namespace

HeadlessBrowserPolicyConnector::HeadlessBrowserPolicyConnector()
    : BrowserPolicyConnector(base::BindRepeating(&BuildHandlerList)) {}

HeadlessBrowserPolicyConnector::~HeadlessBrowserPolicyConnector() = default;

scoped_refptr<PrefStore> HeadlessBrowserPolicyConnector::CreatePrefStore(
    policy::PolicyLevel policy_level) {
  return base::MakeRefCounted<policy::ConfigurationPolicyPrefStore>(
      this, GetPolicyService(), GetHandlerList(), policy_level);
}

void HeadlessBrowserPolicyConnector::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    PolicyLogger::GetInstance()->EnableLogDeletion();
}

bool HeadlessBrowserPolicyConnector::IsDeviceEnterpriseManaged() const {
  return false;
}

bool HeadlessBrowserPolicyConnector::IsCommandLineSwitchSupported() const {
  return false;
}

bool HeadlessBrowserPolicyConnector::HasMachineLevelPolicies() {
  return ProviderHasPolicies(GetPlatformProvider());
}

ConfigurationPolicyProvider*
HeadlessBrowserPolicyConnector::GetPlatformProvider() {
  ConfigurationPolicyProvider* provider =
      BrowserPolicyConnectorBase::GetPolicyProviderForTesting();
  return provider ? provider : platform_provider_.get();
}

std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
HeadlessBrowserPolicyConnector::CreatePolicyProviders() {
  auto providers = BrowserPolicyConnector::CreatePolicyProviders();
  if (!BrowserPolicyConnectorBase::GetPolicyProviderForTesting()) {
    std::unique_ptr<ConfigurationPolicyProvider> platform_provider =
        CreatePlatformProvider();
    if (platform_provider) {
      platform_provider_ = platform_provider.get();
      // PlatformProvider should be before all other providers (highest
      // priority).
      providers.insert(providers.begin(), std::move(platform_provider));
    }
  }
  return providers;
}

std::unique_ptr<ConfigurationPolicyProvider>
HeadlessBrowserPolicyConnector::CreatePlatformProvider() {
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<AsyncPolicyLoader> loader(PolicyLoaderWin::Create(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      policy::PlatformManagementService::GetInstance(),
      kRegistryChromePolicyKey));
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif BUILDFLAG(IS_MAC)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Explicitly watch the "com.google.Chrome" bundle ID, no matter what this
  // app's bundle ID actually is. All channels of Chrome should obey the same
  // policies.
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
#else
  base::apple::ScopedCFTypeRef<CFStringRef> bundle_id_scoper =
      base::SysUTF8ToCFStringRef(base::apple::BaseBundleID());
  CFStringRef bundle_id = bundle_id_scoper.get();
#endif
  auto loader = std::make_unique<PolicyLoaderMac>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
      std::make_unique<MacPreferences>(), bundle_id);
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<AsyncPolicyLoader> loader(new ConfigDirPolicyLoader(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      base::FilePath(policy::kPolicyPath), POLICY_SCOPE_MACHINE));
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#else
  return nullptr;
#endif
}

}  // namespace policy
