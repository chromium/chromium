// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_selector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class ContainerSelectorTest : public testing::Test {
 protected:
  const ContainerQuery* ParseContainerQuery(Document& document, String query) {
    String rule = "@container " + query + " {}";
    auto* style_rule = DynamicTo<StyleRuleContainer>(
        css_test_helpers::ParseRule(document, rule));
    if (!style_rule) {
      return nullptr;
    }
    return style_rule->GetContainerQuerySet().SingleQuery();
  }
};

TEST_F(ContainerSelectorTest, ContainerSelectorHashing) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  ContainerSelectorCache cache;
  const ContainerQuery* query1 =
      ParseContainerQuery(*document, "style(--foo: bar)");
  const ContainerQuery* query2 = ParseContainerQuery(
      *document, "style(--foo: bar) and scroll-state(snapped)");
  ASSERT_TRUE(query1);
  ASSERT_TRUE(query2);
  EXPECT_NE(query1->Selector().GetHash(), query2->Selector().GetHash())
      << "The query selectors should not generate the same hash since they "
         "select different type of containers";
}

TEST_F(ContainerSelectorTest, NameOnlyContainerSelector) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  ContainerSelectorCache cache;
  const ContainerQuery* query = ParseContainerQuery(*document, "--foo");
  ASSERT_TRUE(query);
  EXPECT_TRUE(query->Selector().SelectsNamedContainers());
  EXPECT_FALSE(query->Selector().SelectsStyleContainers());
  EXPECT_TRUE(query->Selector().SelectsStyleOrNameOnlyContainers());

  query = ParseContainerQuery(*document, "style(--foo: bar)");
  ASSERT_TRUE(query);
  EXPECT_FALSE(query->Selector().SelectsNamedContainers());
  EXPECT_TRUE(query->Selector().SelectsStyleContainers());
  EXPECT_TRUE(query->Selector().SelectsStyleOrNameOnlyContainers());

  query = ParseContainerQuery(*document, "--foo style(--foo: bar)");
  ASSERT_TRUE(query);
  EXPECT_TRUE(query->Selector().SelectsNamedContainers());
  EXPECT_TRUE(query->Selector().SelectsStyleContainers());
  EXPECT_TRUE(query->Selector().SelectsStyleOrNameOnlyContainers());
}

}  // namespace blink
