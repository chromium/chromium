// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

constexpr int kLaps = 5000;
constexpr int kWarmupLaps = 5;
constexpr char kMetricCallsPerSecondRunsPerS[] = "calls_per_second";
constexpr float kSvgWidth = 800.0f;
constexpr float kSvgHeight = 600.0f;

class SvgHitTestPerfTest : public RenderingTest {
 public:
  perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
    perf_test::PerfResultReporter reporter("SvgHitTestPerfTest.", story);
    reporter.RegisterImportantMetric(kMetricCallsPerSecondRunsPerS, "runs/s");
    return reporter;
  }

  void SetupSvgHitTest() {
    constexpr size_t num_rows = 10;
    constexpr size_t num_columns = 10;
    constexpr size_t num_group_nodes_per_leaf = 100;
    constexpr float row_stride = kSvgHeight / num_rows;
    constexpr float column_stride = kSvgWidth / num_rows;

    WTF::StringBuilder html;
    html.AppendFormat(
        "<svg width='%.2f' height='%.2f' viewBox='0 0 %.2f %.2f' "
        "style='cursor: auto;'>",
        kSvgWidth, kSvgHeight, kSvgWidth, kSvgHeight);

    for (size_t row = 0; row < num_rows; ++row) {
      for (size_t column = 0; column < num_columns; ++column) {
        for (size_t i = 0; i < num_group_nodes_per_leaf; ++i)
          html.Append("<g>");

        html.AppendFormat(
            "<rect id='leaf_%zu' x='%.2f' y='%.2f' width='%.2f' height='%.2f' "
            "/>",
            column * row, column * column_stride, row * row_stride,
            column_stride, row_stride);

        for (size_t i = 0; i < num_group_nodes_per_leaf; ++i)
          html.Append("</g>");
      }
    }

    html.Append("</svg>");

    SetBodyInnerHTML(html.ToString());
  }

  void RunTest(const std::string& story,
               base::RepeatingCallback<void()> test_case) {
    base::LapTimer timer(kWarmupLaps, base::TimeDelta(), kLaps);
    for (int i = 0; i < kLaps + kWarmupLaps; ++i) {
      test_case.Run();
      timer.NextLap();
    }

    auto reporter = SetUpReporter(story);
    reporter.AddResult(kMetricCallsPerSecondRunsPerS, timer.LapsPerSecond());
  }
};

}  // namespace

TEST_F(SvgHitTestPerfTest, HandleMouseMoveEvent) {
  SetupSvgHitTest();

  EventHandler& event_handler = GetDocument().GetFrame()->GetEventHandler();

  RunTest("HandleMouseMoveEvent",
          WTF::BindRepeating(
              [](EventHandler* event_handler) {
                WebMouseEvent mouse_move_event(
                    WebMouseEvent::Type::kMouseMove, gfx::PointF(1, 1),
                    gfx::PointF(1, 1), WebPointerProperties::Button::kNoButton,
                    0, WebInputEvent::Modifiers::kNoModifiers,
                    WebInputEvent::GetStaticTimeStampForTests());
                mouse_move_event.SetFrameScale(1);
                event_handler->HandleMouseMoveEvent(mouse_move_event,
                                                    Vector<WebMouseEvent>(),
                                                    Vector<WebMouseEvent>());
              },
              WrapWeakPersistent(&event_handler)));
}

TEST_F(SvgHitTestPerfTest, IntersectsClipPath) {
  SetupSvgHitTest();
  LayoutObject* leaf_0_layout_object = GetLayoutObjectByElementId("leaf_0");
  ASSERT_NE(leaf_0_layout_object, nullptr);
  LayoutObject* container = leaf_0_layout_object->Parent();

  PhysicalOffset document_point =
      event_handling_util::ContentPointFromRootFrame(GetDocument().GetFrame(),
                                                     gfx::PointF(1, 1));

  TransformedHitTestLocation local_location(
      HitTestLocation(document_point), container->LocalToSVGParentTransform());
  ASSERT_TRUE(local_location);

  RunTest("IntersectsClipPath",
          WTF::BindRepeating(
              [](const LayoutObject* container,
                 TransformedHitTestLocation& local_location) {
                container->HasClipPath() &&
                    ClipPathClipper::HitTest(*container, *local_location);
              },
              WrapPersistent(container), std::ref(local_location)));
}

}  // namespace blink
