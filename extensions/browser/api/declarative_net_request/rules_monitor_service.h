// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_MONITOR_SERVICE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_MONITOR_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/global_rules_tracker.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class ExtensionPrefs;
class WarningService;

namespace api {
namespace declarative_net_request {
struct Rule;
}  // namespace declarative_net_request
}  // namespace api

namespace declarative_net_request {
class RulesetMatcher;
enum class DynamicRuleUpdateAction;
struct LoadRequestData;
struct RuleCounts;

// Observes loading and unloading of extensions to load and unload their
// rulesets for the Declarative Net Request API. Lives on the UI thread. Note: A
// separate instance of RulesMonitorService is not created for incognito. Both
// the incognito and normal contexts will share the same ruleset.
class RulesMonitorService : public BrowserContextKeyedAPI,
                            public ExtensionRegistryObserver {
 public:
  using ApiCallback =
      base::OnceCallback<void(std::optional<std::string> error)>;
  using ApiCallbackToGetDisabledRuleIds =
      base::OnceCallback<void(std::vector<int> disabled_rule_ids)>;

  // An observer used in tests.
  class TestObserver {
   public:
    // Called when the ruleset load (in response to extension load) is complete
    // for |extension_id|,
    virtual void OnRulesetLoadComplete(const ExtensionId& extension_id) = 0;

   protected:
    virtual ~TestObserver() = default;
  };

  RulesMonitorService(const RulesMonitorService&) = delete;
  RulesMonitorService& operator=(const RulesMonitorService&) = delete;

  // This is public so that it can be deleted by tests.
  ~RulesMonitorService() override;

  // Returns the instance for |browser_context|. An instance is shared between
  // an incognito and a regular context.
  static RulesMonitorService* Get(content::BrowserContext* browser_context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<RulesMonitorService>*
  GetFactoryInstance();

  static std::unique_ptr<RulesMonitorService> CreateInstanceForTesting(
      content::BrowserContext* context);

  // Updates the dynamic rules for the |extension| and then invokes
  // |callback| with an optional error.
  void UpdateDynamicRules(
      const Extension& extension,
      std::vector<int> rule_ids_to_remove,
      std::vector<api::declarative_net_request::Rule> rules_to_add,
      ApiCallback callback);

  // Updates the set of enabled static rulesets for the |extension| and then
  // invokes |callback| with an optional error.
  void UpdateEnabledStaticRulesets(const Extension& extension,
                                   std::set<RulesetID> ids_to_disable,
                                   std::set<RulesetID> ids_to_enable,
                                   ApiCallback callback);

  // Updates the set of disabled rule ids for the |ruleset_id| of the
  // |extension| and then invokes |callback| with an optional error.
  using RuleIdsToUpdate = PrefsHelper::RuleIdsToUpdate;
  void UpdateStaticRules(const Extension& extension,
                         RulesetID ruleset_id,
                         RuleIdsToUpdate rule_ids_to_update,
                         ApiCallback callback);

  // Get the set of disabled rule ids for the |ruleset_id| of the
  // |extension|. The disabled rule ids will be passed though the argument of
  // the |callback|.
  void GetDisabledRuleIds(const Extension& extension,
                          RulesetID ruleset_id,
                          ApiCallbackToGetDisabledRuleIds callback);

  // Returns the list of session scoped rules for |extension_id| as a
  // base::Value::List.
  const base::Value::List& GetSessionRulesValue(
      const ExtensionId& extension_id) const;

  // Returns a copy of the session scoped rules for the given |extension_id|.
  std::vector<api::declarative_net_request::Rule> GetSessionRules(
      const ExtensionId& extension_id) const;

  // Updates the session scoped rules for the given |extension_id|. Invokes
  // |callback| with an optional error.
  void UpdateSessionRules(
      const Extension& extension,
      std::vector<int> rule_ids_to_remove,
      std::vector<api::declarative_net_request::Rule> rules_to_add,
      ApiCallback callback);

  // Returns the RuleCounts for the |extension_id| and |ruleset_id| pair.
  RuleCounts GetRuleCounts(const ExtensionId& extension_id,
                           RulesetID ruleset_id) const;

  RulesetManager* ruleset_manager() { return &ruleset_manager_; }

  const ActionTracker& action_tracker() const { return action_tracker_; }
  ActionTracker& action_tracker() { return action_tracker_; }

  const GlobalRulesTracker& global_rules_tracker() const {
    return global_rules_tracker_;
  }
  GlobalRulesTracker& global_rules_tracker() { return global_rules_tracker_; }

  void SetObserverForTest(TestObserver* observer) { test_observer_ = observer; }

  bool HasAnyExtraHeadersMatcher() const {
    return ruleset_manager_.HasAnyExtraHeadersMatcher();
  }

 private:
  class FileSequenceBridge;
  class ApiCallQueue;

  friend class BrowserContextKeyedAPIFactory<RulesMonitorService>;

  // The constructor is kept private since this should only be created by the
  // BrowserContextKeyedAPIFactory.
  explicit RulesMonitorService(content::BrowserContext* browser_context);

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "RulesMonitorService"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // ExtensionRegistryObserver implementation.
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  // Internal helper for UpdateDynamicRules.
  void UpdateDynamicRulesInternal(
      const ExtensionId& extension_id,
      std::vector<int> rule_ids_to_remove,
      std::vector<api::declarative_net_request::Rule> rules_to_add,
      ApiCallback callback);

  // Internal helper for UpdateEnabledStaticRulesets.
  void UpdateEnabledStaticRulesetsInternal(const ExtensionId& extension_id,
                                           std::set<RulesetID> ids_to_disable,
                                           std::set<RulesetID> ids_to_enable,
                                           ApiCallback callback);

  // Internal helper for UpdateStaticRules.
  void UpdateStaticRulesInternal(const ExtensionId& extension_id,
                                 RulesetID ruleset_id,
                                 RuleIdsToUpdate rule_ids_to_update,
                                 ApiCallback callback);

  // Internal helper for GetDisabledRuleIds.
  void GetDisabledRuleIdsInternal(const ExtensionId& extension_id,
                                  RulesetID ruleset_id,
                                  ApiCallbackToGetDisabledRuleIds callback);

  // Internal helper for UpdateSessionRules.
  void UpdateSessionRulesInternal(
      const ExtensionId& extension_id,
      std::vector<int> rule_ids_to_remove,
      std::vector<api::declarative_net_request::Rule> rules_to_add,
      ApiCallback callback);

  // Invoked when we have loaded the rulesets in |load_data| on
  // |file_task_runner_| in response to OnExtensionLoaded.
  void OnInitialRulesetsLoadedFromDisk(LoadRequestData load_data);

  // Invoked when rulesets are loaded in response to
  // UpdateEnabledStaticRulesets.
  void OnNewStaticRulesetsLoaded(ApiCallback callback,
                                 std::set<RulesetID> ids_to_disable,
                                 std::set<RulesetID> ids_to_enable,
                                 LoadRequestData load_data);

  // Invoked when the dynamic rules for the extension have been updated in
  // response to UpdateDynamicRules.
  void OnDynamicRulesUpdated(ApiCallback callback,
                             LoadRequestData load_data,
                             std::optional<std::string> error);

  // Unloads all rulesets for the given |extension_id|.
  void RemoveCompositeMatcher(const ExtensionId& extension_id);

  // Creates and adds a `CompositeMatcher` for the given `extension`.
  void AddCompositeMatcher(const Extension& extension,
                           CompositeMatcher::MatcherList matchers);

  // Adds the given `ruleset_matcher` to the set of matchers for the given
  // `extension`. If a RulesetMatcher with the same ID is already present for
  // the `extension`, it is replaced.
  void UpdateRulesetMatcher(const Extension& extension,
                            std::unique_ptr<RulesetMatcher> ruleset_matcher);

  // Logs metrics related to the result of loading rulesets and updates ruleset
  // checksum in preferences from |load_data|.
  void LogMetricsAndUpdateChecksumsIfNeeded(const LoadRequestData& load_data);

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  // Helper to bridge tasks to a sequence which allows file IO.
  std::unique_ptr<const FileSequenceBridge> file_sequence_bridge_;

  // Guaranteed to be valid through-out the lifetime of this instance.
  const raw_ptr<ExtensionPrefs> prefs_;
  const raw_ptr<ExtensionRegistry> extension_registry_;
  const raw_ptr<WarningService> warning_service_;

  const raw_ptr<content::BrowserContext> context_;

  declarative_net_request::RulesetManager ruleset_manager_;

  ActionTracker action_tracker_;

  GlobalRulesTracker global_rules_tracker_;

  // Non-owned pointer.
  raw_ptr<TestObserver> test_observer_ = nullptr;

  // Api call queues to ensure only one api call of the given type proceeds at a
  // time. Only maintained for enabled extensions.
  std::map<ExtensionId, ApiCallQueue> update_enabled_rulesets_queue_map_;
  std::map<ExtensionId, ApiCallQueue>
      update_dynamic_or_session_rules_queue_map_;

  // Session scoped rules value corresponding to extensions.
  // TODO(crbug.com/40733652): Currently we are storing session scoped rules in
  // two forms: one as a base::Value::List and second in the indexed format as
  // part of RulesetMatcher, leading to double memory usage. We should be able
  // to do away with the base::Value::List representation.
  base::flat_map<ExtensionId, base::Value::List> session_rules_;

  // Must be the last member variable. See WeakPtrFactory documentation for
  // details.
  base::WeakPtrFactory<RulesMonitorService> weak_factory_{this};
};

}  // namespace declarative_net_request

template <>
void BrowserContextKeyedAPIFactory<
    declarative_net_request::RulesMonitorService>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_MONITOR_SERVICE_H_
