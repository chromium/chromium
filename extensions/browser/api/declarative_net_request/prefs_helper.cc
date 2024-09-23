// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/prefs_helper.h"

#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace extensions::declarative_net_request {

namespace {

// Additional preferences keys, which are not needed by external clients.

// Key corresponding to which we store a ruleset's checksum for the Declarative
// Net Request API.
constexpr std::string_view kChecksumKey = "checksum";

// Key corresponding to which we store a ruleset's disabled rule ids for the
// Declarative Net Request API.
constexpr std::string_view kDisabledStaticRuleIds =
    "dnr_disabled_static_rule_ids";

// Key corresponding to the list of enabled static ruleset IDs for an extension.
// Used for the Declarative Net Request API.
constexpr std::string_view kEnabledStaticRulesetIDs = "dnr_enabled_ruleset_ids";

// A preference that indicates the amount of rules allocated to an extension
// from the global pool.
constexpr std::string_view kExtensionRulesAllocated =
    "dnr_extension_rules_allocated";

// A boolean that indicates if a ruleset should be ignored.
constexpr std::string_view kIgnoreRulesetKey = "ignore_ruleset";

// A boolean that indicates if an extension should have its unused rule
// allocation kept during its next load.
constexpr std::string_view kKeepExcessAllocation = "dnr_keep_excess_allocation";

// A boolean preference that indicates whether the extension's icon should be
// automatically badged to the matched action count for a tab. False by default.
constexpr std::string_view kUseActionCountAsBadgeText =
    "dnr_use_action_count_as_badge_text";

// Stores preferences corresponding to dynamic indexed ruleset for the
// Declarative Net Request API. Note: we use a separate preference key for
// dynamic rulesets instead of using the `kDNRStaticRulesetPref` dictionary.
// This is because the `kDNRStaticRulesetPref` dictionary is re-populated on
// each packed extension update and also on reloads of unpacked extensions.
// However for both of these cases, we want the dynamic ruleset preferences to
// stay unchanged. Also, this helps provide flexibility to have the dynamic
// ruleset preference schema diverge from the static one.
constexpr std::string_view kDynamicRulesetPref = "dnr_dynamic_ruleset";

base::flat_set<int> GetDisabledStaticRuleIdsFromDict(
    const base::Value::Dict* disabled_rule_ids_dict,
    RulesetID ruleset_id) {
  if (!disabled_rule_ids_dict) {
    return {};
  }

  const base::Value::List* disabled_rule_id_list =
      disabled_rule_ids_dict->FindList(
          base::NumberToString(ruleset_id.value()));
  if (!disabled_rule_id_list) {
    // Just ignore the prefs value if it is corrupted.
    // TODO(blee@igalia.com) The corrupted prefs value should be recovered by
    // wiping the value so that the extension returns to the sane state.
    return {};
  }

  base::flat_set<int> disabled_rule_ids;
  for (const base::Value& disabled_rule_id : *disabled_rule_id_list) {
    if (!disabled_rule_id.is_int()) {
      // Just ignore the prefs value if it is corrupted.
      // TODO(blee@igalia.com) The corrupted prefs value should be recovered by
      // wiping the value so that the extension returns to the sane state.
      return {};
    }

    disabled_rule_ids.insert(disabled_rule_id.GetInt());
  }

  return disabled_rule_ids;
}

size_t CountDisabledRules(const base::Value::Dict* disabled_rule_ids_dict) {
  if (!disabled_rule_ids_dict) {
    return 0;
  }

  size_t count = 0;
  for (const auto [key, value] : *disabled_rule_ids_dict) {
    if (!value.is_list()) {
      continue;
    }
    count += value.GetList().size();
  }
  return count;
}

bool ReadPrefAsBooleanAndReturn(const ExtensionPrefs& prefs,
                                const ExtensionId& extension_id,
                                std::string_view key) {
  bool value = false;
  if (prefs.ReadPrefAsBoolean(extension_id, key, &value)) {
    return value;
  }

  return false;
}

}  // namespace

PrefsHelper::PrefsHelper(ExtensionPrefs& prefs)
    : prefs_(prefs) {}
PrefsHelper::~PrefsHelper() = default;

PrefsHelper::RuleIdsToUpdate::RuleIdsToUpdate(
    const std::optional<std::vector<int>>& ids_to_disable,
    const std::optional<std::vector<int>>& ids_to_enable) {
  if (ids_to_disable) {
    this->ids_to_disable.insert(ids_to_disable->begin(), ids_to_disable->end());
  }

  if (ids_to_enable) {
    for (int id : *ids_to_enable) {
      // |ids_to_disable| takes priority over |ids_to_enable|.
      if (base::Contains(this->ids_to_disable, id)) {
        continue;
      }
      this->ids_to_enable.insert(id);
    }
  }
}

PrefsHelper::RuleIdsToUpdate::RuleIdsToUpdate(
    RuleIdsToUpdate&& other) = default;
PrefsHelper::RuleIdsToUpdate::~RuleIdsToUpdate() = default;

PrefsHelper::UpdateDisabledStaticRulesResult::
    UpdateDisabledStaticRulesResult() = default;
PrefsHelper::UpdateDisabledStaticRulesResult::
    UpdateDisabledStaticRulesResult(UpdateDisabledStaticRulesResult&& other) =
        default;
PrefsHelper::UpdateDisabledStaticRulesResult::
    ~UpdateDisabledStaticRulesResult() = default;

const base::Value::Dict*
PrefsHelper::GetDisabledRuleIdsDict(
    const ExtensionId& extension_id) const {
  return prefs_->ReadPrefAsDict(
      extension_id,
      ExtensionPrefs::JoinPrefs(
          {ExtensionPrefs::kDNRStaticRulesetPref, kDisabledStaticRuleIds}));
}

base::flat_set<int> PrefsHelper::GetDisabledStaticRuleIds(
    const ExtensionId& extension_id,
    RulesetID ruleset_id) const {
  return GetDisabledStaticRuleIdsFromDict(GetDisabledRuleIdsDict(extension_id),
                                          ruleset_id);
}

size_t PrefsHelper::GetDisabledStaticRuleCount(
    const ExtensionId& extension_id) const {
  return CountDisabledRules(GetDisabledRuleIdsDict(extension_id));
}

void PrefsHelper::SetDisabledStaticRuleIds(
    const ExtensionId& extension_id,
    RulesetID ruleset_id,
    const base::flat_set<int>& disabled_rule_ids) {
  std::string key = ExtensionPrefs::JoinPrefs(
      {ExtensionPrefs::kDNRStaticRulesetPref, kDisabledStaticRuleIds});

  ExtensionPrefs::ScopedDictionaryUpdate update(&*prefs_, extension_id, key);

  if (disabled_rule_ids.empty()) {
    std::unique_ptr<prefs::DictionaryValueUpdate> disabled_rule_ids_dict =
        update.Get();
    if (disabled_rule_ids_dict) {
      disabled_rule_ids_dict->Remove(base::NumberToString(ruleset_id.value()));
    }
    return;
  }

  std::unique_ptr<prefs::DictionaryValueUpdate> disabled_rule_ids_dict =
      update.Create();

  base::Value::List ids_list;
  ids_list.reserve(disabled_rule_ids.size());
  for (int id : disabled_rule_ids) {
    ids_list.Append(id);
  }

  disabled_rule_ids_dict->Set(base::NumberToString(ruleset_id.value()),
                              base::Value(std::move(ids_list)));
}

PrefsHelper::UpdateDisabledStaticRulesResult
PrefsHelper::UpdateDisabledStaticRules(
    const ExtensionId& extension_id,
    RulesetID ruleset_id,
    const RuleIdsToUpdate& rule_ids_to_update) {
  UpdateDisabledStaticRulesResult result;

  const base::Value::Dict* disabled_rule_ids_dict =
      GetDisabledRuleIdsDict(extension_id);

  base::flat_set<int> old_disabled_rule_ids(
      GetDisabledStaticRuleIdsFromDict(disabled_rule_ids_dict, ruleset_id));

  for (int id : old_disabled_rule_ids) {
    if (base::Contains(rule_ids_to_update.ids_to_enable, id)) {
      result.changed = true;
      continue;
    }
    result.disabled_rule_ids_after_update.insert(id);
  }
  for (int id : rule_ids_to_update.ids_to_disable) {
    auto pair = result.disabled_rule_ids_after_update.insert(id);
    if (pair.second) {
      result.changed = true;
    }
  }

  if (!result.changed) {
    result.disabled_rule_ids_after_update.clear();
    return result;
  }

  int count_before = old_disabled_rule_ids.size();
  int count_after = result.disabled_rule_ids_after_update.size();
  int new_count =
      CountDisabledRules(disabled_rule_ids_dict) + count_after - count_before;
  DCHECK_GE(new_count, 0);

  if (new_count > GetDisabledStaticRuleLimit()) {
    result.error = kDisabledStaticRuleCountExceeded;
    result.changed = false;
    result.disabled_rule_ids_after_update.clear();
    return result;
  }

  SetDisabledStaticRuleIds(extension_id, ruleset_id,
                           result.disabled_rule_ids_after_update);
  return result;
}

bool PrefsHelper::GetStaticRulesetChecksum(
    const ExtensionId& extension_id,
    declarative_net_request::RulesetID ruleset_id,
    int& checksum) const {
  std::string pref = ExtensionPrefs::JoinPrefs(
      {ExtensionPrefs::kDNRStaticRulesetPref,
       base::NumberToString(ruleset_id.value()), kChecksumKey});
  return prefs_->ReadPrefAsInteger(extension_id, pref, &checksum);
}

void PrefsHelper::SetStaticRulesetChecksum(
    const ExtensionId& extension_id,
    declarative_net_request::RulesetID ruleset_id,
    int checksum) {
  std::string pref = ExtensionPrefs::JoinPrefs(
      {ExtensionPrefs::kDNRStaticRulesetPref,
       base::NumberToString(ruleset_id.value()), kChecksumKey});
  prefs_->UpdateExtensionPref(extension_id, pref, base::Value(checksum));
}

bool PrefsHelper::GetDynamicRulesetChecksum(const ExtensionId& extension_id,
                                            int& checksum) const {
  std::string pref =
      ExtensionPrefs::JoinPrefs({kDynamicRulesetPref, kChecksumKey});
  return prefs_->ReadPrefAsInteger(extension_id, pref, &checksum);
}

void PrefsHelper::SetDynamicRulesetChecksum(const ExtensionId& extension_id,
                                            int checksum) {
  std::string pref =
      ExtensionPrefs::JoinPrefs({kDynamicRulesetPref, kChecksumKey});
  prefs_->UpdateExtensionPref(extension_id, pref, base::Value(checksum));
}

std::optional<std::set<RulesetID>> PrefsHelper::GetEnabledStaticRulesets(
    const ExtensionId& extension_id) const {
  std::set<RulesetID> ids;
  const base::Value::List* ids_value =
      prefs_->ReadPrefAsList(extension_id, kEnabledStaticRulesetIDs);
  if (!ids_value) {
    return std::nullopt;
  }

  for (const base::Value& id_value : *ids_value) {
    if (!id_value.is_int()) {
      return std::nullopt;
    }

    ids.insert(RulesetID(id_value.GetInt()));
  }

  return ids;
}

void PrefsHelper::SetEnabledStaticRulesets(const ExtensionId& extension_id,
                                           const std::set<RulesetID>& ids) {
  base::Value::List ids_list;
  for (const auto& id : ids) {
    ids_list.Append(id.value());
  }

  prefs_->UpdateExtensionPref(extension_id, kEnabledStaticRulesetIDs,
                              base::Value(std::move(ids_list)));
}

bool PrefsHelper::GetUseActionCountAsBadgeText(
    const ExtensionId& extension_id) const {
  return ReadPrefAsBooleanAndReturn(*prefs_, extension_id,
                                    kUseActionCountAsBadgeText);
}

void PrefsHelper::SetUseActionCountAsBadgeText(
    const ExtensionId& extension_id,
    bool use_action_count_as_badge_text) {
  prefs_->UpdateExtensionPref(extension_id, kUseActionCountAsBadgeText,
                              base::Value(use_action_count_as_badge_text));
}

// Whether the ruleset for the given `extension_id` and `ruleset_id` should be
// ignored while loading the extension.
bool PrefsHelper::ShouldIgnoreRuleset(const ExtensionId& extension_id,
                                      RulesetID ruleset_id) const {
  std::string pref = ExtensionPrefs::JoinPrefs(
      {ExtensionPrefs::kDNRStaticRulesetPref,
       base::NumberToString(ruleset_id.value()), kIgnoreRulesetKey});
  return ReadPrefAsBooleanAndReturn(*prefs_, extension_id, pref);
}

// Returns the global rule allocation for the given |extension_id|. If no
// rules are allocated to the extension, false is returned.
bool PrefsHelper::GetAllocatedGlobalRuleCount(const ExtensionId& extension_id,
                                              int& rule_count) const {
  if (!prefs_->ReadPrefAsInteger(extension_id, kExtensionRulesAllocated,
                                 &rule_count)) {
    return false;
  }

  DCHECK_GT(rule_count, 0);

  return true;
}

void PrefsHelper::SetAllocatedGlobalRuleCount(const ExtensionId& extension_id,
                                              int rule_count) {
  DCHECK_LE(rule_count, GetGlobalStaticRuleLimit());

  // Clear the pref entry if the extension has a global allocation of 0.
  std::optional<base::Value> pref_value;
  if (rule_count > 0) {
    pref_value = base::Value(rule_count);
  }
  prefs_->UpdateExtensionPref(extension_id, kExtensionRulesAllocated,
                              std::move(pref_value));
}

bool PrefsHelper::GetKeepExcessAllocation(
    const ExtensionId& extension_id) const {
  return ReadPrefAsBooleanAndReturn(*prefs_, extension_id,
                                    kKeepExcessAllocation);
}

void PrefsHelper::SetKeepExcessAllocation(const ExtensionId& extension_id,
                                          bool keep_excess_allocation) {
  // Clear the pref entry if the extension will not keep its excess global rules
  // allocation.
  std::optional<base::Value> pref_value;
  if (keep_excess_allocation) {
    pref_value = base::Value(true);
  }
  prefs_->UpdateExtensionPref(extension_id, kKeepExcessAllocation,
                              std::move(pref_value));
}

}  // namespace extensions::declarative_net_request
