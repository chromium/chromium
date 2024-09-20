// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/rules_registry_service.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace extensions {

const int RulesRegistryService::kDefaultRulesRegistryID = 0;
const int RulesRegistryService::kInvalidRulesRegistryID = -1;

RulesRegistryService::RulesRegistryService(content::BrowserContext* context)
    : current_rules_registry_id_(kDefaultRulesRegistryID),
      content_rules_registry_(nullptr),
      browser_context_(context) {
  if (browser_context_) {
    extension_registry_observation_.Observe(
        ExtensionRegistry::Get(browser_context_));
    EnsureDefaultRulesRegistriesRegistered();
  }
}

RulesRegistryService::~RulesRegistryService() = default;

int RulesRegistryService::GetNextRulesRegistryID() {
  return ++current_rules_registry_id_;
}

void RulesRegistryService::Shutdown() {
  // Notify the registries.
  for (const auto& [_, registry] : rule_registries_) {
    registry->OnShutdown();
  }

  // Release the references to all registries, and remove the default registry
  // from ExtensionWebRequestEventRouter.
  rule_registries_.clear();
  // TODO(crbug.com/40264286): This could be moved to
  // WebRequestEventRouter::Shutdown when the new per-BrowserContext event
  // router is the only implementation. Or we might just remove it completely,
  // since that instance will be destroyed when this RulesRegistryService
  // instance is.
  WebRequestEventRouter::Get(browser_context_)
      ->RegisterRulesRegistry(browser_context_,
                              RulesRegistryService::kDefaultRulesRegistryID,
                              nullptr);
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
  DCHECK(!base::Contains(rule_registries_, key));
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
  // construction.
  if (!browser_context_ || rules_registry_id == kDefaultRulesRegistryID) {
    return nullptr;
  }

  // Only web request rules registries are created for webviews.
  DCHECK_EQ(declarative_webrequest_constants::kOnRequest, event_name);

  scoped_refptr<RulesRegistry> registry = RegisterWebRequestRulesRegistry(
      rules_registry_id, RulesCacheDelegate::Type::kEphemeral);
  DCHECK(base::Contains(rule_registries_, key));
  return registry;
}

void RulesRegistryService::RemoveRulesRegistriesByID(int rules_registry_id) {
  std::set<RulesRegistryKey> registries_to_delete;
  for (auto& [key, rule_registry] : rule_registries_) {
    if (key.rules_registry_id != rules_registry_id) {
      continue;
    }
    // Modifying a container while iterating over it can lead to badness. So we
    // save the keys in another container and delete them in another loop.
    registries_to_delete.insert(key);
  }

  for (const auto& registry : registries_to_delete) {
    rule_registries_.erase(registry);
  }
}

bool RulesRegistryService::HasAnyRegisteredRules() const {
  return base::ranges::any_of(
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
  return base::Contains(rule_registries_,
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
  DCHECK(!base::Contains(
      rule_registries_,
      RulesRegistryKey(declarative_webrequest_constants::kOnRequest,
                       rules_registry_id)));

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
  DCHECK(!base::Contains(
      rule_registries_,
      RulesRegistryKey(declarative_webrequest_constants::kOnRequest,
                       kDefaultRulesRegistryID)));

  // Only register the default web request rules registry if the
  // declarativeWebRequest API is enabled. See crbug.com/693243.
  const bool is_api_enabled =
      FeatureProvider::GetAPIFeature("declarativeWebRequest")
          ->IsAvailableToEnvironment(
              util::GetBrowserContextId(browser_context_))
          .is_available();
  if (is_api_enabled) {
    // Persist the cache since it pertains to regular pages (i.e. not webviews).
    RegisterWebRequestRulesRegistry(kDefaultRulesRegistryID,
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
