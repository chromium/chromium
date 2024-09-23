// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"

#include <gtest/gtest.h>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using test::RunPendingTasks;

class TextFragmentHandlerTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

  void RunAsyncMatchingTasks() {
    ThreadScheduler::Current()
        ->ToMainThreadScheduler()
        ->StartIdlePeriodForTesting();
    RunPendingTasks();
  }

  void SetSelection(const Position& start, const Position& end) {
    GetDocument().GetFrame()->Selection().SetSelection(
        SelectionInDOMTree::Builder().SetBaseAndExtent(start, end).Build(),
        SetSelectionOptions());
  }

  void SetLocationHash(Document& document, String hash) {
    ScriptState* script_state = ToScriptStateForMainWorld(document.GetFrame());
    ScriptState::Scope entered_context_scope(script_state);
    document.GetFrame()->DomWindow()->location()->setHash(
        script_state->GetIsolate(), hash, ASSERT_NO_EXCEPTION);
  }

  String SelectThenRequestSelector(const Position& start, const Position& end) {
    SetSelection(start, end);
    TextFragmentHandler::OpenedContextMenuOverSelection(
        GetDocument().GetFrame());
    return RequestSelector();
  }

  String RequestSelector() {
    bool callback_called = false;
    String selector;
    auto lambda =
        [](bool& callback_called, String& selector,
           const String& generated_selector,
           shared_highlighting::LinkGenerationError error,
           shared_highlighting::LinkGenerationReadyStatus ready_status) {
          selector = generated_selector;
          callback_called = true;
        };
    auto callback =
        WTF::BindOnce(lambda, std::ref(callback_called), std::ref(selector));
    GetTextFragmentHandler().RequestSelector(std::move(callback));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(callback_called);
    return selector;
  }

  Vector<String> ExtractTextFragmentsMatches() {
    bool callback_called = false;
    Vector<String> target_texts;
    auto lambda = [](bool& callback_called, Vector<String>& target_texts,
                     const Vector<String>& fetched_target_texts) {
      target_texts = fetched_target_texts;
      callback_called = true;
    };
    auto callback = WTF::BindOnce(lambda, std::ref(callback_called),
                                  std::ref(target_texts));

    GetTextFragmentHandler().ExtractTextFragmentsMatches(std::move(callback));

    EXPECT_TRUE(callback_called);
    return target_texts;
  }

  gfx::Rect ExtractFirstTextFragmentsRect() {
    bool callback_called = false;
    gfx::Rect text_fragment_rect;
    auto lambda = [](bool& callback_called, gfx::Rect& text_fragment_rect,
                     const gfx::Rect& fetched_text_fragment_rect) {
      text_fragment_rect = fetched_text_fragment_rect;
      callback_called = true;
    };
    auto callback = WTF::BindOnce(lambda, std::ref(callback_called),
                                  std::ref(text_fragment_rect));

    GetTextFragmentHandler().ExtractFirstFragmentRect(std::move(callback));

    EXPECT_TRUE(callback_called);
    return text_fragment_rect;
  }

  void LoadAhem() {
    std::optional<Vector<char>> data =
        test::ReadFromFile(test::CoreTestDataPath("Ahem.ttf"));
    ASSERT_TRUE(data);
    auto* buffer =
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
            DOMArrayBuffer::Create(base::as_byte_span(*data)));
    FontFace* ahem = FontFace::Create(GetDocument().GetExecutionContext(),
                                      AtomicString("Ahem"), buffer,
                                      FontFaceDescriptors::Create());

    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    DummyExceptionStateForTesting exception_state;
    FontFaceSetDocument::From(GetDocument())
        ->addForBinding(script_state, ahem, exception_state);
  }

  TextFragmentHandler& GetTextFragmentHandler() {
    if (!GetDocument().GetFrame()->GetTextFragmentHandler())
      GetDocument().GetFrame()->CreateTextFragmentHandler();
    return *GetDocument().GetFrame()->GetTextFragmentHandler();
  }

  bool HasTextFragmentHandler(LocalFrame* frame) {
    return frame->GetTextFragmentHandler();
  }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TextFragmentHandlerTest, RemoveTextFragments) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">With some more text</p>
  )HTML");
  RunAsyncMatchingTasks();

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  GetTextFragmentHandler().RemoveFragments();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

TEST_F(TextFragmentHandlerTest,
       ExtractTextFragmentWithWithMultipleTextFragments) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">With some more text</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  Vector<String> target_texts = ExtractTextFragmentsMatches();

  EXPECT_EQ(2u, target_texts.size());
  EXPECT_EQ("test page", target_texts[0]);
  EXPECT_EQ("more text", target_texts[1]);
}

TEST_F(TextFragmentHandlerTest, ExtractTextFragmentWithNoMatch) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=not%20on%20the%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=not%20on%20the%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p>This is a test page, with some more text</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  Vector<String> target_texts = ExtractTextFragmentsMatches();

  EXPECT_EQ(0u, target_texts.size());
}

TEST_F(TextFragmentHandlerTest, ExtractTextFragmentWithRange) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=This,text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=This,text");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p>This is a test page, with some more text</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  Vector<String> target_texts = ExtractTextFragmentsMatches();

  EXPECT_EQ(1u, target_texts.size());
  EXPECT_EQ("This is a test page, with some more text", target_texts[0]);
}

TEST_F(TextFragmentHandlerTest, ExtractTextFragmentWithRangeAndContext) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=this,is&text=a-,test,page&text=with,some,-content&"
      "text=about-,nothing,at,-all",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=this,is&text=a-,test,page&text=with,some,-content&"
      "text=about-,nothing,at,-all");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test for the page</p>
    <p>With some content</p>
    <p>About nothing at all</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(4u, GetDocument().Markers().Markers().size());

  Vector<String> target_texts = ExtractTextFragmentsMatches();

  EXPECT_EQ(4u, target_texts.size());
  EXPECT_EQ("This is", target_texts[0]);
  EXPECT_EQ("test for the page", target_texts[1]);
  EXPECT_EQ("With some", target_texts[2]);
  EXPECT_EQ("nothing at", target_texts[3]);
}

TEST_F(TextFragmentHandlerTest, ExtractFirstTextFragmentRect) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=This,page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=This,page");
  LoadAhem();
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=device-width">
    <style>p { font: 10px/1 Ahem; }</style>
    <p id="first">This is a test page</p>
    <p id="second">with some more text</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 19);
  ASSERT_EQ("This is a test page", PlainText(EphemeralRange(start, end)));
  gfx::Rect rect(ComputeTextRect(EphemeralRange(start, end)));
  gfx::Rect expected_rect =
      GetDocument().GetFrame()->View()->FrameToViewport(rect);
  // ExtractFirstTextFragmentsRect should return the first matched viewport
  // relative location.
  ASSERT_EQ(expected_rect.ToString(), "8,10 190x10");

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_EQ(expected_rect.ToString(), text_fragment_rect.ToString());
}

TEST_F(TextFragmentHandlerTest, ExtractFirstTextFragmentRectScroll) {
  // Android settings to correctly extract the rect when the page is loaded
  // zoomed in
  WebView().GetPage()->GetSettings().SetViewportEnabled(true);
  WebView().GetPage()->GetSettings().SetViewportMetaEnabled(true);
  WebView().GetPage()->GetSettings().SetShrinksViewportContentToFit(true);
  WebView().GetPage()->GetSettings().SetMainFrameResizesAreOrientationChanges(
      true);
  SimRequest request("https://example.com/test.html#:~:text=test,page",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test,page");
  LoadAhem();
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="initial-scale=4">
    <style>
      body {
        height: 2200px;
      }
      p {
        position: absolute;
        top: 2000px;
        font: 10px/1 Ahem;
      }
    </style>
    <p id="first">This is a test page</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& start = Position(first_paragraph, 10);
  const auto& end = Position(first_paragraph, 19);
  ASSERT_EQ("test page", PlainText(EphemeralRange(start, end)));
  gfx::Rect rect(ComputeTextRect(EphemeralRange(start, end)));
  gfx::Rect expected_rect =
      GetDocument().GetFrame()->View()->FrameToViewport(rect);
  // ExtractFirstTextFragmentsRect should return the first matched scaled
  // viewport relative location since the page is loaded zoomed in 4X
  ASSERT_EQ(gfx::Rect(432, 300, 360, 40), expected_rect);

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_EQ(expected_rect, text_fragment_rect);
}

TEST_F(TextFragmentHandlerTest, ExtractFirstTextFragmentRectMultipleHighlight) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text");
  LoadAhem();
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=device-width">
    <style>
      p {
        font: 10px/1 Ahem;
      }
      body {
        height: 1200px;
      }
      #second {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">With some more text</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& start = Position(first_paragraph, 10);
  const auto& end = Position(first_paragraph, 19);
  ASSERT_EQ("test page", PlainText(EphemeralRange(start, end)));
  gfx::Rect rect(ComputeTextRect(EphemeralRange(start, end)));
  gfx::Rect expected_rect =
      GetDocument().GetFrame()->View()->FrameToViewport(rect);
  // ExtractFirstTextFragmentsRect should return the first matched viewport
  // relative location.
  ASSERT_EQ(expected_rect.ToString(), "108,10 90x10");

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_EQ(expected_rect.ToString(), text_fragment_rect.ToString());
}

TEST_F(TextFragmentHandlerTest,
       ExtractFirstTextFragmentRectMultipleHighlightWithNoFoundText) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=fake&text=test%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=fake&text=test%20page");
  LoadAhem();
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=device-width">
    <style>
      p {
        font: 10px/1 Ahem;
      }
      body {
        height: 1200px;
      }
      #second {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="first">This is a test page</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& start = Position(first_paragraph, 10);
  const auto& end = Position(first_paragraph, 19);
  ASSERT_EQ("test page", PlainText(EphemeralRange(start, end)));
  gfx::Rect rect(ComputeTextRect(EphemeralRange(start, end)));
  gfx::Rect expected_rect =
      GetDocument().GetFrame()->View()->FrameToViewport(rect);
  // ExtractFirstTextFragmentsRect should return the first matched viewport
  // relative location.
  ASSERT_EQ(expected_rect.ToString(), "108,10 90x10");

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_EQ(expected_rect.ToString(), text_fragment_rect.ToString());
}

TEST_F(TextFragmentHandlerTest, RejectExtractFirstTextFragmentRect) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=not%20on%20the%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=not%20on%20the%20page");
  LoadAhem();
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=device-width">
    <style>
      p {
        font: 10px/1 Ahem;
      }
      body {
        height: 1200px;
      }
      #second {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">With some more text</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_TRUE(text_fragment_rect.IsEmpty());
}

// Checks that the selector is preemptively generated.
TEST_F(TextFragmentHandlerTest, CheckPreemptiveGeneration) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph</p>
    )HTML");

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);
  TextFragmentHandler::OpenedContextMenuOverSelection(GetDocument().GetFrame());

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated", 1);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 0);
}

// When URL is blocklisted, the selector shouldn't be preemptively generated.
TEST_F(TextFragmentHandlerTest, CheckNoPreemptiveGenerationBlocklist) {
  SimRequest request("https://instagram.com/test.html", "text/html");
  LoadURL("https://instagram.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph</p>
    )HTML");

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);
  TextFragmentHandler::OpenedContextMenuOverSelection(GetDocument().GetFrame());

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated", 0);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 0);
}

// Check that selector is not generated for editable text.
TEST_F(TextFragmentHandlerTest, CheckNoPreemptiveGenerationEditable) {
  SimRequest request("https://instagram.com/test.html", "text/html");
  LoadURL("https://instagram.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <input type="text" id="input" value="default text in input">
    )HTML");

  Node* input_text = FlatTreeTraversal::Next(
                         *GetDocument().getElementById(AtomicString("input")))
                         ->firstChild();
  const auto& selected_start = Position(input_text, 0);
  const auto& selected_end = Position(input_text, 12);
  ASSERT_EQ("default text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);
  TextFragmentHandler::OpenedContextMenuOverSelection(GetDocument().GetFrame());

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated", 0);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 0);
}

// TODO(crbug.com/1192047): Update the test to better reflect the real repro
// steps. Test case for crash in crbug.com/1190137. When selector is requested
// after callback is set and unused.
TEST_F(TextFragmentHandlerTest, SecondGenerationCrash) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <p id='p'>First paragraph text</p>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* p = GetDocument().getElementById(AtomicString("p"));
  const auto& start = Position(p->lastChild(), 0);
  const auto& end = Position(p->lastChild(), 15);
  ASSERT_EQ("First paragraph", PlainText(EphemeralRange(start, end)));
  SetSelection(start, end);

  auto callback =
      WTF::BindOnce([](const TextFragmentSelector& selector,
                       shared_highlighting::LinkGenerationError error) {});
  MakeGarbageCollected<TextFragmentSelectorGenerator>(GetDocument().GetFrame())
      ->SetCallbackForTesting(std::move(callback));

  // This shouldn't crash.
  TextFragmentHandler::OpenedContextMenuOverSelection(GetDocument().GetFrame());
  base::RunLoop().RunUntilIdle();
}

// Verifies metrics for preemptive generation are correctly recorded when the
// selector is successfully generated.
TEST_F(TextFragmentHandlerTest, CheckMetrics_Success) {
  base::test::ScopedFeatureList feature_list;
  // Basic exact selector case.
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("First paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  String selector = SelectThenRequestSelector(selected_start, selected_end);
  EXPECT_EQ(selector, "First%20paragraph%20text%20that%20is");
}

// Verifies metrics for preemptive generation are correctly recorded when the
// selector request fails, in this case, because the context limit is reached.
TEST_F(TextFragmentHandlerTest, CheckMetrics_Failure) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph prefix one two three four five six seven
     eight nine ten to not unique snippet of text followed by suffix</p>
    <p id='second'>Second paragraph prefix one two three four five six seven
     eight nine ten to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& selected_start = Position(first_paragraph, 80);
  const auto& selected_end = Position(first_paragraph, 106);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));
  String selector = SelectThenRequestSelector(selected_start, selected_end);
  EXPECT_EQ(selector, "");
}

TEST_F(TextFragmentHandlerTest,
       ShouldCreateTextFragmentHandlerAndRemoveHighlightForIframes) {
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id="iframe" src="child.html"></iframe>
  )HTML");

  child_request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 1000px;
      }
    </style>
    <p>
      test
    </p>
    <script>
      window.location.hash = ':~:text=test';
    </script>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  Element* iframe = GetDocument().getElementById(AtomicString("iframe"));
  auto* child_frame =
      To<LocalFrame>(To<HTMLFrameOwnerElement>(iframe)->ContentFrame());

  EXPECT_EQ(1u, child_frame->GetDocument()->Markers().Markers().size());
  EXPECT_TRUE(HasTextFragmentHandler(child_frame));

  TextFragmentHandler::OpenedContextMenuOverSelection(GetDocument().GetFrame());

  mojo::Remote<mojom::blink::TextFragmentReceiver> remote;
  child_frame->BindTextFragmentReceiver(remote.BindNewPipeAndPassReceiver());

  ASSERT_TRUE(remote.is_bound());
  remote.get()->RemoveFragments();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, child_frame->GetDocument()->Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(child_frame->GetDocument()->View()->GetFragmentAnchor());
}

TEST_F(TextFragmentHandlerTest, NonMatchingTextDirectiveCreatesHandler) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  ASSERT_FALSE(HasTextFragmentHandler(GetDocument().GetFrame()));

  // Navigate to a text directive that doesn't exist on the page.
  SetLocationHash(GetDocument(), ":~:text=non%20existent%20text");

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  ASSERT_EQ(0u, GetDocument().Markers().Markers().size());

  // Even though the directive didn't find a match, a handler is created by the
  // attempt.
  EXPECT_TRUE(HasTextFragmentHandler(GetDocument().GetFrame()));
}

TEST_F(TextFragmentHandlerTest, MatchingTextDirectiveCreatesHandler) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  ASSERT_FALSE(HasTextFragmentHandler(GetDocument().GetFrame()));

  // Navigate to a text directive that highlights "test page".
  SetLocationHash(GetDocument(), ":~:text=test%20page");

  Compositor().BeginFrame();
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());

  EXPECT_TRUE(HasTextFragmentHandler(GetDocument().GetFrame()));
}

TEST_F(TextFragmentHandlerTest,
       ShouldCreateTextFragmentHandlerAndRemoveHighlight) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">With some more text</p>
  )HTML");
  Compositor().BeginFrame();

  ASSERT_EQ(0u, GetDocument().Markers().Markers().size());
  ASSERT_FALSE(HasTextFragmentHandler(GetDocument().GetFrame()));

  // Binding a receiver should create a handler.
  mojo::Remote<mojom::blink::TextFragmentReceiver> remote;
  GetDocument().GetFrame()->BindTextFragmentReceiver(
      remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(remote.is_bound());
  EXPECT_TRUE(HasTextFragmentHandler(GetDocument().GetFrame()));

  // Set the fragment to two text directives.
  SetLocationHash(GetDocument(), ":~:text=test%20page&text=more%20text");

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  ASSERT_EQ(2u, GetDocument().Markers().Markers().size());

  // Ensure RemoveFragments called via Mojo removes the document markers.
  remote.get()->RemoveFragments();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Ensure the fragment was uninstalled.
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

TEST_F(TextFragmentHandlerTest,
       ShouldCreateTextFragmentHandlerAndRequestSelector) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("First paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);

  mojo::Remote<mojom::blink::TextFragmentReceiver> remote;
  EXPECT_FALSE(HasTextFragmentHandler(GetDocument().GetFrame()));
  EXPECT_FALSE(remote.is_bound());

  TextFragmentHandler::OpenedContextMenuOverSelection(GetDocument().GetFrame());
  GetDocument().GetFrame()->BindTextFragmentReceiver(
      remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(HasTextFragmentHandler(GetDocument().GetFrame()));
  EXPECT_TRUE(remote.is_bound());

  bool callback_called = false;
  String selector;
  auto lambda =
      [](bool& callback_called, String& selector,
         const String& generated_selector,
         shared_highlighting::LinkGenerationError error,
         shared_highlighting::LinkGenerationReadyStatus ready_status) {
        selector = generated_selector;
        callback_called = true;
      };
  auto callback =
      WTF::BindOnce(lambda, std::ref(callback_called), std::ref(selector));
  remote->RequestSelector(std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);

  EXPECT_EQ(selector, "First%20paragraph%20text%20that%20is");
}

// Verify that removing a shared highlight removes document markers and the
// text directive from the URL, for both main frame and subframe.
TEST_F(TextFragmentHandlerTest,
       ShouldRemoveFromMainFrameAndIframeWhenBothHaveHighlights) {
  SimRequest main_request("https://example.com/test.html#:~:text=test",
                          "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test");
  main_request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="first">This is a test page</p>
    <iframe id="iframe" src="child.html"></iframe>
  )HTML");

  child_request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 1000px;
      }
    </style>
    <p>
      iframe
    </p>
    <script>
      window.location.hash = ':~:text=iframe';
    </script>
  )HTML");
  RunAsyncMatchingTasks();

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();

  Element* iframe = GetDocument().getElementById(AtomicString("iframe"));
  auto* child_frame =
      To<LocalFrame>(To<HTMLFrameOwnerElement>(iframe)->ContentFrame());
  auto* main_frame = GetDocument().GetFrame();

  ASSERT_EQ(1u, child_frame->GetDocument()->Markers().Markers().size());
  ASSERT_EQ("https://example.com/child.html#:~:text=iframe",
            child_frame->Loader().GetDocumentLoader()->GetHistoryItem()->Url());

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_EQ("https://example.com/test.html#:~:text=test",
            main_frame->Loader().GetDocumentLoader()->GetHistoryItem()->Url());

  // Remove shared highlights from the iframe.
  {
    mojo::Remote<mojom::blink::TextFragmentReceiver> remote;
    child_frame->BindTextFragmentReceiver(remote.BindNewPipeAndPassReceiver());
    remote->RemoveFragments();
    remote.FlushForTesting();

    EXPECT_EQ(0u, child_frame->GetDocument()->Markers().Markers().size());
    EXPECT_FALSE(child_frame->GetDocument()->View()->GetFragmentAnchor());
    EXPECT_EQ(
        "https://example.com/child.html",
        child_frame->Loader().GetDocumentLoader()->GetHistoryItem()->Url());
  }

  // Remove shared highlights from the main frame.
  {
    mojo::Remote<mojom::blink::TextFragmentReceiver> remote;
    main_frame->BindTextFragmentReceiver(remote.BindNewPipeAndPassReceiver());
    remote->RemoveFragments();
    remote.FlushForTesting();

    EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
    EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
    EXPECT_EQ(
        "https://example.com/test.html",
        main_frame->Loader().GetDocumentLoader()->GetHistoryItem()->Url());
  }
}

// crbug.com/1266937 Even if |TextFragmentSelectorGenerator| gets reset between
// generation completion and selector request we should record the correct error
// code.
// TODO(https://crbug.com/338340754): It's not clear how useful this behavior is
// and it prevents us from clearing the TextFragmentHandler and
// TextFragmentSelectorGenerator entirely between navigations.
TEST_F(TextFragmentHandlerTest, IfGeneratorResetShouldRecordCorrectError) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& selected_start = Position(first_paragraph, 5);
  const auto& selected_end = Position(first_paragraph, 6);
  ASSERT_EQ(" ", PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);
  TextFragmentHandler::OpenedContextMenuOverSelection(GetDocument().GetFrame());

  // Reset |TextFragmentSelectorGenerator|.
  GetTextFragmentHandler().DidDetachDocumentOrFrame();

  EXPECT_EQ(RequestSelector(), "");

  shared_highlighting::LinkGenerationError expected_error =
      shared_highlighting::LinkGenerationError::kEmptySelection;
  EXPECT_EQ(expected_error, GetTextFragmentHandler().error_);
}

// crbug.com/1301794 If generation didn't start requesting selector shouldn't
// crash.
TEST_F(TextFragmentHandlerTest, NotGenerated) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");

  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& selected_start = Position(first_paragraph, 5);
  const auto& selected_end = Position(first_paragraph, 6);
  ASSERT_EQ(" ", PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);
  EXPECT_EQ(RequestSelector(), "");

  shared_highlighting::LinkGenerationError expected_error =
      shared_highlighting::LinkGenerationError::kNotGenerated;
  EXPECT_EQ(expected_error, GetTextFragmentHandler().error_);
}

TEST_F(TextFragmentHandlerTest, InvalidateOverflowOnRemoval) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      ::target-text {
        text-decoration: wavy underline overline green 5px;
        text-underline-offset: 20px;
        background-color: transparent;
      }
    </style>
    <p id="first">This is a test page</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  Text* first_paragraph = To<Text>(
      GetDocument().getElementById(AtomicString("first"))->firstChild());
  LayoutText* layout_text = first_paragraph->GetLayoutObject();
  PhysicalRect marker_rect = layout_text->VisualOverflowRect();

  GetTextFragmentHandler().RemoveFragments();
  Compositor().BeginFrame();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
  PhysicalRect removed_rect = layout_text->VisualOverflowRect();

  // Platforms differ in exact sizes, but the relative sizes are sufficient
  // for testing.
  EXPECT_EQ(removed_rect.X(), marker_rect.X());
  EXPECT_GT(removed_rect.Y(), marker_rect.Y());
  EXPECT_EQ(removed_rect.Width(), marker_rect.Width());
  EXPECT_GT(marker_rect.Height(), removed_rect.Height());
}

}  // namespace blink
