// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CanvasRenderingAPIUkmMetricsTest : public PageTestBase {
 public:
  CanvasRenderingAPIUkmMetricsTest();

  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().documentElement()->setInnerHTML(
        "<body><canvas id='c'></canvas></body>");
    canvas_element_ =
        To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
    UpdateAllLifecyclePhasesForTest();
  }

  void CheckContext(String context_type,
                    CanvasRenderingContext::CanvasRenderingAPI expected_value) {
    CanvasContextCreationAttributesCore attributes;
    canvas_element_->GetCanvasRenderingContext(context_type, attributes);

    auto entries = recorder_.GetEntriesByName(
        ukm::builders::ClientRenderingAPI::kEntryName);
    EXPECT_EQ(1ul, entries.size());
    auto* entry = entries[0].get();
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, ukm::builders::ClientRenderingAPI::kCanvas_RenderingContextName,
        static_cast<int>(expected_value));
  }

 private:
  ukm::TestAutoSetUkmRecorder recorder_;
  Persistent<HTMLCanvasElement> canvas_element_;
};

CanvasRenderingAPIUkmMetricsTest::CanvasRenderingAPIUkmMetricsTest() = default;

TEST_F(CanvasRenderingAPIUkmMetricsTest, Canvas2D) {
  CheckContext("2d", CanvasRenderingContext::CanvasRenderingAPI::k2D);
}

TEST_F(CanvasRenderingAPIUkmMetricsTest, CanvasBitmapRenderer) {
  CheckContext("bitmaprenderer",
               CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer);
}

// Skip tests for WebGL context for now

}  // namespace blink
