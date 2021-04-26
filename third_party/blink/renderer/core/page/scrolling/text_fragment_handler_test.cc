// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_handler.h"

#include <gtest/gtest.h>

#include "base/test/scoped_feature_list.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
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

}  // namespace

}  // namespace blink
