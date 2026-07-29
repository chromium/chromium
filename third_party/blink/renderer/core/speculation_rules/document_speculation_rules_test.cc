// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace {

class MockSpeculationHost : public mojom::blink::SpeculationHost {
 public:
  MockSpeculationHost() = default;
  ~MockSpeculationHost() override = default;

  void UpdateSpeculationCandidates(
      Vector<mojom::blink::SpeculationCandidatePtr> candidates,
      bool enable_cross_origin_prerender_iframes) override {
    last_candidates_ = std::move(candidates);
  }
  void OnLCPPredicted() override {}
  void EnactCandidate(mojom::blink::SpeculationCandidatePtr candidate,
                      mojom::blink::SpeculationHeuristic heuristic) override {
    last_enacted_candidates_.push_back(std::move(candidate));
    last_enactment_heuristics_.push_back(heuristic);
  }

  void BindNewEndpointAndPassReceiver(mojo::ScopedMessagePipeHandle receiver) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::SpeculationHost>(
        std::move(receiver)));
  }

  const Vector<mojom::blink::SpeculationCandidatePtr>& last_candidates() const {
    return last_candidates_;
  }
  const Vector<mojom::blink::SpeculationCandidatePtr>& last_enacted_candidates()
      const {
    return last_enacted_candidates_;
  }
  const Vector<mojom::blink::SpeculationHeuristic>& last_enactment_heuristics()
      const {
    return last_enactment_heuristics_;
  }
  void ClearCandidates() { last_candidates_.clear(); }

 private:
  Vector<mojom::blink::SpeculationCandidatePtr> last_candidates_;
  Vector<mojom::blink::SpeculationCandidatePtr> last_enacted_candidates_;
  Vector<mojom::blink::SpeculationHeuristic> last_enactment_heuristics_;
  mojo::Receiver<mojom::blink::SpeculationHost> receiver_{this};
};

class DocumentSpeculationRulesTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetBaseURLOverride(KURL("https://example.com/"));
    GetDocument().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::SpeculationHost::Name_,
        BindRepeating(&MockSpeculationHost::BindNewEndpointAndPassReceiver,
                      Unretained(&mock_host_)));
  }

 protected:
  void ProcessAllRuleSets(
      DocumentSpeculationRules& document_speculation_rules) {
    GetDocument().UpdateStyleAndLayoutTree();
    GetDocument().GetAgent().event_loop()->PerformMicrotaskCheckpoint();
    document_speculation_rules.FlushMojoMessageForTesting();
  }

  MockSpeculationHost& mock_host() { return mock_host_; }

 private:
  MockSpeculationHost mock_host_;
};

TEST_F(DocumentSpeculationRulesTest, AddRuleSet_PrefetchUrl) {
  Document& document = GetDocument();
  DocumentSpeculationRules& document_speculation_rules =
      DocumentSpeculationRules::From(document);

  auto* source = SpeculationRuleSet::Source::FromInlineScript(
      R"({"prefetch": [{"urls": ["/prefetched.html"]}]})", document,
      static_cast<DOMNodeId>(1));
  auto* rule_set =
      SpeculationRuleSet::Parse(source, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set);

  ProcessAllRuleSets(document_speculation_rules);

  const auto& candidates = mock_host().last_candidates();
  ASSERT_EQ(candidates.size(), 1u);
  EXPECT_EQ(candidates[0]->url, KURL("https://example.com/prefetched.html"));
  EXPECT_EQ(candidates[0]->action, mojom::blink::SpeculationAction::kPrefetch);
}

TEST_F(DocumentSpeculationRulesTest, RemoveRuleSet) {
  Document& document = GetDocument();
  DocumentSpeculationRules& document_speculation_rules =
      DocumentSpeculationRules::From(document);

  auto* source = SpeculationRuleSet::Source::FromInlineScript(
      R"({"prefetch": [{"urls": ["/prefetched.html"]}]})", document,
      static_cast<DOMNodeId>(1));
  auto* rule_set =
      SpeculationRuleSet::Parse(source, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set);
  ProcessAllRuleSets(document_speculation_rules);

  ASSERT_EQ(mock_host().last_candidates().size(), 1u);

  document_speculation_rules.RemoveRuleSet(rule_set);
  ProcessAllRuleSets(document_speculation_rules);

  EXPECT_TRUE(mock_host().last_candidates().empty());
}

TEST_F(DocumentSpeculationRulesTest, NoVarySearchDedupesSentCandidates) {
  Document& document = GetDocument();
  DocumentSpeculationRules& document_speculation_rules =
      DocumentSpeculationRules::From(document);

  // First rule set: /p?a=1 with No-Vary-Search hint that excludes "a".
  auto* source1 = SpeculationRuleSet::Source::FromInlineScript(
      R"json({"prefetch": [{
        "urls": ["/p?a=1"],
        "expects_no_vary_search": "params=(\"a\")"
      }]})json",
      document, static_cast<DOMNodeId>(1));
  auto* rule_set1 =
      SpeculationRuleSet::Parse(source1, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set1);
  ProcessAllRuleSets(document_speculation_rules);

  ASSERT_EQ(document_speculation_rules.sent_candidates().size(), 1u);

  // Second rule set: /p?a=2 with the same NVS hint. This URL is equivalent
  // to /p?a=1 under the hint, so it should not produce a second entry in
  // sent_candidates().
  auto* source2 = SpeculationRuleSet::Source::FromInlineScript(
      R"json({"prefetch": [{
        "urls": ["/p?a=2"],
        "expects_no_vary_search": "params=(\"a\")"
      }]})json",
      document, static_cast<DOMNodeId>(2));
  auto* rule_set2 =
      SpeculationRuleSet::Parse(source2, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set2);
  ProcessAllRuleSets(document_speculation_rules);

  EXPECT_EQ(document_speculation_rules.sent_candidates().size(), 1u);

  auto* source3 = SpeculationRuleSet::Source::FromInlineScript(
      R"json({"prefetch": [{
        "urls": ["/p?b=1"],
        "expects_no_vary_search": "params=(\"a\")"
      }]})json",
      document, static_cast<DOMNodeId>(3));
  auto* rule_set3 =
      SpeculationRuleSet::Parse(source3, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set3);
  ProcessAllRuleSets(document_speculation_rules);

  EXPECT_EQ(document_speculation_rules.sent_candidates().size(), 2u);
}

TEST_F(DocumentSpeculationRulesTest, PointerDownHeuristicEnactsCandidate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSpeculationRulesRendererSideHeuristics);

  Document& document = GetDocument();
  DocumentSpeculationRules& document_speculation_rules =
      DocumentSpeculationRules::From(document);

  // A conservative-eagerness candidate is sent to the browser but not enacted
  // until a pointer interaction occurs.
  const KURL url("https://example.com/prefetched.html");
  auto* source = SpeculationRuleSet::Source::FromInlineScript(
      R"({"prefetch": [{"urls": ["/prefetched.html"],
                       "eagerness": "conservative"}]})",
      document, static_cast<DOMNodeId>(1));
  auto* rule_set =
      SpeculationRuleSet::Parse(source, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set);
  ProcessAllRuleSets(document_speculation_rules);

  ASSERT_EQ(document_speculation_rules.sent_candidates().size(), 1u);
  EXPECT_TRUE(mock_host().last_enacted_candidates().empty());

  // A pointerdown on the URL enacts the matching non-immediate candidate.
  document_speculation_rules.OnPointerDownHeuristic(url);
  document_speculation_rules.FlushMojoMessageForTesting();

  const auto& enacted = mock_host().last_enacted_candidates();
  ASSERT_EQ(enacted.size(), 1u);
  EXPECT_EQ(enacted[0]->url, url);
  EXPECT_EQ(enacted[0]->action, mojom::blink::SpeculationAction::kPrefetch);
  ASSERT_EQ(mock_host().last_enactment_heuristics().size(), 1u);
  EXPECT_EQ(mock_host().last_enactment_heuristics()[0],
            mojom::blink::SpeculationHeuristic::kPointerDown);
}

TEST_F(DocumentSpeculationRulesTest,
       HoverHeuristicEnactsOnlyTriggeredEagerness) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSpeculationRulesRendererSideHeuristics);

  Document& document = GetDocument();
  DocumentSpeculationRules& document_speculation_rules =
      DocumentSpeculationRules::From(document);

  const KURL url("https://example.com/prefetched.html");
  auto* source = SpeculationRuleSet::Source::FromInlineScript(
      R"({"prefetch": [{"urls": ["/prefetched.html"],
                        "eagerness": "conservative"},
                       {"urls": ["/prefetched.html"],
                        "eagerness": "moderate"}]})",
      document, static_cast<DOMNodeId>(1));
  auto* rule_set =
      SpeculationRuleSet::Parse(source, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set);
  ProcessAllRuleSets(document_speculation_rules);

  ASSERT_EQ(document_speculation_rules.sent_candidates().size(), 2u);
  EXPECT_TRUE(mock_host().last_enacted_candidates().empty());

  document_speculation_rules.OnHoverHeuristic(
      url, mojom::blink::SpeculationEagerness::kModerate);
  document_speculation_rules.FlushMojoMessageForTesting();

  const auto& enacted = mock_host().last_enacted_candidates();
  ASSERT_EQ(enacted.size(), 1u);
  EXPECT_EQ(enacted[0]->url, url);
  EXPECT_EQ(enacted[0]->eagerness,
            mojom::blink::SpeculationEagerness::kModerate);
  ASSERT_EQ(mock_host().last_enactment_heuristics().size(), 1u);
  EXPECT_EQ(mock_host().last_enactment_heuristics()[0],
            mojom::blink::SpeculationHeuristic::kPointerHover);
}

TEST_F(DocumentSpeculationRulesTest,
       PointerDownHeuristicNoOpWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kSpeculationRulesRendererSideHeuristics);

  Document& document = GetDocument();
  DocumentSpeculationRules& document_speculation_rules =
      DocumentSpeculationRules::From(document);

  const KURL url("https://example.com/prefetched.html");
  auto* source = SpeculationRuleSet::Source::FromInlineScript(
      R"({"prefetch": [{"urls": ["/prefetched.html"],
                       "eagerness": "conservative"}]})",
      document, static_cast<DOMNodeId>(1));
  auto* rule_set =
      SpeculationRuleSet::Parse(source, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set);
  ProcessAllRuleSets(document_speculation_rules);

  document_speculation_rules.OnPointerDownHeuristic(url);
  document_speculation_rules.FlushMojoMessageForTesting();

  EXPECT_TRUE(mock_host().last_enacted_candidates().empty());
}

TEST_F(DocumentSpeculationRulesTest,
       PointerDownHeuristicEnactsNoVarySearchMatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSpeculationRulesRendererSideHeuristics);

  Document& document = GetDocument();
  DocumentSpeculationRules& document_speculation_rules =
      DocumentSpeculationRules::From(document);

  // Candidate for /p?a=1 whose No-Vary-Search hint declares that "a" does not
  // vary the response.
  auto* source = SpeculationRuleSet::Source::FromInlineScript(
      R"json({"prefetch": [{
        "urls": ["/p?a=1"],
        "eagerness": "conservative",
        "expects_no_vary_search": "params=(\"a\")"
      }]})json",
      document, static_cast<DOMNodeId>(1));
  auto* rule_set =
      SpeculationRuleSet::Parse(source, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set);
  ProcessAllRuleSets(document_speculation_rules);

  ASSERT_EQ(document_speculation_rules.sent_candidates().size(), 1u);

  // A pointerdown on /p?a=2 differs only in the non-varying "a" param, so it
  // matches the candidate under No-Vary-Search and enacts it.
  document_speculation_rules.OnPointerDownHeuristic(
      KURL("https://example.com/p?a=2"));
  document_speculation_rules.FlushMojoMessageForTesting();

  const auto& enacted = mock_host().last_enacted_candidates();
  ASSERT_EQ(enacted.size(), 1u);
  EXPECT_EQ(enacted[0]->url, KURL("https://example.com/p?a=1"));
}

TEST_F(DocumentSpeculationRulesTest,
       PointerDownHeuristicSkipsNonMatchingQuery) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSpeculationRulesRendererSideHeuristics);

  Document& document = GetDocument();
  DocumentSpeculationRules& document_speculation_rules =
      DocumentSpeculationRules::From(document);

  auto* source = SpeculationRuleSet::Source::FromInlineScript(
      R"json({"prefetch": [{
        "urls": ["/p?a=1"],
        "eagerness": "conservative",
        "expects_no_vary_search": "params=(\"a\")"
      }]})json",
      document, static_cast<DOMNodeId>(1));
  auto* rule_set =
      SpeculationRuleSet::Parse(source, document.GetExecutionContext());
  document_speculation_rules.AddRuleSet(rule_set);
  ProcessAllRuleSets(document_speculation_rules);

  ASSERT_EQ(document_speculation_rules.sent_candidates().size(), 1u);

  // A pointerdown on /p?b=2 differs in a param that the No-Vary-Search hint
  // does not cover, so it does not match the candidate.
  document_speculation_rules.OnPointerDownHeuristic(
      KURL("https://example.com/p?b=2"));
  document_speculation_rules.FlushMojoMessageForTesting();

  EXPECT_TRUE(mock_host().last_enacted_candidates().empty());
}
}  // namespace

}  // namespace blink
