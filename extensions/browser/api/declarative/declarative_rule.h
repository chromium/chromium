// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeclarativeRule<>, DeclarativeConditionSet<>, and DeclarativeActionSet<>
// templates usable with multiple different declarativeFoo systems.  These are
// templated on the Condition and Action types that define the behavior of a
// particular declarative event.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_RULE_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_RULE_H__

#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/url_matcher/url_matcher.h"
#include "extensions/common/api/events.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace base {
class Time;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// This class stores a set of conditions that may be part of a DeclarativeRule.
// If any condition is fulfilled, the Actions of the DeclarativeRule can be
// triggered.
//
// ConditionT should be immutable after creation.  It must define the following
// members:
//
//   // Arguments passed through from DeclarativeConditionSet::Create.
//   static std::unique_ptr<ConditionT> Create(
//       const Extension* extension,
//       URLMatcherConditionFactory* url_matcher_condition_factory,
//       // Except this argument gets elements of the Values array.
//       const base::Value& definition,
//       std::string* error);
//   // If the Condition needs to be filtered by some URLMatcherConditionSets,
//   // append them to |condition_sets|.
//   // DeclarativeConditionSet::GetURLMatcherConditionSets forwards here.
//   void GetURLMatcherConditionSets(
//       URLMatcherConditionSet::Vector* condition_sets);
//   // |match_data| passed through from DeclarativeConditionSet::IsFulfilled.
//   bool IsFulfilled(const ConditionT::MatchData& match_data);
template<typename ConditionT>
class DeclarativeConditionSet {
 public:
  using Conditions = std::vector<std::unique_ptr<const ConditionT>>;
  using const_iterator = typename Conditions::const_iterator;

  DeclarativeConditionSet(const DeclarativeConditionSet&) = delete;
  DeclarativeConditionSet& operator=(const DeclarativeConditionSet&) = delete;

  // Factory method that creates a DeclarativeConditionSet for |extension|
  // according to the JSON array |conditions| passed by the extension API. Sets
  // |error| and returns NULL in case of an error.
  static std::unique_ptr<DeclarativeConditionSet> Create(
      const Extension* extension,
      url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value::List& condition_values,
      std::string* error);

  const Conditions& conditions() const {
    return conditions_;
  }

  const_iterator begin() const { return conditions_.begin(); }
  const_iterator end() const { return conditions_.end(); }

  // If |url_match_trigger| is not -1, this function looks for a condition
  // with this URLMatcherConditionSet, and forwards to that condition's
  // IsFulfilled(|match_data|). If there is no such condition, then false is
  // returned. If |url_match_trigger| is -1, this function returns whether any
  // of the conditions without URL attributes is satisfied.
  bool IsFulfilled(base::MatcherStringPattern::ID url_match_trigger,
                   const typename ConditionT::MatchData& match_data) const;

  // Appends the URLMatcherConditionSet from all conditions to |condition_sets|.
  void GetURLMatcherConditionSets(
      url_matcher::URLMatcherConditionSet::Vector* condition_sets) const;

  // Returns whether there are some conditions without UrlFilter attributes.
  bool HasConditionsWithoutUrls() const {
    return !conditions_without_urls_.empty();
  }

 private:
  using URLMatcherIdToCondition =
      std::map<base::MatcherStringPattern::ID, const ConditionT*>;

  DeclarativeConditionSet(
      Conditions conditions,
      const URLMatcherIdToCondition& match_id_to_condition,
      const std::vector<const ConditionT*>& conditions_without_urls);

  const URLMatcherIdToCondition match_id_to_condition_;
  const Conditions conditions_;
  const std::vector<const ConditionT*> conditions_without_urls_;
};

// Immutable container for multiple actions.
//
// ActionT should be immutable after creation.  It must define the following
// members:
//
//   // Arguments passed through from ActionSet::Create.
//   static std::unique_ptr<ActionT> Create(
//       const Extension* extension,
//       // Except this argument gets elements of the Values array.
//       const base::Value::Dict& definition,
//       std::string* error, bool* bad_message);
//   void Apply(const ExtensionId& extension_id,
//              const base::Time& extension_install_time,
//              // Contains action-type-specific in/out parameters.
//              typename ActionT::ApplyInfo* apply_info) const;
//   // Only needed if the RulesRegistry calls DeclarativeActionSet::Revert().
//   void Revert(const ExtensionId& extension_id,
//               const base::Time& extension_install_time,
//               // Contains action-type-specific in/out parameters.
//               typename ActionT::ApplyInfo* apply_info) const;
//   // Return the minimum priority of rules that can be evaluated after this
//   // action runs.  A suitable default value is MIN_INT.
//   int minimum_priority() const;
//
// TODO(battre): As DeclarativeActionSet can become the single owner of all
// actions, we can optimize here by making some of them singletons (e.g. Cancel
// actions).
template<typename ActionT>
class DeclarativeActionSet {
 public:
  using Actions = std::vector<scoped_refptr<const ActionT>>;

  explicit DeclarativeActionSet(const Actions& actions);

  DeclarativeActionSet(const DeclarativeActionSet&) = delete;
  DeclarativeActionSet& operator=(const DeclarativeActionSet&) = delete;

  // Factory method that instantiates a DeclarativeActionSet for |extension|
  // according to |actions| which represents the array of actions received from
  // the extension API.
  static std::unique_ptr<DeclarativeActionSet> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value::List& action_values,
      std::string* error,
      bool* bad_message);

  // Rules call this method when their conditions are fulfilled.
  void Apply(const ExtensionId& extension_id,
             const base::Time& extension_install_time,
             typename ActionT::ApplyInfo* apply_info) const;

  // Rules call this method when their conditions are fulfilled, but Apply has
  // already been called.
  void Reapply(const ExtensionId& extension_id,
               const base::Time& extension_install_time,
               typename ActionT::ApplyInfo* apply_info) const;

  // Rules call this method when they have stateful conditions, and those
  // conditions stop being fulfilled.  Rules with event-based conditions (e.g. a
  // network request happened) will never Revert() an action.
  void Revert(const ExtensionId& extension_id,
              const base::Time& extension_install_time,
              typename ActionT::ApplyInfo* apply_info) const;

  // Returns the minimum priority of rules that may be evaluated after
  // this rule. Defaults to MIN_INT.
  int GetMinimumPriority() const;

  const Actions& actions() const { return actions_; }

 private:
  const Actions actions_;
};

// Representation of a rule of a declarative API:
// https://developer.chrome.com/beta/extensions/events.html#declarative.
// Generally a RulesRegistry will hold a collection of Rules for a given
// declarative API and contain the logic for matching and applying them.
//
// See DeclarativeConditionSet and DeclarativeActionSet for the requirements on
// ConditionT and ActionT.
template<typename ConditionT, typename ActionT>
class DeclarativeRule {
 public:
  using RuleId = std::string;
  using GlobalRuleId = std::pair<ExtensionId, RuleId>;
  using Priority = int;
  using ConditionSet = DeclarativeConditionSet<ConditionT>;
  using ActionSet = DeclarativeActionSet<ActionT>;
  using JsonRule = extensions::api::events::Rule;
  using Tags = std::vector<std::string>;

  // Checks whether the set of |conditions| and |actions| are consistent.
  // Returns true in case of consistency and MUST set |error| otherwise.
  using ConsistencyChecker =
      base::OnceCallback<bool(const ConditionSet* conditions,
                              const ActionSet* actions,
                              std::string* error)>;

  DeclarativeRule(const GlobalRuleId& id,
                  const Tags& tags,
                  base::Time extension_installation_time,
                  std::unique_ptr<ConditionSet> conditions,
                  std::unique_ptr<ActionSet> actions,
                  Priority priority);

  DeclarativeRule(const DeclarativeRule&) = delete;
  DeclarativeRule& operator=(const DeclarativeRule&) = delete;

  // Creates a DeclarativeRule for |extension| given a json definition.  The
  // format of each condition and action's json is up to the specific ConditionT
  // and ActionT.  |extension| may be NULL in tests.
  //
  // Before constructing the final rule, calls check_consistency(conditions,
  // actions, error) and returns NULL if it fails.  Pass NULL if no consistency
  // check is needed.  If |error| is empty, the translation was successful and
  // the returned rule is internally consistent.
  static std::unique_ptr<DeclarativeRule> Create(
      url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
      content::BrowserContext* browser_context,
      const Extension* extension,
      base::Time extension_installation_time,
      const JsonRule& rule,
      ConsistencyChecker check_consistency,
      std::string* error);

  const GlobalRuleId& id() const { return id_; }
  const Tags& tags() const { return tags_; }
  const ExtensionId& extension_id() const { return id_.first; }
  const ConditionSet& conditions() const { return *conditions_; }
  const ActionSet& actions() const { return *actions_; }
  Priority priority() const { return priority_; }

  // Calls actions().Apply(extension_id(), extension_installation_time_,
  // apply_info). This function should only be called when the conditions_ are
  // fulfilled (from a semantic point of view; no harm is done if this function
  // is called at other times for testing purposes).
  void Apply(typename ActionT::ApplyInfo* apply_info) const;

  // Returns the minimum priority of rules that may be evaluated after
  // this rule. Defaults to MIN_INT. Only valid if the conditions of this rule
  // are fulfilled.
  Priority GetMinimumPriority() const;

 private:
  GlobalRuleId id_;
  Tags tags_;
  base::Time extension_installation_time_;  // For precedences of rules.
  std::unique_ptr<ConditionSet> conditions_;
  std::unique_ptr<ActionSet> actions_;
  Priority priority_;
};

// Implementation details below here.

//
// DeclarativeConditionSet
//

template <typename ConditionT>
bool DeclarativeConditionSet<ConditionT>::IsFulfilled(
    base::MatcherStringPattern::ID url_match_trigger,
    const typename ConditionT::MatchData& match_data) const {
  if (url_match_trigger == base::MatcherStringPattern::kInvalidId) {
    // Invalid trigger -- indication that we should only check conditions
    // without URL attributes.
    for (const ConditionT* condition : conditions_without_urls_) {
      if (condition->IsFulfilled(match_data))
        return true;
    }
    return false;
  }

  typename URLMatcherIdToCondition::const_iterator triggered =
      match_id_to_condition_.find(url_match_trigger);
  return (triggered != match_id_to_condition_.end() &&
          triggered->second->IsFulfilled(match_data));
}

template<typename ConditionT>
void DeclarativeConditionSet<ConditionT>::GetURLMatcherConditionSets(
    url_matcher::URLMatcherConditionSet::Vector* condition_sets) const {
  for (const auto& condition : conditions_)
    condition->GetURLMatcherConditionSets(condition_sets);
}

// static
template <typename ConditionT>
std::unique_ptr<DeclarativeConditionSet<ConditionT>>
DeclarativeConditionSet<ConditionT>::Create(
    const Extension* extension,
    url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
    const base::Value::List& condition_values,
    std::string* error) {
  Conditions result;

  for (const auto& value : condition_values) {
    std::unique_ptr<ConditionT> condition = ConditionT::Create(
        extension, url_matcher_condition_factory, value, error);
    if (!error->empty())
      return nullptr;
    result.push_back(std::move(condition));
  }

  URLMatcherIdToCondition match_id_to_condition;
  std::vector<const ConditionT*> conditions_without_urls;
  url_matcher::URLMatcherConditionSet::Vector condition_sets;

  for (const auto& condition : result) {
    condition_sets.clear();
    condition->GetURLMatcherConditionSets(&condition_sets);
    if (condition_sets.empty()) {
      conditions_without_urls.push_back(condition.get());
    } else {
      for (const scoped_refptr<url_matcher::URLMatcherConditionSet>& match_set :
           condition_sets)
        match_id_to_condition[match_set->id()] = condition.get();
    }
  }

  return base::WrapUnique(new DeclarativeConditionSet(
      std::move(result), match_id_to_condition, conditions_without_urls));
}

template <typename ConditionT>
DeclarativeConditionSet<ConditionT>::DeclarativeConditionSet(
    Conditions conditions,
    const URLMatcherIdToCondition& match_id_to_condition,
    const std::vector<const ConditionT*>& conditions_without_urls)
    : match_id_to_condition_(match_id_to_condition),
      conditions_(std::move(conditions)),
      conditions_without_urls_(conditions_without_urls) {}

//
// DeclarativeActionSet
//

template<typename ActionT>
DeclarativeActionSet<ActionT>::DeclarativeActionSet(const Actions& actions)
    : actions_(actions) {}

// static
template <typename ActionT>
std::unique_ptr<DeclarativeActionSet<ActionT>>
DeclarativeActionSet<ActionT>::Create(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      const base::Value::List& action_values,
                                      std::string* error,
                                      bool* bad_message) {
  *error = "";
  *bad_message = false;
  Actions result;

  for (const auto& value : action_values) {
    if (!value.is_dict()) {
      *bad_message = true;
      *error = "Action must be an object.";
      return nullptr;
    }
    scoped_refptr<const ActionT> action = ActionT::Create(
        browser_context, extension, value.GetDict(), error, bad_message);
    if (!error->empty() || *bad_message)
      return nullptr;
    result.push_back(action);
  }

  return std::make_unique<DeclarativeActionSet>(result);
}

template <typename ActionT>
void DeclarativeActionSet<ActionT>::Apply(
    const ExtensionId& extension_id,
    const base::Time& extension_install_time,
    typename ActionT::ApplyInfo* apply_info) const {
  for (const scoped_refptr<const ActionT>& action : actions_)
    action->Apply(extension_id, extension_install_time, apply_info);
}

template <typename ActionT>
void DeclarativeActionSet<ActionT>::Reapply(
    const ExtensionId& extension_id,
    const base::Time& extension_install_time,
    typename ActionT::ApplyInfo* apply_info) const {
  for (const scoped_refptr<const ActionT>& action : actions_)
    action->Reapply(extension_id, extension_install_time, apply_info);
}

template <typename ActionT>
void DeclarativeActionSet<ActionT>::Revert(
    const ExtensionId& extension_id,
    const base::Time& extension_install_time,
    typename ActionT::ApplyInfo* apply_info) const {
  for (const scoped_refptr<const ActionT>& action : actions_)
    action->Revert(extension_id, extension_install_time, apply_info);
}

template<typename ActionT>
int DeclarativeActionSet<ActionT>::GetMinimumPriority() const {
  int minimum_priority = std::numeric_limits<int>::min();
  for (typename Actions::const_iterator i = actions_.begin();
       i != actions_.end(); ++i) {
    minimum_priority = std::max(minimum_priority, (*i)->minimum_priority());
  }
  return minimum_priority;
}

//
// DeclarativeRule
//

template <typename ConditionT, typename ActionT>
DeclarativeRule<ConditionT, ActionT>::DeclarativeRule(
    const GlobalRuleId& id,
    const Tags& tags,
    base::Time extension_installation_time,
    std::unique_ptr<ConditionSet> conditions,
    std::unique_ptr<ActionSet> actions,
    Priority priority)
    : id_(id),
      tags_(tags),
      extension_installation_time_(extension_installation_time),
      conditions_(std::move(conditions)),
      actions_(std::move(actions)),
      priority_(priority) {
  CHECK(conditions_.get());
  CHECK(actions_.get());
}

// static
template <typename ConditionT, typename ActionT>
std::unique_ptr<DeclarativeRule<ConditionT, ActionT>>
DeclarativeRule<ConditionT, ActionT>::Create(
    url_matcher::URLMatcherConditionFactory* url_matcher_condition_factory,
    content::BrowserContext* browser_context,
    const Extension* extension,
    base::Time extension_installation_time,
    const JsonRule& rule,
    ConsistencyChecker check_consistency,
    std::string* error) {
  std::unique_ptr<DeclarativeRule> error_result;

  std::unique_ptr<ConditionSet> conditions = ConditionSet::Create(
      extension, url_matcher_condition_factory, rule.conditions, error);
  if (!error->empty())
    return std::move(error_result);
  CHECK(conditions.get());

  bool bad_message = false;
  std::unique_ptr<ActionSet> actions = ActionSet::Create(
      browser_context, extension, rule.actions, error, &bad_message);
  if (bad_message) {
    // TODO(battre) Export concept of bad_message to caller, the extension
    // should be killed in case it is true.
    *error = "An action of a rule set had an invalid "
        "structure that should have been caught by the JSON validator.";
    return std::move(error_result);
  }
  if (!error->empty() || bad_message)
    return std::move(error_result);
  CHECK(actions.get());

  if (!check_consistency.is_null() &&
      !std::move(check_consistency)
           .Run(conditions.get(), actions.get(), error)) {
    DCHECK(!error->empty());
    return std::move(error_result);
  }

  CHECK(rule.priority);
  int priority = *(rule.priority);

  GlobalRuleId rule_id(extension->id(), *(rule.id));
  Tags tags = rule.tags ? *rule.tags : Tags();
  return std::make_unique<DeclarativeRule>(
      rule_id, tags, extension_installation_time, std::move(conditions),
      std::move(actions), priority);
}

template<typename ConditionT, typename ActionT>
void DeclarativeRule<ConditionT, ActionT>::Apply(
    typename ActionT::ApplyInfo* apply_info) const {
  return actions_->Apply(extension_id(),
                         extension_installation_time_,
                         apply_info);
}

template<typename ConditionT, typename ActionT>
int DeclarativeRule<ConditionT, ActionT>::GetMinimumPriority() const {
  return actions_->GetMinimumPriority();
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_RULE_H__
