// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_handler.h"

#include <gtest/gtest.h>

#include "base/test/scoped_feature_list.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using test::RunPendingTasks;

class TextFragmentHandlerTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

  void RunAsyncMatchingTasks() {
    auto* scheduler =
        ThreadScheduler::Current()->GetWebMainThreadSchedulerForTest();
    blink::scheduler::RunIdleTasksForTesting(scheduler,
                                             base::BindOnce([]() {}));
    RunPendingTasks();
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

    GetDocument()
        .GetFrame()
        ->GetTextFragmentHandler()
        ->ExtractTextFragmentsMatches(std::move(callback));

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

    GetDocument()
        .GetFrame()
        ->GetTextFragmentHandler()
        ->ExtractFirstFragmentRect(std::move(callback));

    EXPECT_TRUE(callback_called);
    return text_fragment_rect;
  }

  void LoadAhem() {
    scoped_refptr<SharedBuffer> shared_buffer =
        test::ReadFromFile(test::CoreTestDataPath("Ahem.ttf"));
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    auto* buffer =
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
            DOMArrayBuffer::Create(shared_buffer));
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    StringOrArrayBufferOrArrayBufferView buffer =
        StringOrArrayBufferOrArrayBufferView::FromArrayBuffer(
            DOMArrayBuffer::Create(shared_buffer));
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    FontFace* ahem =
        FontFace::Create(GetDocument().GetExecutionContext(), "Ahem", buffer,
                         FontFaceDescriptors::Create());

    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    DummyExceptionStateForTesting exception_state;
    FontFaceSetDocument::From(GetDocument())
        ->addForBinding(script_state, ahem, exception_state);
  }
};

TEST_F(TextFragmentHandlerTest, RemoveTextFragments) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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

  GetDocument().GetFrame()->GetTextFragmentHandler()->RemoveFragments();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

TEST_F(TextFragmentHandlerTest,
       ExtractTextFragmentWithWithMultipleTextFragments) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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

TEST_F(TextFragmentHandlerTest, ExtractFirstTextFragmentRectScroll) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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

TEST_F(TextFragmentHandlerTest, ExtractFirstTextFragmentRectMultipleHighlight) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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

TEST_F(TextFragmentHandlerTest,
       ExtractFirstTextFragmentRectMultipleHighlightWithNoFoundText) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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

TEST_F(TextFragmentHandlerTest, RejectExtractFirstTextFragmentRect) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(
      shared_highlighting::kSharedHighlightingV2);
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

}  // namespace

}  // namespace blink
