// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/declarative_net_request_prefs_helper.h"

#include <string>

#include "base/containers/contains.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace extensions::declarative_net_request {

namespace {

// Additional preferences keys, which are not needed by external clients.

// Key corresponding to which we store a ruleset's disabled rule ids for the
// Declarative Net Request API.
constexpr const char kDNRDisabledStaticRuleIds[] =
    "dnr_disabled_static_rule_ids";

base::flat_set<int> GetDisabledStaticRuleIdsFromDict(
    const base::Value::Dict* disabled_rule_ids_dict,
    RulesetID ruleset_id) {
  if (!disabled_rule_ids_dict)
    return {};

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
  if (!disabled_rule_ids_dict)
    return 0;

  size_t count = 0;
  for (const auto [key, value] : *disabled_rule_ids_dict) {
    if (!value.is_list())
      continue;
    count += value.GetList().size();
  }
  return count;
}

}  // namespace

DeclarativeNetRequestPrefsHelper::DeclarativeNetRequestPrefsHelper(
    ExtensionPrefs& prefs)
    : prefs_(prefs) {}
DeclarativeNetRequestPrefsHelper::~DeclarativeNetRequestPrefsHelper() = default;

DeclarativeNetRequestPrefsHelper::RuleIdsToUpdate::RuleIdsToUpdate(
    const absl::optional<std::vector<int>>& ids_to_disable,
    const absl::optional<std::vector<int>>& ids_to_enable) {
  if (ids_to_disable)
    this->ids_to_disable.insert(ids_to_disable->begin(), ids_to_disable->end());

  if (ids_to_enable) {
    for (int id : *ids_to_enable) {
      // |ids_to_disable| takes priority over |ids_to_enable|.
      if (base::Contains(this->ids_to_disable, id))
        continue;
      this->ids_to_enable.insert(id);
    }
  }
}

DeclarativeNetRequestPrefsHelper::RuleIdsToUpdate::RuleIdsToUpdate(
    RuleIdsToUpdate&& other) = default;
DeclarativeNetRequestPrefsHelper::RuleIdsToUpdate::~RuleIdsToUpdate() = default;

DeclarativeNetRequestPrefsHelper::UpdateDisabledStaticRulesResult::
    UpdateDisabledStaticRulesResult() = default;
DeclarativeNetRequestPrefsHelper::UpdateDisabledStaticRulesResult::
    UpdateDisabledStaticRulesResult(UpdateDisabledStaticRulesResult&& other) =
        default;
DeclarativeNetRequestPrefsHelper::UpdateDisabledStaticRulesResult::
    ~UpdateDisabledStaticRulesResult() = default;

const base::Value::Dict*
DeclarativeNetRequestPrefsHelper::GetDisabledRuleIdsDict(
    const ExtensionId& extension_id) const {
  return prefs_.ReadPrefAsDict(
      extension_id,
      ExtensionPrefs::JoinPrefs(
          {ExtensionPrefs::kDNRStaticRulesetPref, kDNRDisabledStaticRuleIds}));
}

base::flat_set<int> DeclarativeNetRequestPrefsHelper::GetDisabledStaticRuleIds(
    const ExtensionId& extension_id,
    RulesetID ruleset_id) const {
  return GetDisabledStaticRuleIdsFromDict(GetDisabledRuleIdsDict(extension_id),
                                          ruleset_id);
}

size_t DeclarativeNetRequestPrefsHelper::GetDisabledStaticRuleCount(
    const ExtensionId& extension_id) const {
  return CountDisabledRules(GetDisabledRuleIdsDict(extension_id));
}

void DeclarativeNetRequestPrefsHelper::SetDisabledStaticRuleIds(
    const ExtensionId& extension_id,
    RulesetID ruleset_id,
    const base::flat_set<int>& disabled_rule_ids) {
  std::string key = ExtensionPrefs::JoinPrefs(
      {ExtensionPrefs::kDNRStaticRulesetPref, kDNRDisabledStaticRuleIds});

  ExtensionPrefs::ScopedDictionaryUpdate update(&prefs_, extension_id, key);

  if (disabled_rule_ids.empty()) {
    std::unique_ptr<prefs::DictionaryValueUpdate> disabled_rule_ids_dict =
        update.Get();
    if (disabled_rule_ids_dict)
      disabled_rule_ids_dict->Remove(base::NumberToString(ruleset_id.value()));
    return;
  }

  std::unique_ptr<prefs::DictionaryValueUpdate> disabled_rule_ids_dict =
      update.Create();

  base::Value::List ids_list;
  ids_list.reserve(disabled_rule_ids.size());
  for (int id : disabled_rule_ids)
    ids_list.Append(id);

  disabled_rule_ids_dict->Set(base::NumberToString(ruleset_id.value()),
                              base::Value(std::move(ids_list)));
}

DeclarativeNetRequestPrefsHelper::UpdateDisabledStaticRulesResult
DeclarativeNetRequestPrefsHelper::UpdateDisabledStaticRules(
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
    if (pair.second)
      result.changed = true;
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

}  // namespace extensions::declarative_net_request