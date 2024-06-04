// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/javascript_framework_detection.h"
#include "third_party/blink/public/mojom/loader/javascript_framework_detection.mojom-shared.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/speculation_rules/auto_speculation_rules_test_helper.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

class DocumentLoaderAutoSpeculationRulesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize();
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://start.example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
    web_view_impl_ = web_view_helper_.InitializeAndLoad(
        "https://start.example.com/foo.html");

    // We leave the "config" parameter at its default value, since
    // SpeculationRulesConfigOverride takes care of that in each test.
    scoped_feature_list_.InitAndEnableFeature(features::kAutoSpeculationRules);
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  LocalFrame& GetLocalFrame() const {
    return *To<LocalFrame>(web_view_impl_->GetPage()->MainFrame());
  }
  Document& GetDocument() const { return *GetLocalFrame().GetDocument(); }
  DocumentLoader& GetDocumentLoader() const {
    return *GetLocalFrame().Loader().GetDocumentLoader();
  }
  DocumentSpeculationRules& GetDocumentSpeculationRules() const {
    return DocumentSpeculationRules::From(GetDocument());
  }

 private:
  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  WebViewImpl* web_view_impl_;
};

enum class OptOutRuleSetType { kInline, kExternal };
class DocumentLoaderAutoSpeculationRulesOptOutTest
    : public DocumentLoaderAutoSpeculationRulesTest,
      public testing::WithParamInterface<OptOutRuleSetType> {
 public:
  SpeculationRuleSet* GetOptOutRuleSet() const {
    switch (GetParam()) {
      case OptOutRuleSetType::kInline:
        return SpeculationRuleSet::Parse(
            SpeculationRuleSet::Source::FromInlineScript("{}", GetDocument(),
                                                         0),
            GetLocalFrame().DomWindow());
      case OptOutRuleSetType::kExternal:
        return SpeculationRuleSet::Parse(
            SpeculationRuleSet::Source::FromRequest(
                "{}", KURL("https://example.com/speculation-rules.json"), 0u),
            GetLocalFrame().DomWindow());
    }
  }
};

TEST_F(DocumentLoaderAutoSpeculationRulesTest, InvalidJSON) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "framework_to_speculation_rules": {
      "1": "true"
    },
    "url_match_pattern_to_speculation_rules": {
      "https://start.example.com/foo.html": "true"
    }
  }
  )");

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  static_assert(base::to_underlying(mojom::JavaScriptFramework::kVuePress) ==
                1);
  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress, kNoFrameworkVersionDetected}}});

  EXPECT_EQ(rules.rule_sets().size(), 0u);
}

TEST_F(DocumentLoaderAutoSpeculationRulesTest, ValidFrameworkRules) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "framework_to_speculation_rules": {
      "1": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/foo.html\"]}]}"
    }
  }
  )");

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  static_assert(base::to_underlying(mojom::JavaScriptFramework::kVuePress) ==
                1);
  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress, kNoFrameworkVersionDetected}}});

  EXPECT_EQ(rules.rule_sets().size(), 1u);
  // Assume the rule was parsed correctly; testing that would be redundant with
  // the speculation rules tests.
}

TEST_F(DocumentLoaderAutoSpeculationRulesTest, MultipleFrameworkRules) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "framework_to_speculation_rules": {
      "1": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/foo.html\"]}]}",
      "2": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/bar.html\"]}]}",
      "3": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/baz.html\"]}]}"
    }
  }
  )");

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  static_assert(base::to_underlying(mojom::JavaScriptFramework::kVuePress) ==
                1);
  static_assert(base::to_underlying(mojom::JavaScriptFramework::kGatsby) == 3);
  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress, kNoFrameworkVersionDetected},
        {mojom::JavaScriptFramework::kGatsby, kNoFrameworkVersionDetected}}});

  // Test that we got the rules we expect from the framework mapping, and not
  // any more.
  EXPECT_EQ(rules.rule_sets().size(), 2u);
  EXPECT_EQ(
      rules.rule_sets().at(0)->prefetch_rules().at(0)->urls().at(0).GetString(),
      "https://example.com/foo.html");
  EXPECT_EQ(
      rules.rule_sets().at(1)->prefetch_rules().at(0)->urls().at(0).GetString(),
      "https://example.com/baz.html");
}

TEST_F(DocumentLoaderAutoSpeculationRulesTest, ValidUrlMatchPatternRules) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "url_match_pattern_to_speculation_rules": {
      "https://start.example.com/foo.html": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/1.html\"]}]}",
      "https://*.example.com/*": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/2.html\"]}]}",
      "https://*.example.org/*": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/3.html\"]}]}"
    }
  }
  )");

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  static_assert(base::to_underlying(mojom::JavaScriptFramework::kVuePress) ==
                1);
  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress, kNoFrameworkVersionDetected}}});

  EXPECT_EQ(rules.rule_sets().size(), 2u);
  // Assume the rules were parsed correctly; testing that would be redundant
  // with the speculation rules tests.
}

TEST_P(DocumentLoaderAutoSpeculationRulesOptOutTest, ExistingRuleSetOptsOut) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "framework_to_speculation_rules": {
      "1": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/foo.html\"]}]}"
    },
    "url_match_pattern_to_speculation_rules": {
      "https://start.example.com/foo.html": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/1.html\"]}]}"
    }
  }
  )");

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  auto* rule_set = GetOptOutRuleSet();
  rules.AddRuleSet(rule_set);

  EXPECT_EQ(rules.rule_sets().size(), 1u);
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kAutoSpeculationRulesOptedOut));

  base::HistogramTester histogram_tester;

  static_assert(base::to_underlying(mojom::JavaScriptFramework::kVuePress) ==
                1);
  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress, kNoFrameworkVersionDetected}}});

  // Still just one, but now the UseCounter and histogram have triggered.
  EXPECT_EQ(rules.rule_sets().size(), 1u);
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kAutoSpeculationRulesOptedOut));
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kAutoSpeculationRulesOptedOut,
      /*expected_bucket_count=*/2);
}

TEST_P(DocumentLoaderAutoSpeculationRulesOptOutTest,
       ExistingRuleSetOptOutIgnored) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "url_match_pattern_to_speculation_rules_ignore_opt_out": {
      "https://start.example.com/foo.html": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/1.html\"]}]}"
    }
  }
  )");

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  auto* rule_set = GetOptOutRuleSet();
  rules.AddRuleSet(rule_set);

  EXPECT_EQ(rules.rule_sets().size(), 1u);
  EXPECT_FALSE(rules.rule_sets()[0]->source()->IsFromBrowserInjected());
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kAutoSpeculationRulesOptedOut));

  base::HistogramTester histogram_tester;

  GetDocumentLoader().DidObserveJavaScriptFrameworks({});

  // The rule set is added, the UseCounter has not triggered, and the only
  // histogram update is +1 success.
  EXPECT_EQ(rules.rule_sets().size(), 2u);
  EXPECT_FALSE(rules.rule_sets().at(0)->source()->IsFromBrowserInjected());
  EXPECT_TRUE(rules.rule_sets().at(1)->source()->IsFromBrowserInjected());
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kAutoSpeculationRulesOptedOut));
  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess,
                                      /*expected_bucket_count=*/1);
}

TEST_P(DocumentLoaderAutoSpeculationRulesOptOutTest, AddedLaterRuleSetOptsOut) {
  // Test 2 auto speculation rule sets per type to ensure we remove both of them
  // correctly.
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "framework_to_speculation_rules": {
      "1": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/foo.html\"]}]}",
      "3": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/baz.html\"]}]}"
    },
    "url_match_pattern_to_speculation_rules": {
      "https://start.example.com/foo.html": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/1.html\"]}]}",
      "https://*.example.com/*": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/2.html\"]}]}"
    }
  }
  )");

  base::HistogramTester histogram_tester;

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  static_assert(base::to_underlying(mojom::JavaScriptFramework::kVuePress) ==
                1);
  static_assert(base::to_underlying(mojom::JavaScriptFramework::kGatsby) == 3);
  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress, kNoFrameworkVersionDetected},
        {mojom::JavaScriptFramework::kGatsby, kNoFrameworkVersionDetected}}});

  EXPECT_EQ(rules.rule_sets().size(), 4u);
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kAutoSpeculationRulesOptedOut));

  auto* manually_added_rule_set = GetOptOutRuleSet();
  rules.AddRuleSet(manually_added_rule_set);

  EXPECT_EQ(rules.rule_sets().size(), 1u);
  EXPECT_EQ(rules.rule_sets().at(0), manually_added_rule_set);

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kAutoSpeculationRulesOptedOut));

  // The load outcome should not be AutoSpeculationRulesOptedOut, since it did
  // load correctly. Instead, we should get 5 succeses: 4 auto speculation rules
  // + 1 normal speculation rule.
  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess,
                                      /*expected_bucket_count=*/5);
}

TEST_P(DocumentLoaderAutoSpeculationRulesOptOutTest,
       AddedLaterRuleSetOptOutIgnored) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "url_match_pattern_to_speculation_rules_ignore_opt_out": {
      "https://start.example.com/foo.html": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/1.html\"]}]}",
      "https://*.example.com/*": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/2.html\"]}]}"
    }
  }
  )");

  base::HistogramTester histogram_tester;

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  GetDocumentLoader().DidObserveJavaScriptFrameworks({});

  EXPECT_EQ(rules.rule_sets().size(), 2u);
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kAutoSpeculationRulesOptedOut));

  auto* manually_added_rule_set = GetOptOutRuleSet();
  rules.AddRuleSet(manually_added_rule_set);

  EXPECT_EQ(rules.rule_sets().size(), 3u);
  EXPECT_EQ(rules.rule_sets().at(2), manually_added_rule_set);

  // The UseCounter has not triggered, and the histogram is at 3 successes: 2
  // auto speculation rules + 1 normal speculation rule.
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kAutoSpeculationRulesOptedOut));
  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess,
                                      /*expected_bucket_count=*/3);
}

INSTANTIATE_TEST_SUITE_P(FromInlineOrExternal,
                         DocumentLoaderAutoSpeculationRulesOptOutTest,
                         testing::Values(OptOutRuleSetType::kInline,
                                         OptOutRuleSetType::kExternal));

}  // namespace
}  // namespace blink
