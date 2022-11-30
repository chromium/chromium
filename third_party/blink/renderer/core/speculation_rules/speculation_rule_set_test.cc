// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"

#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/speculation_rules/stub_speculation_host.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Not;

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

SpeculationRuleSet* CreateSpeculationRuleSetWithTargetHint(
    const char* target_hint) {
  return SpeculationRuleSet::Parse(
      String::Format(R"({
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
      KURL("https://example.com/"));
}

class SpeculationRuleSetTest : public ::testing::Test {
 private:
  ScopedSpeculationRulesPrefetchProxyForTest enable_prefetch_{true};
  ScopedPrerender2ForTest enable_prerender2_{true};
};

TEST_F(SpeculationRuleSetTest, Empty) {
  auto* rule_set =
      SpeculationRuleSet::Parse("{}", KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, SimplePrefetchRule) {
  auto* rule_set = SpeculationRuleSet::Parse(
      R"({
        "prefetch": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, SimplePrerenderRule) {
  auto* rule_set = SpeculationRuleSet::Parse(
      R"({
        "prerender": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(
      rule_set->prerender_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, SimplePrefetchWithSubresourcesRule) {
  auto* rule_set = SpeculationRuleSet::Parse(
      R"({
        "prefetch_with_subresources": [{
          "source": "list",
          "urls": ["https://example.com/index2.html"]
        }]
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(
      rule_set->prefetch_with_subresources_rules(),
      ElementsAre(MatchesListOfURLs("https://example.com/index2.html")));
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, ResolvesURLs) {
  auto* rule_set = SpeculationRuleSet::Parse(
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
      KURL("https://example.com/foo/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs(
                  "https://example.com/foo/bar", "https://example.com/baz",
                  "https://example.org/", "http://example.net/")));
}

TEST_F(SpeculationRuleSetTest, RequiresAnonymousClientIPWhenCrossOrigin) {
  auto* rule_set = SpeculationRuleSet::Parse(
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
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(
      rule_set->prefetch_rules(),
      ElementsAre(AllOf(MatchesListOfURLs("https://example.net/anonymous.html"),
                        RequiresAnonymousClientIPWhenCrossOrigin()),
                  AllOf(MatchesListOfURLs("https://example.net/direct.html"),
                        Not(RequiresAnonymousClientIPWhenCrossOrigin()))));
}

TEST_F(SpeculationRuleSetTest, RejectsInvalidJSON) {
  String parse_error;
  auto* rule_set = SpeculationRuleSet::Parse(
      "[invalid]", KURL("https://example.com"), &parse_error);
  EXPECT_FALSE(rule_set);
  EXPECT_TRUE(parse_error.Contains("Syntax error"));
}

TEST_F(SpeculationRuleSetTest, RejectsNonObject) {
  String parse_error;
  auto* rule_set = SpeculationRuleSet::Parse("42", KURL("https://example.com"),
                                             &parse_error);
  EXPECT_FALSE(rule_set);
  EXPECT_TRUE(parse_error.Contains("must be an object"));
}

TEST_F(SpeculationRuleSetTest, IgnoresUnknownOrDifferentlyTypedTopLevelKeys) {
  auto* rule_set = SpeculationRuleSet::Parse(
      R"({
        "unrecognized_key": true,
        "prefetch": 42,
        "prefetch_with_subresources": false
      })",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
}

TEST_F(SpeculationRuleSetTest, DropUnrecognizedRules) {
  auto* rule_set = SpeculationRuleSet::Parse(
      R"({"prefetch": [)"

      // A rule that doesn't elaborate on its source.
      R"({"urls": ["no-source.html"]},)"

      // A rule with an unrecognized source.
      R"({"source": "magic-8-ball", "urls": ["no-source.html"]},)"

      // A list rule with no "urls" key.
      R"({"source": "list"},)"

      // A list rule where some URL is not a string.
      R"({"source": "list", "urls": [42]},)"

      // A rule with an unrecognized requirement.
      R"({"source": "list", "urls": ["/"], "requires": ["more-vespene-gas"]},)"

      // Invalid URLs within a list rule should be discarded.
      // This includes totally invalid ones and ones with unacceptable schemes.
      R"({"source": "list",
          "urls": [
            "valid.html", "mailto:alice@example.com", "http://@:",
            "blob:https://bar"
           ]
         }]})",
      KURL("https://example.com/"));
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/valid.html")));
}

// Test that only prerender rule can process a "_blank" target hint.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_Blank) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("_blank");
  ASSERT_TRUE(rule_set);
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
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(), ElementsAre());
}

// Test that the the validation of the browsing context keywords runs an ASCII
// case-insensitive match.
TEST_F(SpeculationRuleSetTest, RulesWithTargetHint_CaseInsensitive) {
  auto* rule_set = CreateSpeculationRuleSetWithTargetHint("_BlAnK");
  ASSERT_TRUE(rule_set);
  EXPECT_THAT(rule_set->prefetch_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prefetch_with_subresources_rules(), ElementsAre());
  EXPECT_THAT(rule_set->prerender_rules(),
              ElementsAre(MatchesListOfURLs("https://example.com/hint.html")));
  EXPECT_EQ(rule_set->prerender_rules()[0]->target_browsing_context_name_hint(),
            mojom::blink::SpeculationTargetHint::kBlank);
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
  script->setAttribute(html_names::kTypeAttr, "SpEcUlAtIoNrUlEs");
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
  script->setAttribute(html_names::kTypeAttr, "SpEcUlAtIoNrUlEs");
  script->setText(speculation_script);
  document.head()->appendChild(script);
  return script;
}

// This runs the functor while observing any speculation rules sent by it.
// At least one update is expected.
template <typename F>
void PropagateRulesToStubSpeculationHost(DummyPageHolder& page_holder,
                                         StubSpeculationHost& speculation_host,
                                         const F& functor) {
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
  functor();
  run_loop.Run();

  broker.SetBinderForTesting(mojom::blink::SpeculationHost::Name_, {});
}

// This function adds a speculationrules script to the given page, and simulates
// the process of sending the parsed candidates to the browser.
void PropagateRulesToStubSpeculationHost(DummyPageHolder& page_holder,
                                         StubSpeculationHost& speculation_host,
                                         const String& speculation_script) {
  PropagateRulesToStubSpeculationHost(page_holder, speculation_host, [&]() {
    InsertSpeculationRules(page_holder.GetDocument(), speculation_script);
  });
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
  // Overwrite the kSpeculationRulesPrefetchProxy flag.
  ScopedSpeculationRulesPrefetchProxyForTest enable_prefetch{false};

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
  script->setAttribute(html_names::kTypeAttr, "speculationrules");
  script->setText("[invalid]");
  document.head()->appendChild(script);

  EXPECT_TRUE(base::ranges::any_of(
      chrome_client->ConsoleMessages(),
      [](const String& message) { return message.Contains("Syntax error"); }));
}

}  // namespace
}  // namespace blink
