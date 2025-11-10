// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/policy/headless_policy_blocklist_service_factory.h"

#include <memory>

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace headless {

// static
PolicyBlocklistService*
HeadlessPolicyBlocklistServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PolicyBlocklistService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HeadlessPolicyBlocklistServiceFactory*
HeadlessPolicyBlocklistServiceFactory::GetInstance() {
  static base::NoDestructor<HeadlessPolicyBlocklistServiceFactory> instance;
  return instance.get();
}

HeadlessPolicyBlocklistServiceFactory::HeadlessPolicyBlocklistServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "HeadlessPolicyBlocklistService",
          BrowserContextDependencyManager::GetInstance()) {}

HeadlessPolicyBlocklistServiceFactory::
    ~HeadlessPolicyBlocklistServiceFactory() = default;

std::unique_ptr<KeyedService>
HeadlessPolicyBlocklistServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  PrefService* pref_service = user_prefs::UserPrefs::Get(context);
  auto url_blocklist_manager = std::make_unique<policy::URLBlocklistManager>(
      pref_service, policy::policy_prefs::kUrlBlocklist,
      policy::policy_prefs::kUrlAllowlist);
  return std::make_unique<PolicyBlocklistService>(
      std::move(url_blocklist_manager), pref_service);
}

content::BrowserContext*
HeadlessPolicyBlocklistServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace headless
