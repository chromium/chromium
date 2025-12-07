// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  void InitiatePreview(const KURL& url) override {}

  void BindNewEndpointAndPassReceiver(mojo::ScopedMessagePipeHandle receiver) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::SpeculationHost>(
        std::move(receiver)));
  }

  const Vector<mojom::blink::SpeculationCandidatePtr>& last_candidates() const {
    return last_candidates_;
  }
  void ClearCandidates() { last_candidates_.clear(); }

 private:
  Vector<mojom::blink::SpeculationCandidatePtr> last_candidates_;
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
}  // namespace

}  // namespace blink
