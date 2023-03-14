// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_PREFS_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_PREFS_HELPER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
class DeclarativeNetRequestPrefsHelper {
 public:
  explicit DeclarativeNetRequestPrefsHelper(ExtensionPrefs&);
  ~DeclarativeNetRequestPrefsHelper();

  // Prevent copy constructor and dynamic allocation so that the instance
  // lifecycle can be scoped by stack.
  DeclarativeNetRequestPrefsHelper(const DeclarativeNetRequestPrefsHelper&) =
      delete;
  void* operator new(size_t) = delete;

  // Struct that contains the rule ids to disable or enable.
  struct RuleIdsToUpdate {
    RuleIdsToUpdate(const absl::optional<std::vector<int>>& ids_to_disable,
                    const absl::optional<std::vector<int>>& ids_to_enable);
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
    absl::optional<std::string> error;
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

 private:
  const base::Value::Dict* GetDisabledRuleIdsDict(const ExtensionId&) const;
  void SetDisabledStaticRuleIds(const ExtensionId& extension_id,
                                RulesetID ruleset_id,
                                const base::flat_set<int>& disabled_rule_ids);

  const raw_ref<ExtensionPrefs> prefs_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_PREFS_HELPER_H_
