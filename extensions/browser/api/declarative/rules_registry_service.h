// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/declarative/rules_cache_delegate.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/api/web_request/web_request_event_router_factory.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ContentRulesRegistry;
}

namespace extensions {

// This class owns all RulesRegistries implementations of an ExtensionService.
// This class lives on the UI thread.
class RulesRegistryService : public BrowserContextKeyedAPI,
                             public ExtensionRegistryObserver,
                             public RulesCacheDelegate::Observer {
 public:
  static const int kDefaultRulesRegistryID;
  static const int kInvalidRulesRegistryID;

  struct RulesRegistryKey {
    std::string event_name;
    int rules_registry_id;
    RulesRegistryKey(const std::string& event_name, int rules_registry_id)
        : event_name(event_name), rules_registry_id(rules_registry_id) {}
    bool operator<(const RulesRegistryKey& other) const {
      return std::tie(event_name, rules_registry_id) <
             std::tie(other.event_name, other.rules_registry_id);
    }
  };

  class Observer {
   public:
    // Called when any of the |cache_delegates_| have rule updates.
    virtual void OnUpdateRules() = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit RulesRegistryService(content::BrowserContext* context);

  RulesRegistryService(const RulesRegistryService&) = delete;
  RulesRegistryService& operator=(const RulesRegistryService&) = delete;

  ~RulesRegistryService() override;

  // Unregisters refptrs to concrete RulesRegistries at other objects that were
  // created by us so that the RulesRegistries can be released.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<RulesRegistryService>*
      GetFactoryInstance();

  // Convenience method to get the RulesRegistryService for a context. If a
  // RulesRegistryService does not already exist for |context|, one will be
  // created and returned.
  static RulesRegistryService* Get(content::BrowserContext* context);

  // The same as Get(), except that if a RulesRegistryService does not already
  // exist for |context|, nullptr is returned.
  static RulesRegistryService* GetIfExists(content::BrowserContext* context);

  int GetNextRulesRegistryID();

  // Registers a RulesRegistry and wraps it in an InitializingRulesRegistry.
  void RegisterRulesRegistry(scoped_refptr<RulesRegistry> rule_registry);

  // Returns the RulesRegistry for |event_name| and |rules_registry_id|.
  // Attempts to create and register the rules registry if necessary. Might
  // return null if no corresponding rules registry was registered.
  scoped_refptr<RulesRegistry> GetRulesRegistry(int rules_registry_id,
                                                const std::string& event_name);

  // Remove all rules registries of the given rules_registry_id.
  void RemoveRulesRegistriesByID(int rules_registry_id);

  // Accessors for each type of rules registry.
  ContentRulesRegistry* content_rules_registry() const {
    CHECK(content_rules_registry_);
    return content_rules_registry_;
  }

  // Indicates whether any registry has rules registered.
  bool HasAnyRegisteredRules() const;

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // For testing.
  void SimulateExtensionUninstalled(const Extension* extension);

  // For testing. Returns true if `rule_registries_` has the RulesRegistry for
  // `event_name` and `rules_registry_id`.
  bool HasRulesRegistryForTesting(int rules_registry_id,
                                  const std::string& event_name);

 private:
  friend class BrowserContextKeyedAPIFactory<RulesRegistryService>;

  scoped_refptr<RulesRegistry> RegisterWebRequestRulesRegistry(
      int rules_registry_id,
      RulesCacheDelegate::Type cache_delegate_type);

  // Registers the default RulesRegistries used in Chromium.
  void EnsureDefaultRulesRegistriesRegistered();

  // Maps <event name, rules registry ID> to RuleRegistries that handle these
  // events.
  typedef std::map<RulesRegistryKey, scoped_refptr<RulesRegistry> >
      RulesRegistryMap;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // RulesCacheDelegate::Observer implementation.
  void OnUpdateRules() override;

  // Iterates over all registries, and calls |notification_callback| on them
  // with |extension| as the argument. If a registry lives on a different
  // thread, the call is posted to that thread, so no guarantee of synchronous
  // processing.
  void NotifyRegistriesHelper(
      void (RulesRegistry::*notification_callback)(const Extension*),
      const Extension* extension);

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "RulesRegistryService";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  int current_rules_registry_id_;

  RulesRegistryMap rule_registries_;

  // We own the parts of the registries which need to run on the UI thread.
  std::vector<std::unique_ptr<RulesCacheDelegate>> cache_delegates_;

  // Weak pointer into rule_registries_ to make it easier to handle content rule
  // conditions.
  raw_ptr<ContentRulesRegistry, AcrossTasksDanglingUntriaged>
      content_rules_registry_;

  // Listen to extension load, unloaded notification.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  raw_ptr<content::BrowserContext> browser_context_;

  base::ObserverList<Observer>::Unchecked observers_;
};

template <>
struct BrowserContextFactoryDependencies<RulesRegistryService> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<RulesRegistryService>* factory) {
    factory->DependsOn(
        ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
    factory->DependsOn(WebRequestEventRouterFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
