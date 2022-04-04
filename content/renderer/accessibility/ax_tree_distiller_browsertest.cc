// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_tree_distiller.h"

#include <memory>
#include <string>

#include "content/public/test/render_view_test.h"
#include "content/renderer/render_frame_impl.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace content {

class AXTreeDistillerTestBase : public RenderViewTest {
 public:
  AXTreeDistillerTestBase() = default;
  AXTreeDistillerTestBase(const AXTreeDistillerTestBase&) = delete;
  AXTreeDistillerTestBase& operator=(const AXTreeDistillerTestBase&) = delete;
  ~AXTreeDistillerTestBase() override = default;

  void DistillPage(const char* html) {
    LoadHTML(html);
    RenderFrameImpl* render_frame_impl =
        RenderFrameImpl::FromWebFrame(GetMainFrame());
    distiller_ = std::make_unique<AXTreeDistiller>(render_frame_impl);
    distiller_->Distill();
  }

  void CheckTextNodes(const std::vector<std::string>& text_node_contents) {
    // AXTree snapshot from distiller should unserialize successfully.
    ui::AXTree tree;
    EXPECT_TRUE(tree.Unserialize(*(distiller_->GetSnapshot())));

    // Text node IDs list from distiller should be the same length as
    // |text_node_contents| passed in.
    auto* text_node_ids = distiller_->GetTextNodeIDs();
    EXPECT_EQ(text_node_ids->size(), text_node_contents.size());

    // Iterate through each text node ID from distiller and check that the text
    // value equals the passed-in string from |text_node_contents|.
    for (size_t i = 0; i < text_node_ids->size(); i++) {
      ui::AXNode* node = tree.GetFromId(text_node_ids->at(i));
      EXPECT_TRUE(node);
      std::string value;
      EXPECT_TRUE(
          node->GetStringAttribute(ax::mojom::StringAttribute::kName, &value));
      EXPECT_EQ(value, text_node_contents[i]);
    }
  }

 private:
  std::unique_ptr<AXTreeDistiller> distiller_;
};

struct TestCase {
  const char* test_name;
  const char* html;
  std::vector<std::string> text_node_contents;
};

class AXTreeDistillerTest : public AXTreeDistillerTestBase,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<TestCase> param_info) {
    return param_info.param.test_name;
  }
};

// AXTreeDistillerTest is a parameterized test. Add each test case to the object
// below in the form of struct |TestCase|. Include a dividing line as a comment
// for easy reading.
const TestCase kDistillWebPageTestCases[] = {
    {"simple_page",
     R"HTML(<!doctype html>
      <body>
        <div>Test</div>
      <body>)HTML",
     {"Test"}},
    /* ----------------------- */
    {"simple_page_two_nodes",
     R"HTML(<!doctype html>
      <body>
        <div>Test 1</div>
        <div>Test 2</div>
      <body>)HTML",
     {"Test 1", "Test 2"}},
    /* ----------------------- */
    {"simple_page_no_content",
     R"HTML(<!doctype html>
      <body>
        <header>Header</header>
        <div role='banner'>Banner</div>
        <div role="navigation'>Navigation</div>
        <audio>Audio</audio>
        <img alt='Image alt'></img>
        <button>Button</button>
        <div aria-label='Label'></div>
        <div role='complementary'>Complementary</div>
        <div role='content'>Content Info</div>
        <footer>Footer</footer>
      <body>)HTML",
     {}},
    /* ----------------------- */
    {"simple_page_article",
     R"HTML(<!doctype html>
      <body>
        <div>Not content</div>
        <div role='article'>Content</div>
      <body>)HTML",
     {"Content"}},
};

TEST_P(AXTreeDistillerTest, DistillsWebPage) {
  TestCase param = GetParam();
  DistillPage(param.html);
  CheckTextNodes(param.text_node_contents);
}

INSTANTIATE_TEST_SUITE_P(/* prefix */,
                         AXTreeDistillerTest,
                         ::testing::ValuesIn(kDistillWebPageTestCases),
                         AXTreeDistillerTest::ParamInfoToString);

}  // namespace content
