// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"

#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/types/strong_alias.h"
#include "services/network/public/mojom/no_vary_search.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_base_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/core/speculation_rules/stub_speculation_host.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::PrintToString;

// Convenience matcher for list rules that sub-matches on their URLs.
class ListRuleMatcher {
 public:
  explicit ListRuleMatcher(::testing::Matcher<const Vector<KURL>&> url_matcher)
      : url_matcher_(std::move(url_matcher)) {}

  bool MatchAndExplain(const Member<SpeculationRule>& rule,
                       ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(*rule, listener);
  }

  bool MatchAndExplain(const SpeculationRule& rule,
                       ::testing::MatchResultListener* listener) const {
    ::testing::StringMatchResultListener inner_listener;
    const bool matches =
        url_matcher_.MatchAndExplain(rule.urls(), &inner_listener);
    std::string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty())
      *listener << "whose URLs " << inner_explanation;
    return matches;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "is a list rule whose URLs ";
    url_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "is not list rule whose URLs ";
    url_matcher_.DescribeTo(os);
  }

 private:
  ::testing::Matcher<const Vector<KURL>&> url_matcher_;
};

class URLPatternMatcher {
 public:
  explicit URLPatternMatcher(v8::Isolate* isolate,
                             String pattern,
                             const KURL& base_url) {
    auto* url_pattern_input = MakeGarbageCollected<V8URLPatternInput>(pattern);
    url_pattern_ = URLPattern::Create(isolate, url_pattern_input, base_url,
                                      ASSERT_NO_EXCEPTION);
  }

  bool MatchAndExplain(URLPattern* pattern,
                       ::testing::MatchResultListener* listener) const {
    if (!pattern) {
      return false;
    }

    using Component = V8URLPatternComponent::Enum;
    Component components[] = {Component::kProtocol, Component::kUsername,
                              Component::kPassword, Component::kHostname,
                              Component::kPort,     Component::kPathname,
                              Component::kSearch,   Component::kHash};
    for (auto component : components) {
      if (URLPattern::compareComponent(V8URLPatternComponent(component),
                                       url_pattern_, pattern) != 0) {
        return false;
      }
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const { *os << url_pattern_->ToString(); }

  void DescribeNegationTo(::std::ostream* os) const { DescribeTo(os); }

 private:
  Persistent<URLPattern> url_pattern_;
};

template <typename... Matchers>
auto MatchesListOfURLs(Matchers&&... matchers) {
  return ::testing::MakePolymorphicMatcher(
      ListRuleMatcher(ElementsAre(std::forward<Matchers>(matchers)...)));
}

MATCHER(RequiresAnonymousClientIPWhenCrossOrigin,
        negation ? "doesn't require anonymous client IP when cross origin"
                 : "requires anonymous client IP when cross origin") {
  return arg->requires_anonymous_client_ip_when_cross_origin();
}

MATCHER(SetsReferrerPolicy,
        std::string(negation ? "doesn't set" : "sets") + " a referrer policy") {
  return arg->referrer_policy().has_value();
}

MATCHER_P(ReferrerPolicyIs,
          policy,
          std::string(negation ? "doesn't have" : "has") + " " +
              PrintToString(policy) + " as the referrer policy") {
  return arg->referrer_policy() == policy;
}

class SpeculationRuleSetTest : public ::testing::Test {
 public:
  SpeculationRuleSetTest()
      : execution_context_(MakeGarbageCollected<NullExecutionContext>()) {}
  ~SpeculationRuleSetTest() override {
    execution_context_->NotifyContextDestroyed();
  }

  SpeculationRuleSet* CreateRuleSet(const String& source_text,
                                    const KURL& base_url,
                                    ExecutionContext* context) {
    return SpeculationRuleSet::Parse(
        SpeculationRuleSet::Source::FromRequest(source_text, base_url,
                                                /* request_id */ 0),
        context);
  }

  SpeculationRuleSet* CreateSpeculationRuleSetWithTargetHint(
      const char* target_hint) {
    return CreateRuleSet(String::Format(R"({
        "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/hint.html"],
          "target_hint": "%s"
        }],
        "prefetch_with_subresources": [{
          "source": "list",
          "urls": ["https://example.com/hint.html"],
          "target_hint": "%s"
        }],
        "prerender": [{
          "source": "list",
          "urls": ["https://example.com/hint.html"],
          "target_hint": "%s"
        }]
      })",
                                        target_hint, target_hint, target_hint),
                         KURL("https://example.com/"), execution_context_);
  }

  NullExecutionContext* execution_context() {
    return static_cast<NullExecutionContext*>(execution_context_.Get());
  }

  auto URLPattern(String pattern,
                  const KURL& base_url = KURL("https://example.com/")) {
    return ::testing::MakePolymorphicMatcher(
        URLPatternMatcher(execution_context_->GetIsolate(), pattern, base_url));
  }

 private:
  ScopedPrerender2ForTest enable_prerender2_{true};
  test::TaskEnvironment task_environment_;
  Persistent<ExecutionContext> execution_context_;
};

// Matches a SpeculationCandidatePtr list with a KURL list (without requiring
// candidates to be in a specific order).
template <typename... Matchers>
auto HasURLs(Matchers&&... urls) {
  return ::testing::ResultOf(
      "urls",
      [](const auto& candidates) {
        Vector<KURL> urls;
        base::ranges::transform(
            candidates.begin(), candidates.end(), std::back_inserter(urls),
            [](const auto& candidate) { return candidate->url; });
        return urls;
      },
      ::testing::UnorderedElementsAre(urls...));
}

// Matches a SpeculationCandidatePtr with an Eagerness.
auto HasEagerness(
    ::testing::Matcher<blink::mojom::SpeculationEagerness> matcher) {
  return ::testing::Pointee(::testing::Field(
      "eagerness", &mojom::blink::SpeculationCandidate::eagerness, matcher));
}

// Matches a SpeculationCandidatePtr with a KURL.
auto HasURL(::testing::Matcher<KURL> matcher) {
  return ::testing::Pointee(::testing::Field(
      "url", &mojom::blink::SpeculationCandidate::url, matcher));
}

// Matches a SpeculationCandidatePtr with a SpeculationAction.
auto HasAction(::testing::Matcher<mojom::blink::SpeculationAction> matcher) {
  return ::testing::Pointee(::testing::Field(
      "action", &mojom::blink::SpeculationCandidate::action, matcher));
}

// Matches a SpeculationCandidatePtr with a SpeculationTargetHint.
auto HasTargetHint(
    ::testing::Matcher<mojom::blink::SpeculationTargetHint> matcher) {
  return ::testing::Pointee(::testing::Field(
      "target_hint",
      &mojom::blink::SpeculationCandidate::target_browsing_context_name_hint,
      matcher));
}

// Matches a SpeculationCandidatePtr with a ReferrerPolicy.
auto HasReferrerPolicy(
    ::testing::Matcher<network::mojom::ReferrerPolicy> matcher) {
  return ::testing::Pointee(::testing::Field(
      "referrer", &mojom::blink::SpeculationCandidate::referrer,
      ::testing::Pointee(::testing::Field(
          "policy", &mojom::blink::Referrer::policy, matcher))));
}

auto HasNoVarySearchHint() {
  return ::testing::Pointee(
      ::testing::Field("no_vary_search_hint",
                       &mojom::blink::SpeculationCandidate::no_vary_search_hint,
                       ::testing::IsTrue()));
}

auto NVSVariesOnKeyOrder() {
  return ::testing::AllOf(
      HasNoVarySearchHint(),
      ::testing::Pointee(::testing::Field(
          "no_vary_search_hint",
          &mojom::blink::SpeculationCandidate::no_vary_search_hint,
          testing::Pointee(::testing::Field(
              "vary_on_key_order",
              &network::mojom::blink::NoVarySearch::vary_on_key_order,
              ::testing::IsTrue())))));
}

template <typename... Matchers>
auto NVSHasNoVaryParams(Matchers&&... params) {
  return ::testing::ResultOf(
      "no_vary_params",
      [](const auto& nvs) {
        if (!nvs->no_vary_search_hint ||
            !nvs->no_vary_search_hint->search_variance ||
            !nvs->no_vary_search_hint->search_variance->is_no_vary_params()) {
          return Vector<String>();
        }
        return nvs->no_vary_search_hint->search_variance->get_no_vary_params();
      },
      ::testing::UnorderedElementsAre(params...));
}

TEST_F(SpeculationRuleSetTest, Empty) {
  auto* rule_set =
      CreateRuleSet("{}", KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

void AssertParseError(const SpeculationRuleSet* rule_set) {
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kSourceIsNotJsonObject);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, RejectsInvalidJSON) {
  auto* rule_set = CreateRuleSet("[invalid]", KURL("https://example.com"),
                                 execution_context());
  ASSERT_TRUE(rule_set);
  AssertParseError(rule_set);
  EXPECT_TRUE(rule_set->error_message().Contains("Syntax error"))
      << rule_set->error_message();
}

TEST_F(SpeculationRuleSetTest, RejectsNonObject) {
  auto* rule_set =
      CreateRuleSet("42", KURL("https://example.com"), execution_context());
  ASSERT_TRUE(rule_set);
  AssertParseError(rule_set);
  EXPECT_TRUE(rule_set->error_message().Contains("must be an object"))
      << rule_set->error_message();
}

TEST_F(SpeculationRuleSetTest, RejectsComments) {
  auto* rule_set = CreateRuleSet(
      "{ /* comments! */ }", KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  AssertParseError(rule_set);
  EXPECT_TRUE(rule_set->error_message().Contains("Syntax error"))
      << rule_set->error_message();
}

TEST_F(SpeculationRuleSetTest, SimplePrefetchRule) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, SimplePrerenderRule) {
  auto* rule_set = CreateRuleSet(

      R"({
        "prerender": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(
      rule_set->prerender_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, SimplePrefetchWithSubresourcesRule) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch_with_subresources": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(
      rule_set->prefetch_with_subresources_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, ResolvesURLs) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": [
            "bar",
            "/baz",
            "//example.org/",
            "http://example.net/"
          ]
        }]
      })",
      KURL("https://example.com/foo/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs(
                  "https://example.com/foo/bar", "https://example.com/baz",
                  "https://example.org/", "http://example.net/")));
}

TEST_F(SpeculationRuleSetTest, ResolvesURLsWithRelativeTo) {
  // Document base URL.
  execution_context()->SetURL(KURL("https://document.com/foo/"));

  // "relative_to": "ruleset" is an allowed value and results in default
  // behaviour.
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": [
            "bar",
            "/baz",
            "//example.org/",
            "http://example.net/"
          ],
          "relative_to": "ruleset"
        }]
      })",
      KURL("https://example.com/foo/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs(
                  "https://example.com/foo/bar", "https://example.com/baz",
                  "https://example.org/", "http://example.net/")));

  // "relative_to": "document" only affects relative URLs: "bar" and "/baz".
  rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": [
            "bar",
            "/baz",
            "//example.org/",
            "http://example.net/"
          ],
          "relative_to": "document"
        }]
      })",
      KURL("https://example.com/foo/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs(
                  "https://document.com/foo/bar", "https://document.com/baz",
                  "https://example.org/", "http://example.net/")));
}

TEST_F(SpeculationRuleSetTest, RequiresAnonymousClientIPWhenCrossOrigin) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": ["//example.net/anonymous.html"],
          "requires": ["anonymous-client-ip-when-cross-origin"]
        }, {
          "source": "list",
          "urls": ["//example.net/direct.html"]
        }]
      })",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(AllOf(MatchesListOfURLs("https://example.net/anonymous.html"),
                        RequiresAnonymousClientIPWhenCrossOrigin()),
                  AllOf(MatchesListOfURLs("https://example.net/direct.html"),
                        Not(RequiresAnonymousClientIPWhenCrossOrigin()))));
}

TEST_F(SpeculationRuleSetTest, IgnoresUnknownOrDifferentlyTypedTopLevelKeys) {
  auto* rule_set = CreateRuleSet(
      R"({
        "unrecognized_key": true,
        "prefetch": 42,
        "prefetch_with_subresources": false
      })",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, DropUnrecognizedRules) {
  auto* rule_set = CreateRuleSet(
      R"({"prefetch": [)"

      // A rule of incorrect type.
      R"("not an object",)"

      // This used to be invalid, but now is, even with no source.
      // TODO(crbug.com/1517696): Remove this when SpeculationRulesImplictSource
      // is permanently shipped, so keep the test focused.
      R"({"urls": ["no-source.html"]},)"

      // A rule with an unrecognized source.
      R"({"source": "magic-8-ball", "urls": ["no-source.html"]},)"

      // A list rule with no "urls" key.
      R"({"source": "list"},)"

      // A list rule where some URL is not a string.
      R"({"source": "list", "urls": [42]},)"

      // A rule with an unrecognized requirement.
      R"({"source": "list", "urls": ["/"], "requires": ["more-vespene-gas"]},)"

      // A rule with requirements not given as an array.
      R"({"source": "list", "urls": ["/"],
          "requires": "anonymous-client-ip-when-cross-origin"},)"

      // A rule with requirements of incorrect type.
      R"({"source": "list", "urls": ["/"], "requires": [42]},)"

      // A rule with a referrer_policy of incorrect type.
      R"({"source": "list", "urls": ["/"], "referrer_policy": 42},)"

      // A rule with an unrecognized referrer_policy.
      R"({"source": "list", "urls": ["/"],
          "referrer_policy": "no-referrrrrrrer"},)"

      // A rule with a legacy value for referrer_policy.
      R"({"source": "list", "urls": ["/"], "referrer_policy": "never"},)"

      // Invalid value of "relative_to".
      R"({"source": "list",
          "urls": ["/no-source.html"],
          "relative_to": 2022},)"

      // Invalid string value of "relative_to".
      R"({"source": "list",
          "urls": ["/no-source.html"],
          "relative_to": "not_document"},)"

      // A rule with a "target_hint" of incorrect type (in addition to being
      // invalid to use target_hint in a prefetch rule).
      R"({"source": "list", "urls": ["/"], "target_hint": 42},)"

      // Invalid URLs within a list rule should be discarded.
      // This includes totally invalid ones and ones with unacceptable schemes.
      R"({"source": "list",
          "urls": [
            "valid.html", "mailto:alice@example.com", "http://@:",
            "blob:https://bar"
           ]},)"

      // Invalid No-Vary-Search hint
      R"nvs({
        "source": "list",
        "urls": ["no-source.html"],
        "expects_no_vary_search": 0
      }]})nvs",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  // The rule set itself is valid, however many of the individual rules are
  // invalid. So we should have populated a warning message.
  EXPECT_FALSE(rule_set->error_message().empty());
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/no-source.html"),
                  MatchesListOfURLs("https://example.com/valid.html")));
}

// Test that only prerender rule can process a "_blank" target hint.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_Blank) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("_blank");
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains(
      "\"target_hint\" may not be set for prefetch"))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/hint.html")));
  EXPECT_EQ(rule_set->prerender_rules()[0]->target_browsing_context_name_hint(),
            mojom::blink::SpeculationTargetHint::kBlank);
}

// Test that only prerender rule can process a "_self" target hint.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_Self) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("_self");
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains(
      "\"target_hint\" may not be set for prefetch"))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/hint.html")));
  EXPECT_EQ(rule_set->prerender_rules()[0]->target_browsing_context_name_hint(),
            mojom::blink::SpeculationTargetHint::kSelf);
}

// Test that only prerender rule can process a "_parent" target hint but treat
// it as no hint.
// TODO(https://crbug.com/1354049): Support the "_parent" keyword for
// prerendering.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_Parent) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("_parent");
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains(
      "\"target_hint\" may not be set for prefetch"))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/hint.html")));
  EXPECT_EQ(rule_set->prerender_rules()[0]->target_browsing_context_name_hint(),
            mojom::blink::SpeculationTargetHint::kNoHint);
}

// Test that only prerender rule can process a "_top" target hint but treat it
// as no hint.
// Test that rules with a "_top" hint are ignored.
// TODO(https://crbug.com/1354049): Support the "_top" keyword for prerendering.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_Top) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("_top");
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains(
      "\"target_hint\" may not be set for prefetch"))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/hint.html")));
  EXPECT_EQ(rule_set->prerender_rules()[0]->target_browsing_context_name_hint(),
            mojom::blink::SpeculationTargetHint::kNoHint);
}

// Test that rules with an empty target hint are ignored.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_EmptyString) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("");
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains("invalid \"target_hint\""))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

// Test that only prerender rule can process a browsing context name target hint
// but treat it as no hint.
// TODO(https://crbug.com/1354049): Support valid browsing context names.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_ValidBrowsingContextName) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("valid");
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains(
      "\"target_hint\" may not be set for prefetch"))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/hint.html")));
  EXPECT_EQ(rule_set->prerender_rules()[0]->target_browsing_context_name_hint(),
            mojom::blink::SpeculationTargetHint::kNoHint);
}

// Test that rules with an invalid browsing context name target hint are
// ignored.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_InvalidBrowsingContextName) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("_invalid");
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains("invalid \"target_hint\""))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

// Test that the the validation of the browsing context keywords runs an ASCII
// case-insensitive match.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_CaseInsensitive) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("_BlAnK");
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/hint.html")));
  EXPECT_EQ(rule_set->prerender_rules()[0]->target_browsing_context_name_hint(),
            mojom::blink::SpeculationTargetHint::kBlank);
}

// Test that only prefetch rule supports "anonymous-client-ip-when-cross-origin"
// requirement.
TEST_F(SpeculationRuleSetTest,
       RulesWithRequiresAnonymousClientIpWhenCrossOrigin) {
  auto* rule_set =
      CreateRuleSet(R"({
        "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/requires-proxy.html"],
          "requires": ["anonymous-client-ip-when-cross-origin"]
        }],
        "prefetch_with_subresources": [{
          "source": "list",
          "urls": ["https://example.com/requires-proxy.html"],
          "requires": ["anonymous-client-ip-when-cross-origin"]
        }],
        "prerender": [{
          "source": "list",
          "urls": ["https://example.com/requires-proxy.html"],
          "requires": ["anonymous-client-ip-when-cross-origin"]
        }]
      })",
                    KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_EQ(rule_set->error_message(),
            "requirement \"anonymous-client-ip-when-cross-origin\" for "
            "\"prefetch_with_subresources\" is not supported.");
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs(
                  "https://example.com/requires-proxy.html")));
  EXPECT_TRUE(rule_set->prefetch_rules()[0]
                  ->requires_anonymous_client_ip_when_cross_origin());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, ReferrerPolicy) {
  auto* rule_set =
      CreateRuleSet(R"({
        "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"],
          "referrer_policy": "strict-origin"
        }, {
          "source": "list",
          "urls": ["https://example.com/index3.html"]
        }]
      })",
                    KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(AllOf(MatchesListOfURLs("https://example.com/index2.html"),
                        ReferrerPolicyIs(
                            network::mojom::ReferrerPolicy::kStrictOrigin)),
                  AllOf(MatchesListOfURLs("https://example.com/index3.html"),
                        Not(SetsReferrerPolicy()))));
}

TEST_F(SpeculationRuleSetTest, EmptyReferrerPolicy) {
  // If an empty string is used for referrer_policy, treat this as if the key
  // were omitted.
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"],
          "referrer_policy": ""
        }]
      })",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(), SpeculationRuleSetErrorType::kNoError);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(AllOf(MatchesListOfURLs("https://example.com/index2.html"),
                        Not(SetsReferrerPolicy()))));
}

TEST_F(SpeculationRuleSetTest, PropagatesToDocument) {
  // A <script> with a case-insensitive type match should be propagated to the
  // document.
  // TODO(jbroman): Should we need to enable script? Should that be bypassed?
  DummyPageHolder page_holder;
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("SpEcUlAtIoNrUlEs"));
  script->setText(
      R"({"prefetch": [
           {"source": "list", "urls": ["https://example.com/foo"]}
         ],
         "prerender": [
           {"source": "list", "urls": ["https://example.com/bar"]}
         ]
         })");
  document.head()->appendChild(script);

  auto* supplement = DocumentSpeculationRules::FromIfExists(document);
  ASSERT_TRUE(supplement);
  ASSERT_EQ(supplement->rule_sets().size(), 1u);
  SpeculationRuleSet* rule_set = supplement->rule_sets()[0];
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/foo")));
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/bar")));
}

HTMLScriptElement* InsertSpeculationRules(Document& document,
                                          const String& speculation_script) {
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("SpEcUlAtIoNrUlEs"));
  script->setText(speculation_script);
  document.head()->appendChild(script);
  return script;
}

using IncludesStyleUpdate =
    base::StrongAlias<class IncludesStyleUpdateTag, bool>;

// This runs the functor while observing any speculation rules sent by it.
// Since updates may be queued in a microtask or be blocked by style update,
// those are also awaited.
// At least one update is expected.
template <typename F>
void PropagateRulesToStubSpeculationHost(
    DummyPageHolder& page_holder,
    StubSpeculationHost& speculation_host,
    const F& functor,
    IncludesStyleUpdate includes_style_update = IncludesStyleUpdate{true}) {
  // A <script> with a case-insensitive type match should be propagated to the
  // browser via Mojo.
  // TODO(jbroman): Should we need to enable script? Should that be bypassed?
  LocalFrame& frame = page_holder.GetFrame();
  frame.GetSettings()->SetScriptEnabled(true);

  auto& broker = frame.DomWindow()->GetBrowserInterfaceBroker();
  broker.SetBinderForTesting(
      mojom::blink::SpeculationHost::Name_,
      WTF::BindRepeating(&StubSpeculationHost::BindUnsafe,
                         WTF::Unretained(&speculation_host)));

  base::RunLoop run_loop;
  speculation_host.SetDoneClosure(run_loop.QuitClosure());
  {
    auto* script_state = ToScriptStateForMainWorld(&frame);
    v8::MicrotasksScope microtasks_scope(script_state->GetIsolate(),
                                         ToMicrotaskQueue(script_state),
                                         v8::MicrotasksScope::kRunMicrotasks);
    functor();
    if (includes_style_update) {
      page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
    }
  }
  run_loop.Run();

  broker.SetBinderForTesting(mojom::blink::SpeculationHost::Name_, {});
}

void PropagateRulesToStubSpeculationHost(DummyPageHolder& page_holder,
                                         StubSpeculationHost& speculation_host,
                                         const String& speculation_script) {
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    InsertSpeculationRules(page_holder.GetDocument(), speculation_script);
  });
}

template <typename F>
testing::AssertionResult NoRulesPropagatedToStubSpeculationHost(
    DummyPageHolder& page_holder,
    StubSpeculationHost& speculation_host,
    const F& functor,
    IncludesStyleUpdate includes_style_update = IncludesStyleUpdate{true}) {
  LocalFrame& frame = page_holder.GetFrame();
  auto& broker = frame.DomWindow()->GetBrowserInterfaceBroker();
  broker.SetBinderForTesting(
      mojom::blink::SpeculationHost::Name_,
      WTF::BindRepeating(&StubSpeculationHost::BindUnsafe,
                         WTF::Unretained(&speculation_host)));

  bool done_was_called = false;

  base::RunLoop run_loop;
  speculation_host.SetDoneClosure(base::BindLambdaForTesting(
      [&done_was_called] { done_was_called = true; }));
  {
    auto* script_state = ToScriptStateForMainWorld(&frame);
    v8::MicrotasksScope microtasks_scope(script_state->GetIsolate(),
                                         ToMicrotaskQueue(script_state),
                                         v8::MicrotasksScope::kRunMicrotasks);
    functor();
    if (includes_style_update) {
      page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
    }
  }
  run_loop.RunUntilIdle();

  broker.SetBinderForTesting(mojom::blink::SpeculationHost::Name_, {});
  return done_was_called ? testing::AssertionFailure()
                         : testing::AssertionSuccess();
}

TEST_F(SpeculationRuleSetTest, PropagatesAllRulesToBrowser) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  const String speculation_script =
      R"({"prefetch": [
           {"source": "list",
            "urls": ["https://example.com/foo", "https://example.com/bar"],
            "requires": ["anonymous-client-ip-when-cross-origin"]}
         ],
          "prerender": [
           {"source": "list", "urls": ["https://example.com/prerender"]}
         ]
         })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  const auto& candidates = speculation_host.candidates();
  ASSERT_EQ(candidates.size(), 3u);
  {
    const auto& candidate = candidates[0];
    EXPECT_EQ(candidate->action, mojom::blink::SpeculationAction::kPrefetch);
    EXPECT_EQ(candidate->url, "https://example.com/foo");
    EXPECT_TRUE(candidate->requires_anonymous_client_ip_when_cross_origin);
  }
  {
    const auto& candidate = candidates[1];
    EXPECT_EQ(candidate->action, mojom::blink::SpeculationAction::kPrefetch);
    EXPECT_EQ(candidate->url, "https://example.com/bar");
    EXPECT_TRUE(candidate->requires_anonymous_client_ip_when_cross_origin);
  }
  {
    const auto& candidate = candidates[2];
    EXPECT_EQ(candidate->action, mojom::blink::SpeculationAction::kPrerender);
    EXPECT_EQ(candidate->url, "https://example.com/prerender");
  }
}

// Tests that prefetch rules are ignored unless SpeculationRulesPrefetchProxy
// is enabled.
TEST_F(SpeculationRuleSetTest, PrerenderIgnorePrefetchRules) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  const String speculation_script =
      R"({"prefetch_with_subresources": [
           {"source": "list",
            "urls": ["https://example.com/foo", "https://example.com/bar"],
            "requires": ["anonymous-client-ip-when-cross-origin"]}
         ],
          "prerender": [
           {"source": "list", "urls": ["https://example.com/prerender"]}
         ]
         })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  const auto& candidates = speculation_host.candidates();
  EXPECT_EQ(candidates.size(), 1u);
  EXPECT_FALSE(base::ranges::any_of(candidates, [](const auto& candidate) {
    return candidate->action ==
           mojom::blink::SpeculationAction::kPrefetchWithSubresources;
  }));
}

// Tests that prerender rules are ignored unless Prerender2 is enabled.
TEST_F(SpeculationRuleSetTest, PrefetchIgnorePrerenderRules) {
  // Overwrite the kPrerender2 flag.
  ScopedPrerender2ForTest enable_prerender{false};

  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  const String speculation_script =
      R"({"prefetch": [
           {"source": "list",
            "urls": ["https://example.com/foo", "https://example.com/bar"],
            "requires": ["anonymous-client-ip-when-cross-origin"]}
         ],
          "prerender": [
           {"source": "list", "urls": ["https://example.com/prerender"]}
         ]
         })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  const auto& candidates = speculation_host.candidates();
  EXPECT_EQ(candidates.size(), 2u);
  EXPECT_FALSE(base::ranges::any_of(candidates, [](const auto& candidate) {
    return candidate->action == mojom::blink::SpeculationAction::kPrerender;
  }));
}

// Tests that the presence of a speculationrules script is recorded.
TEST_F(SpeculationRuleSetTest, UseCounter) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  EXPECT_FALSE(
      page_holder.GetDocument().IsUseCounted(WebFeature::kSpeculationRules));

  const String speculation_script =
      R"({"prefetch": [{"source": "list", "urls": ["/foo"]}]})";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  EXPECT_TRUE(
      page_holder.GetDocument().IsUseCounted(WebFeature::kSpeculationRules));
}

// Tests that the presence of a speculationrules No-Vary-Search hint is
// recorded.
TEST_F(SpeculationRuleSetTest, NoVarySearchHintUseCounter) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  EXPECT_FALSE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesNoVarySearchHint));

  const String speculation_script =
      R"nvs({"prefetch": [{
        "source": "list",
        "urls": ["/foo"],
        "expects_no_vary_search": "params=(\"a\")"
      }]})nvs";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesNoVarySearchHint))
      << "No-Vary-Search hint functionality is counted";
}

// Tests that the document's URL is excluded from candidates.
TEST_F(SpeculationRuleSetTest, ExcludesFragmentLinks) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  page_holder.GetDocument().SetURL(KURL("https://example.com/"));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      String(R"({"prefetch": [
           {"source": "list", "urls":
              ["https://example.com/", "#foo", "/b#bar"]}]})"));
  EXPECT_THAT(
      speculation_host.candidates(),
      HasURLs(KURL("https://example.com"), KURL("https://example.com/b#bar")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&] {
    page_holder.GetDocument().SetURL(KURL("https://example.com/b"));
  });
  EXPECT_THAT(speculation_host.candidates(),
              HasURLs(KURL("https://example.com")));
}

// Tests that the document's URL is excluded from candidates, even when its
// changes do not affect the base URL.
TEST_F(SpeculationRuleSetTest, ExcludesFragmentLinksWithBase) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  page_holder.GetDocument().SetURL(KURL("https://example.com/"));
  page_holder.GetDocument().head()->setInnerHTML(
      "<base href=\"https://not-example.com/\">");

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      String(R"({"prefetch": [
           {"source": "list", "urls":
              ["https://example.com/#baz", "#foo", "/b#bar"]}]})"));
  EXPECT_THAT(speculation_host.candidates(),
              HasURLs(KURL("https://not-example.com/#foo"),
                      KURL("https://not-example.com/b#bar")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&] {
    page_holder.GetDocument().SetURL(KURL("https://example.com/b"));
  });
  EXPECT_THAT(speculation_host.candidates(),
              HasURLs(KURL("https://example.com/#baz"),
                      KURL("https://not-example.com/#foo"),
                      KURL("https://not-example.com/b#bar")));
}

// Tests that rules removed before the task to update speculation candidates
// runs are not reported.
TEST_F(SpeculationRuleSetTest, AddAndRemoveInSameTask) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    InsertSpeculationRules(page_holder.GetDocument(),
                           R"({"prefetch": [
             {"source": "list", "urls": ["https://example.com/foo"]}]})");
    HTMLScriptElement* to_remove =
        InsertSpeculationRules(page_holder.GetDocument(),
                               R"({"prefetch": [
             {"source": "list", "urls": ["https://example.com/bar"]}]})");
    InsertSpeculationRules(page_holder.GetDocument(),
                           R"({"prefetch": [
             {"source": "list", "urls": ["https://example.com/baz"]}]})");
    to_remove->remove();
  });

  const auto& candidates = speculation_host.candidates();
  ASSERT_EQ(candidates.size(), 2u);
  EXPECT_EQ(candidates[0]->url, "https://example.com/foo");
  EXPECT_EQ(candidates[1]->url, "https://example.com/baz");
}

// Tests that rules removed after being previously reported are reported as
// removed.
TEST_F(SpeculationRuleSetTest, AddAndRemoveAfterReport) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  HTMLScriptElement* to_remove = nullptr;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    InsertSpeculationRules(page_holder.GetDocument(),
                           R"({"prefetch": [
             {"source": "list", "urls": ["https://example.com/foo"]}]})");
    to_remove = InsertSpeculationRules(page_holder.GetDocument(),
                                       R"({"prefetch": [
             {"source": "list", "urls": ["https://example.com/bar"]}]})");
    InsertSpeculationRules(page_holder.GetDocument(),
                           R"({"prefetch": [
             {"source": "list", "urls": ["https://example.com/baz"]}]})");
  });

  {
    const auto& candidates = speculation_host.candidates();
    ASSERT_EQ(candidates.size(), 3u);
    EXPECT_EQ(candidates[0]->url, "https://example.com/foo");
    EXPECT_EQ(candidates[1]->url, "https://example.com/bar");
    EXPECT_EQ(candidates[2]->url, "https://example.com/baz");
  }

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      [&]() { to_remove->remove(); });

  {
    const auto& candidates = speculation_host.candidates();
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0]->url, "https://example.com/foo");
    EXPECT_EQ(candidates[1]->url, "https://example.com/baz");
  }
}

// Tests that removed candidates are reported in a microtask.
// This is somewhat difficult to observe in practice, but most sharply visible
// if a removal occurs and then in a subsequent microtask an addition occurs.
TEST_F(SpeculationRuleSetTest, RemoveInMicrotask) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  base::RunLoop run_loop;
  base::MockCallback<base::RepeatingCallback<void(
      const Vector<mojom::blink::SpeculationCandidatePtr>&)>>
      mock_callback;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(mock_callback, Run(::testing::SizeIs(2)));
    EXPECT_CALL(mock_callback, Run(::testing::SizeIs(1)));
    EXPECT_CALL(mock_callback, Run(::testing::SizeIs(2)))
        .WillOnce(::testing::Invoke([&]() { run_loop.Quit(); }));
  }
  speculation_host.SetCandidatesUpdatedCallback(mock_callback.Get());

  LocalFrame& frame = page_holder.GetFrame();
  frame.GetSettings()->SetScriptEnabled(true);
  auto& broker = frame.DomWindow()->GetBrowserInterfaceBroker();
  broker.SetBinderForTesting(
      mojom::blink::SpeculationHost::Name_,
      WTF::BindRepeating(&StubSpeculationHost::BindUnsafe,
                         WTF::Unretained(&speculation_host)));

  // First simulated task adds the rule sets.
  InsertSpeculationRules(page_holder.GetDocument(),
                         R"({"prefetch": [
           {"source": "list", "urls": ["https://example.com/foo"]}]})");
  HTMLScriptElement* to_remove =
      InsertSpeculationRules(page_holder.GetDocument(),
                             R"({"prefetch": [
             {"source": "list", "urls": ["https://example.com/bar"]}]})");
  scoped_refptr<scheduler::EventLoop> event_loop =
      frame.DomWindow()->GetAgent()->event_loop();
  event_loop->PerformMicrotaskCheckpoint();
  frame.View()->UpdateAllLifecyclePhasesForTest();

  // Second simulated task removes the rule sets, then adds another one in a
  // microtask which is queued later than any queued during the removal.
  to_remove->remove();
  event_loop->EnqueueMicrotask(base::BindLambdaForTesting([&] {
    InsertSpeculationRules(page_holder.GetDocument(),
                           R"({"prefetch": [
           {"source": "list", "urls": ["https://example.com/baz"]}]})");
  }));
  event_loop->PerformMicrotaskCheckpoint();

  run_loop.Run();
  broker.SetBinderForTesting(mojom::blink::SpeculationHost::Name_, {});
}

class ConsoleCapturingChromeClient : public EmptyChromeClient {
 public:
  void AddMessageToConsole(LocalFrame*,
                           mojom::ConsoleMessageSource,
                           mojom::ConsoleMessageLevel,
                           const String& message,
                           unsigned line_number,
                           const String& source_id,
                           const String& stack_trace) override {
    messages_.push_back(message);
  }

  const Vector<String>& ConsoleMessages() const { return messages_; }

 private:
  Vector<String> messages_;
};

// Tests that parse errors are logged to the console.
TEST_F(SpeculationRuleSetTest, ConsoleWarning) {
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);

  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setText("[invalid]");
  document.head()->appendChild(script);

  EXPECT_TRUE(base::ranges::any_of(
      chrome_client->ConsoleMessages(),
      [](const String& message) { return message.Contains("Syntax error"); }));
}

// Tests that errors of individual rules which cause them to be ignored are
// logged to the console.
TEST_F(SpeculationRuleSetTest, ConsoleWarningForInvalidRule) {
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);

  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setText(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": [["a", ".", "c", "o", "m"]]
        }]
      })");
  document.head()->appendChild(script);

  EXPECT_TRUE(base::ranges::any_of(
      chrome_client->ConsoleMessages(), [](const String& message) {
        return message.Contains("URLs must be given as strings");
      }));
}

// Tests that a warning is shown when speculation rules are added using the
// innerHTML setter, which doesn't currently do what the author meant.
TEST_F(SpeculationRuleSetTest, ConsoleWarningForSetInnerHTML) {
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);

  Document& document = page_holder.GetDocument();
  document.head()->setInnerHTML("<script type=speculationrules>{}</script>");

  EXPECT_TRUE(base::ranges::any_of(
      chrome_client->ConsoleMessages(), [](const String& message) {
        return message.Contains("speculation rule") &&
               message.Contains("will be ignored");
      }));
}

// Tests that a console warning mentions that child modifications are
// ineffective.
TEST_F(SpeculationRuleSetTest, ConsoleWarningForChildModification) {
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);

  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setText("{}");
  document.head()->appendChild(script);

  script->setText(R"({"prefetch": [{"urls": "/2"}]})");

  EXPECT_TRUE(base::ranges::any_of(
      chrome_client->ConsoleMessages(), [](const String& message) {
        return message.Contains("speculation rule") &&
               message.Contains("modified");
      }));
}

// Tests that a console warning mentions duplicate keys.
TEST_F(SpeculationRuleSetTest, ConsoleWarningForDuplicateKey) {
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);

  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setText(
      R"({
        "prefetch": [{"urls": ["a.html"]}],
        "prefetch": [{"urls": ["b.html"]}]
      })");
  document.head()->appendChild(script);

  EXPECT_TRUE(base::ranges::any_of(
      chrome_client->ConsoleMessages(), [](const String& message) {
        return message.Contains("speculation rule") &&
               message.Contains("more than one") &&
               message.Contains("prefetch");
      }));
}
TEST_F(SpeculationRuleSetTest, DropNotArrayAtRuleSetPosition) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": "invalid"
      })",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains(
      "A rule set for a key must be an array: path = [\"prefetch\"]"))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, DropNotObjectAtRulePosition) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": ["invalid"]
      })",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_TRUE(rule_set->error_message().Contains(
      "A rule must be an object: path = [\"prefetch\"][0]"))
      << rule_set->error_message();
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

MATCHER_P(MatchesPredicate,
          matcher,
          ::testing::DescribeMatcher<DocumentRulePredicate*>(matcher)) {
  if (!arg->predicate()) {
    *result_listener << "does not have a predicate";
    return false;
  }
  return ExplainMatchResult(matcher, arg->predicate(), result_listener);
}

String GetTypeString(DocumentRulePredicate::Type type) {
  switch (type) {
    case DocumentRulePredicate::Type::kAnd:
      return "And";
    case DocumentRulePredicate::Type::kOr:
      return "Or";
    case DocumentRulePredicate::Type::kNot:
      return "Not";
    case DocumentRulePredicate::Type::kURLPatterns:
      return "Href";
    case DocumentRulePredicate::Type::kCSSSelectors:
      return "Selector";
  }
}

template <typename ItemType>
class PredicateMatcher {
 public:
  using DocumentRulePredicateGetter =
      HeapVector<Member<ItemType>> (DocumentRulePredicate::*)() const;

  explicit PredicateMatcher(Vector<::testing::Matcher<ItemType*>> matchers,
                            DocumentRulePredicate::Type type,
                            DocumentRulePredicateGetter getter)
      : matchers_(std::move(matchers)), type_(type), getter_(getter) {}

  bool MatchAndExplain(DocumentRulePredicate* predicate,
                       ::testing::MatchResultListener* listener) const {
    if (!predicate) {
      return false;
    }

    if (predicate->GetTypeForTesting() != type_) {
      *listener << predicate->ToString();
      return false;
    }

    HeapVector<Member<ItemType>> items = ((*predicate).*(getter_))();
    if (items.size() != matchers_.size()) {
      *listener << predicate->ToString();
      return false;
    }

    ::testing::StringMatchResultListener inner_listener;
    for (wtf_size_t i = 0; i < matchers_.size(); i++) {
      if (!matchers_[i].MatchAndExplain(items[i], &inner_listener)) {
        *listener << predicate->ToString();
        return false;
      }
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << GetTypeString(type_) << "([";
    for (wtf_size_t i = 0; i < matchers_.size(); i++) {
      matchers_[i].DescribeTo(os);
      if (i != matchers_.size() - 1) {
        *os << ", ";
      }
    }
    *os << "])";
  }

  void DescribeNegationTo(::std::ostream* os) const { DescribeTo(os); }

 private:
  Vector<::testing::Matcher<ItemType*>> matchers_;
  DocumentRulePredicate::Type type_;
  DocumentRulePredicateGetter getter_;
};

template <typename ItemType>
auto MakePredicateMatcher(
    Vector<::testing::Matcher<ItemType*>> matchers,
    DocumentRulePredicate::Type type,
    typename PredicateMatcher<ItemType>::DocumentRulePredicateGetter getter) {
  return testing::MakePolymorphicMatcher(
      PredicateMatcher<ItemType>(std::move(matchers), type, getter));
}

auto MakeConditionMatcher(
    Vector<::testing::Matcher<DocumentRulePredicate*>> matchers,
    DocumentRulePredicate::Type type) {
  return MakePredicateMatcher(
      std::move(matchers), type,
      &DocumentRulePredicate::GetSubPredicatesForTesting);
}

auto And(Vector<::testing::Matcher<DocumentRulePredicate*>> matchers = {}) {
  return MakeConditionMatcher(std::move(matchers),
                              DocumentRulePredicate::Type::kAnd);
}

auto Or(Vector<::testing::Matcher<DocumentRulePredicate*>> matchers = {}) {
  return MakeConditionMatcher(std::move(matchers),
                              DocumentRulePredicate::Type::kOr);
}

auto Neg(::testing::Matcher<DocumentRulePredicate*> matcher) {
  return MakeConditionMatcher({matcher}, DocumentRulePredicate::Type::kNot);
}

auto Href(Vector<::testing::Matcher<URLPattern*>> pattern_matchers = {}) {
  return MakePredicateMatcher(std::move(pattern_matchers),
                              DocumentRulePredicate::Type::kURLPatterns,
                              &DocumentRulePredicate::GetURLPatternsForTesting);
}

auto Selector(Vector<::testing::Matcher<StyleRule*>> style_rule_matchers = {}) {
  return MakePredicateMatcher(std::move(style_rule_matchers),
                              DocumentRulePredicate::Type::kCSSSelectors,
                              &DocumentRulePredicate::GetStyleRulesForTesting);
}

class StyleRuleMatcher {
 public:
  explicit StyleRuleMatcher(String selector_text)
      : selector_text_(std::move(selector_text)) {}

  bool MatchAndExplain(StyleRule* style_rule,
                       ::testing::MatchResultListener* listener) const {
    if (!style_rule) {
      return false;
    }
    return style_rule->SelectorsText() == selector_text_;
  }

  void DescribeTo(::std::ostream* os) const { *os << selector_text_; }

  void DescribeNegationTo(::std::ostream* os) const { DescribeTo(os); }

 private:
  String selector_text_;
};

auto StyleRuleWithSelectorText(String selector_text) {
  return ::testing::MakePolymorphicMatcher(StyleRuleMatcher(selector_text));
}

class DocumentRulesTest : public SpeculationRuleSetTest {
 public:
  ~DocumentRulesTest() override = default;

  DocumentRulePredicate* CreatePredicate(
      String where_text,
      KURL base_url = KURL("https://example.com/")) {
    auto* rule_set = CreateRuleSetWithPredicate(where_text, base_url);
    DCHECK(!rule_set->prefetch_rules().empty())
        << "Invalid predicate: " << rule_set->error_message();
    return rule_set->prefetch_rules()[0]->predicate();
  }

  String CreateInvalidPredicate(String where_text) {
    auto* rule_set =
        CreateRuleSetWithPredicate(where_text, KURL("https://example.com"));
    EXPECT_TRUE(!rule_set || rule_set->prefetch_rules().empty())
        << "Rule set is valid.";
    return rule_set->error_message();
  }

 private:
  SpeculationRuleSet* CreateRuleSetWithPredicate(String where_text,
                                                 KURL base_url) {
    // clang-format off
    auto* rule_set =
        CreateRuleSet(
          String::Format(
            R"({
              "prefetch": [{
                "source": "document",
                "where": {%s}
              }]
            })",
            where_text.Latin1().c_str()),
          base_url, execution_context());
    // clang-format on
    return rule_set;
  }
};

TEST_F(DocumentRulesTest, ParseAnd) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "document",
          "where": { "and": [] }
        }, {
          "source": "document",
          "where": {"and": [{"and": []}, {"and": []}]}
        }]
      })",
      KURL("https://example.com/"), execution_context());
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesPredicate(And()),
                          MatchesPredicate(And({And(), And()}))));
}

TEST_F(DocumentRulesTest, ParseOr) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "document",
          "where": { "or": [] }
        }, {
          "source": "document",
          "where": {"or": [{"and": []}, {"or": []}]}
        }]
      })",
      KURL("https://example.com/"), execution_context());
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(MatchesPredicate(Or()), MatchesPredicate(Or({And(), Or()}))));
}

TEST_F(DocumentRulesTest, ParseNot) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "document",
          "where": {"not": {"and": []}}
        }, {
          "source": "document",
          "where": {"not": {"or": [{"and": []}, {"or": []}]}}
        }]
      })",
      KURL("https://example.com/"), execution_context());
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesPredicate(Neg(And())),
                          MatchesPredicate(Neg(Or({And(), Or()})))));
}

TEST_F(DocumentRulesTest, ParseHref) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "document",
          "where": {"href_matches": "/foo#bar"}
        }, {
          "source": "document",
          "where": {"href_matches": {"pathname": "/foo"}}
        }, {
          "source": "document",
          "where": {"href_matches": [
            {"pathname": "/buzz"},
            "/fizz",
            {"hostname": "bar.com"}
          ]}
        }, {
          "source": "document",
          "where": {"or": [
            {"href_matches": {"hostname": "foo.com"}},
            {"not": {"href_matches": {"protocol": "http", "hostname": "*"}}}
          ]}
        }]
      })",
      KURL("https://example.com/"), execution_context());
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(
          MatchesPredicate(Href({URLPattern("/foo#bar")})),
          MatchesPredicate(Href({URLPattern("/foo")})),
          MatchesPredicate(Href({URLPattern("/buzz"), URLPattern("/fizz"),
                                 URLPattern("https://bar.com:*")})),
          MatchesPredicate(Or({Href({URLPattern("https://foo.com:*")}),
                               Neg(Href({URLPattern("http://*:*")}))}))));
}

TEST_F(DocumentRulesTest, ParseHref_AllUrlPatternKeys) {
  auto* href_matches = CreatePredicate(R"("href_matches": {
    "username": "",
    "password": "",
    "port": "*",
    "pathname": "/*",
    "search": "*",
    "hash": "",
    "protocol": "https",
    "hostname": "abc.xyz",
    "baseURL": "https://example.com"
  })");
  EXPECT_THAT(href_matches, Href({URLPattern("https://:@abc.xyz:*/*\\?*#")}));
}

TEST_F(DocumentRulesTest, HrefMatchesWithBaseURL) {
  auto* without_base_specified = CreatePredicate(
      R"("href_matches": {"pathname": "/hello"})", KURL("http://foo.com"));
  EXPECT_THAT(without_base_specified,
              Href({URLPattern("http://foo.com/hello")}));
  auto* with_base_specified = CreatePredicate(
      R"("href_matches": {"pathname": "hello", "baseURL": "http://bar.com"})",
      KURL("http://foo.com"));
  EXPECT_THAT(with_base_specified, Href({URLPattern("http://bar.com/hello")}));
}

// Testing on http://bar.com requesting a ruleset from http://foo.com.
TEST_F(DocumentRulesTest, HrefMatchesWithBaseURLAndRelativeTo) {
  execution_context()->SetURL(KURL{"http://bar.com"});

  auto* with_relative_to = CreatePredicate(
      R"(
        "href_matches": "/hello",
        "relative_to": "document"
      )",
      KURL("http://foo.com"));
  EXPECT_THAT(with_relative_to, Href({URLPattern("http://bar.com/hello")}));

  auto* relative_to_no_effect = CreatePredicate(
      R"(
        "href_matches": {"pathname": "/hello", "baseURL": "http://buz.com"},
        "relative_to": "document"
      )",
      KURL("http://foo.com"));
  EXPECT_THAT(relative_to_no_effect,
              Href({URLPattern("http://buz.com/hello")}));

  auto* nested_relative_to = CreatePredicate(
      R"(
        "or": [
          {
            "href_matches": {"pathname": "/hello"},
            "relative_to": "document"
          },
          {"not": {"href_matches": "/world"}}
        ]
      )",
      KURL("http://foo.com/"));

  EXPECT_THAT(nested_relative_to,
              Or({Href({URLPattern("http://bar.com/hello")}),
                  Neg(Href({URLPattern("http://foo.com/world")}))}));

  auto* relative_to_ruleset = CreatePredicate(R"(
        "href_matches": {"pathname": "/hello"},
        "relative_to": "ruleset"
      )",
                                              KURL("http://foo.com"));
  EXPECT_THAT(relative_to_ruleset, Href({URLPattern("http://foo.com/hello")}));
}

TEST_F(DocumentRulesTest, DropInvalidRules) {
  auto* rule_set = CreateRuleSet(
      R"({"prefetch": [)"

      // A rule that doesn't elaborate on its source (previously disallowed).
      // TODO(crbug.com/1517696): Remove this when SpeculationRulesImplictSource
      // is permanently shipped, so keep the test focused.
      R"({"where": {"and": []}},)"

      // A rule with an unrecognized source.
      R"({"source": "magic-8-ball", "where": {"and": []}},)"

      // A list rule with a "where" key.
      R"({"source": "list", "where": {"and": []}},)"

      // A document rule with a "urls" key.
      R"({"source": "document", "urls": ["foo.html"]},)"

      // "where" clause is not a map.
      R"({"source": "document", "where": [{"and": []}]},)"

      // "where" clause does not contain one of "and", "or", "not",
      // "href_matches" and "selector_matches"
      R"({"source": "document", "where": {"foo": "bar"}},)"

      // "where" clause has both "and" and "or" as keys
      R"({"source": "document", "where": {"and": [], "or": []}},)"

      // "and" key has object value.
      R"({"source": "document", "where": {"and": {}}},)"

      // "or" key has object value.
      R"({"source": "document", "where": {"or": {}}},)"

      // "and" key has invalid list value.
      R"({"source": "document", "where": {"and": ["foo"]}},)"

      // "not" key has list value.
      R"({"source": "document", "where": {"not": [{"and": []}]}},)"

      // "not" key has empty object value.
      R"({"source": "document", "where": {"not": {}}},)"

      // "not" key has invalid object value.
      R"({"source": "document", "where": {"not": {"foo": "bar"}}},)"

      // pattern is not a string or map value.
      R"({"source": "document", "where": {"href_matches": false}},)"

      // pattern string is invalid.
      R"({"source": "document", "where": {"href_matches": "::"}},)"

      // pattern object has invalid key.
      R"({"source": "document", "where": {"href_matches": {"foo": "bar"}}},)"

      // pattern object has invalid value.
      R"({"source": "document",
          "where": {"href_matches": {"protocol": "::"}}},)"

      // Invalid key pairs.
      R"({
          "source": "document",
          "where": {"href_matches": "/hello.html",
                    "invalid_key": "invalid_val"}
        },)"

      // Invalid values of "relative_to".
      R"({
          "source": "document",
          "where": {"href_matches": "/hello.html",
                    "relative_to": 2022}
        },)"
      R"({
          "source": "document",
          "where": {"href_matches": "/hello.html",
                    "relative_to": "not_document"}
        },)"

      // "relative_to" appears at speculation rule level instead of the
      // "href_matches" clause.
      R"({
          "source": "document",
          "where": {"href_matches": "/hello"},
          "relative_to": "document"
        },)"

      // Currently the spec does not allow three keys.
      R"({"source": "document",
          "where":{"href_matches": "/hello.html",
                   "relative_to": "document",
                   "world-cup": "2022"}},)"

      // "selector_matches" paired with another key.
      R"({"source": "document",
          "where": {"selector_matches": ".valid", "second": "value"}
        },)"

      // "selector_matches" with an object value.
      R"({"source": "document",
          "where": {"selector_matches": {"selector": ".valid"}}
        },)"

      // "selector_matches" with an invalid CSS selector.
      R"({"source": "document",
          "where": {"selector_matches": "#invalid#"}
        },)"

      // "selector_matches" with a list with an object.
      R"({"source": "document",
          "where": {"selector_matches": [{"selector": ".valid"}]}
        },)"

      // "selector_matches" with a list with one valid and one invalid CSS
      // selector.
      R"({"source": "document",
        "where": {"selector_matches": [".valid", "#invalid#"]}
        },)"

      // Invalid no-vary-search hint value.
      R"({"source": "list",
        "urls": ["/prefetch/list/page1.html"],
        "expects_no_vary_search": 0
        },)"

      // Both "where" and "urls" with implicit source.
      R"({"urls": ["/"], "where": {"selector_matches": "*"}},)"

      // Neither "where" nor "urls" with implicit source.
      R"({},)"

      // valid document rule.
      R"({"source": "document",
        "where": {"and": [
          {"or": [{"href_matches": "/hello.html"},
                  {"selector_matches": ".valid"}]},
          {"not": {"and": [{"href_matches": {"hostname": "world.com"}}]}}
        ]}
    }]})",
      KURL("https://example.com/"), execution_context());
  ASSERT_TRUE(rule_set);
  EXPECT_EQ(rule_set->error_type(),
            SpeculationRuleSetErrorType::kInvalidRulesSkipped);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(
          MatchesPredicate(And({})),
          MatchesPredicate(
              And({Or({Href({URLPattern("/hello.html")}),
                       Selector({StyleRuleWithSelectorText(".valid")})}),
                   Neg(And({Href({URLPattern("https://world.com:*")})}))}))));
}

// Tests that errors of individual rules which cause them to be ignored are
// logged to the console.
TEST_F(DocumentRulesTest, ConsoleWarningForInvalidRule) {
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);

  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setText(
      R"({
        "prefetch": [{
          "source": "document",
          "where": {"and": [], "or": []}
        }]
      })");
  document.head()->appendChild(script);

  EXPECT_TRUE(base::ranges::any_of(
      chrome_client->ConsoleMessages(), [](const String& message) {
        return message.Contains("Document rule predicate type is ambiguous");
      }));
}

TEST_F(DocumentRulesTest, DocumentRuleParseErrors) {
  auto* rule_set1 =
      CreateRuleSet(R"({"prefetch": [{
    "source": "document", "relative_to": "document"
  }]})",
                    KURL("https://example.com"), execution_context());
  EXPECT_THAT(
      rule_set1->error_message().Utf8(),
      ::testing::HasSubstr("A document rule cannot have \"relative_to\" "
                           "outside the \"where\" clause"));

  auto* rule_set2 =
      CreateRuleSet(R"({"prefetch": [{
    "source": "document",
    "urls": ["/one",  "/two"]
  }]})",
                    KURL("https://example.com"), execution_context());
  EXPECT_THAT(
      rule_set2->error_message().Utf8(),
      ::testing::HasSubstr("A document rule cannot have a \"urls\" key"));
}

TEST_F(DocumentRulesTest, DocumentRulePredicateParseErrors) {
  String parse_error;

  parse_error = CreateInvalidPredicate(R"("and": [], "not": {})");
  EXPECT_THAT(
      parse_error.Utf8(),
      ::testing::HasSubstr(
          "Document rule predicate type is ambiguous, two types found"));

  parse_error = CreateInvalidPredicate(R"()");
  EXPECT_THAT(parse_error.Utf8(),
              ::testing::HasSubstr("Could not infer type of document rule "
                                   "predicate, no valid type specified"));

  parse_error =
      CreateInvalidPredicate(R"("not": [{"href_matches": "foo.com"}])");
  EXPECT_THAT(
      parse_error.Utf8(),
      ::testing::HasSubstr("Document rule predicate must be an object"));

  parse_error =
      CreateInvalidPredicate(R"("and": [], "relative_to": "document")");
  EXPECT_THAT(
      parse_error.Utf8(),
      ::testing::HasSubstr(
          "Document rule predicate with \"and\" key cannot have other keys."));

  parse_error = CreateInvalidPredicate(R"("or": {})");
  EXPECT_THAT(parse_error.Utf8(),
              ::testing::HasSubstr("\"or\" key should have a list value"));

  parse_error = CreateInvalidPredicate(R"("href_matches": {"port": 1234})");
  EXPECT_THAT(
      parse_error.Utf8(),
      ::testing::HasSubstr("Values for a URL pattern object must be strings"));

  parse_error =
      CreateInvalidPredicate(R"("href_matches": {"path_name": "foo"})");
  EXPECT_THAT(parse_error.Utf8(),
              ::testing::HasSubstr("Invalid key \"path_name\" for a URL "
                                   "pattern object found"));

  parse_error =
      CreateInvalidPredicate(R"("href_matches": [["bar.com/foo.html"]])");
  EXPECT_THAT(parse_error.Utf8(),
              ::testing::HasSubstr("Value for \"href_matches\" should "
                                   "either be a string"));

  parse_error = CreateInvalidPredicate(
      R"("href_matches": "/home", "relative_to": "window")");
  EXPECT_THAT(
      parse_error.Utf8(),
      ::testing::HasSubstr("Unrecognized \"relative_to\" value: \"window\""));

  parse_error = CreateInvalidPredicate(
      R"("href_matches": "/home", "relativeto": "document")");
  EXPECT_THAT(parse_error.Utf8(),
              ::testing::HasSubstr("Unrecognized key found: \"relativeto\""));

  parse_error = CreateInvalidPredicate(R"("href_matches": "https//:")");
  EXPECT_THAT(parse_error.Utf8(),
              ::testing::HasSubstr("URL Pattern for \"href_matches\" could not "
                                   "be parsed: \"https//:\""));

  parse_error = CreateInvalidPredicate(R"("selector_matches": {})");
  EXPECT_THAT(
      parse_error.Utf8(),
      ::testing::HasSubstr("Value for \"selector_matches\" must be a string"));

  parse_error =
      CreateInvalidPredicate(R"("selector_matches": "##bad_selector")");
  EXPECT_THAT(
      parse_error.Utf8(),
      ::testing::HasSubstr("\"##bad_selector\" is not a valid selector"));
}

TEST_F(DocumentRulesTest, DefaultPredicate) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "source": "document"
        }]
      })",
      KURL("https://example.com/"), execution_context());
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre(MatchesPredicate(And())));
}

TEST_F(DocumentRulesTest, EvaluateCombinators) {
  DummyPageHolder page_holder;
  Document& document = page_holder.GetDocument();
  HTMLAnchorElement* link = MakeGarbageCollected<HTMLAnchorElement>(document);

  auto* empty_and = CreatePredicate(R"("and": [])");
  EXPECT_THAT(empty_and, And());
  EXPECT_TRUE(empty_and->Matches(*link));

  auto* empty_or = CreatePredicate(R"("or": [])");
  EXPECT_THAT(empty_or, Or());
  EXPECT_FALSE(empty_or->Matches(*link));

  auto* and_false_false_false =
      CreatePredicate(R"("and": [{"or": []}, {"or": []}, {"or": []}])");
  EXPECT_THAT(and_false_false_false, And({Or(), Or(), Or()}));
  EXPECT_FALSE(and_false_false_false->Matches(*link));

  auto* and_false_true_false =
      CreatePredicate(R"("and": [{"or": []}, {"and": []}, {"or": []}])");
  EXPECT_THAT(and_false_true_false, And({Or(), And(), Or()}));
  EXPECT_FALSE(and_false_true_false->Matches(*link));

  auto* and_true_true_true =
      CreatePredicate(R"("and": [{"and": []}, {"and": []}, {"and": []}])");
  EXPECT_THAT(and_true_true_true, And({And(), And(), And()}));
  EXPECT_TRUE(and_true_true_true->Matches(*link));

  auto* or_false_false_false =
      CreatePredicate(R"("or": [{"or": []}, {"or": []}, {"or": []}])");
  EXPECT_THAT(or_false_false_false, Or({Or(), Or(), Or()}));
  EXPECT_FALSE(or_false_false_false->Matches(*link));

  auto* or_false_true_false =
      CreatePredicate(R"("or": [{"or": []}, {"and": []}, {"or": []}])");
  EXPECT_THAT(or_false_true_false, Or({Or(), And(), Or()}));
  EXPECT_TRUE(or_false_true_false->Matches(*link));

  auto* or_true_true_true =
      CreatePredicate(R"("or": [{"and": []}, {"and": []}, {"and": []}])");
  EXPECT_THAT(or_true_true_true, Or({And(), And(), And()}));
  EXPECT_TRUE(or_true_true_true->Matches(*link));

  auto* not_true = CreatePredicate(R"("not": {"and": []})");
  EXPECT_THAT(not_true, Neg(And()));
  EXPECT_FALSE(not_true->Matches(*link));

  auto* not_false = CreatePredicate(R"("not": {"or": []})");
  EXPECT_THAT(not_false, Neg(Or()));
  EXPECT_TRUE(not_false->Matches(*link));
}

TEST_F(DocumentRulesTest, EvaluateHrefMatches) {
  DummyPageHolder page_holder;
  Document& document = page_holder.GetDocument();
  HTMLAnchorElement* link = MakeGarbageCollected<HTMLAnchorElement>(document);
  link->setHref("https://foo.com/bar.html?fizz=buzz");

  // No patterns specified, will not match any link.
  auto* empty = CreatePredicate(R"("href_matches": [])");
  EXPECT_FALSE(empty->Matches(*link));

  // Single pattern (should match).
  auto* single =
      CreatePredicate(R"("href_matches": "https://foo.com/bar.html?*")");
  EXPECT_TRUE(single->Matches(*link));

  // Two patterns which don't match.
  auto* double_fail = CreatePredicate(
      R"("href_matches": ["http://foo.com/*", "https://bar.com/*"])");
  EXPECT_FALSE(double_fail->Matches(*link));

  // One pattern that matches, one that doesn't - should still pass due to
  // an implicit or between patterns in a href_matches list.
  auto* pass_fail = CreatePredicate(
      R"("href_matches": ["https://foo.com/bar.html?*", "https://bar.com/*"])");
  EXPECT_TRUE(pass_fail->Matches(*link));
}

HTMLAnchorElement* AddAnchor(ContainerNode& parent, const String& href) {
  HTMLAnchorElement* link =
      MakeGarbageCollected<HTMLAnchorElement>(parent.GetDocument());
  link->setHref(href);
  parent.appendChild(link);
  return link;
}

HTMLAreaElement* AddAreaElement(ContainerNode& parent, const String& href) {
  HTMLAreaElement* area =
      MakeGarbageCollected<HTMLAreaElement>(parent.GetDocument());
  area->setHref(href);
  parent.appendChild(area);
  return area;
}

// Tests that speculation candidates based of existing links are reported after
// a document rule is inserted.
TEST_F(DocumentRulesTest, SpeculationCandidatesReportedAfterInitialization) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  AddAnchor(*document.body(), "https://foo.com/doc.html");
  AddAnchor(*document.body(), "https://bar.com/doc.html");
  AddAnchor(*document.body(), "https://foo.com/doc2.html");

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/doc.html"),
                                  KURL("https://foo.com/doc2.html")));
}

// Tests that speculation candidates based of existing links are reported after
// a document rule is inserted. Test that the speculation candidates include
// No-Vary-Search hint.
TEST_F(DocumentRulesTest,
       SpeculationCandidatesReportedAfterInitializationWithNVS) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  AddAnchor(*document.body(), "https://foo.com/doc.html");
  AddAnchor(*document.body(), "https://bar.com/doc.html");
  AddAnchor(*document.body(), "https://foo.com/doc2.html");

  String speculation_script = R"nvs(
    {"prefetch": [{
      "source": "document",
      "where": {"href_matches": "https://foo.com/*"},
      "expects_no_vary_search": "params=(\"a\")"
    }]}
  )nvs";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/doc.html"),
                                  KURL("https://foo.com/doc2.html")));
  //  Check that the candidates have the correct No-Vary-Search hint.
  EXPECT_THAT(candidates, ::testing::Each(::testing::AllOf(
                              HasNoVarySearchHint(), NVSVariesOnKeyOrder(),
                              NVSHasNoVaryParams("a"))));
}

// Tests that a new speculation candidate is reported after different
// modifications to a link.
TEST_F(DocumentRulesTest, SpeculationCandidatesUpdatedAfterLinkModifications) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  ASSERT_TRUE(candidates.empty());
  HTMLAnchorElement* link = nullptr;

  // Add link with href that matches.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    link = AddAnchor(*document.body(), "https://foo.com/action.html");
  });
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_EQ(candidates[0]->url, KURL("https://foo.com/action.html"));

  // Update link href to URL that doesn't match.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    link->setHref("https://bar.com/document.html");
  });
  EXPECT_TRUE(candidates.empty());

  // Update link href to URL that matches.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    link->setHref("https://foo.com/document.html");
  });
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_EQ(candidates[0]->url, KURL("https://foo.com/document.html"));

  // Remove link.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      [&]() { link->remove(); });
  EXPECT_TRUE(candidates.empty());
}

// Tests that a new list of speculation candidates is reported after a rule set
// is added/removed.
TEST_F(DocumentRulesTest, SpeculationCandidatesUpdatedAfterRuleSetsChanged) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  KURL url_1 = KURL("https://foo.com/abc");
  KURL url_2 = KURL("https://foo.com/xyz");
  AddAnchor(*document.body(), url_1);
  AddAnchor(*document.body(), url_2);

  String speculation_script_1 = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script_1);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(url_1, url_2));

  // Add a new rule set; the number of candidates should double.
  String speculation_script_2 = R"(
    {"prerender": [
      {"source": "document", "where": {"not":
        {"href_matches": {"protocol": "https", "hostname": "bar.com"}}
      }}
    ]}
  )";
  HTMLScriptElement* script_el = nullptr;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    script_el = InsertSpeculationRules(document, speculation_script_2);
  });
  EXPECT_THAT(candidates, HasURLs(url_1, url_1, url_2, url_2));
  EXPECT_THAT(candidates, ::testing::UnorderedElementsAre(
                              HasAction(mojom::SpeculationAction::kPrefetch),
                              HasAction(mojom::SpeculationAction::kPrefetch),
                              HasAction(mojom::SpeculationAction::kPrerender),
                              HasAction(mojom::SpeculationAction::kPrerender)));

  // Remove the recently added rule set, the number of candidates should be
  // halved.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      [&]() { script_el->remove(); });
  ASSERT_EQ(candidates.size(), 2u);
  EXPECT_THAT(candidates, HasURLs(url_1, url_2));
  EXPECT_THAT(candidates,
              ::testing::Each(HasAction(mojom::SpeculationAction::kPrefetch)));
}

// Tests that list and document speculation rules work in combination correctly.
TEST_F(DocumentRulesTest, ListRuleCombinedWithDocumentRule) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  AddAnchor(*document.body(), "https://foo.com/bar");
  String speculation_script = R"(
    {"prefetch": [
      {"source": "document"},
      {"source": "list", "urls": ["https://bar.com/foo"]}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar"),
                                  KURL("https://bar.com/foo")));
}

// Tests that candidates created for document rules are correct when
// "anonymous-client-ip-when-cross-origin" is specified.
TEST_F(DocumentRulesTest, RequiresAnonymousClientIPWhenCrossOrigin) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  AddAnchor(*document.body(), "https://foo.com/bar");
  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "requires": ["anonymous-client-ip-when-cross-origin"]
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_TRUE(candidates[0]->requires_anonymous_client_ip_when_cross_origin);
}

// Tests that a link inside a shadow tree is included when creating
// document-rule based speculation candidates. Also tests that an "unslotted"
// link (link inside shadow host that isn't assigned to a slot) is not included.
TEST_F(DocumentRulesTest, LinkInShadowTreeIncluded) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  Document& document = page_holder.GetDocument();
  ShadowRoot& shadow_root =
      document.body()->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  auto* shadow_tree_link = AddAnchor(shadow_root, "https://foo.com/bar.html");
  AddAnchor(*document.body(), "https://foo.com/unslotted");

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar.html")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    shadow_tree_link->setHref("https://not-foo.com/");
  });
  EXPECT_TRUE(candidates.empty());

  HTMLAnchorElement* shadow_tree_link_2 = nullptr;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    shadow_tree_link_2 = AddAnchor(shadow_root, "https://foo.com/buzz");
  });
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_EQ(candidates[0]->url, KURL("https://foo.com/buzz"));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      [&]() { shadow_tree_link_2->remove(); });
  EXPECT_TRUE(candidates.empty());
}

// Tests that an anchor element with no href attribute is handled correctly.
TEST_F(DocumentRulesTest, LinkWithNoHrefAttribute) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* link = MakeGarbageCollected<HTMLAnchorElement>(document);
  document.body()->appendChild(link);
  ASSERT_FALSE(link->FastHasAttribute(html_names::kHrefAttr));

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  ASSERT_TRUE(candidates.empty());

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    link->setHref("https://foo.com/bar");
  });
  ASSERT_EQ(candidates.size(), 1u);
  ASSERT_EQ(candidates[0]->url, "https://foo.com/bar");

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    link->removeAttribute(html_names::kHrefAttr);
  });
  ASSERT_TRUE(candidates.empty());

  // Just to test that no DCHECKs are hit.
  link->remove();
}

// Tests that links with non-HTTP(s) urls are ignored.
TEST_F(DocumentRulesTest, LinkWithNonHttpHref) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* link = AddAnchor(*document.body(), "mailto:abc@xyz.com");
  String speculation_script = R"({"prefetch": [{"source": "document"}]})";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  ASSERT_TRUE(candidates.empty());

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    link->setHref("https://foo.com/bar");
  });
  EXPECT_THAT(candidates, HasURLs("https://foo.com/bar"));
}

// Tests a couple of edge cases:
// 1) Removing a link that doesn't match any rules
// 2) Adding and removing a link before running microtasks (i.e. before calling
// UpdateSpeculationCandidates).
TEST_F(DocumentRulesTest, RemovingUnmatchedAndPendingLinks) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* unmatched_link = AddAnchor(*document.body(), "https://bar.com/foo");
  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_TRUE(candidates.empty());

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    auto* pending_link = AddAnchor(*document.body(), "https://foo.com/bar");
    unmatched_link->remove();
    pending_link->remove();
  });
  EXPECT_TRUE(candidates.empty());
}

// Tests if things still work if we use <area> instead of <a>.
TEST_F(DocumentRulesTest, AreaElement) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();
  HTMLAreaElement* area =
      AddAreaElement(*document.body(), "https://foo.com/action.html");

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_EQ(candidates[0]->url, KURL("https://foo.com/action.html"));

  // Update area href to URL that doesn't match.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    area->setHref("https://bar.com/document.html");
  });
  EXPECT_TRUE(candidates.empty());

  // Update area href to URL that matches.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    area->setHref("https://foo.com/document.html");
  });
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_EQ(candidates[0]->url, KURL("https://foo.com/document.html"));

  // Remove area.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      [&]() { area->remove(); });
  EXPECT_TRUE(candidates.empty());
}

// Test that adding a link to an element that isn't connected doesn't DCHECK.
TEST_F(DocumentRulesTest, DisconnectedLink) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  ASSERT_TRUE(candidates.empty());

  HTMLDivElement* div = nullptr;
  HTMLAnchorElement* link = nullptr;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    div = MakeGarbageCollected<HTMLDivElement>(document);
    link = AddAnchor(*div, "https://foo.com/blah.html");
    document.body()->AppendChild(div);
  });
  EXPECT_EQ(candidates.size(), 1u);

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    div->remove();
    link->remove();
  });
  EXPECT_TRUE(candidates.empty());
}

// Similar to test above, but now inside a shadow tree.
TEST_F(DocumentRulesTest, DisconnectedLinkInShadowTree) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  ASSERT_TRUE(candidates.empty());

  HTMLDivElement* div = nullptr;
  HTMLAnchorElement* link = nullptr;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    div = MakeGarbageCollected<HTMLDivElement>(document);
    ShadowRoot& shadow_root =
        div->AttachShadowRootForTesting(ShadowRootMode::kOpen);
    link = AddAnchor(shadow_root, "https://foo.com/blah.html");
    document.body()->AppendChild(div);
  });
  EXPECT_EQ(candidates.size(), 1u);

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    div->remove();
    link->remove();
  });
  EXPECT_TRUE(candidates.empty());
}

// Tests that a document rule's specified referrer policy is used.
TEST_F(DocumentRulesTest, ReferrerPolicy) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* link_with_referrer = AddAnchor(*document.body(), "https://foo.com/abc");
  link_with_referrer->setAttribute(html_names::kReferrerpolicyAttr,
                                   AtomicString("same-origin"));
  auto* link_with_rel_no_referrer =
      AddAnchor(*document.body(), "https://foo.com/def");
  link_with_rel_no_referrer->setAttribute(html_names::kRelAttr,
                                          AtomicString("noreferrer"));

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"href_matches": "https://foo.com/*"},
      "referrer_policy": "strict-origin"
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, ::testing::Each(HasReferrerPolicy(
                              network::mojom::ReferrerPolicy::kStrictOrigin)));
}

// Tests that a link's referrer-policy value is used if one is not specified
// in the document rule.
TEST_F(DocumentRulesTest, LinkReferrerPolicy) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();
  page_holder.GetFrame().DomWindow()->SetReferrerPolicy(
      network::mojom::ReferrerPolicy::kStrictOrigin);

  auto* link_with_referrer = AddAnchor(*document.body(), "https://foo.com/abc");
  link_with_referrer->setAttribute(html_names::kReferrerpolicyAttr,
                                   AtomicString("same-origin"));
  auto* link_with_no_referrer =
      AddAnchor(*document.body(), "https://foo.com/xyz");
  auto* link_with_rel_noreferrer =
      AddAnchor(*document.body(), "https://foo.com/mno");
  link_with_rel_noreferrer->setAttribute(html_names::kRelAttr,
                                         AtomicString("noreferrer"));
  auto* link_with_invalid_referrer =
      AddAnchor(*document.body(), "https://foo.com/pqr");
  link_with_invalid_referrer->setAttribute(html_names::kReferrerpolicyAttr,
                                           AtomicString("invalid"));
  auto* link_with_disallowed_referrer =
      AddAnchor(*document.body(), "https://foo.com/aaa");
  link_with_disallowed_referrer->setAttribute(html_names::kReferrerpolicyAttr,
                                              AtomicString("unsafe-url"));

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(
      candidates,
      ::testing::UnorderedElementsAre(
          ::testing::AllOf(
              HasURL(link_with_referrer->HrefURL()),
              HasReferrerPolicy(network::mojom::ReferrerPolicy::kSameOrigin)),
          ::testing::AllOf(
              HasURL(link_with_rel_noreferrer->HrefURL()),
              HasReferrerPolicy(network::mojom::ReferrerPolicy::kNever)),
          ::testing::AllOf(
              HasURL(link_with_no_referrer->HrefURL()),
              HasReferrerPolicy(network::mojom::ReferrerPolicy::kStrictOrigin)),
          ::testing::AllOf(
              HasURL(link_with_invalid_referrer->HrefURL()),
              HasReferrerPolicy(
                  network::mojom::ReferrerPolicy::kStrictOrigin))));

  // Console message should have been logged for
  // |link_with_disallowed_referrer|.
  const auto& console_message_storage =
      page_holder.GetPage().GetConsoleMessageStorage();
  EXPECT_EQ(console_message_storage.size(), 1u);
  EXPECT_THAT(console_message_storage.at(0)->Nodes(),
              testing::Contains(link_with_disallowed_referrer->GetDomNodeId()));
}

// Tests that changing the "referrerpolicy" attribute results in the
// corresponding speculation candidate updating.
TEST_F(DocumentRulesTest, ReferrerPolicyAttributeChangeCausesLinkInvalidation) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* link_with_referrer = AddAnchor(*document.body(), "https://foo.com/abc");
  link_with_referrer->setAttribute(html_names::kReferrerpolicyAttr,
                                   AtomicString("same-origin"));
  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, ElementsAre(HasReferrerPolicy(
                              network::mojom::ReferrerPolicy::kSameOrigin)));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    link_with_referrer->setAttribute(html_names::kReferrerpolicyAttr,
                                     AtomicString("strict-origin"));
  });
  EXPECT_THAT(candidates, ElementsAre(HasReferrerPolicy(
                              network::mojom::ReferrerPolicy::kStrictOrigin)));
}

// Tests that changing the "rel" attribute results in the corresponding
// speculation candidate updating. Also tests that "rel=noreferrer" overrides
// the referrerpolicy attribute.
TEST_F(DocumentRulesTest, RelAttributeChangeCausesLinkInvalidation) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* link = AddAnchor(*document.body(), "https://foo.com/abc");
  link->setAttribute(html_names::kReferrerpolicyAttr,
                     AtomicString("same-origin"));

  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, ElementsAre(HasReferrerPolicy(
                              network::mojom::ReferrerPolicy::kSameOrigin)));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    link->setAttribute(html_names::kRelAttr, AtomicString("noreferrer"));
  });
  EXPECT_THAT(
      candidates,
      ElementsAre(HasReferrerPolicy(network::mojom::ReferrerPolicy::kNever)));
}

TEST_F(DocumentRulesTest, ReferrerMetaChangeShouldInvalidateCandidates) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  AddAnchor(*document.body(), "https://foo.com/abc");
  String speculation_script = R"(
    {"prefetch": [
      {"source": "document", "where": {"href_matches": "https://foo.com/*"}}
    ]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(
      candidates,
      ElementsAre(HasReferrerPolicy(
          network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin)));

  auto* meta =
      MakeGarbageCollected<HTMLMetaElement>(document, CreateElementFlags());
  meta->setAttribute(html_names::kNameAttr, AtomicString("referrer"));
  meta->setAttribute(html_names::kContentAttr, AtomicString("strict-origin"));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    document.head()->appendChild(meta);
  });
  EXPECT_THAT(candidates, ElementsAre(HasReferrerPolicy(
                              network::mojom::ReferrerPolicy::kStrictOrigin)));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    meta->setAttribute(html_names::kContentAttr, AtomicString("same-origin"));
  });
  EXPECT_THAT(candidates, ElementsAre(HasReferrerPolicy(
                              network::mojom::ReferrerPolicy::kSameOrigin)));
}

TEST_F(DocumentRulesTest, BaseURLChanged) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();
  document.SetBaseURLOverride(KURL("https://foo.com"));

  AddAnchor(*document.body(), "https://foo.com/bar");
  AddAnchor(*document.body(), "/bart");

  HTMLScriptElement* speculation_script;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    speculation_script = InsertSpeculationRules(page_holder.GetDocument(),
                                                R"(
      {"prefetch": [
        {"source": "document", "where": {"href_matches": "/bar*"}}
      ]}
    )");
  });
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar"),
                                  KURL("https://foo.com/bart")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    document.SetBaseURLOverride(KURL("https://bar.com"));
  });
  // After the base URL changes, "https://foo.com/bar" is matched against
  // "https://bar.com/bar*" and doesn't match. "/bart" is resolved to
  // "https://bar.com/bart" and matches with "https://bar.com/bar*".
  EXPECT_THAT(candidates, HasURLs("https://bar.com/bart"));

  // Test that removing the script causes the candidates to be removed.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      [&]() { speculation_script->remove(); });
  EXPECT_EQ(candidates.size(), 0u);
}

TEST_F(DocumentRulesTest, TargetHintFromLink) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* anchor_1 = AddAnchor(*document.body(), "https://foo.com/bar");
  anchor_1->setAttribute(html_names::kTargetAttr, AtomicString("_blank"));
  auto* anchor_2 = AddAnchor(*document.body(), "https://fizz.com/buzz");
  anchor_2->setAttribute(html_names::kTargetAttr, AtomicString("_self"));
  AddAnchor(*document.body(), "https://hello.com/world");

  String speculation_script = R"(
    {
      "prefetch": [{
        "source": "document",
        "where": {"href_matches": "https://foo.com/bar"}
      }],
      "prerender": [{"source": "document"}]
    }
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(
      candidates,
      ::testing::UnorderedElementsAre(
          ::testing::AllOf(
              HasAction(mojom::blink::SpeculationAction::kPrefetch),
              HasTargetHint(mojom::blink::SpeculationTargetHint::kNoHint)),
          ::testing::AllOf(
              HasURL(KURL("https://foo.com/bar")),
              HasAction(mojom::blink::SpeculationAction::kPrerender),
              HasTargetHint(mojom::blink::SpeculationTargetHint::kBlank)),
          ::testing::AllOf(
              HasURL(KURL("https://fizz.com/buzz")),
              HasAction(mojom::blink::SpeculationAction::kPrerender),
              HasTargetHint(mojom::blink::SpeculationTargetHint::kSelf)),
          ::testing::AllOf(
              HasURL(KURL("https://hello.com/world")),
              HasAction(mojom::blink::SpeculationAction::kPrerender),
              HasTargetHint(mojom::blink::SpeculationTargetHint::kNoHint))));
}

TEST_F(DocumentRulesTest, TargetHintFromSpeculationRuleOverridesLinkTarget) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* anchor = AddAnchor(*document.body(), "https://foo.com/bar");
  anchor->setAttribute(html_names::kTargetAttr, AtomicString("_blank"));

  String speculation_script = R"(
    {"prerender": [{"source": "document", "target_hint": "_self"}]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, ::testing::ElementsAre(HasTargetHint(
                              mojom::blink::SpeculationTargetHint::kSelf)));
}

TEST_F(DocumentRulesTest, TargetHintFromLinkDynamic) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  auto* anchor = AddAnchor(*document.body(), "https://foo.com/bar");

  String speculation_script = R"({"prerender": [{"source": "document"}]})";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, ::testing::ElementsAre(HasTargetHint(
                              mojom::blink::SpeculationTargetHint::kNoHint)));

  HTMLBaseElement* base_element;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    base_element = MakeGarbageCollected<HTMLBaseElement>(document);
    base_element->setAttribute(html_names::kTargetAttr, AtomicString("_self"));
    document.head()->appendChild(base_element);
  });
  EXPECT_THAT(candidates, ::testing::ElementsAre(HasTargetHint(
                              mojom::blink::SpeculationTargetHint::kSelf)));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    anchor->setAttribute(html_names::kTargetAttr, AtomicString("_blank"));
  });
  EXPECT_THAT(candidates, ::testing::ElementsAre(HasTargetHint(
                              mojom::blink::SpeculationTargetHint::kBlank)));
}

TEST_F(DocumentRulesTest, ParseSelectorMatches) {
  auto* simple_selector_matches = CreatePredicate(R"(
    "selector_matches": ".valid"
  )");
  EXPECT_THAT(simple_selector_matches,
              Selector({StyleRuleWithSelectorText(".valid")}));

  auto* simple_selector_matches_list = CreatePredicate(R"(
    "selector_matches": [".one", "#two"]
  )");
  EXPECT_THAT(simple_selector_matches_list,
              Selector({StyleRuleWithSelectorText(".one"),
                        StyleRuleWithSelectorText("#two")}));

  auto* selector_matches_with_compound_selector = CreatePredicate(R"(
    "selector_matches": ".interesting-section > a"
  )");
  EXPECT_THAT(
      selector_matches_with_compound_selector,
      Selector({StyleRuleWithSelectorText(".interesting-section > a")}));
}

TEST_F(DocumentRulesTest, GetStyleRules) {
  auto* predicate = CreatePredicate(R"(
    "and": [
      {"or": [
        {"not": {"selector_matches": "span.fizz > a"}},
        {"selector_matches": "#bar a"}
      ]},
      {"selector_matches": "a.foo"}
    ]
  )");
  EXPECT_THAT(
      predicate,
      And({Or({Neg(Selector({StyleRuleWithSelectorText("span.fizz > a")})),
               Selector({StyleRuleWithSelectorText("#bar a")})}),
           Selector({StyleRuleWithSelectorText("a.foo")})}));
  EXPECT_THAT(predicate->GetStyleRules(),
              UnorderedElementsAre(StyleRuleWithSelectorText("span.fizz > a"),
                                   StyleRuleWithSelectorText("#bar a"),
                                   StyleRuleWithSelectorText("a.foo")));
}

TEST_F(DocumentRulesTest, SelectorMatchesAddsCandidates) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
    <div id="unimportant-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* unimportant_section =
      document.getElementById(AtomicString("unimportant-section"));

  AddAnchor(*important_section, "https://foo.com/foo");
  AddAnchor(*unimportant_section, "https://foo.com/bar");
  AddAnchor(*important_section, "https://foo.com/fizz");
  AddAnchor(*unimportant_section, "https://foo.com/buzz");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section > a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/foo"),
                                  KURL("https://foo.com/fizz")));
}

TEST_F(DocumentRulesTest, SelectorMatchesIsDynamic) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
    <div id="unimportant-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* unimportant_section =
      document.getElementById(AtomicString("unimportant-section"));

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"or": [
        {"selector_matches": "#important-section > a"},
        {"selector_matches": ".important-link"}
      ]}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_TRUE(candidates.empty());

  HTMLAnchorElement* second_anchor = nullptr;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    AddAnchor(*important_section, "https://foo.com/fizz");
    second_anchor = AddAnchor(*unimportant_section, "https://foo.com/buzz");
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/fizz")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    second_anchor->setAttribute(html_names::kClassAttr,
                                AtomicString("important-link"));
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/fizz"),
                                  KURL("https://foo.com/buzz")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->SetIdAttribute(AtomicString("random-section"));
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/buzz")));
}

TEST_F(DocumentRulesTest, AddingDocumentRulesInvalidatesStyle) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
    <div id="unimportant-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* unimportant_section =
      document.getElementById(AtomicString("unimportant-section"));

  AddAnchor(*important_section, "https://foo.com/fizz");
  AddAnchor(*unimportant_section, "https://foo.com/buzz");

  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(document.NeedsLayoutTreeUpdate());

  auto* script_without_selector_matches = InsertSpeculationRules(document, R"(
    {"prefetch": [{"source": "document", "where": {"href_matches": "/foo"}}]}
  )");
  ASSERT_FALSE(important_section->ChildNeedsStyleRecalc());

  auto* script_with_irrelevant_selector_matches =
      InsertSpeculationRules(document, R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#irrelevant a"}
    }]}
  )");
  ASSERT_FALSE(important_section->ChildNeedsStyleRecalc());

  auto* script_with_selector_matches = InsertSpeculationRules(document, R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )");
  EXPECT_TRUE(important_section->ChildNeedsStyleRecalc());

  page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(important_section->ChildNeedsStyleRecalc());

  // Test removing SpeculationRuleSets, removing a ruleset should also cause
  // invalidations.
  script_with_selector_matches->remove();
  EXPECT_TRUE(important_section->ChildNeedsStyleRecalc());
  page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();

  script_without_selector_matches->remove();
  ASSERT_FALSE(important_section->ChildNeedsStyleRecalc());

  script_with_irrelevant_selector_matches->remove();
  ASSERT_FALSE(important_section->ChildNeedsStyleRecalc());
}

TEST_F(DocumentRulesTest, BasicStyleInvalidation) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
    <div id="unimportant-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* unimportant_section =
      document.getElementById(AtomicString("unimportant-section"));

  AddAnchor(*important_section, "https://foo.com/fizz");
  AddAnchor(*unimportant_section, "https://foo.com/buzz");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section > a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);

  EXPECT_FALSE(document.NeedsLayoutTreeUpdate());
  unimportant_section->SetIdAttribute(AtomicString("random-section"));
  EXPECT_FALSE(document.NeedsLayoutTreeUpdate());
  unimportant_section->SetIdAttribute(AtomicString("important-section"));
  EXPECT_TRUE(document.NeedsLayoutTreeUpdate());
}

TEST_F(DocumentRulesTest, IrrelevantDOMChangeShouldNotInvalidateCandidateList) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
    <div id="unimportant-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* unimportant_section =
      document.getElementById(AtomicString("unimportant-section"));

  AddAnchor(*important_section, "https://foo.com/fizz");
  AddAnchor(*unimportant_section, "https://foo.com/buzz");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section > a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/fizz")));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        unimportant_section->SetIdAttribute(AtomicString("random-section"));
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));
}

TEST_F(DocumentRulesTest, SelectorMatchesInsideShadowTree) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  ShadowRoot& shadow_root =
      document.body()->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <div id="important-section"></div>
    <div id="unimportant-section"></div>
  )HTML");
  auto* important_section =
      shadow_root.getElementById(AtomicString("important-section"));
  auto* unimportant_section =
      shadow_root.getElementById(AtomicString("unimportant-section"));

  AddAnchor(*important_section, "https://foo.com/fizz");
  AddAnchor(*unimportant_section, "https://foo.com/buzz");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section > a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/fizz")));
}

TEST_F(DocumentRulesTest, SelectorMatchesWithScopePseudoSelector) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setAttribute(html_names::kClassAttr, AtomicString("foo"));
  document.body()->setInnerHTML(R"HTML(
    <a href="https://foo.com/fizz"></a>
    <div class="foo">
      <a href="https://foo.com/buzz"></a>
    </div>
  )HTML");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": ":scope > .foo > a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/fizz")));
}

// Basic test to check that we wait for UpdateStyle before sending a list of
// updated candidates to the browser process when "selector_matches" is
// enabled.
TEST_F(DocumentRulesTest, UpdateQueueingWithSelectorMatches_1) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
    <div id="unimportant-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* unimportant_section =
      document.getElementById(AtomicString("unimportant-section"));

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"href_matches": "https://bar.com/*"}
    }]}
  )";
  // No update should be sent before running a style update after inserting
  // the rules.
  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host,
      [&]() {
        page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
        InsertSpeculationRules(document, speculation_script);
      },
      IncludesStyleUpdate{false}));
  ASSERT_TRUE(document.NeedsLayoutTreeUpdate());
  // The list of candidates is updated after a style update.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, []() {});
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs());

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host,
      [&]() { AddAnchor(*document.body(), "https://bar.com/fizz.html"); },
      IncludesStyleUpdate{false}));
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, []() {});
  EXPECT_THAT(candidates, HasURLs(KURL("https://bar.com/fizz.html")));

  String speculation_script_with_selector_matches = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  // Insert a speculation ruleset with "selector_matches". This will not require
  // a style update, as adding the ruleset itself will not cause any
  // invalidations (there are no existing elements that match the selector in
  // the new ruleset).
  PropagateRulesToStubSpeculationHost(
      page_holder, speculation_host,
      [&]() {
        InsertSpeculationRules(document,
                               speculation_script_with_selector_matches);
      },
      IncludesStyleUpdate{false});
  ASSERT_FALSE(document.NeedsLayoutTreeUpdate());
  EXPECT_THAT(candidates, HasURLs(KURL("https://bar.com/fizz.html")));

  // Add two new links. We should not update speculation candidates until we run
  // UpdateStyle.
  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host,
      [&]() {
        AddAnchor(*important_section, "https://foo.com/fizz.html");
        AddAnchor(*unimportant_section, "https://foo.com/buzz.html");
      },
      IncludesStyleUpdate{false}));
  ASSERT_TRUE(document.NeedsLayoutTreeUpdate());
  // Runs UpdateStyle; new speculation candidates should be sent.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, []() {});
  EXPECT_THAT(candidates, HasURLs(KURL("https://bar.com/fizz.html"),
                                  KURL("https://foo.com/fizz.html")));
}

// This tests that we don't need to wait for a style update if an operation
// does not invalidate style.
TEST_F(DocumentRulesTest, UpdateQueueingWithSelectorMatches_2) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  AddAnchor(*important_section, "https://foo.com/bar");
  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  // We shouldn't have to wait for UpdateStyle if the update doesn't cause
  // style invalidation.
  PropagateRulesToStubSpeculationHost(
      page_holder, speculation_host,
      [&]() {
        EXPECT_FALSE(document.NeedsLayoutTreeUpdate());
        auto* referrer_meta = MakeGarbageCollected<HTMLMetaElement>(
            document, CreateElementFlags());
        referrer_meta->setAttribute(html_names::kNameAttr,
                                    AtomicString("referrer"));
        referrer_meta->setAttribute(html_names::kContentAttr,
                                    AtomicString("strict-origin"));
        document.head()->appendChild(referrer_meta);
        EXPECT_FALSE(document.NeedsLayoutTreeUpdate());
      },
      IncludesStyleUpdate{false});
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));
}

// This tests a scenario where we queue an update microtask, invalidate style,
// update style, and then run the microtask.
TEST_F(DocumentRulesTest, UpdateQueueingWithSelectorMatches_3) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs());

  // Note: AddAnchor below will queue a microtask before invalidating style
  // (Node::InsertedInto is called before style invalidation).
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    AddAnchor(*important_section, "https://foo.com/bar.html");
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar.html")));
}

// This tests a scenario where we queue a microtask update, invalidate style,
// and then run the microtask.
TEST_F(DocumentRulesTest, UpdateQueueingWithSelectorMatches_4) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs());

  // A microtask will be queued and run before a style update - but no list of
  // candidates should be sent as style isn't clean. Note: AddAnchor below will
  // queue a microtask before invalidating style (Node::InsertedInto is called
  // before style invalidation).
  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host,
      [&]() { AddAnchor(*important_section, "https://foo.com/bar"); },
      IncludesStyleUpdate{false}));
  ASSERT_TRUE(document.NeedsLayoutTreeUpdate());
  // Updating style should trigger UpdateSpeculationCandidates.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, []() {});
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));
}

// Tests update queueing after making a DOM modification that doesn't directly
// affect a link.
TEST_F(DocumentRulesTest, UpdateQueueingWithSelectorMatches_5) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  AddAnchor(*important_section, "https://foo.com/bar");
  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  // Changing the link's container's ID will not queue a microtask on its own.
  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host,
      [&]() {
        important_section->SetIdAttribute(AtomicString("unimportant-section"));
      },
      IncludesStyleUpdate{false}));
  // After style updates, we should update the list of speculation candidates.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, []() {});
  EXPECT_THAT(candidates, HasURLs());
}

TEST_F(DocumentRulesTest, LinksWithoutComputedStyle) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  AddAnchor(*important_section, "https://foo.com/bar");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    InsertSpeculationRules(document, speculation_script);
  });
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  // Changing a link's ancestor to display:none should trigger an update and
  // remove it from the candidate list.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                              CSSValueID::kNone);
  });
  EXPECT_THAT(candidates, HasURLs());

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  // Adding a shadow root will remove the anchor from the flat tree, and it will
  // stop being rendered. It should trigger an update and be removed from
  // the candidate list.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  });
  EXPECT_THAT(candidates, HasURLs());
}

TEST_F(DocumentRulesTest, LinksWithoutComputedStyle_HrefMatches) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* anchor = AddAnchor(*important_section, "https://foo.com/bar");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"href_matches": "https://foo.com/*"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    InsertSpeculationRules(document, speculation_script);
  });
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    anchor->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
  });
  EXPECT_THAT(candidates, HasURLs());
}

TEST_F(DocumentRulesTest, LinkInsideDisplayLockedElement) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  AddAnchor(*important_section, "https://foo.com/bar");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                              CSSValueID::kHidden);
  });
  EXPECT_THAT(candidates, HasURLs());

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->RemoveInlineStyleProperty(
        CSSPropertyID::kContentVisibility);
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));
}

TEST_F(DocumentRulesTest, LinkInsideNestedDisplayLockedElement) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section">
      <div id="links"></div>
    </div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* links = document.getElementById(AtomicString("links"));
  AddAnchor(*links, "https://foo.com/bar");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  // Scenario 1: Lock links, lock important-section, unlock important-section,
  // unlock links.

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    links->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                  CSSValueID::kHidden);
  });
  EXPECT_THAT(candidates, HasURLs());

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        important_section->SetInlineStyleProperty(
            CSSPropertyID::kContentVisibility, CSSValueID::kHidden);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        important_section->RemoveInlineStyleProperty(
            CSSPropertyID::kContentVisibility);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    links->RemoveInlineStyleProperty(CSSPropertyID::kContentVisibility);
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  // Scenario 2: Lock links, lock important-section, unlock links, unlock
  // important-section.

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    links->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                  CSSValueID::kHidden);
  });
  EXPECT_THAT(candidates, HasURLs());

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        important_section->SetInlineStyleProperty(
            CSSPropertyID::kContentVisibility, CSSValueID::kHidden);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        links->RemoveInlineStyleProperty(CSSPropertyID::kContentVisibility);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->RemoveInlineStyleProperty(
        CSSPropertyID::kContentVisibility);
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  // Scenario 3: Lock important-section, lock links, unlock important-section,
  // unlock links.

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                              CSSValueID::kHidden);
  });
  EXPECT_THAT(candidates, HasURLs());

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        links->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                      CSSValueID::kHidden);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        important_section->RemoveInlineStyleProperty(
            CSSPropertyID::kContentVisibility);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    links->RemoveInlineStyleProperty(CSSPropertyID::kContentVisibility);
  });

  // Scenario 4: Lock links and important-section together, unlock links and
  // important-section together.

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                              CSSValueID::kHidden);
    links->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                  CSSValueID::kHidden);
  });
  EXPECT_THAT(candidates, HasURLs());

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->RemoveInlineStyleProperty(
        CSSPropertyID::kContentVisibility);
    links->RemoveInlineStyleProperty(CSSPropertyID::kContentVisibility);
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));
}

TEST_F(DocumentRulesTest, DisplayLockedLink) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* anchor = AddAnchor(*important_section, "https://foo.com/bar");
  anchor->setInnerText("Bar");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        anchor->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                       CSSValueID::kHidden);
      }));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        anchor->RemoveInlineStyleProperty(CSSPropertyID::kContentVisibility);
      }));
}

TEST_F(DocumentRulesTest, AddLinkToDisplayLockedContainer) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section">
    </div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"selector_matches": "#important-section a"}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs());

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        important_section->SetInlineStyleProperty(
            CSSPropertyID::kContentVisibility, CSSValueID::kHidden);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  HTMLAnchorElement* anchor = nullptr;
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    anchor = AddAnchor(*important_section, "https://foo.com/bar");
  });
  EXPECT_THAT(candidates, HasURLs());

  // Tests removing a display-locked container with links.
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      [&]() { important_section->remove(); });
  EXPECT_THAT(candidates, HasURLs());
}

TEST_F(DocumentRulesTest, DisplayLockedContainerTracking) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  document.body()->setInnerHTML(R"HTML(
    <div id="important-section"></div>
    <div id="irrelevant-section"><span></span></div>
  )HTML");
  auto* important_section =
      document.getElementById(AtomicString("important-section"));
  auto* irrelevant_section =
      document.getElementById(AtomicString("irrelevant-section"));
  auto* anchor_1 = AddAnchor(*important_section, "https://foo.com/bar");
  AddAnchor(*important_section, "https://foo.com/logout");
  AddAnchor(*document.body(), "https://foo.com/logout");

  String speculation_script = R"(
    {"prefetch": [{
      "source": "document",
      "where": {"and": [{
        "selector_matches": "#important-section a"
      }, {
        "not": {"href_matches": "https://*/logout"}
      }]}
    }]}
  )";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/bar")));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                              CSSValueID::kHidden);
    anchor_1->SetHref(AtomicString("https://foo.com/fizz.html"));
  });
  EXPECT_THAT(candidates, HasURLs());

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        // Changing style of the display-locked container should not cause an
        // update.
        important_section->SetInlineStyleProperty(CSSPropertyID::kColor,
                                                  CSSValueID::kDarkviolet);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    important_section->SetInlineStyleProperty(CSSPropertyID::kContentVisibility,
                                              CSSValueID::kVisible);
  });
  EXPECT_THAT(candidates, HasURLs(KURL("https://foo.com/fizz.html")));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        // Changing style of the display-locked container should not cause an
        // update.
        important_section->SetInlineStyleProperty(CSSPropertyID::kColor,
                                                  CSSValueID::kDeepskyblue);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        irrelevant_section->SetInlineStyleProperty(
            CSSPropertyID::kContentVisibility, CSSValueID::kHidden);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));

  ASSERT_TRUE(NoRulesPropagatedToStubSpeculationHost(
      page_holder, speculation_host, [&]() {
        irrelevant_section->RemoveInlineStyleProperty(
            CSSPropertyID::kContentVisibility);
        page_holder.GetFrameView().UpdateAllLifecyclePhasesForTest();
      }));
}

// Similar to SpeculationRulesTest.RemoveInMicrotask, but with relevant changes
// to style/layout which necessitate forcing a style update after removal.
TEST_F(DocumentRulesTest, RemoveForcesStyleUpdate) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  base::RunLoop run_loop;
  base::MockCallback<base::RepeatingCallback<void(
      const Vector<mojom::blink::SpeculationCandidatePtr>&)>>
      mock_callback;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(mock_callback, Run(::testing::SizeIs(2)));
    EXPECT_CALL(mock_callback, Run(::testing::SizeIs(3)))
        .WillOnce(::testing::Invoke([&]() { run_loop.Quit(); }));
  }
  speculation_host.SetCandidatesUpdatedCallback(mock_callback.Get());

  LocalFrame& frame = page_holder.GetFrame();
  Document& doc = page_holder.GetDocument();
  frame.GetSettings()->SetScriptEnabled(true);
  auto& broker = frame.DomWindow()->GetBrowserInterfaceBroker();
  broker.SetBinderForTesting(
      mojom::blink::SpeculationHost::Name_,
      WTF::BindRepeating(&StubSpeculationHost::BindUnsafe,
                         WTF::Unretained(&speculation_host)));

  for (StringView path : {"/baz", "/quux"}) {
    AddAnchor(*doc.body(), "https://example.com" + path);
  }

  // First simulated task adds the rule sets.
  InsertSpeculationRules(doc,
                         R"({"prefetch": [
           {"source": "list", "urls": ["https://example.com/foo"]}]})");
  HTMLScriptElement* to_remove = InsertSpeculationRules(doc,
                                                        R"({"prefetch": [
             {"source": "list", "urls": ["https://example.com/bar"]}]})");
  scoped_refptr<scheduler::EventLoop> event_loop =
      frame.DomWindow()->GetAgent()->event_loop();
  event_loop->PerformMicrotaskCheckpoint();
  frame.View()->UpdateAllLifecyclePhasesForTest();

  // Second simulated task removes a rule set, then adds a new rule set which
  // will match some newly added links. Since we are forced to update to handle
  // the removal, these will be discovered during that microtask.
  //
  // There's some extra subtlety here -- the speculation rules update needs to
  // propagate the new invalidation sets for this selector before the
  // setAttribute call occurs. Otherwise this test fails because the change goes
  // unnoticed.
  to_remove->remove();
  InsertSpeculationRules(doc,
                         R"({"prefetch": [{"source": "document",
                        "where": {"selector_matches": ".magic *"}}]})");
  doc.body()->setAttribute(html_names::kClassAttr, AtomicString("magic"));

  event_loop->PerformMicrotaskCheckpoint();

  run_loop.Run();
  broker.SetBinderForTesting(mojom::blink::SpeculationHost::Name_, {});
}

// Checks a subtle case, wherein a ruleset is removed while speculation
// candidate update is waiting for clean style. In this case there is a race
// between the style update and the new microtask. In the case where the
// microtask wins, care is needed to avoid re-entrantly updating speculation
// candidates once it forces style clean.
TEST_F(DocumentRulesTest, RemoveWhileWaitingForStyle) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  base::RunLoop run_loop;
  ::testing::StrictMock<base::MockCallback<base::RepeatingCallback<void(
      const Vector<mojom::blink::SpeculationCandidatePtr>&)>>>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(::testing::SizeIs(1)))
      .WillOnce(::testing::Invoke([&]() { run_loop.Quit(); }));
  speculation_host.SetCandidatesUpdatedCallback(mock_callback.Get());

  LocalFrame& frame = page_holder.GetFrame();
  Document& doc = page_holder.GetDocument();
  frame.GetSettings()->SetScriptEnabled(true);
  auto& broker = frame.DomWindow()->GetBrowserInterfaceBroker();
  broker.SetBinderForTesting(
      mojom::blink::SpeculationHost::Name_,
      WTF::BindRepeating(&StubSpeculationHost::BindUnsafe,
                         WTF::Unretained(&speculation_host)));
  auto event_loop = frame.DomWindow()->GetAgent()->event_loop();

  // First, add the rule set and matching links. Style is not yet clean for the
  // newly added links, even after the microtask. We also add a rule set with a
  // fixed URL to avoid any optimizations that skip empty updates.
  for (StringView path : {"/baz", "/quux"}) {
    AddAnchor(*doc.body(), "https://example.com" + path);
  }
  HTMLScriptElement* to_remove = InsertSpeculationRules(doc,
                                                        R"({"prefetch": [
           {"source": "document", "where": {"selector_matches": "*"}}]})");
  InsertSpeculationRules(doc,
                         R"({"prefetch": [
           {"source": "list", "urls": ["https://example.com/keep"]}]})");
  event_loop->PerformMicrotaskCheckpoint();
  EXPECT_TRUE(doc.NeedsLayoutTreeUpdate());

  // Then, the rule set is removed, and we run another microtask checkpoint.
  to_remove->remove();
  event_loop->PerformMicrotaskCheckpoint();

  // At this point, style should have been forced clean, and we should have
  // received the mock update above.
  EXPECT_FALSE(doc.NeedsLayoutTreeUpdate());

  run_loop.Run();
  broker.SetBinderForTesting(mojom::blink::SpeculationHost::Name_, {});
}

// Regression test, since the universal select sets rule set flags indicating
// that the rule set potentially invalidates all elements.
TEST_F(DocumentRulesTest, UniversalSelector) {
  DummyPageHolder page_holder;
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  StubSpeculationHost speculation_host;
  InsertSpeculationRules(
      page_holder.GetDocument(),
      R"({"prefetch": [{"source":"document", "where":{"selector_matches":"*"}}]})");
}

TEST_F(SpeculationRuleSetTest, Eagerness) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;
  Document& document = page_holder.GetDocument();

  const KURL kUrl1{"https://example.com/prefetch/list/page1.html"};
  const KURL kUrl2{"https://example.com/prefetch/document/page1.html"};
  const KURL kUrl3{"https://example.com/prerender/list/page1.html"};
  const KURL kUrl4{"https://example.com/prerender/document/page1.html"};
  const KURL kUrl5{"https://example.com/prefetch/list/page2.html"};
  const KURL kUrl6{"https://example.com/prefetch/document/page2.html"};
  const KURL kUrl7{"https://example.com/prerender/list/page2.html"};
  const KURL kUrl8{"https://example.com/prerender/document/page2.html"};
  const KURL kUrl9{"https://example.com/prefetch/list/page3.html"};

  AddAnchor(*document.body(), kUrl2.GetString());
  AddAnchor(*document.body(), kUrl4.GetString());
  AddAnchor(*document.body(), kUrl6.GetString());
  AddAnchor(*document.body(), kUrl8.GetString());

  String speculation_script = R"({
        "prefetch": [
          {
            "source": "list",
            "urls": ["https://example.com/prefetch/list/page1.html"],
            "eagerness": "conservative"
          },
          {
            "source": "document",
            "eagerness": "eager",
            "where": {"href_matches": "https://example.com/prefetch/document/page1.html"}
          },
          {
            "source": "list",
            "urls": ["https://example.com/prefetch/list/page2.html"]
          },
          {
            "source": "document",
            "where": {"href_matches": "https://example.com/prefetch/document/page2.html"}
          },
          {
            "source": "list",
            "urls": ["https://example.com/prefetch/list/page3.html"],
            "eagerness": "immediate"
          }
        ],
        "prerender": [
          {
            "eagerness": "moderate",
            "source": "list",
            "urls": ["https://example.com/prerender/list/page1.html"]
          },
          {
            "source": "document",
            "where": {"href_matches": "https://example.com/prerender/document/page1.html"},
            "eagerness": "eager"
          },
          {
            "source": "list",
            "urls": ["https://example.com/prerender/list/page2.html"]
          },
          {
            "source": "document",
            "where": {"href_matches": "https://example.com/prerender/document/page2.html"}
          }
        ]
      })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_THAT(
      candidates,
      UnorderedElementsAre(
          AllOf(
              HasURL(kUrl1),
              HasEagerness(blink::mojom::SpeculationEagerness::kConservative)),
          AllOf(HasURL(kUrl2),
                HasEagerness(blink::mojom::SpeculationEagerness::kEager)),
          AllOf(HasURL(kUrl3),
                HasEagerness(blink::mojom::SpeculationEagerness::kModerate)),
          AllOf(HasURL(kUrl4),
                HasEagerness(blink::mojom::SpeculationEagerness::kEager)),
          AllOf(HasURL(kUrl5),
                HasEagerness(blink::mojom::SpeculationEagerness::kEager)),
          AllOf(
              HasURL(kUrl6),
              HasEagerness(blink::mojom::SpeculationEagerness::kConservative)),
          AllOf(HasURL(kUrl7),
                HasEagerness(blink::mojom::SpeculationEagerness::kEager)),
          AllOf(
              HasURL(kUrl8),
              HasEagerness(blink::mojom::SpeculationEagerness::kConservative)),
          AllOf(HasURL(kUrl9),
                HasEagerness(blink::mojom::SpeculationEagerness::kEager))));
}

TEST_F(SpeculationRuleSetTest, InvalidUseOfEagerness1) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  const char* kUrl1 = "https://example.com/prefetch/list/page1.html";

  String speculation_script = R"({
        "eagerness": "conservative",
        "prefetch": [
          {
            "source": "list",
            "urls": ["https://example.com/prefetch/list/page1.html"]
          }
        ]
      })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  // It should just ignore the "eagerness" key
  EXPECT_THAT(candidates, HasURLs(KURL(kUrl1)));
}

TEST_F(SpeculationRuleSetTest, InvalidUseOfEagerness2) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  const char* kUrl1 = "https://example.com/prefetch/list/page1.html";

  String speculation_script = R"({
        "prefetch": [
          "eagerness",
          {
            "source": "list",
            "urls": ["https://example.com/prefetch/list/page1.html"]
          }
        ]
      })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  // It should just ignore the "eagerness" key
  EXPECT_THAT(candidates, HasURLs(KURL(kUrl1)));
}

TEST_F(SpeculationRuleSetTest, InvalidEagernessValue) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  String speculation_script = R"({
        "prefetch": [
          {
            "source": "list",
            "urls": ["https://example.com/prefetch/list/page1.html"],
            "eagerness": 0
          },
          {
            "eagerness": 1.0,
            "source": "list",
            "urls": ["https://example.com/prefetch/list/page2.html"]
          },
          {
            "source": "list",
            "eagerness": true,
            "urls": ["https://example.com/prefetch/list/page3.html"]
          },
          {
            "source": "list",
            "urls": ["https://example.com/prefetch/list/page4.html"],
            "eagerness": "xyz"
          }
        ]
      })";
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_TRUE(candidates.empty());
}

// Test that a valid No-Vary-Search hint will generate a speculation
// candidate.
TEST_F(SpeculationRuleSetTest, ValidNoVarySearchHintValueGeneratesCandidate) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  String speculation_script = R"({
    "prefetch": [{
        "source": "list",
        "urls": ["https://example.com/prefetch/list/page1.html"],
        "expects_no_vary_search": "params=(\"a\") "
      }]
    })";

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_EQ(candidates.size(), 1u);

  // Check that the candidate has the correct No-Vary-Search hint.
  EXPECT_THAT(candidates, ElementsAre(::testing::AllOf(
                              HasNoVarySearchHint(), NVSVariesOnKeyOrder(),
                              NVSHasNoVaryParams("a"))));
}

TEST_F(SpeculationRuleSetTest, InvalidNoVarySearchHintValueGeneratesCandidate) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  String speculation_script = R"({
    "prefetch": [{
        "source": "list",
        "urls": ["https://example.com/prefetch/list/page1.html"],
        "expects_no_vary_search": "params=(a) "
      }]
    })";

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_EQ(candidates.size(), 1u);

  // Check that the candidate doesn't have No-Vary-Search hint.
  EXPECT_THAT(candidates, ElementsAre(Not(HasNoVarySearchHint())));
}

// Test that an empty but valid No-Vary-Search hint will generate a speculation
// candidate.
TEST_F(SpeculationRuleSetTest, EmptyNoVarySearchHintValueGeneratesCandidate) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  String speculation_script = R"({
    "prefetch": [{
        "source": "list",
        "urls": ["https://example.com/prefetch/list/page1.html"],
        "expects_no_vary_search": ""
      }]
    })";

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_EQ(candidates.size(), 1u);

  // Check that the candidate has the correct No-Vary-Search hint.
  EXPECT_THAT(candidates[0], Not(HasNoVarySearchHint()));
}

// Test that a No-Vary-Search hint equivalent to the default
// will generate a speculation candidate.
TEST_F(SpeculationRuleSetTest, DefaultNoVarySearchHintValueGeneratesCandidate) {
  DummyPageHolder page_holder;
  StubSpeculationHost speculation_host;

  String speculation_script = R"({
    "prefetch": [{
        "source": "list",
        "urls": ["https://example.com/prefetch/list/page1.html"],
        "expects_no_vary_search": "key-order=?0"
      }]
    })";

  PropagateRulesToStubSpeculationHost(page_holder, speculation_host,
                                      speculation_script);
  const auto& candidates = speculation_host.candidates();
  EXPECT_EQ(candidates.size(), 1u);

  // Check that the candidate has the correct No-Vary-Search hint.
  EXPECT_THAT(candidates[0], Not(HasNoVarySearchHint()));
}

// Tests that No-Vary-Search errors that cause the speculation rules to be
// skipped are logged to the console.
TEST_F(SpeculationRuleSetTest, ConsoleWarningForNoVarySearchHintNotAString) {
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);

  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setText(
      R"({
    "prefetch": [{
        "source": "list",
        "urls": ["https://example.com/prefetch/list/page1.html"],
        "expects_no_vary_search": 0
      }]
    })");
  document.head()->appendChild(script);

  EXPECT_TRUE(base::ranges::any_of(
      chrome_client->ConsoleMessages(), [](const String& message) {
        return message.Contains(
            "expects_no_vary_search's value must be a string");
      }));
}

// Tests that No-Vary-Search errors that cause the speculation rules to be
// skipped are logged to the console.
TEST_F(SpeculationRuleSetTest, NoVarySearchHintParseErrorRuleSkipped) {
  auto* rule_set =
      CreateRuleSet(R"({
    "prefetch": [{
        "source": "list",
        "urls": ["https://example.com/prefetch/list/page1.html"],
        "expects_no_vary_search": 0
      }]
    })",
                    KURL("https://example.com"), execution_context());
  ASSERT_TRUE(rule_set->HasError());
  EXPECT_FALSE(rule_set->HasWarnings());
  EXPECT_THAT(
      rule_set->error_message().Utf8(),
      ::testing::HasSubstr("expects_no_vary_search's value must be a string"));
}

// Tests that No-Vary-Search parsing errors that cause the speculation rules
// to still be accepted are logged to the console.
TEST_F(SpeculationRuleSetTest, NoVarySearchHintParseErrorRuleAccepted) {
  {
    auto* rule_set =
        CreateRuleSet(R"({
      "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/prefetch/list/page1.html"],
          "expects_no_vary_search": "?1"
        }]
      })",
                      KURL("https://example.com"), execution_context());
    EXPECT_FALSE(rule_set->HasError());
    ASSERT_TRUE(rule_set->HasWarnings());
    EXPECT_THAT(
        rule_set->warning_messages()[0].Utf8(),
        ::testing::HasSubstr("No-Vary-Search hint value is not a dictionary"));
  }

  {
    auto* rule_set =
        CreateRuleSet(R"({
      "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/prefetch/list/page1.html"],
          "expects_no_vary_search": "key-order=a"
        }
      ]
    })",
                      KURL("https://example.com"), execution_context());
    EXPECT_FALSE(rule_set->HasError());
    ASSERT_TRUE(rule_set->HasWarnings());
    EXPECT_THAT(
        rule_set->warning_messages()[0].Utf8(),
        ::testing::HasSubstr(
            "No-Vary-Search hint value contains a \"key-order\" dictionary"));
  }
  {
    auto* rule_set =
        CreateRuleSet(R"({
      "prefetch": [
        {
          "source": "list",
          "urls": ["https://example.com/prefetch/list/page1.html"],
          "expects_no_vary_search": "params=a"
        }
      ]
    })",
                      KURL("https://example.com"), execution_context());
    EXPECT_FALSE(rule_set->HasError());
    ASSERT_TRUE(rule_set->HasWarnings());
    EXPECT_THAT(
        rule_set->warning_messages()[0].Utf8(),
        ::testing::HasSubstr("contains a \"params\" dictionary value"
                             " that is not a list of strings or a boolean"));
  }
  {
    auto* rule_set =
        CreateRuleSet(R"({
      "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/prefetch/list/page1.html"],
          "expects_no_vary_search": "params,except=a"
        }
      ]
    })",
                      KURL("https://example.com"), execution_context());
    EXPECT_FALSE(rule_set->HasError());
    ASSERT_TRUE(rule_set->HasWarnings());
    EXPECT_THAT(rule_set->warning_messages()[0].Utf8(),
                ::testing::HasSubstr("contains an \"except\" dictionary value"
                                     " that is not a list of strings"));
  }
  {
    auto* rule_set =
        CreateRuleSet(R"({
      "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/prefetch/list/page1.html"],
          "expects_no_vary_search": "except=(\"a\") "
        }
      ]
    })",
                      KURL("https://example.com"), execution_context());
    EXPECT_FALSE(rule_set->HasError());
    ASSERT_TRUE(rule_set->HasWarnings());
    EXPECT_THAT(
        rule_set->warning_messages()[0].Utf8(),
        ::testing::HasSubstr(
            "contains an \"except\" dictionary key"
            " without the \"params\" dictionary key being set to true."));
  }
}

TEST_F(SpeculationRuleSetTest, ValidNoVarySearchHintNoErrorOrWarningMessages) {
  {
    auto* rule_set =
        CreateRuleSet(R"({
      "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/prefetch/list/page1.html"],
          "expects_no_vary_search": "params=?0"
        }
      ]
    })",
                      KURL("https://example.com"), execution_context());
    EXPECT_FALSE(rule_set->HasError());
    EXPECT_FALSE(rule_set->HasWarnings());
  }
  {
    auto* rule_set =
        CreateRuleSet(R"({
      "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/prefetch/list/page1.html"],
          "expects_no_vary_search": ""
        }
      ]
    })",
                      KURL("https://example.com"), execution_context());
    EXPECT_FALSE(rule_set->HasError());
    EXPECT_FALSE(rule_set->HasWarnings());
  }
}

TEST_F(SpeculationRuleSetTest, DocumentReportsSuccessMetric) {
  base::HistogramTester histogram_tester;
  DummyPageHolder page_holder;
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setText("{}");
  document.head()->appendChild(script);
  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess, 1);
}

TEST_F(SpeculationRuleSetTest, DocumentReportsParseErrorFromScript) {
  base::HistogramTester histogram_tester;
  DummyPageHolder page_holder;
  page_holder.GetFrame().GetSettings()->SetScriptEnabled(true);
  Document& document = page_holder.GetDocument();
  HTMLScriptElement* script =
      MakeGarbageCollected<HTMLScriptElement>(document, CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setText("{---}");
  document.head()->appendChild(script);
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kParseErrorInline, 1);
}

TEST_F(SpeculationRuleSetTest, DocumentReportsParseErrorFromRequest) {
  base::HistogramTester histogram_tester;
  DummyPageHolder page_holder;
  Document& document = page_holder.GetDocument();
  SpeculationRuleSet* rule_set = SpeculationRuleSet::Parse(
      SpeculationRuleSet::Source::FromRequest(
          "{---}", KURL("https://fake.test/sr.json"), 0),
      document.GetExecutionContext());
  DocumentSpeculationRules::From(document).AddRuleSet(rule_set);
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kParseErrorFetched, 1);
}

TEST_F(SpeculationRuleSetTest, DocumentReportsParseErrorFromBrowserInjection) {
  base::HistogramTester histogram_tester;
  DummyPageHolder page_holder;
  Document& document = page_holder.GetDocument();
  SpeculationRuleSet* rule_set = SpeculationRuleSet::Parse(
      SpeculationRuleSet::Source::FromBrowserInjected(
          "{---}", KURL(), BrowserInjectedSpeculationRuleOptOut::kRespect),
      document.GetExecutionContext());
  DocumentSpeculationRules::From(document).AddRuleSet(rule_set);
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kParseErrorBrowserInjected, 1);
}

TEST_F(SpeculationRuleSetTest, ImplicitSource) {
  auto* rule_set = CreateRuleSet(
      R"({
        "prefetch": [{
          "where": {"href_matches": "/foo"}
        }, {
          "urls": ["/bar"]
        }]
      })",
      KURL("https://example.com/"), execution_context());
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesPredicate(Href({URLPattern("/foo")})),
                          MatchesListOfURLs("https://example.com/bar")));
}

}  // namespace
}  // namespace blink
