// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/html_canvas_element_module.h"

using testing::Mock;

namespace blink {

class OffscreenCanvasRenderingAPIUkmMetricsTest : public PageTestBase {
 public:
  OffscreenCanvasRenderingAPIUkmMetricsTest();

  void SetUp() override {
    PageTestBase::SetUp();
    InstallTestUkmRecorder();
    GetDocument().documentElement()->setInnerHTML(
        "<body><canvas id='c'></canvas></body>");
    auto* canvas_element =
        To<HTMLCanvasElement>(GetDocument().getElementById("c"));

    DummyExceptionStateForTesting exception_state;
    offscreen_canvas_element_ =
        HTMLCanvasElementModule::transferControlToOffscreen(
            GetDocument().domWindow(), *canvas_element, exception_state);
    UpdateAllLifecyclePhasesForTest();
  }

  void CheckContext(String context_type,
                    CanvasRenderingContext::CanvasRenderingAPI expected_value) {
    CanvasContextCreationAttributesCore attributes;
    offscreen_canvas_element_->GetCanvasRenderingContext(
        GetDocument().domWindow(), context_type, attributes);

    auto entries = test_ukm_recorder_->GetEntriesByName(
        ukm::builders::ClientRenderingAPI::kEntryName);
    EXPECT_EQ(1ul, entries.size());
    auto* entry = entries[0];
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry,
        ukm::builders::ClientRenderingAPI::
            kOffscreenCanvas_RenderingContextName,
        static_cast<int>(expected_value));
  }

 private:
  void InstallTestUkmRecorder() {
    DCHECK(!test_ukm_recorder_);  // Should be installed only once.
    auto temp_recorder = std::make_unique<ukm::TestUkmRecorder>();
    test_ukm_recorder_ = temp_recorder.get();
    GetDocument().ukm_recorder_ = std::move(temp_recorder);
  }

  Persistent<OffscreenCanvas> offscreen_canvas_element_;
  ukm::TestUkmRecorder* test_ukm_recorder_ = nullptr;
};

OffscreenCanvasRenderingAPIUkmMetricsTest::
    OffscreenCanvasRenderingAPIUkmMetricsTest() = default;

TEST_F(OffscreenCanvasRenderingAPIUkmMetricsTest, OffscreenCanvas2D) {
  CheckContext("2d", CanvasRenderingContext::CanvasRenderingAPI::k2D);
}

TEST_F(OffscreenCanvasRenderingAPIUkmMetricsTest,
       OffscreenCanvasBitmapRenderer) {
  CheckContext("bitmaprenderer",
               CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer);
}

// Skip tests for WebGL context for now

}  // namespace blink
