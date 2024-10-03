// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
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
    GetDocument().documentElement()->setInnerHTML(
        "<body><canvas id='c'></canvas></body>");
    auto* canvas_element =
        To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));

    DummyExceptionStateForTesting exception_state;
    offscreen_canvas_element_ =
        HTMLCanvasElementModule::transferControlToOffscreen(
            ToScriptStateForMainWorld(GetDocument().GetFrame()),
            *canvas_element, exception_state);
    UpdateAllLifecyclePhasesForTest();
  }

  void CheckContext(CanvasRenderingContext::CanvasRenderingAPI context_type) {
    CanvasContextCreationAttributesCore attributes;
    offscreen_canvas_element_->GetCanvasRenderingContext(
        GetDocument().domWindow(), context_type, attributes);

    auto entries = recorder_.GetEntriesByName(
        ukm::builders::ClientRenderingAPI::kEntryName);
    EXPECT_EQ(1ul, entries.size());
    auto* entry = entries[0].get();
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry,
        ukm::builders::ClientRenderingAPI::
            kOffscreenCanvas_RenderingContextName,
        static_cast<int>(context_type));
  }

 private:
  Persistent<OffscreenCanvas> offscreen_canvas_element_;
  ukm::TestAutoSetUkmRecorder recorder_;
};

OffscreenCanvasRenderingAPIUkmMetricsTest::
    OffscreenCanvasRenderingAPIUkmMetricsTest() = default;

TEST_F(OffscreenCanvasRenderingAPIUkmMetricsTest, OffscreenCanvas2D) {
  CheckContext(CanvasRenderingContext::CanvasRenderingAPI::k2D);
}

TEST_F(OffscreenCanvasRenderingAPIUkmMetricsTest,
       OffscreenCanvasBitmapRenderer) {
  CheckContext(CanvasRenderingContext::CanvasRenderingAPI::kBitmaprenderer);
}

// Skip tests for WebGL context for now

}  // namespace blink
