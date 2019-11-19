// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/declarative_rule.h"

#include "base/bind.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/url_matcher/url_matcher_constants.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ParseJsonDeprecated;
using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionFactory;
using url_matcher::URLMatcherConditionSet;

namespace extensions {

namespace {

std::unique_ptr<base::DictionaryValue> SimpleManifest() {
  return DictionaryBuilder()
      .Set("name", "extension")
      .Set("manifest_version", 2)
      .Set("version", "1.0")
      .Build();
}

}  // namespace

struct RecordingCondition {
  typedef int MatchData;

  URLMatcherConditionFactory* factory;
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
    const base::DictionaryValue* dict = nullptr;
    if (condition.GetAsDictionary(&dict) && dict->HasKey("bad_key")) {
      *error = "Found error key";
      return std::unique_ptr<RecordingCondition>();
    }

    std::unique_ptr<RecordingCondition> result(new RecordingCondition());
    result->factory = url_matcher_condition_factory;
    result->value.reset(condition.DeepCopy());
    return result;
  }
};
typedef DeclarativeConditionSet<RecordingCondition> RecordingConditionSet;

TEST(DeclarativeConditionTest, ErrorConditionSet) {
  URLMatcher matcher;
  RecordingConditionSet::Values conditions;
  conditions.push_back(ParseJsonDeprecated("{\"key\": 1}"));
  conditions.push_back(ParseJsonDeprecated("{\"bad_key\": 2}"));

  std::string error;
  std::unique_ptr<RecordingConditionSet> result = RecordingConditionSet::Create(
      nullptr, matcher.condition_factory(), conditions, &error);
  EXPECT_EQ("Found error key", error);
  ASSERT_FALSE(result);
}

TEST(DeclarativeConditionTest, CreateConditionSet) {
  URLMatcher matcher;
  RecordingConditionSet::Values conditions;
  conditions.push_back(ParseJsonDeprecated("{\"key\": 1}"));
  conditions.push_back(ParseJsonDeprecated("[\"val1\", 2]"));

  // Test insertion
  std::string error;
  std::unique_ptr<RecordingConditionSet> result = RecordingConditionSet::Create(
      nullptr, matcher.condition_factory(), conditions, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result);
  EXPECT_EQ(2u, result->conditions().size());

  EXPECT_EQ(matcher.condition_factory(), result->conditions()[0]->factory);
  EXPECT_TRUE(ParseJsonDeprecated("{\"key\": 1}")
                  ->Equals(result->conditions()[0]->value.get()));
}

struct FulfillableCondition {
  struct MatchData {
    int value;
    const std::set<URLMatcherConditionSet::ID>& url_matches;
  };

  scoped_refptr<URLMatcherConditionSet> condition_set;
  int condition_set_id;
  int max_value;

  URLMatcherConditionSet::ID url_matcher_condition_set_id() const {
    return condition_set_id;
  }

  scoped_refptr<URLMatcherConditionSet> url_matcher_condition_set() const {
    return condition_set;
  }

  void GetURLMatcherConditionSets(
      URLMatcherConditionSet::Vector* condition_sets) const {
    if (condition_set.get())
      condition_sets->push_back(condition_set);
  }

  bool IsFulfilled(const MatchData& match_data) const {
    if (condition_set_id != -1 &&
        !base::Contains(match_data.url_matches, condition_set_id))
      return false;
    return match_data.value <= max_value;
  }

  static std::unique_ptr<FulfillableCondition> Create(
      const Extension* extension,
      URLMatcherConditionFactory* url_matcher_condition_factory,
      const base::Value& condition,
      std::string* error) {
    std::unique_ptr<FulfillableCondition> result(new FulfillableCondition());
    const base::DictionaryValue* dict;
    if (!condition.GetAsDictionary(&dict)) {
      *error = "Expected dict";
      return result;
    }
    if (!dict->GetInteger("url_id", &result->condition_set_id))
      result->condition_set_id = -1;
    if (!dict->GetInteger("max", &result->max_value))
      *error = "Expected integer at ['max']";
    if (result->condition_set_id != -1) {
      result->condition_set = new URLMatcherConditionSet(
          result->condition_set_id,
          URLMatcherConditionSet::Conditions());
    }
    return result;
  }
};

TEST(DeclarativeConditionTest, FulfillConditionSet) {
  typedef DeclarativeConditionSet<FulfillableCondition> FulfillableConditionSet;
  FulfillableConditionSet::Values conditions;
  conditions.push_back(ParseJsonDeprecated("{\"url_id\": 1, \"max\": 3}"));
  conditions.push_back(ParseJsonDeprecated("{\"url_id\": 2, \"max\": 5}"));
  conditions.push_back(ParseJsonDeprecated("{\"url_id\": 3, \"max\": 1}"));
  conditions.push_back(ParseJsonDeprecated("{\"max\": -5}"));  // No url.

  // Test insertion
  std::string error;
  std::unique_ptr<FulfillableConditionSet> result =
      FulfillableConditionSet::Create(nullptr, nullptr, conditions, &error);
  ASSERT_EQ("", error);
  ASSERT_TRUE(result);
  EXPECT_EQ(4u, result->conditions().size());

  std::set<URLMatcherConditionSet::ID> url_matches;
  FulfillableCondition::MatchData match_data = { 0, url_matches };
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
  EXPECT_EQ(1, condition_sets[0]->id());
  EXPECT_EQ(2, condition_sets[1]->id());
  EXPECT_EQ(3, condition_sets[2]->id());
}

// DeclarativeAction

class SummingAction : public base::RefCounted<SummingAction> {
 public:
  typedef int ApplyInfo;

  SummingAction(int increment, int min_priority)
      : increment_(increment), min_priority_(min_priority) {}

  static scoped_refptr<const SummingAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value& action,
      std::string* error,
      bool* bad_message) {
    int increment = 0;
    int min_priority = 0;
    const base::DictionaryValue* dict = nullptr;
    EXPECT_TRUE(action.GetAsDictionary(&dict));
    if (dict->HasKey("error")) {
      EXPECT_TRUE(dict->GetString("error", error));
      return nullptr;
    }
    if (dict->HasKey("bad")) {
      *bad_message = true;
      return nullptr;
    }

    EXPECT_TRUE(dict->GetInteger("value", &increment));
    dict->GetInteger("priority", &min_priority);
    return scoped_refptr<const SummingAction>(
        new SummingAction(increment, min_priority));
  }

  void Apply(const std::string& extension_id,
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
typedef DeclarativeActionSet<SummingAction> SummingActionSet;

TEST(DeclarativeActionTest, ErrorActionSet) {
  SummingActionSet::Values actions;
  actions.push_back(ParseJsonDeprecated("{\"value\": 1}"));
  actions.push_back(ParseJsonDeprecated("{\"error\": \"the error\"}"));

  std::string error;
  bool bad = false;
  std::unique_ptr<SummingActionSet> result =
      SummingActionSet::Create(nullptr, nullptr, actions, &error, &bad);
  EXPECT_EQ("the error", error);
  EXPECT_FALSE(bad);
  EXPECT_FALSE(result);

  actions.clear();
  actions.push_back(ParseJsonDeprecated("{\"value\": 1}"));
  actions.push_back(ParseJsonDeprecated("{\"bad\": 3}"));
  result = SummingActionSet::Create(nullptr, nullptr, actions, &error, &bad);
  EXPECT_EQ("", error);
  EXPECT_TRUE(bad);
  EXPECT_FALSE(result);
}

TEST(DeclarativeActionTest, ApplyActionSet) {
  SummingActionSet::Values actions;
  actions.push_back(
      ParseJsonDeprecated("{\"value\": 1,"
                          " \"priority\": 5}"));
  actions.push_back(ParseJsonDeprecated("{\"value\": 2}"));

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
  typedef DeclarativeRule<FulfillableCondition, SummingAction> Rule;
  Rule::JsonRule json_rule;
  ASSERT_TRUE(Rule::JsonRule::Populate(
      *ParseJsonDeprecated("{ \n"
                           "  \"id\": \"rule1\", \n"
                           "  \"conditions\": [ \n"
                           "    {\"url_id\": 1, \"max\": 3}, \n"
                           "    {\"url_id\": 2, \"max\": 5}, \n"
                           "  ], \n"
                           "  \"actions\": [ \n"
                           "    { \n"
                           "      \"value\": 2 \n"
                           "    } \n"
                           "  ], \n"
                           "  \"priority\": 200 \n"
                           "}"),
      &json_rule));

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
      json_rule, Rule::ConsistencyChecker(), &error));
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
  typedef DeclarativeRule<FulfillableCondition, SummingAction> Rule;
  URLMatcher matcher;
  std::string error;
  Rule::JsonRule json_rule;
  const char kExtensionId[] = "ext1";
  scoped_refptr<const Extension> extension = ExtensionBuilder()
                                                 .SetManifest(SimpleManifest())
                                                 .SetID(kExtensionId)
                                                 .Build();

  ASSERT_TRUE(Rule::JsonRule::Populate(
      *ParseJsonDeprecated("{ \n"
                           "  \"id\": \"rule1\", \n"
                           "  \"conditions\": [ \n"
                           "    {\"url_id\": 1, \"max\": 3}, \n"
                           "    {\"url_id\": 2, \"max\": 5}, \n"
                           "  ], \n"
                           "  \"actions\": [ \n"
                           "    { \n"
                           "      \"value\": 2 \n"
                           "    } \n"
                           "  ], \n"
                           "  \"priority\": 200 \n"
                           "}"),
      &json_rule));
  std::unique_ptr<Rule> rule(Rule::Create(
      matcher.condition_factory(), nullptr, extension.get(), base::Time(),
      json_rule, base::Bind(AtLeastOneCondition), &error));
  EXPECT_TRUE(rule);
  EXPECT_EQ("", error);

  ASSERT_TRUE(
      Rule::JsonRule::Populate(*ParseJsonDeprecated("{ \n"
                                                    "  \"id\": \"rule1\", \n"
                                                    "  \"conditions\": [ \n"
                                                    "  ], \n"
                                                    "  \"actions\": [ \n"
                                                    "    { \n"
                                                    "      \"value\": 2 \n"
                                                    "    } \n"
                                                    "  ], \n"
                                                    "  \"priority\": 200 \n"
                                                    "}"),
                               &json_rule));
  rule = Rule::Create(matcher.condition_factory(), nullptr, extension.get(),
                      base::Time(), json_rule, base::Bind(AtLeastOneCondition),
                      &error);
  EXPECT_FALSE(rule);
  EXPECT_EQ("No conditions", error);
}

}  // namespace extensions
