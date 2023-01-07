#include "third_party/blink/renderer/core/html/blocking_attribute.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class BlockingAttributeTest : public PageTestBase {};

TEST_F(BlockingAttributeTest, CountRenderTokenUsageInLink) {
  ScopedBlockingAttributeForTest enabled_scope(true);

  SetHtmlInnerHTML("<link blocking=render rel=preload as=font href=foo.ttf>");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kBlockingAttributeRenderToken));
}

TEST_F(BlockingAttributeTest, CountRenderTokenUsageInScript) {
  ScopedBlockingAttributeForTest enabled_scope(true);

  SetHtmlInnerHTML("<script blocking=render src=foo.js></script>");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kBlockingAttributeRenderToken));
}

TEST_F(BlockingAttributeTest, CountRenderTokenUsageInStyle) {
  ScopedBlockingAttributeForTest enabled_scope(true);

  SetHtmlInnerHTML("<style blocking=render>foo {}</style>");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kBlockingAttributeRenderToken));
}

TEST_F(BlockingAttributeTest, NoCountIfElementDoesNotSupportTheAttribute) {
  ScopedBlockingAttributeForTest enabled_scope(true);

  // div does not support the blocking attribute. Usage should not be counted.
  SetHtmlInnerHTML("<div blocking=render>foo bar</div>");
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kBlockingAttributeRenderToken));
}

TEST_F(BlockingAttributeTest, NoCountIfFeatureDisabled) {
  ScopedBlockingAttributeForTest disabled_scope(false);

  SetHtmlInnerHTML(R"HTML(
    <link blocking=render rel=preload as=font href=foo.ttf>
    <script blocking=render src=foo.js></script>
    <style blocking=render>foo {}</style>
  )HTML");
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kBlockingAttributeRenderToken));
}

}  // namespace blink
