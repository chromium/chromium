// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_preload_agent.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink::internal {

class InspectorPreloadAgentTest : public testing::Test {
 public:
  InspectorPreloadAgentTest()
      : execution_context_(MakeGarbageCollected<NullExecutionContext>()) {}
  ~InspectorPreloadAgentTest() override {
    execution_context_->NotifyContextDestroyed();
  }

  NullExecutionContext* execution_context() {
    return static_cast<NullExecutionContext*>(execution_context_.Get());
  }

 private:
  test::TaskEnvironment task_environment_;

  Persistent<ExecutionContext> execution_context_;
};

// Test the conversion of out-of-document SpeculationRules by a unit test
// because it is difficult to check in web tests.
TEST_F(InspectorPreloadAgentTest, OutOfDocumentSpeculationRules) {
  const String source_text = R"({
    "prefetch": [{
      "source": "list",
      "urls": ["https://example.com/prefetched.js"]
    }]
  })";

  auto* source = SpeculationRuleSet::Source::FromRequest(
      source_text, KURL("https://example.com/speculationrules.js"), 42);
  auto* rule_set = SpeculationRuleSet::Parse(source, execution_context());
  CHECK(rule_set);

  auto built = BuildProtocolRuleSet(*rule_set, "loaderId");
  EXPECT_EQ(built->getLoaderId(), "loaderId");
  EXPECT_EQ(built->getSourceText(), source_text);
  EXPECT_EQ(built->getUrl(""), "https://example.com/speculationrules.js");
  EXPECT_EQ(built->hasRequestId(), true);
  EXPECT_EQ(built->hasErrorType(), false);
  EXPECT_EQ(built->hasErrorMessage(), false);
}

TEST_F(InspectorPreloadAgentTest, NoRequestIdIfInvalidId) {
  const String source_text = R"({
    "prefetch": [{
      "source": "list",
      "urls": ["https://example.com/prefetched.js"]
    }]
  })";

  auto* source = SpeculationRuleSet::Source::FromRequest(
      source_text, KURL("https://example.com/speculationrules.js"), 0);
  auto* rule_set = SpeculationRuleSet::Parse(source, execution_context());
  CHECK(rule_set);

  auto built = BuildProtocolRuleSet(*rule_set, "loaderId");
  EXPECT_EQ(built->hasRequestId(), false);
}

}  // namespace blink::internal
