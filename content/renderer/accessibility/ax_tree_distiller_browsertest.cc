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

  void CheckNodeContents(const std::vector<std::string>& node_contents) {
    // AXTree snapshot from distiller should unserialize successfully.
    ui::AXTree tree;
    EXPECT_TRUE(tree.Unserialize(*(distiller_->GetSnapshot())));

    // Content node IDs list from distiller should be the same length as
    // |node_contents| passed in.
    auto* content_node_ids = distiller_->GetContentNodeIDs();
    EXPECT_EQ(content_node_ids->size(), node_contents.size());

    // Iterate through each content node ID from distiller and check that the
    // text value equals the passed-in string from |node_contents|.
    for (size_t i = 0; i < content_node_ids->size(); i++) {
      ui::AXNode* node = tree.GetFromId(content_node_ids->at(i));
      EXPECT_TRUE(node);
      EXPECT_TRUE(node->GetTextContentLengthUTF8());
      EXPECT_EQ(node->GetTextContentUTF8(), node_contents[i]);
    }
  }

 private:
  std::unique_ptr<AXTreeDistiller> distiller_;
};

struct TestCase {
  const char* test_name;
  const char* html;
  std::vector<std::string> node_contents;
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
        <p>Test</p>
      <body>)HTML",
     {"Test"}},
    /* ----------------------- */
    {"simple_page_two_paragraphs",
     R"HTML(<!doctype html>
      <body>
        <p>Test 1</p>
        <p>Test 2</p>
      <body>)HTML",
     {"Test 1", "Test 2"}},
    /* ----------------------- */
    {"simple_page_no_content",
     R"HTML(<!doctype html>
      <body>
        <p>
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
        </p>
      <body>)HTML",
     {}},
    /* ----------------------- */
    {"simple_page_no_paragraph",
     R"HTML(<!doctype html>
      <body>
        <div tabindex='0'>
          <div>Not paragraph</div>
          <div>Not paragraph</div>
        </div>
        <div tabindex='0'>
          <p>Paragraph</p>
        </div>
      <body>)HTML",
     {"Paragraph"}},
    /* ----------------------- */
    {"article_is_node_with_most_paragraphs",
     R"HTML(<!doctype html>
      <body>
        <div tabindex='0'>
          <p>P1</p>
        </div>
        <div tabindex='0'>
          <p>P2</p>
          <p>P3</p>
        </div>
      <body>)HTML",
     {"P2", "P3"}},
    /* ----------------------- */
    {"include_paragraphs_in_collapsed_nodes",
     R"HTML(<!doctype html>
      <body>
        <p>P1</p>
        <div>
          <p>P2</p>
          <p>P3</p>
        </div>
      <body>)HTML",
     {"P1", "P2", "P3"}},
    /* ----------------------- */
    {"node_with_most_paragraphs_may_be_deep_in_tree",
     R"HTML(<!doctype html>
      <body>
        <p>P1</p>
        <div tabindex='0'>
          <p>P2</p>
          <p>P3</p>
        </div>
      <body>)HTML",
     {"P2", "P3"}},
    /* ----------------------- */
    {"paragraph_with_bold",
     R"HTML(<!doctype html>
      <body>
        <p>Some <b>bolded</b> text</p>
      <body>)HTML",
     {"Some bolded text"}},
};

TEST_P(AXTreeDistillerTest, DistillsWebPage) {
  TestCase param = GetParam();
  DistillPage(param.html);
  CheckNodeContents(param.node_contents);
}

INSTANTIATE_TEST_SUITE_P(/* prefix */,
                         AXTreeDistillerTest,
                         ::testing::ValuesIn(kDistillWebPageTestCases),
                         AXTreeDistillerTest::ParamInfoToString);

}  // namespace content
