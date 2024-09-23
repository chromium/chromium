// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PREFS_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PREFS_HELPER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class ExtensionPrefs;

namespace declarative_net_request {

// ExtensionPrefs helper class for DeclarativeNetRequest.
// This class is stateless, as it is intended to provides one-off operations
// that hits ExtensionPrefs whenever DeclarativeNetRequest preference value
// is fetched or set.
// If any performance issues are noticed, we could look into optimizing this
// class to keep a bit more state. (e.g. keeping disabled static rule count
// and updating it whenever UpdateDisabledStaticRules() is called, so that
// GetDisabledStaticRuleCount() doesn't hit ExtensionPrefs to calculate the
// size each time)
class PrefsHelper {
 public:
  explicit PrefsHelper(ExtensionPrefs&);
  ~PrefsHelper();

  // Prevent copy constructor and dynamic allocation so that the instance
  // lifecycle can be scoped by stack.
  PrefsHelper(const PrefsHelper&) = delete;
  void* operator new(size_t) = delete;

  // Struct that contains the rule ids to disable or enable.
  struct RuleIdsToUpdate {
    RuleIdsToUpdate(const std::optional<std::vector<int>>& ids_to_disable,
                    const std::optional<std::vector<int>>& ids_to_enable);
    RuleIdsToUpdate(RuleIdsToUpdate&&);
    ~RuleIdsToUpdate();

    bool Empty() { return ids_to_disable.empty() && ids_to_enable.empty(); }

    base::flat_set<int> ids_to_disable;
    base::flat_set<int> ids_to_enable;
  };

  // Struct that contains UpdateDisabledStaticRules() result.
  struct UpdateDisabledStaticRulesResult {
    UpdateDisabledStaticRulesResult();
    UpdateDisabledStaticRulesResult(UpdateDisabledStaticRulesResult&&);
    ~UpdateDisabledStaticRulesResult();

    // True if the UpdateDisabledStaticRules() call changed the disabled rule
    // ids.
    bool changed = false;

    // Disabled rule ids after update. (empty if it was not changed)
    base::flat_set<int> disabled_rule_ids_after_update;

    // Error while updating the disabled rule ids.
    std::optional<std::string> error;
  };

  // Returns the set of disabled rule ids of a static ruleset.
  base::flat_set<int> GetDisabledStaticRuleIds(const ExtensionId& extension_id,
                                               RulesetID ruleset_id) const;

  // Returns the disabled static rule count for the given |extension_id|.
  size_t GetDisabledStaticRuleCount(const ExtensionId& extension_id) const;

  // Updates the set of disabled rule ids of a static ruleset.
  UpdateDisabledStaticRulesResult UpdateDisabledStaticRules(
      const ExtensionId& extension_id,
      RulesetID ruleset_id,
      const RuleIdsToUpdate& rule_ids_to_update);

  // Returns false if there is no ruleset checksum corresponding to the given
  // |extension_id| and |ruleset_id|. On success, returns true and populates the
  // checksum.
  bool GetStaticRulesetChecksum(const ExtensionId& extension_id,
                                RulesetID ruleset_id,
                                int& checksum) const;

  void SetStaticRulesetChecksum(const ExtensionId& extension_id,
                                RulesetID ruleset_id,
                                int checksum);

  // Returns false if there is no dynamic ruleset corresponding to
  // `extension_id`. On success, returns true and populates the checksum.
  bool GetDynamicRulesetChecksum(const ExtensionId& extension_id,
                                 int& checksum) const;
  void SetDynamicRulesetChecksum(const ExtensionId& extension_id, int checksum);

  // Returns the set of enabled static ruleset IDs or std::nullopt if the
  // extension hasn't updated the set of enabled static rulesets.
  std::optional<std::set<RulesetID>> GetEnabledStaticRulesets(
      const ExtensionId& extension_id) const;
  // Updates the set of enabled static rulesets for the `extension_id`. This
  // preference gets cleared on extension update.
  void SetEnabledStaticRulesets(const ExtensionId& extension_id,
                                const std::set<RulesetID>& ids);

  // Whether the extension with the given `extension_id` is using its ruleset's
  // matched action count for the badge text. This is set via the
  // setExtensionActionOptions API call.
  bool GetUseActionCountAsBadgeText(const ExtensionId& extension_id) const;
  void SetUseActionCountAsBadgeText(const ExtensionId& extension_id,
                                    bool use_action_count_as_badge_text);

  // Whether the ruleset for the given `extension_id` and `ruleset_id` should be
  // ignored while loading the extension.
  bool ShouldIgnoreRuleset(const ExtensionId& extension_id,
                           RulesetID ruleset_id) const;

  // Returns the global rule allocation for the given `extension_id`. If no
  // rules are allocated to the extension, false is returned.
  bool GetAllocatedGlobalRuleCount(const ExtensionId& extension_id,
                                   int& rule_count) const;
  void SetAllocatedGlobalRuleCount(const ExtensionId& extension_id,
                                   int rule_count);

  // Whether the extension with the given `extension_id` should have its excess
  // global rules allocation kept during its next load.
  bool GetKeepExcessAllocation(const ExtensionId& extension_id) const;
  void SetKeepExcessAllocation(const ExtensionId& extension_id,
                               bool keep_excess_allocation);

 private:
  const base::Value::Dict* GetDisabledRuleIdsDict(const ExtensionId&) const;
  void SetDisabledStaticRuleIds(const ExtensionId& extension_id,
                                RulesetID ruleset_id,
                                const base::flat_set<int>& disabled_rule_ids);

  const raw_ref<ExtensionPrefs> prefs_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PREFS_HELPER_H_
