// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/rules_registry.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/api/declarative/rules_cache_delegate.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/api/declarative/declarative_manifest_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

const char kSuccess[] = "";
const char kDuplicateRuleId[] = "Duplicate rule ID: %s";
const char kErrorCannotRemoveManifestRules[] =
    "Rules declared in the 'event_rules' manifest field cannot be removed";

base::Value RulesToValue(const std::vector<const api::events::Rule*>& rules) {
  base::Value value(base::Value::Type::LIST);
  for (const auto* rule : rules)
    value.Append(std::move(*rule->ToValue()));
  return value;
}

std::vector<api::events::Rule> RulesFromValue(const base::Value* value) {
  std::vector<api::events::Rule> rules;

  const base::ListValue* list = NULL;
  if (!value || !value->GetAsList(&list))
    return rules;

  rules.reserve(list->GetSize());
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict = NULL;
    if (!list->GetDictionary(i, &dict))
      continue;
    api::events::Rule rule;
    if (api::events::Rule::Populate(*dict, &rule))
      rules.push_back(std::move(rule));
  }

  return rules;
}

std::string ToId(int identifier) {
  return base::StringPrintf("_%d_", identifier);
}

}  // namespace


// RulesRegistry

RulesRegistry::RulesRegistry(content::BrowserContext* browser_context,
                             const std::string& event_name,
                             content::BrowserThread::ID owner_thread,
                             RulesCacheDelegate* cache_delegate,
                             int id)
    : browser_context_(browser_context),
      owner_thread_(owner_thread),
      event_name_(event_name),
      id_(id),
      ready_(/*signaled=*/!cache_delegate),  // Immediately ready if no cache
                                             // delegate to wait for.
      last_generated_rule_identifier_id_(0) {
  if (cache_delegate) {
    cache_delegate_ = cache_delegate->GetWeakPtr();
    cache_delegate->Init(this);
  }
}

std::string RulesRegistry::AddRulesNoFill(
    const std::string& extension_id,
    std::vector<api::events::Rule> rules_in,
    RulesDictionary* destination,
    std::vector<const api::events::Rule*>* rules_out) {
  DCHECK_CURRENTLY_ON(owner_thread());

  // Verify that all rule IDs are new.
  for (const auto& rule : rules_in) {
    const RuleId& rule_id = *(rule.id);
    // Every rule should have a priority assigned.
    DCHECK(rule.priority);
    RulesDictionaryKey key(extension_id, rule_id);
    if (rules_.find(key) != rules_.end() ||
        manifest_rules_.find(key) != manifest_rules_.end())
      return base::StringPrintf(kDuplicateRuleId, rule_id.c_str());
  }

  std::vector<const api::events::Rule*> rule_ptrs;
  rule_ptrs.reserve(rules_in.size());
  for (auto& rule : rules_in)
    rule_ptrs.push_back(&rule);
  std::string error = AddRulesImpl(extension_id, rule_ptrs);

  if (!error.empty())
    return error;

  // Commit all rules into |rules_| on success.
  rule_ptrs.clear();
  for (auto& rule : rules_in) {
    const RuleId& rule_id = *(rule.id);
    RulesDictionaryKey key(extension_id, rule_id);

    auto insert_result = destination->emplace(key, std::move(rule));
    // All rule IDs are new so insert had better not have failed.
    DCHECK(insert_result.second);

    if (rules_out)
      rule_ptrs.push_back(&(insert_result.first->second));
  }
  if (rules_out)
    *rules_out = rule_ptrs;

  MaybeProcessChangedRules(extension_id);
  return kSuccess;
}

std::string RulesRegistry::AddRules(
    const std::string& extension_id,
    std::vector<api::events::Rule> rules_in,
    std::vector<const api::events::Rule*>* rules_out) {
  return AddRulesInternal(extension_id, std::move(rules_in), &rules_,
                          rules_out);
}

std::string RulesRegistry::AddRulesInternal(
    const std::string& extension_id,
    std::vector<api::events::Rule> rules_in,
    RulesDictionary* destination,
    std::vector<const api::events::Rule*>* rules_out) {
  DCHECK_CURRENTLY_ON(owner_thread());

  std::string error = CheckAndFillInOptionalRules(extension_id, &rules_in);
  if (!error.empty())
    return error;
  FillInOptionalPriorities(&rules_in);

  return AddRulesNoFill(extension_id, std::move(rules_in), destination,
                        rules_out);
}

std::string RulesRegistry::RemoveRules(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  DCHECK_CURRENTLY_ON(owner_thread());

  // Check if any of the rules are non-removable.
  for (RuleId rule_id : rule_identifiers) {
    RulesDictionaryKey lookup_key(extension_id, rule_id);
    auto itr = manifest_rules_.find(lookup_key);
    if (itr != manifest_rules_.end())
      return kErrorCannotRemoveManifestRules;
  }

  std::string error = RemoveRulesImpl(extension_id, rule_identifiers);

  if (!error.empty())
    return error;

  for (auto i = rule_identifiers.cbegin(); i != rule_identifiers.cend(); ++i) {
    RulesDictionaryKey lookup_key(extension_id, *i);
    rules_.erase(lookup_key);
  }

  MaybeProcessChangedRules(extension_id);
  RemoveUsedRuleIdentifiers(extension_id, rule_identifiers);
  return kSuccess;
}

std::string RulesRegistry::RemoveAllRules(const std::string& extension_id) {
  std::string result =
      RulesRegistry::RemoveAllRulesNoStoreUpdate(extension_id, false);
  MaybeProcessChangedRules(extension_id);  // Now update the prefs and store.
  return result;
}

std::string RulesRegistry::RemoveAllRulesNoStoreUpdate(
    const std::string& extension_id,
    bool remove_manifest_rules) {
  DCHECK_CURRENTLY_ON(owner_thread());

  std::string error = RemoveAllRulesImpl(extension_id);

  if (!error.empty())
    return error;

  auto remove_rules = [&extension_id](RulesDictionary& dictionary) {
    for (auto it = dictionary.begin(); it != dictionary.end();) {
      if (it->first.first == extension_id)
        dictionary.erase(it++);
      else
        ++it;
    }
  };
  remove_rules(rules_);
  if (remove_manifest_rules)
    remove_rules(manifest_rules_);

  RemoveAllUsedRuleIdentifiers(extension_id);
  return kSuccess;
}

void RulesRegistry::GetRules(const std::string& extension_id,
                             const std::vector<std::string>& rule_identifiers,
                             std::vector<const api::events::Rule*>* out) {
  DCHECK_CURRENTLY_ON(owner_thread());

  for (const auto& i : rule_identifiers) {
    RulesDictionaryKey lookup_key(extension_id, i);
    auto entry = rules_.find(lookup_key);
    if (entry != rules_.end())
      out->push_back(&entry->second);
    entry = manifest_rules_.find(lookup_key);
    if (entry != manifest_rules_.end())
      out->push_back(&entry->second);
  }
}

void RulesRegistry::GetRules(const std::string& extension_id,
                             RulesDictionary* rules,
                             std::vector<const api::events::Rule*>* out) {
  for (auto& i : *rules) {
    const RulesDictionaryKey& key = i.first;
    if (key.first == extension_id)
      out->push_back(&i.second);
  }
}

void RulesRegistry::GetAllRules(const std::string& extension_id,
                                std::vector<const api::events::Rule*>* out) {
  DCHECK_CURRENTLY_ON(owner_thread());
  GetRules(extension_id, &manifest_rules_, out);
  GetRules(extension_id, &rules_, out);
}

void RulesRegistry::OnExtensionUnloaded(const Extension* extension) {
  DCHECK_CURRENTLY_ON(owner_thread());
  std::string error = RemoveAllRulesImpl(extension->id());
  if (!error.empty())
    ReportInternalError(extension->id(), error);
}

void RulesRegistry::OnExtensionUninstalled(const Extension* extension) {
  DCHECK_CURRENTLY_ON(owner_thread());
  std::string error = RemoveAllRulesNoStoreUpdate(extension->id(), true);
  if (!error.empty())
    ReportInternalError(extension->id(), error);
}

void RulesRegistry::OnExtensionLoaded(const Extension* extension) {
  DCHECK_CURRENTLY_ON(owner_thread());

  std::vector<const api::events::Rule*> rules;
  GetAllRules(extension->id(), &rules);

  DeclarativeManifestData* declarative_data =
      DeclarativeManifestData::Get(extension);
  if (declarative_data) {
    std::vector<api::events::Rule> manifest_rules =
        declarative_data->RulesForEvent(event_name_);
    if (manifest_rules.size()) {
      std::string error =
          AddRulesInternal(extension->id(), std::move(manifest_rules),
                           &manifest_rules_, nullptr);
      if (!error.empty())
        ReportInternalError(extension->id(), error);
    }
  }

  std::string error = AddRulesImpl(extension->id(), rules);
  if (!error.empty())
    ReportInternalError(extension->id(), error);
}

size_t RulesRegistry::GetNumberOfUsedRuleIdentifiersForTesting() const {
  size_t entry_count = 0u;
  for (auto extension = used_rule_identifiers_.cbegin();
       extension != used_rule_identifiers_.cend(); ++extension) {
    // Each extension is counted as 1 just for being there. Otherwise we miss
    // keys with empty values.
    entry_count += 1u + extension->second.size();
  }
  return entry_count;
}

void RulesRegistry::DeserializeAndAddRules(const std::string& extension_id,
                                           std::unique_ptr<base::Value> rules) {
  DCHECK_CURRENTLY_ON(owner_thread());

  std::string error = AddRulesNoFill(extension_id, RulesFromValue(rules.get()),
                                     &rules_, nullptr);
  if (!error.empty())
    ReportInternalError(extension_id, error);
}

void RulesRegistry::ReportInternalError(const std::string& extension_id,
                                        const std::string& error) {
  std::unique_ptr<ExtensionError> error_instance(new InternalError(
      extension_id, base::ASCIIToUTF16(error), logging::LOG_ERROR));
  ExtensionsBrowserClient::Get()->ReportError(browser_context_,
                                              std::move(error_instance));
}

RulesRegistry::~RulesRegistry() {
}

void RulesRegistry::MarkReady(base::Time storage_init_time) {
  DCHECK_CURRENTLY_ON(owner_thread());

  if (!storage_init_time.is_null()) {
    UMA_HISTOGRAM_TIMES("Extensions.DeclarativeRulesStorageInitialization",
                        base::Time::Now() - storage_init_time);
  }

  ready_.Signal();
}

void RulesRegistry::ProcessChangedRules(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(owner_thread());

  DCHECK(base::Contains(process_changed_rules_requested_, extension_id));
  process_changed_rules_requested_[extension_id] = NOT_SCHEDULED_FOR_PROCESSING;

  std::vector<const api::events::Rule*> new_rules;
  GetRules(extension_id, &rules_, &new_rules);
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&RulesCacheDelegate::UpdateRules, cache_delegate_,
                     extension_id, RulesToValue(new_rules)));
}

void RulesRegistry::MaybeProcessChangedRules(const std::string& extension_id) {
  // Read and initialize |process_changed_rules_requested_[extension_id]| if
  // necessary. (Note that the insertion below will not overwrite
  // |process_changed_rules_requested_[extension_id]| if that already exists.
  std::pair<ProcessStateMap::iterator, bool> insertion =
      process_changed_rules_requested_.insert(std::make_pair(
          extension_id,
          browser_context_ ? NOT_SCHEDULED_FOR_PROCESSING : NEVER_PROCESS));
  if (insertion.first->second != NOT_SCHEDULED_FOR_PROCESSING)
    return;

  process_changed_rules_requested_[extension_id] = SCHEDULED_FOR_PROCESSING;
  ready_.Post(FROM_HERE,
              base::BindOnce(&RulesRegistry::ProcessChangedRules,
                             weak_ptr_factory_.GetWeakPtr(), extension_id));
}

bool RulesRegistry::IsUniqueId(const std::string& extension_id,
                               const std::string& rule_id) const {
  auto identifiers = used_rule_identifiers_.find(extension_id);
  if (identifiers == used_rule_identifiers_.end())
    return true;
  return identifiers->second.find(rule_id) == identifiers->second.end();
}

std::string RulesRegistry::GenerateUniqueId(const std::string& extension_id) {
  while (!IsUniqueId(extension_id, ToId(last_generated_rule_identifier_id_)))
    ++last_generated_rule_identifier_id_;
  return ToId(last_generated_rule_identifier_id_);
}

std::string RulesRegistry::CheckAndFillInOptionalRules(
    const std::string& extension_id,
    std::vector<api::events::Rule>* rules) {
  // IDs we have inserted, in case we need to rollback this operation.
  // TODO(rdevlin.cronin, avi): Um, this variable is never inserted into; is
  // this a bug? https://crbug.com/913655
  std::vector<std::string> rollback_log;

  // First we insert all rules with existing identifier, so that generated
  // identifiers cannot collide with identifiers passed by the caller.
  for (const auto& rule : *rules) {
    if (rule.id.get()) {
      std::string id = *(rule.id);
      if (!IsUniqueId(extension_id, id)) {
        RemoveUsedRuleIdentifiers(extension_id, rollback_log);
        return "Id " + id + " was used multiple times.";
      }
      used_rule_identifiers_[extension_id].insert(id);
    }
  }
  // Now we generate IDs in case they were not specified in the rules. This
  // cannot fail so we do not need to keep track of a rollback log.
  for (auto& rule : *rules) {
    if (!rule.id.get()) {
      rule.id.reset(new std::string(GenerateUniqueId(extension_id)));
      used_rule_identifiers_[extension_id].insert(*(rule.id));
    }
  }
  return std::string();
}

void RulesRegistry::FillInOptionalPriorities(
    std::vector<api::events::Rule>* rules) {
  for (auto& rule : *rules) {
    if (!rule.priority.get())
      rule.priority.reset(new int(DEFAULT_PRIORITY));
  }
}

void RulesRegistry::RemoveUsedRuleIdentifiers(
    const std::string& extension_id,
    const std::vector<std::string>& identifiers) {
  std::vector<std::string>::const_iterator i;
  for (i = identifiers.begin(); i != identifiers.end(); ++i)
    used_rule_identifiers_[extension_id].erase(*i);
}

void RulesRegistry::RemoveAllUsedRuleIdentifiers(
    const std::string& extension_id) {
  used_rule_identifiers_.erase(extension_id);
}

}  // namespace extensions
