// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/declarative_rule.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/url_matcher/url_matcher_constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ParseJson;
using base::test::ParseJsonDict;
using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionFactory;
using url_matcher::URLMatcherConditionSet;

namespace extensions {

namespace {

base::Value::Dict SimpleManifest() {
  return base::Value::Dict()
      .Set("name", "extension")
      .Set("manifest_version", 2)
      .Set("version", "1.0");
}

}  // namespace

struct RecordingCondition {
  using MatchData = int;

  raw_ptr<URLMatcherConditionFactory> factory;
  std::unique_ptr<base::Value> value;

  void GetURLMatcherConditionSets(
      URLMatcherConditionSet::Vector* condition_sets) const {
    // No condition sets.
  }

  static std::unique_ptr<RecordingCondition> Create(
      const Extension* extension,
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& condition,
      std::string* error) {
    if (condition.is_dict() && condition.GetDict().Find("bad_key")) {
      *error = "Found error key";
      return nullptr;
    }

    std::unique_ptr<RecordingCondition> result(new RecordingCondition());
    result->factory = url_matcher_condition_factory;
    result->value = base::Value::ToUniquePtrValue(condition.Clone());
    return result;
  }
};
using RecordingConditionSet = DeclarativeConditionSet<RecordingCondition>;

TEST(DeclarativeConditionTest, ErrorConditionSet) {
  URLMatcher matcher;
  base::Value::List conditions;
  conditions.Append(ParseJson("{\"key\": 1}"));
  conditions.Append(ParseJson("{\"bad_key\": 2}"));

  std::string error;
  std::unique_ptr<RecordingConditionSet> result = RecordingConditionSet::Create(
      nullptr, matcher.condition_factory(), conditions, &error);
  EXPECT_EQ("Found error key", error);
  ASSERT_FALSE(result);
}

TEST(DeclarativeConditionTest, CreateConditionSet) {
  URLMatcher matcher;
  base::Value::List conditions;
  conditions.Append(ParseJson("{\"key\": 1}"));
  conditions.Append(ParseJson("[\"val1\", 2]"));

  // Test insertion
  std::string error;
  std::unique_ptr<RecordingConditionSet> result = RecordingConditionSet::Create(
      nullptr, matcher.condition_factory(), conditions, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result);
  EXPECT_EQ(2u, result->conditions().size());

  EXPECT_EQ(matcher.condition_factory(), result->conditions()[0]->factory);
  EXPECT_EQ(ParseJson("{\"key\": 1}"), *result->conditions()[0]->value);
}

struct FulfillableCondition {
  struct MatchData {
    int value;
    const raw_ref<const std::set<base::MatcherStringPattern::ID>> url_matches;
  };

  scoped_refptr<URLMatcherConditionSet> condition_set;
  base::MatcherStringPattern::ID condition_set_id;
  int max_value;

  base::MatcherStringPattern::ID url_matcher_condition_set_id() const {
    return condition_set_id;
  }

  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set() const {
    return condition_set;
  }

  void GetURLMatcherConditionSets(
      URLMatcherConditionSet::Vector* condition_sets) const {
    if (condition_set.get()) {
      condition_sets->push_back(condition_set);
    }
  }

  bool IsFulfilled(const MatchData& match_data) const {
    if (condition_set_id != base::MatcherStringPattern::kInvalidId &&
        !base::Contains(*match_data.url_matches, condition_set_id))
      return false;
    return match_data.value <= max_value;
  }

  static std::unique_ptr<FulfillableCondition> Create(
      const Extension* extension,
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& condition,
      std::string* error) {
    std::unique_ptr<FulfillableCondition> result(new FulfillableCondition());
    if (!condition.is_dict()) {
      *error = "Expected dict";
      return result;
    }
    const base::Value::Dict& dict = condition.GetDict();
    const auto id = dict.FindInt("url_id");
    result->condition_set_id =
        id.has_value() ? static_cast<base::MatcherStringPattern::ID>(id.value())
                       : base::MatcherStringPattern::kInvalidId;
    if (std::optional<int> max_value_int = dict.FindInt("max")) {
      result->max_value = *max_value_int;
    } else {
      *error = "Expected integer at ['max']";
    }
    if (result->condition_set_id != base::MatcherStringPattern::kInvalidId) {
      result->condition_set = new URLMatcherConditionSet(
          result->condition_set_id,
          URLMatcherConditionSet::Conditions());
    }
    return result;
  }
};

TEST(DeclarativeConditionTest, FulfillConditionSet) {
  using FulfillableConditionSet = DeclarativeConditionSet<FulfillableCondition>;
  base::Value::List conditions;
  conditions.Append(ParseJson("{\"url_id\": 1, \"max\": 3}"));
  conditions.Append(ParseJson("{\"url_id\": 2, \"max\": 5}"));
  conditions.Append(ParseJson("{\"url_id\": 3, \"max\": 1}"));
  conditions.Append(ParseJson("{\"max\": -5}"));  // No url.

  // Test insertion
  std::string error;
  std::unique_ptr<FulfillableConditionSet> result =
      FulfillableConditionSet::Create(nullptr, nullptr, conditions, &error);
  ASSERT_EQ("", error);
  ASSERT_TRUE(result);
  EXPECT_EQ(4u, result->conditions().size());

  std::set<base::MatcherStringPattern::ID> url_matches;
  FulfillableCondition::MatchData match_data = {0, raw_ref(url_matches)};
  EXPECT_FALSE(result->IsFulfilled(1, match_data))
      << "Testing an ID that's not in url_matches forwards to the Condition, "
      << "which doesn't match.";
  EXPECT_FALSE(result->IsFulfilled(-1, match_data))
      << "Testing the 'no ID' value tries to match the 4th condition, but "
      << "its max is too low.";
  match_data.value = -5;
  EXPECT_TRUE(result->IsFulfilled(-1, match_data))
      << "Testing the 'no ID' value tries to match the 4th condition, and "
      << "this value is low enough.";

  url_matches.insert(1);
  match_data.value = 3;
  EXPECT_TRUE(result->IsFulfilled(1, match_data))
      << "Tests a condition with a url matcher, for a matching value.";
  match_data.value = 4;
  EXPECT_FALSE(result->IsFulfilled(1, match_data))
      << "Tests a condition with a url matcher, for a non-matching value "
      << "that would match a different condition.";
  url_matches.insert(2);
  EXPECT_TRUE(result->IsFulfilled(2, match_data))
      << "Tests with 2 elements in the match set.";

  // Check the condition sets:
  URLMatcherConditionSet::Vector condition_sets;
  result->GetURLMatcherConditionSets(&condition_sets);
  ASSERT_EQ(3U, condition_sets.size());
  EXPECT_EQ(1U, condition_sets[0]->id());
  EXPECT_EQ(2U, condition_sets[1]->id());
  EXPECT_EQ(3U, condition_sets[2]->id());
}

// DeclarativeAction

class SummingAction : public base::RefCounted<SummingAction> {
 public:
  using ApplyInfo = int;

  SummingAction(int increment, int min_priority)
      : increment_(increment), min_priority_(min_priority) {}

  static scoped_refptr<const SummingAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value::Dict& dict,
      std::string* error,
      bool* bad_message) {
    if (const base::Value* value = dict.Find("error")) {
      EXPECT_TRUE(value->is_string());
      *error = value->GetString();
      return nullptr;
    }
    if (dict.Find("bad")) {
      *bad_message = true;
      return nullptr;
    }

    std::optional<int> increment = dict.FindInt("value");
    EXPECT_TRUE(increment);
    int min_priority = dict.FindInt("priority").value_or(0);
    return scoped_refptr<const SummingAction>(
        new SummingAction(*increment, min_priority));
  }

  void Apply(const ExtensionId& extension_id,
             const base::Time& install_time,
             int* sum) const {
    *sum += increment_;
  }

  int increment() const { return increment_; }
  int minimum_priority() const {
    return min_priority_;
  }

 private:
  friend class base::RefCounted<SummingAction>;
  virtual ~SummingAction() {}

  int increment_;
  int min_priority_;
};
using SummingActionSet = DeclarativeActionSet<SummingAction>;

TEST(DeclarativeActionTest, ErrorActionSet) {
  base::Value::List actions;
  actions.Append(ParseJson("{\"value\": 1}"));
  actions.Append(ParseJson("{\"error\": \"the error\"}"));

  std::string error;
  bool bad = false;
  std::unique_ptr<SummingActionSet> result =
      SummingActionSet::Create(nullptr, nullptr, actions, &error, &bad);
  EXPECT_EQ("the error", error);
  EXPECT_FALSE(bad);
  EXPECT_FALSE(result);

  actions.clear();
  actions.Append(ParseJson("{\"value\": 1}"));
  actions.Append(ParseJson("{\"bad\": 3}"));
  result = SummingActionSet::Create(nullptr, nullptr, actions, &error, &bad);
  EXPECT_EQ("", error);
  EXPECT_TRUE(bad);
  EXPECT_FALSE(result);
}

TEST(DeclarativeActionTest, ApplyActionSet) {
  base::Value::List actions;
  actions.Append(ParseJson("{\"value\": 1, \"priority\": 5}"));
  actions.Append(ParseJson("{\"value\": 2}"));

  // Test insertion
  std::string error;
  bool bad = false;
  std::unique_ptr<SummingActionSet> result =
      SummingActionSet::Create(nullptr, nullptr, actions, &error, &bad);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad);
  ASSERT_TRUE(result);
  EXPECT_EQ(2u, result->actions().size());

  int sum = 0;
  result->Apply("ext_id", base::Time(), &sum);
  EXPECT_EQ(3, sum);
  EXPECT_EQ(5, result->GetMinimumPriority());
}

TEST(DeclarativeRuleTest, Create) {
  using Rule = DeclarativeRule<FulfillableCondition, SummingAction>;
  auto json_rule = Rule::JsonRule::FromValue(ParseJsonDict(R"(
      {
        "id": "rule1",
        "conditions": [
          {"url_id": 1, "max": 3},
          {"url_id": 2, "max": 5},
        ],
        "actions": [
          {
            "value": 2
          }
        ],
        "priority": 200
      })"));
  ASSERT_TRUE(json_rule);

  const char kExtensionId[] = "ext1";
  scoped_refptr<const Extension> extension = ExtensionBuilder()
                                                 .SetManifest(SimpleManifest())
                                                 .SetID(kExtensionId)
                                                 .Build();

  base::Time install_time = base::Time::Now();

  URLMatcher matcher;
  std::string error;
  std::unique_ptr<Rule> rule(Rule::Create(
      matcher.condition_factory(), nullptr, extension.get(), install_time,
      *json_rule, Rule::ConsistencyChecker(), &error));
  EXPECT_EQ("", error);
  ASSERT_TRUE(rule.get());

  EXPECT_EQ(kExtensionId, rule->id().first);
  EXPECT_EQ("rule1", rule->id().second);

  EXPECT_EQ(200, rule->priority());

  const Rule::ConditionSet& condition_set = rule->conditions();
  const Rule::ConditionSet::Conditions& conditions =
      condition_set.conditions();
  ASSERT_EQ(2u, conditions.size());
  EXPECT_EQ(3, conditions[0]->max_value);
  EXPECT_EQ(5, conditions[1]->max_value);

  const Rule::ActionSet& action_set = rule->actions();
  const Rule::ActionSet::Actions& actions = action_set.actions();
  ASSERT_EQ(1u, actions.size());
  EXPECT_EQ(2, actions[0]->increment());

  int sum = 0;
  rule->Apply(&sum);
  EXPECT_EQ(2, sum);
}

bool AtLeastOneCondition(
    const DeclarativeConditionSet<FulfillableCondition>* conditions,
    const DeclarativeActionSet<SummingAction>* actions,
    std::string* error) {
  if (conditions->conditions().empty()) {
    *error = "No conditions";
    return false;
  }
  return true;
}

TEST(DeclarativeRuleTest, CheckConsistency) {
  using Rule = DeclarativeRule<FulfillableCondition, SummingAction>;
  URLMatcher matcher;
  std::string error;
  const char kExtensionId[] = "ext1";
  scoped_refptr<const Extension> extension = ExtensionBuilder()
                                                 .SetManifest(SimpleManifest())
                                                 .SetID(kExtensionId)
                                                 .Build();

  auto json_rule = Rule::JsonRule::FromValue(ParseJsonDict(R"(
      {
        "id": "rule1",
        "conditions": [
          {"url_id": 1, "max": 3},
          {"url_id": 2, "max": 5},
        ],
        "actions": [
          {
            "value": 2
          }
        ],
        "priority": 200
      })"));
  ASSERT_TRUE(json_rule);
  std::unique_ptr<Rule> rule(Rule::Create(
      matcher.condition_factory(), nullptr, extension.get(), base::Time(),
      *json_rule, base::BindOnce(AtLeastOneCondition), &error));
  EXPECT_TRUE(rule);
  EXPECT_EQ("", error);

  json_rule = Rule::JsonRule::FromValue(ParseJsonDict(R"({
                                                   "id": "rule1",
                                                   "conditions": [
                                                   ],
                                                   "actions": [
                                                     {
                                                       "value": 2
                                                     }
                                                   ],
                                                   "priority": 200
                                                 })"));
  ASSERT_TRUE(json_rule);
  rule = Rule::Create(matcher.condition_factory(), nullptr, extension.get(),
                      base::Time(), *json_rule,
                      base::BindOnce(AtLeastOneCondition), &error);
  EXPECT_FALSE(rule);
  EXPECT_EQ("No conditions", error);
}

}  // namespace extensions
