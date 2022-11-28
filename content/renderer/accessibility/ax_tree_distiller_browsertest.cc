// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_tree_distiller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
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

  void DistillPage(const char* html,
                   const std::vector<std::string>& expected_node_contents) {
    expected_node_contents_ = expected_node_contents;
    LoadHTML(html);
    RenderFrameImpl* render_frame_impl =
        RenderFrameImpl::FromWebFrame(GetMainFrame());
    distiller_ = std::make_unique<AXTreeDistiller>(render_frame_impl);
    distiller_->Distill(base::BindOnce(
        &AXTreeDistillerTestBase::OnAXTreeDistilled, base::Unretained(this)));
  }

  void OnAXTreeDistilled(const ui::AXTreeUpdate& snapshot,
                         const std::vector<int32_t>& content_node_ids) {
    // AXTree snapshot should unserialize successfully.
    ui::AXTree tree;
    EXPECT_TRUE(tree.Unserialize(snapshot));

    // Content node IDs list should be the same length as
    // |expected_node_contents_|.
    EXPECT_EQ(content_node_ids.size(), expected_node_contents_.size());

    // Iterate through each content node ID from distiller and check that the
    // text value equals the passed-in string from |expected_node_contents_|.
    for (size_t i = 0; i < content_node_ids.size(); i++) {
      ui::AXNode* node = tree.GetFromId(content_node_ids[i]);
      EXPECT_TRUE(node);
      EXPECT_TRUE(node->GetTextContentLengthUTF8());
      EXPECT_EQ(node->GetTextContentUTF8(), expected_node_contents_[i]);
    }
  }

 private:
  std::unique_ptr<AXTreeDistiller> distiller_;
  std::vector<std::string> expected_node_contents_;
};

struct TestCase {
  const char* test_name;
  const char* html;
  std::vector<std::string> expected_node_contents;
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
      <body role="main">
        <p>Test</p>
      <body>)HTML",
     {"Test"}},
    /* ----------------------- */
    {"simple_page_with_main",
     R"HTML(<!doctype html>
      <body role="main">
        <h1>Heading</h1>
        <p>Test 1</p>
        <p>Test 2</p>
        <div role='header'><h2>Header</h2></div>
      <body>)HTML",
     {"Heading", "Test 1", "Test 2", "Header"}},
    /* ----------------------- */
    {"simple_page_with_main_and_article",
     R"HTML(<!doctype html>
      <body>
        <main>
          <p>Main</p>
        </main>
        <div role="article">
          <p>Article 1</p>
        </div>
        <div role="article">
          <p>Article 2</p>
        </div>
      <body>)HTML",
     {"Main", "Article 1", "Article 2"}},
    /* ----------------------- */
    {"simple_page_no_content",
     R"HTML(<!doctype html>
      <body>
        <main>
          <div role='banner'>Banner</div>
          <div role="navigation'>Navigation</div>
          <audio>Audio</audio>
          <img alt='Image alt'></img>
          <button>Button</button>
          <div aria-label='Label'></div>
          <div role='complementary'>Complementary</div>
          <div role='content'>Content Info</div>
          <footer>Footer</footer>
        </main>
      <body>)HTML",
     {}},
    /* ----------------------- */
    {"simple_page_no_main",
     R"HTML(<!doctype html>
      <body>
        <div tabindex='0'>
          <p>Paragraph</p>
          <p>Paragraph</p>
        </div>
      <body>)HTML",
     {}},
    /* ----------------------- */
    {"include_paragraphs_in_collapsed_nodes",
     R"HTML(<!doctype html>
      <body role="main">
        <p>P1</p>
        <div>
          <p>P2</p>
          <p>P3</p>
        </div>
      <body>)HTML",
     {"P1", "P2", "P3"}},
    /* ----------------------- */
    {"main_may_be_deep_in_tree",
     R"HTML(<!doctype html>
      <body>
        <p>P1</p>
        <main>
          <p>P2</p>
          <p>P3</p>
        </main>
      <body>)HTML",
     {"P2", "P3"}},
    /* ----------------------- */
    {"paragraph_with_bold",
     R"HTML(<!doctype html>
      <body role="main">
        <p>Some <b>bolded</b> text</p>
      <body>)HTML",
     {"Some bolded text"}},
    /* ----------------------- */
    {"simple_page_nested_article",
     R"HTML(<!doctype html>
      <body>
        <div role="main">
          <p>Main</p>
          <div role="article">
            <p>Article 1</p>
          </div>
        </div>
        <div role="article">
          <p>Article 2</p>
          <div role="article">
            <p>Article 3</p>
          </div>
        </div>
      <body>)HTML",
     {"Main", "Article 1", "Article 2", "Article 3"}},
};

TEST_P(AXTreeDistillerTest, DistillsWebPage) {
  TestCase param = GetParam();
  DistillPage(param.html, param.expected_node_contents);
}

INSTANTIATE_TEST_SUITE_P(/* prefix */,
                         AXTreeDistillerTest,
                         ::testing::ValuesIn(kDistillWebPageTestCases),
                         AXTreeDistillerTest::ParamInfoToString);

}  // namespace content
