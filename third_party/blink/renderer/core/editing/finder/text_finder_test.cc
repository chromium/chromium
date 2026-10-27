// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/text_finder.h"

#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_in_page_coordinates.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class TextFinderTest : public testing::Test {
 protected:
  TextFinderTest() {
    web_view_helper_.Initialize();
    WebLocalFrameImpl& frame_impl = *web_view_helper_.LocalMainFrame();
    frame_impl.ViewImpl()->MainFrameViewWidget()->Resize(gfx::Size(640, 480));
    frame_impl.ViewImpl()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
    document_ = static_cast<Document*>(frame_impl.GetDocument());
    text_finder_ = &frame_impl.EnsureTextFinder();
  }

  Document& GetDocument() const;
  TextFinder& GetTextFinder() const;

  v8::Local<v8::Value> EvalJs(const std::string& script);

  static gfx::RectF FindInPageRect(Node* start_container,
                                   int start_offset,
                                   Node* end_container,
                                   int end_offset);

 private:
  test::TaskEnvironment task_environment_;

  frame_test_helpers::WebViewHelper web_view_helper_;
  Persistent<Document> document_;
  Persistent<TextFinder> text_finder_;
};

class TextFinderSimTest : public SimTest {
 protected:
  TextFinder& GetTextFinder() {
    return WebLocalFrameImpl::FromFrame(GetDocument().GetFrame())
        ->EnsureTextFinder();
  }
};

v8::Local<v8::Value> TextFinderTest::EvalJs(const std::string& script) {
  return ClassicScript::CreateUnspecifiedScript(script.c_str())
      ->RunScriptAndReturnValue(GetDocument().domWindow())
      .GetSuccessValueOrEmpty();
}

Document& TextFinderTest::GetDocument() const {
  return *document_;
}

TextFinder& TextFinderTest::GetTextFinder() const {
  return *text_finder_;
}

gfx::RectF TextFinderTest::FindInPageRect(Node* start_container,
                                          int start_offset,
                                          Node* end_container,
                                          int end_offset) {
  const Position start_position(start_container, start_offset);
  const Position end_position(end_container, end_offset);
  const EphemeralRange range(start_position, end_position);
  return FindInPageRectFromRange(range);
}

TEST_F(TextFinderTest, FindTextSimple) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  String search_text("FindMe");
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  bool wrap_within_frame = true;

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(4u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(10u, active_match->endOffset());

  find_options->new_session = false;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(14u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(20u, active_match->endOffset());

  // Should wrap to the first match.
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(4u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(10u, active_match->endOffset());

  // Search in the reverse order.
  identifier = 1;
  find_options = mojom::blink::FindOptions::New();
  find_options->forward = false;

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(14u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(20u, active_match->endOffset());

  find_options->new_session = false;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(4u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(10u, active_match->endOffset());

  // Wrap to the first match (last occurence in the document).
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(14u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(20u, active_match->endOffset());
}

TEST_F(TextFinderTest, FindTextNotFound) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  String search_text("Boo");
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  bool wrap_within_frame = true;

  EXPECT_FALSE(GetTextFinder().Find(identifier, search_text, *find_options,
                                    wrap_within_frame));
  EXPECT_FALSE(GetTextFinder().ActiveMatch());
}

TEST_F(TextFinderTest, FindTextInShadowDom) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<b>FOO</b><i slot='bar'>foo</i>");
  ShadowRoot& shadow_root =
      GetDocument().body()->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<slot name='bar'></slot><u>Foo</u><slot></slot>");
  Node* text_in_b_element = GetDocument().body()->firstChild()->firstChild();
  Node* text_in_i_element = GetDocument().body()->lastChild()->firstChild();
  Node* text_in_u_element = shadow_root.childNodes()->item(1)->firstChild();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  String search_text("foo");
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  bool wrap_within_frame = true;

  // TextIterator currently returns the matches in the flat treeorder, so
  // in this case the matches will be returned in the order of
  // <i> -> <u> -> <b>.
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_i_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_i_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  find_options->new_session = false;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_u_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_u_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_b_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_b_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // Should wrap to the first match.
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_i_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_i_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // Fresh search in the reverse order.
  identifier = 1;
  find_options = mojom::blink::FindOptions::New();
  find_options->forward = false;

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_b_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_b_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  find_options->new_session = false;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_u_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_u_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_i_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_i_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // And wrap.
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_b_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_b_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(TextFinderTest, ScopeTextMatchesSimple) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(2, GetTextFinder().TotalMatchCount());
  Vector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 4, text_node, 10), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 14, text_node, 20), match_rects[1]);

  // Modify the document size and ensure the cached match rects are recomputed
  // to reflect the updated layout.
  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     AtomicString("margin: 2000px"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 4, text_node, 10), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 14, text_node, 20), match_rects[1]);
}

TEST_F(TextFinderTest, ScopeTextMatchesRepeated) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text1(String("XFindMe"));
  WebString search_text2(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text1,
                                            *find_options);
  GetTextFinder().StartScopingStringMatches(identifier, search_text2,
                                            *find_options);

  // Only searchText2 should be highlighted.
  EXPECT_EQ(2, GetTextFinder().TotalMatchCount());
  Vector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 4, text_node, 10), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 14, text_node, 20), match_rects[1]);
}

TEST_F(TextFinderTest, ScopeTextMatchesWithShadowDom) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<b>FOO</b><i slot='bar'>foo</i>");
  ShadowRoot& shadow_root =
      GetDocument().body()->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<slot name='bar'></slot><u>Foo</u><slot></slot>");
  Node* text_in_b_element = GetDocument().body()->firstChild()->firstChild();
  Node* text_in_i_element = GetDocument().body()->lastChild()->firstChild();
  Node* text_in_u_element = shadow_root.childNodes()->item(1)->firstChild();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("fOO"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  // TextIterator currently returns the matches in the flat tree order,
  // so in this case the matches will be returned in the order of
  // <i> -> <u> -> <b>.
  EXPECT_EQ(3, GetTextFinder().TotalMatchCount());
  Vector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(3u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_in_i_element, 0, text_in_i_element, 3),
            match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_in_u_element, 0, text_in_u_element, 3),
            match_rects[1]);
  EXPECT_EQ(FindInPageRect(text_in_b_element, 0, text_in_b_element, 3),
            match_rects[2]);
}

TEST_F(TextFinderTest, ScopeRepeatPatternTextMatches) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("ab ab ab ab ab");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("ab ab"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(2, GetTextFinder().TotalMatchCount());
  Vector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 0, text_node, 5), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 6, text_node, 11), match_rects[1]);
}

TEST_F(TextFinderTest, OverlappingMatches) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("aababaa");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("aba"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  // We shouldn't find overlapped matches.
  EXPECT_EQ(1, GetTextFinder().TotalMatchCount());
  Vector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(1u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 1, text_node, 4), match_rects[0]);
}

TEST_F(TextFinderTest, SequentialMatches) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("ababab");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("ab"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(3, GetTextFinder().TotalMatchCount());
  Vector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(3u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 0, text_node, 2), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 2, text_node, 4), match_rects[1]);
  EXPECT_EQ(FindInPageRect(text_node, 4, text_node, 6), match_rects[2]);
}

TEST_F(TextFinderTest, FindTextJavaScriptUpdatesDom) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<b>XXXXFindMeYYYY</b><i></i>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  String search_text("FindMe");
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;
  bool active_now;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  find_options->new_session = false;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame, &active_now));
  EXPECT_TRUE(active_now);
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame, &active_now));
  EXPECT_TRUE(active_now);

  // Add new text to DOM and try FindNext.
  auto* i_element = To<Element>(GetDocument().body()->lastChild());
  ASSERT_TRUE(i_element);
  i_element->SetInnerHTMLWithoutTrustedTypes("ZZFindMe");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame, &active_now));
  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_FALSE(active_now);
  EXPECT_EQ(2u, active_match->startOffset());
  EXPECT_EQ(8u, active_match->endOffset());

  // Restart full search and check that added text is found.
  find_options->new_session = true;
  GetTextFinder().ResetMatchCount();
  GetTextFinder().CancelPendingScopingEffort();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(2, GetTextFinder().TotalMatchCount());

  Vector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  Node* text_in_b_element = GetDocument().body()->firstChild()->firstChild();
  Node* text_in_i_element = GetDocument().body()->lastChild()->firstChild();
  EXPECT_EQ(FindInPageRect(text_in_b_element, 4, text_in_b_element, 10),
            match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_in_i_element, 2, text_in_i_element, 8),
            match_rects[1]);
}

TEST_F(TextFinderTest, FindTextJavaScriptUpdatesDomAfterNoMatches) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<b>XXXXYYYY</b><i></i>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;
  bool active_now = false;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  find_options->new_session = false;
  ASSERT_FALSE(GetTextFinder().Find(identifier, search_text, *find_options,
                                    wrap_within_frame, &active_now));
  EXPECT_FALSE(active_now);

  // Add new text to DOM and try FindNext.
  auto* i_element = To<Element>(GetDocument().body()->lastChild());
  ASSERT_TRUE(i_element);
  i_element->SetInnerHTMLWithoutTrustedTypes("ZZFindMe");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame, &active_now));
  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_FALSE(active_now);
  EXPECT_EQ(2u, active_match->startOffset());
  EXPECT_EQ(8u, active_match->endOffset());

  // Restart full search and check that added text is found.
  find_options->new_session = true;
  GetTextFinder().ResetMatchCount();
  GetTextFinder().CancelPendingScopingEffort();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(1, GetTextFinder().TotalMatchCount());

  Vector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(1u, match_rects.size());
  Node* text_in_i_element = GetDocument().body()->lastChild()->firstChild();
  EXPECT_EQ(FindInPageRect(text_in_i_element, 2, text_in_i_element, 8),
            match_rects[0]);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(TextFinderTest, ScopeWithTimeouts) {
  // Make a long string.
  String search_pattern("abc");
  StringBuilder text;
  // Make 4 substrings "abc" in text.
  for (int i = 0; i < 100; ++i) {
    if (i == 1 || i == 10 || i == 50 || i == 90) {
      text.Append(search_pattern);
    } else {
      text.Append('a');
    }
  }

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(text.ToString());
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();

  // There will be only one iteration before timeout, because increment
  // of the TimeProxyPlatform timer is greater than timeout threshold.
  GetTextFinder().StartScopingStringMatches(identifier, search_pattern,
                                            *find_options);

  EXPECT_EQ(4, GetTextFinder().TotalMatchCount());
}

TEST_F(TextFinderTest, BeforeMatchEvent) {
  V8TestingScope v8_testing_scope;

  EvalJs(R"(
      const spacer = document.createElement('div');
      spacer.style.height = '2000px';
      document.body.appendChild(spacer);

      const foo = document.createElement('div');
      foo.textContent = 'foo';
      foo.setAttribute('hidden', 'until-found');
      document.body.appendChild(foo);
      window.beforematchFiredOnFoo = false;
      foo.addEventListener('beforematch', () => {
        window.beforematchFiredOnFoo = true;
      });

      const bar = document.createElement('div');
      bar.textContent = 'bar';
      bar.setAttribute('hidden', 'until-found');
      document.body.appendChild(bar);
      window.beforematchFiredOnBar = false;
      bar.addEventListener('beforematch', () => {
        window.YOffsetOnBeforematch = window.pageYOffset;
        window.beforematchFiredOnBar = true;
      });
      )");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  GetTextFinder().Find(/*identifier=*/0, "bar", *find_options,
                       /*wrap_within_frame=*/false);

  v8::Local<v8::Value> beforematch_fired_on_foo =
      EvalJs("window.beforematchFiredOnFoo");
  ASSERT_TRUE(beforematch_fired_on_foo->IsBoolean());
  EXPECT_FALSE(
      beforematch_fired_on_foo->ToBoolean(v8_testing_scope.GetIsolate())
          ->Value());

  v8::Local<v8::Value> beforematch_fired_on_bar =
      EvalJs("window.beforematchFiredOnBar");
  ASSERT_TRUE(beforematch_fired_on_bar->IsBoolean());
  EXPECT_TRUE(beforematch_fired_on_bar->ToBoolean(v8_testing_scope.GetIsolate())
                  ->Value());

  // Scrolling should occur after the beforematch event.
  v8::Local<v8::Context> context =
      v8_testing_scope.GetScriptState()->GetContext();
  v8::Local<v8::Value> beforematch_y_offset =
      EvalJs("window.YOffsetOnBeforematch");
  ASSERT_TRUE(beforematch_y_offset->IsNumber());
  EXPECT_TRUE(
      beforematch_y_offset->ToNumber(context).ToLocalChecked()->Value() == 0);
}

TEST_F(TextFinderTest, BeforeMatchEventRemoveElement) {
  V8TestingScope v8_testing_scope;

  EvalJs(R"(
      const spacer = document.createElement('div');
      spacer.style.height = '2000px';
      document.body.appendChild(spacer);

      const foo = document.createElement('div');
      foo.setAttribute('hidden', 'until-found');
      foo.textContent = 'foo';
      document.body.appendChild(foo);
      window.beforematchFiredOnFoo = false;
      foo.addEventListener('beforematch', () => {
        foo.remove();
        window.beforematchFiredOnFoo = true;
      });
      )");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  GetTextFinder().Find(/*identifier=*/0, "foo", *find_options,
                       /*wrap_within_frame=*/false);

  v8::Local<v8::Value> beforematch_fired_on_foo =
      EvalJs("window.beforematchFiredOnFoo");
  ASSERT_TRUE(beforematch_fired_on_foo->IsBoolean());
  EXPECT_TRUE(beforematch_fired_on_foo->ToBoolean(v8_testing_scope.GetIsolate())
                  ->Value());

  // TODO(jarhar): Update this test to include checks for scrolling behavior
  // once we decide what the behavior should be. Right now it is just here to
  // make sure we avoid a renderer crash due to the detached element.
}

// TODO(jarhar): Write more tests here once we decide on a behavior here:
// https://github.com/WICG/display-locking/issues/150

TEST_F(TextFinderSimTest, BeforeMatchEventAsyncExpandHighlight) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id=hiddenid hidden=until-found>hidden</div>
  )HTML");
  Compositor().BeginFrame();

  GetTextFinder().Find(/*identifier=*/0, "hidden",
                       *mojom::blink::FindOptions::New(),
                       /*wrap_within_frame=*/false);

  Compositor().BeginFrame();

  HeapVector<Member<DocumentMarker>> markers =
      GetDocument().Markers().Markers();
  ASSERT_EQ(markers.size(), 1u);
  DocumentMarker* marker = markers[0];
  EXPECT_TRUE(marker->GetType() == DocumentMarker::kTextMatch);
}

TEST_F(TextFinderSimTest, BeforeMatchExpandedHiddenMatchableUkm) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id=hiddenid hidden=until-found>hidden</div>
  )HTML");
  ukm::TestAutoSetUkmRecorder recorder;
  GetDocument().View()->ResetMetricsAggregatorForTesting();

  Compositor().BeginFrame();
  EXPECT_EQ(recorder.entries_count(), 0u);

  GetTextFinder().Find(/*identifier=*/0, "hidden",
                       *mojom::blink::FindOptions::New(),
                       /*wrap_within_frame=*/false);

  Compositor().BeginFrame();

  auto entries = recorder.GetEntriesByName("Blink.FindInPage");
  // There are two entries because
  // DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded followed by
  // DisplayLockContext::CommitForActivationWithSignal sets a
  // "HasFindInPageContentVisibilityActiveMatch" UKM.
  ASSERT_EQ(entries.size(), 2u);

  EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
      entries[0], "BeforematchExpandedHiddenMatchable"));
  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entries[1], "BeforematchExpandedHiddenMatchable"));
}

TEST_F(TextFinderSimTest, BeforeMatchExpandedHiddenMatchableUseCounter) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id=hiddenid hidden=until-found>hidden</div>
  )HTML");
  Compositor().BeginFrame();

  GetTextFinder().Find(/*identifier=*/0, "hidden",
                       *mojom::blink::FindOptions::New(),
                       /*wrap_within_frame=*/false);

  Compositor().BeginFrame();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kBeforematchRevealedHiddenMatchable));
}

TEST_F(TextFinderSimTest,
       BeforeMatchExpandedHiddenMatchableUseCounterNoHandler) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id=hiddenid>hidden</div>
  )HTML");
  Compositor().BeginFrame();

  GetTextFinder().Find(/*identifier=*/0, "hidden",
                       *mojom::blink::FindOptions::New(),
                       /*wrap_within_frame=*/false);

  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kBeforematchRevealedHiddenMatchable));
}

TEST_F(TextFinderTest, FindTextAcrossCommentNode) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<span>abc</span><!--comment--><span>def</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  String search_text("abcdef");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;

  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  EXPECT_TRUE(GetTextFinder().ActiveMatch());
}

// http://crbug.com/1192487
TEST_F(TextFinderTest, CommentAfterDoucmentElement) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("abc");
  GetDocument().appendChild(Comment::Create(GetDocument(), "xyz"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, "a", *find_options);
  EXPECT_EQ(1, GetTextFinder().TotalMatchCount());
  EXPECT_FALSE(GetTextFinder().ScopingInProgress());
}

TEST_F(TextFinderTest, FindWithScrollToMatchFalse) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div style='height: 2000px;'>Spacer</div>"
      "<span>abc</span> <span>abc</span> <span>abc</span>"
      "<div style='height: 2000px;'>Spacer</div>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Set the DOM selection to the second "abc" span.
  // childNodes are: 0:div (Spacer), 1:span (first abc), 2:#text whitespace,
  // 3:span (second abc), 4:#text whitespace, 5:span (third abc), 6:div (Spacer)
  Node* abc_node = GetDocument().body()->childNodes()->item(3)->firstChild();

  // Make sure the frame has focus/active state so selection is visible and not
  // cleared/ignored.
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDomTree::Builder()
          .Collapse(Position(abc_node, 0))
          .Extend(Position(abc_node, 3))
          .Build(),
      SetSelectionOptions());

  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->Selection()
                   .ComputeVisibleSelectionInDomTree()
                   .IsNone());

  // Ensure scroll position is initialized to (0, 0).
  ScrollableArea* viewport = GetDocument().View()->LayoutViewport();
  viewport->SetScrollOffset(ScrollOffset(0, 0),
                            mojom::blink::ScrollType::kProgrammatic,
                            cc::ScrollSourceType::kNone);
  ASSERT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  int identifier = 0;
  String search_text("abc");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->scroll_to_match = false;
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;

  // We find 'abc' with scroll_to_match set to false.
  // It should successfully identify the second "abc" selection as the active
  // match, set it as active_match_, and set should_locate_active_rect_ to true.
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));

  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(abc_node, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(abc_node, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // Verify that NO scrolling happened since scroll_to_match is false.
  EXPECT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  // Also verify that when we scope, the active match is correctly identified as
  // index 1 (ordinal 2).
  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);
  EXPECT_EQ(3, GetTextFinder().TotalMatchCount());

  // Now perform another search with scroll_to_match set to true.
  auto find_options_scroll = mojom::blink::FindOptions::New();
  find_options_scroll->new_session = true;
  find_options_scroll->find_match = true;
  find_options_scroll->scroll_to_match = true;
  find_options_scroll->run_synchronously_for_testing = true;

  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text,
                                   *find_options_scroll, wrap_within_frame));

  // Verify that scrolling DID happen since scroll_to_match is true.
  EXPECT_GT(viewport->GetScrollOffset().y(), 0);
}

TEST_F(TextFinderTest, FindWithScrollToMatchFalse_LeadingTrailingWhitespace) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div style='height: 2000px;'>Spacer</div>"
      "<span>  abc  </span>"
      "<div style='height: 2000px;'>Spacer</div>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Set the DOM selection to the text node including its spaces.
  Node* abc_node = GetDocument().body()->childNodes()->item(1)->firstChild();

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  // Select "  abc  " (length is 7: 2 spaces, "abc", 2 spaces).
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDomTree::Builder()
          .Collapse(Position(abc_node, 0))
          .Extend(Position(abc_node, 7))
          .Build(),
      SetSelectionOptions());

  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->Selection()
                   .ComputeVisibleSelectionInDomTree()
                   .IsNone());

  // Initialize scroll offset to (0, 0).
  ScrollableArea* viewport = GetDocument().View()->LayoutViewport();
  viewport->SetScrollOffset(ScrollOffset(0, 0),
                            mojom::blink::ScrollType::kProgrammatic,
                            cc::ScrollSourceType::kNone);
  ASSERT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  int identifier = 0;
  String search_text("abc");  // Trimmed search query
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->scroll_to_match = false;
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;

  // Perform find. It should successfully identify the trimmed "abc" within the
  // untrimmed active selection as the match, validating and correcting the
  // range offsets.
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));

  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(abc_node, active_match->startContainer());
  EXPECT_EQ(2u,
            active_match
                ->startOffset());  // Starts at index 2 (after 2 leading spaces)
  EXPECT_EQ(abc_node, active_match->endContainer());
  EXPECT_EQ(
      5u,
      active_match->endOffset());  // Ends at index 5 (before trailing spaces)

  // Verify that NO scrolling happened.
  EXPECT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  // Verify that scoping correctly identifies and matches the trimmed bounds.
  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);
  EXPECT_EQ(1, GetTextFinder().TotalMatchCount());
}

TEST_F(TextFinderTest, FindWithScrollToMatchFalse_UnrelatedSelection) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div style='height: 2000px;'>Spacer</div>"
      "<span>xyz</span> <span>abc</span>"
      "<div style='height: 2000px;'>Spacer</div>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Set the DOM selection to "xyz", while we are searching for "abc".
  Node* xyz_node = GetDocument().body()->childNodes()->item(1)->firstChild();
  Node* abc_node = GetDocument().body()->childNodes()->item(3)->firstChild();

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDomTree::Builder()
          .Collapse(Position(xyz_node, 0))
          .Extend(Position(xyz_node, 3))
          .Build(),
      SetSelectionOptions());

  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->Selection()
                   .ComputeVisibleSelectionInDomTree()
                   .IsNone());

  // Initialize scroll offset to (0, 0).
  ScrollableArea* viewport = GetDocument().View()->LayoutViewport();
  viewport->SetScrollOffset(ScrollOffset(0, 0),
                            mojom::blink::ScrollType::kProgrammatic,
                            cc::ScrollSourceType::kNone);
  ASSERT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  int identifier = 0;
  String search_text("abc");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->scroll_to_match = false;
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;

  // Perform find. Since the selection "xyz" is completely unrelated to "abc",
  // TextFinder should correctly search forward and designate "abc" as the
  // active match.
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));

  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(abc_node, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(abc_node, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // Verify that NO scrolling happened.
  EXPECT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  // Verify scoping.
  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);
  EXPECT_EQ(1, GetTextFinder().TotalMatchCount());
}

TEST_F(TextFinderTest, FindWithScrollToMatchFalse_BackwardSearch) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div style='height: 2000px;'>Spacer</div>"
      "<span>abc</span> <span>abc</span> <span>abc</span>"
      "<div style='height: 2000px;'>Spacer</div>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Set the DOM selection to the second "abc" span.
  // childNodes are: 0:div (Spacer), 1:span (first abc), 2:#text whitespace,
  // 3:span (second abc), 4:#text whitespace, 5:span (third abc), 6:div (Spacer)
  Node* abc_node = GetDocument().body()->childNodes()->item(3)->firstChild();

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDomTree::Builder()
          .Collapse(Position(abc_node, 0))
          .Extend(Position(abc_node, 3))
          .Build(),
      SetSelectionOptions());

  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->Selection()
                   .ComputeVisibleSelectionInDomTree()
                   .IsNone());

  // Initialize scroll offset to (0, 0).
  ScrollableArea* viewport = GetDocument().View()->LayoutViewport();
  viewport->SetScrollOffset(ScrollOffset(0, 0),
                            mojom::blink::ScrollType::kProgrammatic,
                            cc::ScrollSourceType::kNone);
  ASSERT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  int identifier = 0;
  String search_text("abc");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->scroll_to_match = false;
  find_options->forward = false;  // Backward search!
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;

  // Perform find. It should successfully identify the second "abc" selection
  // as the active match (without skipping it backward to the first "abc").
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));

  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(abc_node, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(abc_node, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // Verify that NO scrolling happened.
  EXPECT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());
}

TEST_F(TextFinderTest, FindSkipViewportForwardAndBackward) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<span id='m1'>abc</span> "
      "<span id='m2'>abc</span> "
      "<span id='m3'>abc</span>"
      "<div style='height: 2000px;'>Spacer</div>"
      "<span id='m4'>abc</span> "
      "<span id='m5'>abc</span>"
      "<div style='height: 2000px;'>Spacer</div>"
      "<span id='m6'>abc</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* m1 = GetDocument().getElementById(AtomicString("m1"))->firstChild();
  Node* m2 = GetDocument().getElementById(AtomicString("m2"))->firstChild();
  Node* m4 = GetDocument().getElementById(AtomicString("m4"))->firstChild();
  Node* m6 = GetDocument().getElementById(AtomicString("m6"))->firstChild();

  ScrollableArea* viewport = GetDocument().View()->LayoutViewport();
  viewport->SetScrollOffset(ScrollOffset(0, 0),
                            mojom::blink::ScrollType::kProgrammatic,
                            cc::ScrollSourceType::kNone);
  ASSERT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  int identifier = 0;
  String search_text("abc");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->run_synchronously_for_testing = true;

  // Initial search starts at m1 (index 0). m1, m2, m3 are all in the initial
  // viewport; m4 and m5 are ~2000px down; m6 is ~4000px down.
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);
  EXPECT_EQ(6, GetTextFinder().TotalMatchCount());
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  EXPECT_EQ(m1, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(0, GetTextFinder().ActiveMatchIndexForTesting());

  // Find next with skip_viewport = true should skip m2 and m3 (which are in
  // the current viewport) and jump directly to m4 (index 3).
  find_options->new_session = false;
  find_options->forward = true;
  find_options->skip_viewport = true;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  EXPECT_EQ(m4, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(3, GetTextFinder().ActiveMatchIndexForTesting());

  // Now viewport is scrolled to m4/m5. Find next with skip_viewport = true
  // should skip m5 (visible alongside m4) and jump to m6 (index 5).
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  EXPECT_EQ(m6, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(5, GetTextFinder().ActiveMatchIndexForTesting());

  // From m6 at the bottom, find next with skip_viewport = true should wrap
  // around to m1 (index 0) at the top.
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  EXPECT_EQ(m1, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(0, GetTextFinder().ActiveMatchIndexForTesting());

  // From m1 (with m1, m2, m3 visible at the top), find previous with
  // skip_viewport = true should wrap backwards to m6 (index 5).
  find_options->forward = false;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  EXPECT_EQ(m6, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(5, GetTextFinder().ActiveMatchIndexForTesting());

  // From m1, if we first advance to m2 (inside top viewport) and then search
  // backwards with skip_viewport = true, it should skip m1 (also in top
  // viewport) and wrap to m6 (index 5).
  find_options->forward = true;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m1, GetTextFinder().ActiveMatch()->startContainer());
  find_options->skip_viewport = false;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m2, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(1, GetTextFinder().ActiveMatchIndexForTesting());

  find_options->forward = false;
  find_options->skip_viewport = true;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  EXPECT_EQ(m6, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(5, GetTextFinder().ActiveMatchIndexForTesting());
}

TEST_F(TextFinderTest, FindSkipViewportAllMatchesInViewportFallback) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<span id='m1'>abc</span> "
      "<span id='m2'>abc</span> "
      "<span id='m3'>abc</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* m1 = GetDocument().getElementById(AtomicString("m1"))->firstChild();
  Node* m2 = GetDocument().getElementById(AtomicString("m2"))->firstChild();
  Node* m3 = GetDocument().getElementById(AtomicString("m3"))->firstChild();

  int identifier = 0;
  String search_text("abc");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->run_synchronously_for_testing = true;

  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);
  EXPECT_EQ(3, GetTextFinder().TotalMatchCount());
  EXPECT_EQ(m1, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(0, GetTextFinder().ActiveMatchIndexForTesting());

  // When all matches are visible in the current viewport, skip_viewport = true
  // should gracefully fall back to advancing to the immediate next/previous
  // match rather than getting stuck, both with wrap_within_frame = true and
  // with the two-step wrap_within_frame = false flow used by
  // FindRequestManager.
  find_options->new_session = false;
  find_options->forward = true;
  find_options->skip_viewport = true;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m2, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(1, GetTextFinder().ActiveMatchIndexForTesting());

  EXPECT_FALSE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                    /*wrap_within_frame=*/false));
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/false));
  EXPECT_EQ(m3, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(2, GetTextFinder().ActiveMatchIndexForTesting());

  // Also test backwards (`forward = false`) fallback with both
  // `wrap_within_frame = true` and `wrap_within_frame = false`.
  find_options->forward = false;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m2, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(1, GetTextFinder().ActiveMatchIndexForTesting());

  EXPECT_FALSE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                    /*wrap_within_frame=*/false));
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/false));
  EXPECT_EQ(m1, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_EQ(0, GetTextFinder().ActiveMatchIndexForTesting());

  // Overlapping matches ("aa" in "aaaa") starting from offset 1 via selection
  // must also terminate in one pass when all matches are visible.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<span id='overlap'>aaaa</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Node* overlap =
      GetDocument().getElementById(AtomicString("overlap"))->firstChild();
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDomTree::Builder().Collapse(Position(overlap, 1)).Build(),
      SetSelectionOptions());
  find_options->new_session = false;
  find_options->forward = true;
  find_options->skip_viewport = true;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, String("aa"), *find_options,
                                   /*wrap_within_frame=*/true));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  EXPECT_EQ(1u, GetTextFinder().ActiveMatch()->startOffset());
  EXPECT_EQ(3u, GetTextFinder().ActiveMatch()->endOffset());

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDomTree::Builder().Collapse(Position(overlap, 3)).Build(),
      SetSelectionOptions());
  find_options->forward = false;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, String("aa"), *find_options,
                                   /*wrap_within_frame=*/true));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  EXPECT_EQ(0u, GetTextFinder().ActiveMatch()->startOffset());
  EXPECT_EQ(2u, GetTextFinder().ActiveMatch()->endOffset());
}

TEST_F(TextFinderTest, FindSkipViewportAppliesPendingScroll) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<span id='m1'>abc</span> "
      "<span id='m2'>abc</span>"
      "<div style='height: 2000px;'>Spacer</div>"
      "<span id='m3'>abc</span> "
      "<span id='m4'>abc</span>"
      "<div style='height: 2000px;'>Spacer</div>"
      "<span id='m5'>abc</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* m1 = GetDocument().getElementById(AtomicString("m1"))->firstChild();
  Node* m3 = GetDocument().getElementById(AtomicString("m3"))->firstChild();
  Node* m5 = GetDocument().getElementById(AtomicString("m5"))->firstChild();

  ScrollableArea* viewport = GetDocument().View()->LayoutViewport();
  ASSERT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  int identifier = 0;
  String search_text("abc");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->run_synchronously_for_testing = true;
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m1, GetTextFinder().ActiveMatch()->startContainer());

  // Skip to m3 without running the scroll synchronously, so that scrolling m3
  // into view stays pending in an animation frame task.
  find_options->new_session = false;
  find_options->forward = true;
  find_options->skip_viewport = true;
  find_options->run_synchronously_for_testing = false;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m3, GetTextFinder().ActiveMatch()->startContainer());
  ASSERT_EQ(ScrollOffset(0, 0), viewport->GetScrollOffset());

  // Skipping again must first apply the pending scroll to m3 synchronously, so
  // that m4 (which is visible alongside m3) is skipped and the search lands on
  // m5 below the second 2000px spacer.
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m5, GetTextFinder().ActiveMatch()->startContainer());
  EXPECT_GT(viewport->GetScrollOffset().y(), 0);
}

TEST_F(TextFinderTest, FindSkipViewportInInnerScrollerAndPinchZoom) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='scroller' style='height: 60px; overflow: auto;'>"
      "  <div style='height: 20px;'>"
      "    <span id='m1'>abc</span> <span id='m2'>abc</span>"
      "  </div>"
      "  <div style='height: 200px;'>Spacer</div>"
      "  <div style='height: 20px;'><span id='m3'>abc</span></div>"
      "</div>"
      "<div style='height: 200px;'>Spacer</div>"
      "<span id='m4'>abc</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* m1 = GetDocument().getElementById(AtomicString("m1"))->firstChild();
  Node* m3 = GetDocument().getElementById(AtomicString("m3"))->firstChild();
  Node* m4 = GetDocument().getElementById(AtomicString("m4"))->firstChild();

  int identifier = 0;
  String search_text("abc");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->scroll_to_match = false;
  find_options->run_synchronously_for_testing = true;
  EXPECT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m1, GetTextFinder().ActiveMatch()->startContainer());

  // m3 is within the layout viewport's bounds vertically (~220px down), but it
  // is scrolled out of view inside the 60px-tall inner scroller, so skipping
  // the viewport from m1 must skip m2 and stop at m3.
  find_options->new_session = false;
  find_options->forward = true;
  find_options->skip_viewport = true;
  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m3, GetTextFinder().ActiveMatch()->startContainer());

  // Scroll the inner scroller so m3 is visible, and zoom the visual viewport
  // in so that m3 (at top) is inside the visual viewport while m4 (~260px down)
  // is outside the zoomed visual viewport despite being inside the layout
  // viewport.
  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  scroller->GetLayoutBox()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 200), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
  VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();
  visual_viewport.SetSize(gfx::Size(800, 600));
  visual_viewport.SetScale(4.0f);

  EXPECT_TRUE(GetTextFinder().Find(++identifier, search_text, *find_options,
                                   /*wrap_within_frame=*/true));
  EXPECT_EQ(m4, GetTextFinder().ActiveMatch()->startContainer());
}

TEST_F(TextFinderSimTest, FindSkipViewportInNestedFrame) {
  ResizeView(gfx::Size(640, 480));
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest frame_request("https://example.com/frame.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
    <!DOCTYPE html>
    <span id="m1">abc</span>
    <div style="height: 1500px;">Spacer</div>
    <span id="m2">abc</span> <span id="m3">abc</span>
    <div style="height: 1500px;">Spacer</div>
    <iframe src="frame.html" style="width: 400px; height: 300px;"></iframe>
  )HTML");
  frame_request.Complete(R"HTML(
    <!DOCTYPE html>
    <span id="f1">abc</span> <span id="f2">abc</span>
    <div style="height: 2000px;">Spacer</div>
    <span id="f3">abc</span>
  )HTML");
  Compositor().BeginFrame();

  TextFinder& main_finder = GetTextFinder();
  Node* m2 = GetDocument().getElementById(AtomicString("m2"))->firstChild();
  Node* m3 = GetDocument().getElementById(AtomicString("m3"))->firstChild();
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  Document& child_document = *child_frame->GetDocument();
  TextFinder& text_finder =
      WebLocalFrameImpl::FromFrame(child_frame)->EnsureTextFinder();
  Node* f1 = child_document.getElementById(AtomicString("f1"))->firstChild();
  Node* f2 = child_document.getElementById(AtomicString("f2"))->firstChild();
  Node* f3 = child_document.getElementById(AtomicString("f3"))->firstChild();

  int identifier = 0;
  String search_text("abc");
  auto find_options = mojom::blink::FindOptions::New();
  find_options->new_session = true;
  find_options->find_match = true;
  find_options->scroll_to_match = true;
  find_options->run_synchronously_for_testing = true;

  // Start in the main frame at m1, then skip the viewport with
  // wrap_within_frame = false to jump to m2 (scrolling m2 and m3 into view
  // while m1 is now above the fold).
  EXPECT_TRUE(main_finder.Find(identifier, search_text, *find_options,
                               /*wrap_within_frame=*/false));
  find_options->new_session = false;
  find_options->forward = true;
  find_options->skip_viewport = true;
  EXPECT_TRUE(main_finder.Find(++identifier, search_text, *find_options,
                               /*wrap_within_frame=*/false));
  EXPECT_EQ(m2, main_finder.ActiveMatch()->startContainer());

  // Skipping again from m2 with wrap_within_frame = false must skip m3 (visible
  // alongside m2) and return false at the end of the main frame rather than
  // wrapping back to m1, so the browser can move to the iframe below.
  EXPECT_FALSE(main_finder.Find(++identifier, search_text, *find_options,
                                /*wrap_within_frame=*/false));

  // When the browser advances to the iframe with asynchronous scrolling
  // (run_synchronously_for_testing = false), f1 becomes active and queues a
  // pending scroll_task_ on the iframe. If the user immediately searches
  // backwards with skip_viewport = true before the animation frame runs,
  // returning false at the start of the iframe must still flush its pending
  // scroll_task_ so the main frame scrolls to the iframe and does not skip m3.
  find_options->run_synchronously_for_testing = false;
  EXPECT_TRUE(text_finder.Find(identifier, search_text, *find_options,
                               /*wrap_within_frame=*/false));
  EXPECT_EQ(f1, text_finder.ActiveMatch()->startContainer());
  main_finder.ClearActiveFindMatch();

  find_options->forward = false;
  EXPECT_FALSE(text_finder.Find(++identifier, search_text, *find_options,
                                /*wrap_within_frame=*/false));
  EXPECT_TRUE(main_finder.Find(identifier, search_text, *find_options,
                               /*wrap_within_frame=*/false));
  EXPECT_EQ(m3, main_finder.ActiveMatch()->startContainer());
  main_finder.ClearActiveFindMatch();

  find_options->new_session = true;
  find_options->forward = true;
  find_options->skip_viewport = false;
  find_options->scroll_to_match = false;
  find_options->run_synchronously_for_testing = true;
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 1500), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
  EXPECT_TRUE(text_finder.Find(++identifier, search_text, *find_options,
                               /*wrap_within_frame=*/true));
  EXPECT_EQ(f1, text_finder.ActiveMatch()->startContainer());

  // The iframe is below the fold of the main frame, so even though f2 is within
  // the iframe's own viewport it is not actually visible and must not be
  // skipped.
  find_options->new_session = false;
  find_options->forward = true;
  find_options->skip_viewport = true;
  EXPECT_TRUE(text_finder.Find(++identifier, search_text, *find_options,
                               /*wrap_within_frame=*/true));
  EXPECT_EQ(f2, text_finder.ActiveMatch()->startContainer());

  // Once the iframe is scrolled into view in the main frame, f1 is visible and
  // searching backwards from f2 skips it and wraps around to f3.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 3000), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
  Compositor().BeginFrame();
  find_options->forward = false;
  EXPECT_TRUE(text_finder.Find(++identifier, search_text, *find_options,
                               /*wrap_within_frame=*/true));
  EXPECT_EQ(f3, text_finder.ActiveMatch()->startContainer());
}

}  // namespace blink
