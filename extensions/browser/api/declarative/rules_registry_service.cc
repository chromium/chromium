// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/rules_registry_service.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "base/types/id_type.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/rules_registry_ids.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace extensions {

RulesRegistryService::RulesRegistryService(content::BrowserContext* context)
    : browser_context_(context) {
  if (browser_context_) {
    extension_registry_observation_.Observe(
        ExtensionRegistry::Get(browser_context_));
    EnsureDefaultRulesRegistriesRegistered();
  }
}

RulesRegistryService::~RulesRegistryService() = default;

// static
int RulesRegistryService::GetNextRulesRegistryID() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  using RulesRegistryId = base::IdType<
      class RulesRegistryIdTag, int,
      rules_registry_ids::kInvalidRulesRegistryID,
      /*kFirstGeneratedId=*/1,
      /*kExtraInvalidValues=*/rules_registry_ids::kDefaultRulesRegistryID>;
  static RulesRegistryId::Generator generator;
  return generator.GenerateNextId().GetUnsafeValue();
}

void RulesRegistryService::Shutdown() {
  // Notify the registries.
  for (const auto& [_, registry] : rule_registries_) {
    registry->OnShutdown();
  }

  // Clearing `rule_registries_` releases the last reference to the
  // ContentRulesRegistry and destroys it. Reset the non-owning pointer first so
  // it does not dangle.
  content_rules_registry_ = nullptr;

  // Release the references to all registries.
  rule_registries_.clear();
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<RulesRegistryService>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<RulesRegistryService>*
RulesRegistryService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
RulesRegistryService* RulesRegistryService::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<RulesRegistryService>::Get(context);
}

// static
RulesRegistryService* RulesRegistryService::GetIfExists(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<RulesRegistryService>::GetIfExists(
      context);
}

void RulesRegistryService::RegisterRulesRegistry(
    scoped_refptr<RulesRegistry> rule_registry) {
  const std::string event_name(rule_registry->event_name());
  RulesRegistryKey key(event_name, rule_registry->id());
  DCHECK(!rule_registries_.contains(key));
  rule_registries_[key] = rule_registry;
}

scoped_refptr<RulesRegistry> RulesRegistryService::GetRulesRegistry(
    int rules_registry_id,
    const std::string& event_name) {
  RulesRegistryKey key(event_name, rules_registry_id);
  RulesRegistryMap::const_iterator i = rule_registries_.find(key);
  if (i != rule_registries_.end()) {
    return i->second;
  }

  // We should have attempted creation of the default rule registries at
  // construction, and invalid registry IDs (e.g. `kInvalidRulesRegistryID`)
  // must not lazily create registries.
  if (!browser_context_ ||
      rules_registry_id <= rules_registry_ids::kDefaultRulesRegistryID) {
    return nullptr;
  }

  // Only web request rules registries are created for webviews.
  DCHECK_EQ(declarative_webrequest_constants::kOnRequest, event_name);

  scoped_refptr<RulesRegistry> registry = RegisterWebRequestRulesRegistry(
      rules_registry_id, RulesCacheDelegate::Type::kEphemeral);
  DCHECK(rule_registries_.contains(key));
  return registry;
}

void RulesRegistryService::RemoveRulesRegistriesByID(int rules_registry_id) {
  std::vector<scoped_refptr<RulesRegistry>> removed_registries;
  std::set<RulesCacheDelegate*> delegates_to_delete;
  for (auto it = rule_registries_.begin(); it != rule_registries_.end();) {
    if (it->first.rules_registry_id != rules_registry_id) {
      ++it;
      continue;
    }
    if (RulesCacheDelegate* delegate = it->second->cache_delegate()) {
      delegates_to_delete.insert(delegate);
    }
    removed_registries.push_back(std::move(it->second));
    it = rule_registries_.erase(it);
  }

  if (!delegates_to_delete.empty()) {
    std::erase_if(cache_delegates_,
                  [&delegates_to_delete](
                      const std::unique_ptr<RulesCacheDelegate>& delegate) {
                    return delegates_to_delete.contains(delegate.get());
                  });
  }

  if (browser_context_ && !removed_registries.empty()) {
    if (auto* router = WebRequestEventRouter::Get(browser_context_)) {
      router->RegisterRulesRegistry(browser_context_, rules_registry_id,
                                    nullptr);
    }
  }

  // Notify removed registries after unregistering them from
  // `WebRequestEventRouter` above so any callbacks queued on
  // `RulesRegistry::ready()` (such as
  // `WebRequestEventRouter::OnRulesRegistryReady()` for an in-flight request)
  // see the registry already unregistered when they run.
  for (const auto& registry : removed_registries) {
    registry->OnRemoved();
  }
}

bool RulesRegistryService::HasAnyRegisteredRules() const {
  return std::ranges::any_of(
      cache_delegates_,
      [](const std::unique_ptr<RulesCacheDelegate>& delegate) {
        return delegate->HasRules();
      });
}

void RulesRegistryService::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RulesRegistryService::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void RulesRegistryService::SimulateExtensionUninstalled(
    const Extension* extension) {
  NotifyRegistriesHelper(&RulesRegistry::OnExtensionUninstalled, extension);
}

bool RulesRegistryService::HasRulesRegistryForTesting(
    int rules_registry_id,
    const std::string& event_name) {
  return rule_registries_.contains(
      RulesRegistryKey(event_name, rules_registry_id));
}

void RulesRegistryService::OnUpdateRules() {
  // Forward rule updates to observers.
  for (auto& observer : observers_)
    observer.OnUpdateRules();
}

scoped_refptr<RulesRegistry>
RulesRegistryService::RegisterWebRequestRulesRegistry(
    int rules_registry_id,
    RulesCacheDelegate::Type cache_delegate_type) {
  DCHECK(browser_context_);
  DCHECK(!rule_registries_.contains(RulesRegistryKey(
      declarative_webrequest_constants::kOnRequest, rules_registry_id)));

  auto web_request_cache_delegate =
      std::make_unique<RulesCacheDelegate>(cache_delegate_type);
  auto web_request_rules_registry =
      base::MakeRefCounted<WebRequestRulesRegistry>(
          browser_context_, web_request_cache_delegate.get(),
          rules_registry_id);
  web_request_cache_delegate->AddObserver(this);
  cache_delegates_.push_back(std::move(web_request_cache_delegate));
  RegisterRulesRegistry(web_request_rules_registry);
  WebRequestEventRouter::Get(browser_context_)
      ->RegisterRulesRegistry(browser_context_, rules_registry_id,
                              web_request_rules_registry);
  return web_request_rules_registry;
}

void RulesRegistryService::EnsureDefaultRulesRegistriesRegistered() {
  DCHECK(browser_context_);
  DCHECK(!rule_registries_.contains(
      RulesRegistryKey(declarative_webrequest_constants::kOnRequest,
                       rules_registry_ids::kDefaultRulesRegistryID)));

  // Only register the default web request rules registry if the
  // declarativeWebRequest API is enabled. See crbug.com/41302429.
  const bool is_api_enabled =
      FeatureProvider::GetAPIFeature("declarativeWebRequest")
          ->IsAvailableToEnvironment(
              util::GetBrowserContextId(browser_context_))
          .is_available();
  if (is_api_enabled) {
    // Persist the cache since it pertains to regular pages (i.e. not webviews).
    RegisterWebRequestRulesRegistry(rules_registry_ids::kDefaultRulesRegistryID,
                                    RulesCacheDelegate::Type::kPersistent);
  }

  // Create the ContentRulesRegistry.
  DCHECK(!content_rules_registry_);
  auto content_rules_cache_delegate = std::make_unique<RulesCacheDelegate>(
      RulesCacheDelegate::Type::kPersistent);
  scoped_refptr<ContentRulesRegistry> content_rules_registry =
      ExtensionsAPIClient::Get()->CreateContentRulesRegistry(
          browser_context_, content_rules_cache_delegate.get());
  if (content_rules_registry) {
    content_rules_cache_delegate->AddObserver(this);
    cache_delegates_.push_back(std::move(content_rules_cache_delegate));
    RegisterRulesRegistry(content_rules_registry);
    content_rules_registry_ = content_rules_registry.get();
  }
}

void RulesRegistryService::NotifyRegistriesHelper(
    void (RulesRegistry::*notification_callback)(const Extension*),
    const Extension* extension) {
  RulesRegistryMap::iterator i;
  for (i = rule_registries_.begin(); i != rule_registries_.end(); ++i) {
    scoped_refptr<RulesRegistry> registry = i->second;
    (registry.get()->*notification_callback)(extension);
  }
}

void RulesRegistryService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  NotifyRegistriesHelper(&RulesRegistry::OnExtensionLoaded, extension);
}

void RulesRegistryService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  NotifyRegistriesHelper(&RulesRegistry::OnExtensionUnloaded, extension);
}

void RulesRegistryService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  NotifyRegistriesHelper(&RulesRegistry::OnExtensionUninstalled, extension);
}

}  // namespace extensions
