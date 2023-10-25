// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/javascript_framework_detection.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/speculation_rules/auto_speculation_rules_test_helper.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"

namespace blink {
namespace {

class DocumentLoaderAutoSpeculationRulesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize();
    web_view_impl_ = web_view_helper_.InitializeAndLoad("about:blank");

    // We leave the "config" parameter at its default value, since
    // SpeculationRulesConfigOverride takes care of that in each test.
    scoped_feature_list_.InitAndEnableFeature(features::kAutoSpeculationRules);
  }

  LocalFrame& GetLocalFrame() const {
    return *To<LocalFrame>(web_view_impl_->GetPage()->MainFrame());
  }
  DocumentLoader& GetDocumentLoader() const {
    return *GetLocalFrame().Loader().GetDocumentLoader();
  }
  DocumentSpeculationRules& GetDocumentSpeculationRules() const {
    return DocumentSpeculationRules::From(*GetLocalFrame().GetDocument());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  WebViewImpl* web_view_impl_;
};

TEST_F(DocumentLoaderAutoSpeculationRulesTest, InvalidJSON) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "framework_to_speculation_rules": {
      "1": "true"
    }
  }
  )");

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress /* = 1 */,
         kNoFrameworkVersionDetected}}});

  EXPECT_EQ(rules.rule_sets().size(), 0u);
}

TEST_F(DocumentLoaderAutoSpeculationRulesTest, ValidRules) {
  test::AutoSpeculationRulesConfigOverride override(R"(
  {
    "framework_to_speculation_rules": {
      "1": "{\"prefetch\":[{\"source\":\"list\", \"urls\":[\"https://example.com/foo.html\"]}]}"
    }
  }
  )");

  auto& rules = GetDocumentSpeculationRules();
  CHECK_EQ(rules.rule_sets().size(), 0u);

  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress /* = 1 */,
         kNoFrameworkVersionDetected}}});

  EXPECT_EQ(rules.rule_sets().size(), 1u);
  // Assume the rule was parsed correctly; testing that would be redundant with
  // the speculation rules tests.
}

TEST_F(DocumentLoaderAutoSpeculationRulesTest, MultipleRules) {
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

  GetDocumentLoader().DidObserveJavaScriptFrameworks(
      {{{mojom::JavaScriptFramework::kVuePress /* = 1 */,
         kNoFrameworkVersionDetected},
        {mojom::JavaScriptFramework::kGatsby /* = 2 */,
         kNoFrameworkVersionDetected}}});

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

}  // namespace
}  // namespace blink
