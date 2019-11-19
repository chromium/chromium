// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/rules_registry_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace extensions {

namespace {

void NotifyWithExtensionSafe(
    scoped_refptr<const Extension> extension,
    void (RulesRegistry::*notification_callback)(const Extension*),
    scoped_refptr<RulesRegistry> registry) {
  (registry.get()->*notification_callback)(extension.get());
}

}  // namespace

const int RulesRegistryService::kDefaultRulesRegistryID = 0;
const int RulesRegistryService::kInvalidRulesRegistryID = -1;

RulesRegistryService::RulesRegistryService(content::BrowserContext* context)
    : current_rules_registry_id_(kDefaultRulesRegistryID),
      content_rules_registry_(NULL),
      browser_context_(context) {
  if (browser_context_) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
    EnsureDefaultRulesRegistriesRegistered();
  }
}

RulesRegistryService::~RulesRegistryService() {}

int RulesRegistryService::GetNextRulesRegistryID() {
  return ++current_rules_registry_id_;
}

void RulesRegistryService::Shutdown() {
  // Release the references to all registries, and remove the default registry
  // from ExtensionWebRequestEventRouter.
  rule_registries_.clear();
  ExtensionWebRequestEventRouter::GetInstance()->RegisterRulesRegistry(
      browser_context_, RulesRegistryService::kDefaultRulesRegistryID, nullptr);
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
  DCHECK(rule_registries_.find(key) == rule_registries_.end());
  rule_registries_[key] = rule_registry;
}

scoped_refptr<RulesRegistry> RulesRegistryService::GetRulesRegistry(
    int rules_registry_id,
    const std::string& event_name) {
  RulesRegistryKey key(event_name, rules_registry_id);
  RulesRegistryMap::const_iterator i = rule_registries_.find(key);
  if (i != rule_registries_.end())
    return i->second;

  // We should have attempted creation of the default rule registries at
  // construction.
  if (!browser_context_ || rules_registry_id == kDefaultRulesRegistryID)
    return nullptr;

  // Only web request rules registries are created for webviews.
  DCHECK_EQ(declarative_webrequest_constants::kOnRequest, event_name);

  scoped_refptr<RulesRegistry> registry = RegisterWebRequestRulesRegistry(
      rules_registry_id, RulesCacheDelegate::Type::kEphemeral);
  DCHECK(base::Contains(rule_registries_, key));
  return registry;
}

void RulesRegistryService::RemoveRulesRegistriesByID(int rules_registry_id) {
  std::set<RulesRegistryKey> registries_to_delete;
  for (auto it = rule_registries_.begin(); it != rule_registries_.end(); ++it) {
    const RulesRegistryKey& key = it->first;
    if (key.rules_registry_id != rules_registry_id)
      continue;
    // Modifying a container while iterating over it can lead to badness. So we
    // save the keys in another container and delete them in another loop.
    registries_to_delete.insert(key);
  }

  for (auto it = registries_to_delete.begin(); it != registries_to_delete.end();
       ++it) {
    rule_registries_.erase(*it);
  }
}

bool RulesRegistryService::HasAnyRegisteredRules() const {
  return std::any_of(cache_delegates_.begin(), cache_delegates_.end(),
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

  auto web_request_cache_delegate = std::make_unique<RulesCacheDelegate>(
      cache_delegate_type, true /* log_storage_init_delay */);
  auto web_request_rules_registry =
      base::MakeRefCounted<WebRequestRulesRegistry>(
          browser_context_, web_request_cache_delegate.get(),
          rules_registry_id);
  web_request_cache_delegate->AddObserver(this);
  cache_delegates_.push_back(std::move(web_request_cache_delegate));
  RegisterRulesRegistry(web_request_rules_registry);
  ExtensionWebRequestEventRouter::GetInstance()->RegisterRulesRegistry(
      browser_context_, rules_registry_id, web_request_rules_registry);
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
          ->IsAvailableToEnvironment()
          .is_available();
  if (is_api_enabled) {
    // Persist the cache since it pertains to regular pages (i.e. not webviews).
    RegisterWebRequestRulesRegistry(kDefaultRulesRegistryID,
                                    RulesCacheDelegate::Type::kPersistent);
  }

  // Create the ContentRulesRegistry.
  DCHECK(!content_rules_registry_);
  auto content_rules_cache_delegate = std::make_unique<RulesCacheDelegate>(
      RulesCacheDelegate::Type::kPersistent,
      false /* log_storage_init_delay */);
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
    if (content::BrowserThread::CurrentlyOn(registry->owner_thread())) {
      (registry.get()->*notification_callback)(extension);
    } else {
      base::PostTask(FROM_HERE, {registry->owner_thread()},
                     base::BindOnce(&NotifyWithExtensionSafe,
                                    base::WrapRefCounted(extension),
                                    notification_callback, registry));
    }
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
