// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_handler.h"

#include <gtest/gtest.h>

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using test::RunPendingTasks;

class TextFragmentHandlerTest : public SimTest,
                                public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

    std::vector<base::Feature> enabled;
    std::vector<base::Feature> disabled;

    enabled.push_back(shared_highlighting::kSharedHighlightingV2);

    preemptive_generation_enabled_ = GetParam();
    if (preemptive_generation_enabled_)
      enabled.push_back(shared_highlighting::kPreemptiveLinkToTextGeneration);
    else
      disabled.push_back(shared_highlighting::kPreemptiveLinkToTextGeneration);

    feature_list_.InitWithFeatures(enabled, disabled);
  }

  void BeginEmptyFrame() {
    // If a test case doesn't find a match and therefore doesn't schedule the
    // beforematch event, we should still render a second frame as if we did
    // schedule the event to retain test coverage.
    // When the beforematch event is not scheduled, a DCHECK will fail on
    // BeginFrame() because no event was scheduled, so we schedule an empty task
    // here.
    GetDocument().EnqueueAnimationFrameTask(WTF::Bind([]() {}));
    Compositor().BeginFrame();
  }

  void RunAsyncMatchingTasks() {
    auto* scheduler =
        blink::scheduler::WebThreadScheduler::MainThreadScheduler();
    blink::scheduler::RunIdleTasksForTesting(scheduler,
                                             base::BindOnce([]() {}));
    RunPendingTasks();
  }

  void SetSelection(const Position& start, const Position& end) {
    GetDocument().GetFrame()->Selection().SetSelection(
        SelectionInDOMTree::Builder().SetBaseAndExtent(start, end).Build(),
        SetSelectionOptions());
  }

  String SelectThenRequestSelector(const Position& start, const Position& end) {
    SetSelection(start, end);

    GetTextFragmentHandler().StartPreemptiveGenerationIfNeeded();

    bool callback_called = false;
    String selector;
    auto lambda = [](bool& callback_called, String& selector,
                     const String& generated_selector) {
      selector = generated_selector;
      callback_called = true;
    };
    auto callback =
        WTF::Bind(lambda, std::ref(callback_called), std::ref(selector));
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
    auto callback =
        WTF::Bind(lambda, std::ref(callback_called), std::ref(target_texts));

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
    auto callback = WTF::Bind(lambda, std::ref(callback_called),
                              std::ref(text_fragment_rect));

    GetTextFragmentHandler().ExtractFirstFragmentRect(std::move(callback));

    EXPECT_TRUE(callback_called);
    return text_fragment_rect;
  }

  void LoadAhem() {
    scoped_refptr<SharedBuffer> shared_buffer =
        test::ReadFromFile(test::CoreTestDataPath("Ahem.ttf"));
    auto* buffer =
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
            DOMArrayBuffer::Create(shared_buffer));
    FontFace* ahem =
        FontFace::Create(GetDocument().GetExecutionContext(), "Ahem", buffer,
                         FontFaceDescriptors::Create());

    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    DummyExceptionStateForTesting exception_state;
    FontFaceSetDocument::From(GetDocument())
        ->addForBinding(script_state, ahem, exception_state);
  }

  void VerifyPreemptiveGenerationMetrics(bool success) {
    if (!preemptive_generation_enabled_) {
      histogram_tester_.ExpectTotalCount(
          "SharedHighlights.LinkGenerated.Error.Requested", 0);
      histogram_tester_.ExpectTotalCount(
          "SharedHighlights.LinkGenerated.RequestedAfterReady", 0);
      histogram_tester_.ExpectTotalCount(
          "SharedHighlights.LinkGenerated.RequestedBeforeReady", 0);
    } else {
      EXPECT_EQ(
          1u, histogram_tester_
                      .GetAllSamples(
                          "SharedHighlights.LinkGenerated.RequestedAfterReady")
                      .size() +
                  histogram_tester_
                      .GetAllSamples(
                          "SharedHighlights.LinkGenerated.RequestedBeforeReady")
                      .size());

      if (!success) {
        histogram_tester_.ExpectTotalCount(
            "SharedHighlights.LinkGenerated.Error.Requested", 1);
      } else {
        histogram_tester_.ExpectTotalCount(
            "SharedHighlights.LinkGenerated.Error.Requested", 0);
      }
    }

    // Check async task metrics.
    EXPECT_LT(0u, histogram_tester_
                      .GetAllSamples("SharedHighlights.AsyncTask.Iterations")
                      .size());
    EXPECT_LT(0u,
              histogram_tester_
                  .GetAllSamples("SharedHighlights.AsyncTask.SearchDuration")
                  .size());
  }

  TextFragmentHandler& GetTextFragmentHandler() {
    return *GetDocument().GetFrame()->GetTextFragmentHandler();
  }

  bool HasTextFragmentHandler(LocalFrame* frame) {
    return frame->GetTextFragmentHandler();
  }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  bool preemptive_generation_enabled_;
};

TEST_P(TextFragmentHandlerTest, RemoveTextFragments) {
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
  Compositor().BeginFrame();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  GetTextFragmentHandler().RemoveFragments();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

TEST_P(TextFragmentHandlerTest,
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

TEST_P(TextFragmentHandlerTest, ExtractTextFragmentWithNoMatch) {
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

TEST_P(TextFragmentHandlerTest, ExtractTextFragmentWithRange) {
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

TEST_P(TextFragmentHandlerTest, ExtractTextFragmentWithRangeAndContext) {
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

TEST_P(TextFragmentHandlerTest, ExtractFirstTextFragmentRect) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=This,page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=This,page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=device-width">
    <style>p { font: 10px/1 Ahem; }</style>
    <p id="first">This is a test page</p>
    <p id="second">with some more text</p>
  )HTML");
  RunAsyncMatchingTasks();
  LoadAhem();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 19);
  ASSERT_EQ("This is a test page", PlainText(EphemeralRange(start, end)));
  IntRect rect(ComputeTextRect(EphemeralRange(start, end)));
  gfx::Rect expected_rect =
      gfx::Rect(GetDocument().GetFrame()->View()->FrameToViewport(rect));
  // ExtractFirstTextFragmentsRect should return the first matched viewport
  // relative location.
  ASSERT_EQ(expected_rect.ToString(), "8,10 190x10");

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_EQ(expected_rect.ToString(), text_fragment_rect.ToString());
}

TEST_P(TextFragmentHandlerTest, ExtractFirstTextFragmentRectScroll) {
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
  LoadAhem();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 10);
  const auto& end = Position(first_paragraph, 19);
  ASSERT_EQ("test page", PlainText(EphemeralRange(start, end)));
  IntRect rect(ComputeTextRect(EphemeralRange(start, end)));
  gfx::Rect expected_rect =
      gfx::Rect(GetDocument().GetFrame()->View()->FrameToViewport(rect));
  // ExtractFirstTextFragmentsRect should return the first matched scaled
  // viewport relative location since the page is loaded zoomed in 4X
  ASSERT_EQ(expected_rect.ToString(), "432,296 360x44");

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_EQ(expected_rect.ToString(), text_fragment_rect.ToString());
}

TEST_P(TextFragmentHandlerTest, ExtractFirstTextFragmentRectMultipleHighlight) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text");
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
  LoadAhem();

  Compositor().BeginFrame();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 10);
  const auto& end = Position(first_paragraph, 19);
  ASSERT_EQ("test page", PlainText(EphemeralRange(start, end)));
  IntRect rect(ComputeTextRect(EphemeralRange(start, end)));
  gfx::Rect expected_rect =
      gfx::Rect(GetDocument().GetFrame()->View()->FrameToViewport(rect));
  // ExtractFirstTextFragmentsRect should return the first matched viewport
  // relative location.
  ASSERT_EQ(expected_rect.ToString(), "108,10 90x10");

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_EQ(expected_rect.ToString(), text_fragment_rect.ToString());
}

TEST_P(TextFragmentHandlerTest,
       ExtractFirstTextFragmentRectMultipleHighlightWithNoFoundText) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=fake&text=test%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=fake&text=test%20page");
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
  LoadAhem();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 10);
  const auto& end = Position(first_paragraph, 19);
  ASSERT_EQ("test page", PlainText(EphemeralRange(start, end)));
  IntRect rect(ComputeTextRect(EphemeralRange(start, end)));
  gfx::Rect expected_rect =
      gfx::Rect(GetDocument().GetFrame()->View()->FrameToViewport(rect));
  // ExtractFirstTextFragmentsRect should return the first matched viewport
  // relative location.
  ASSERT_EQ(expected_rect.ToString(), "108,10 90x10");

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_EQ(expected_rect.ToString(), text_fragment_rect.ToString());
}

TEST_P(TextFragmentHandlerTest, RejectExtractFirstTextFragmentRect) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=not%20on%20the%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=not%20on%20the%20page");
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
  LoadAhem();

  Compositor().BeginFrame();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  gfx::Rect text_fragment_rect = ExtractFirstTextFragmentsRect();

  EXPECT_TRUE(text_fragment_rect.IsEmpty());
}

// Checks that the selector is preemptively generated.
TEST_P(TextFragmentHandlerTest, CheckPreemptiveGeneration) {
  if (!preemptive_generation_enabled_)
    return;

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph</p>
    )HTML");

  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);
  GetTextFragmentHandler().StartPreemptiveGenerationIfNeeded();

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated", 1);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 0);
}

// When URL is blocklisted, the selector shouldn't be preemptively generated.
TEST_P(TextFragmentHandlerTest, CheckNoPreemptiveGenerationBlocklist) {
  if (!preemptive_generation_enabled_)
    return;

  SimRequest request("https://instagram.com/test.html", "text/html");
  LoadURL("https://instagram.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph</p>
    )HTML");

  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);
  GetTextFragmentHandler().StartPreemptiveGenerationIfNeeded();

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated", 0);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 0);
}

// Check that selector is not generated for editable text.
TEST_P(TextFragmentHandlerTest, CheckNoPreemptiveGenerationEditable) {
  if (!preemptive_generation_enabled_)
    return;

  SimRequest request("https://instagram.com/test.html", "text/html");
  LoadURL("https://instagram.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <input type="text" id="input" value="default text in input">
    )HTML");

  Node* input_text =
      FlatTreeTraversal::Next(*GetDocument().getElementById("input"))
          ->firstChild();
  const auto& selected_start = Position(input_text, 0);
  const auto& selected_end = Position(input_text, 12);
  ASSERT_EQ("default text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);
  GetTextFragmentHandler().StartPreemptiveGenerationIfNeeded();

  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated", 0);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 0);
}

// TODO(crbug.com/1192047): Update the test to better reflect the real repro
// steps. Test case for crash in crbug.com/1190137. When selector is requested
// after callback is set and unused.
TEST_P(TextFragmentHandlerTest, SecondGenerationCrash) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <p id='p'>First paragraph text</p>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* p = GetDocument().getElementById("p");
  const auto& start = Position(p->lastChild(), 0);
  const auto& end = Position(p->lastChild(), 15);
  ASSERT_EQ("First paragraph", PlainText(EphemeralRange(start, end)));
  SetSelection(start, end);

  auto callback = WTF::Bind([](const TextFragmentSelector& selector) {});
  GetDocument()
      .GetFrame()
      ->GetTextFragmentHandler()
      ->GetTextFragmentSelectorGenerator()
      ->SetCallbackForTesting(std::move(callback));

  // This shouldn't crash.
  GetTextFragmentHandler().StartPreemptiveGenerationIfNeeded();
  base::RunLoop().RunUntilIdle();
}

// Verifies metrics for preemptive generation are correctly recorded when the
// selector is successfully generated.
TEST_P(TextFragmentHandlerTest, CheckMetrics_Success) {
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
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("First paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  String selector = SelectThenRequestSelector(selected_start, selected_end);
  EXPECT_EQ(selector, "First%20paragraph%20text%20that%20is");
  VerifyPreemptiveGenerationMetrics(true);
}

// Verifies metrics for preemptive generation are correctly recorded when the
// selector request fails, in this case, because the context limit is reached.
TEST_P(TextFragmentHandlerTest, CheckMetrics_Failure) {
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
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 80);
  const auto& selected_end = Position(first_paragraph, 106);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));
  String selector = SelectThenRequestSelector(selected_start, selected_end);
  EXPECT_EQ(selector, "");
  VerifyPreemptiveGenerationMetrics(false);
}

TEST_P(TextFragmentHandlerTest,
       ShouldCreateTextFragmentHandlerAndRemoveHighlightForIframes) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingAmp);
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

  // Render two frames to handle the async step added by the beforematch event.
  Compositor().BeginFrame();
  BeginEmptyFrame();

  Element* iframe = GetDocument().getElementById("iframe");
  auto* child_frame =
      To<LocalFrame>(To<HTMLFrameOwnerElement>(iframe)->ContentFrame());

  EXPECT_EQ(1u, child_frame->GetDocument()->Markers().Markers().size());
  EXPECT_FALSE(HasTextFragmentHandler(child_frame));

  child_frame->CreateTextFragmentHandler();
  GetTextFragmentHandler().StartPreemptiveGenerationIfNeeded();

  mojo::Remote<mojom::blink::TextFragmentReceiver> remote;
  EXPECT_FALSE(remote.is_bound());
  child_frame->BindTextFragmentReceiver(remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(HasTextFragmentHandler(child_frame));
  EXPECT_TRUE(remote.is_bound());
  remote.get()->RemoveFragments();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, child_frame->GetDocument()->Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(child_frame->GetDocument()->View()->GetFragmentAnchor());
}

TEST_P(TextFragmentHandlerTest,
       ShouldCreateTextFragmentHandlerAndRemoveHighlight) {
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
  Compositor().BeginFrame();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
  EXPECT_TRUE(HasTextFragmentHandler(GetDocument().GetFrame()));

  mojo::Remote<mojom::blink::TextFragmentReceiver> remote;
  EXPECT_FALSE(remote.is_bound());
  GetDocument().GetFrame()->BindTextFragmentReceiver(
      remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(HasTextFragmentHandler(GetDocument().GetFrame()));
  EXPECT_TRUE(remote.is_bound());
  remote.get()->RemoveFragments();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

TEST_P(TextFragmentHandlerTest,
       ShouldCreateTextFragmentHandlerAndRequestSelector) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");

  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("First paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  SetSelection(selected_start, selected_end);

  mojo::Remote<mojom::blink::TextFragmentReceiver> remote;
  EXPECT_TRUE(HasTextFragmentHandler(GetDocument().GetFrame()));
  EXPECT_FALSE(remote.is_bound());

  GetTextFragmentHandler().StartPreemptiveGenerationIfNeeded();
  GetDocument().GetFrame()->BindTextFragmentReceiver(
      remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(HasTextFragmentHandler(GetDocument().GetFrame()));
  EXPECT_TRUE(remote.is_bound());

  bool callback_called = false;
  String selector;
  auto lambda = [](bool& callback_called, String& selector,
                   const String& generated_selector) {
    selector = generated_selector;
    callback_called = true;
  };
  auto callback =
      WTF::Bind(lambda, std::ref(callback_called), std::ref(selector));
  remote->RequestSelector(std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);

  EXPECT_EQ(selector, "First%20paragraph%20text%20that%20is");
  VerifyPreemptiveGenerationMetrics(true);
}

struct PreemptiveLinkGenerationTestPassToString {
  std::string operator()(const testing::TestParamInfo<bool> b) const {
    return b.param ? "Preemptive" : "NonPreemptive";
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         TextFragmentHandlerTest,
                         ::testing::Bool(),
                         PreemptiveLinkGenerationTestPassToString());

}  // namespace

}  // namespace blink
