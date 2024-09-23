// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_controller.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pointer_action.h"
#include "content/common/input/synthetic_smooth_move_gesture.h"
#include "content/common/input/synthetic_smooth_scroll_gesture.h"
#include "content/common/input/synthetic_tap_gesture.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<title>Mouse event trace events reported.</title>"
    "<script>"
    "  let i=0;"
    "  document.addEventListener('mousemove', () => {"
    "    var end = performance.now() + 20;"
    "    while(performance.now() < end);"
    "    document.body.style.backgroundColor = 'rgb(' + (i++) + ',0,0)'"
    "  });"
    "  document.addEventListener('wheel', (e) => {"
    "    if (!e.cancelable)"
    "      return;"
    "    var end = performance.now() + 50;"
    "    while(performance.now() < end);"
    "  });"
    "</script>"
    "<style>"
    "body {"
    "  height:3000px;"
    // Prevent text selection logic from triggering, as it makes the test flaky.
    "  user-select: none;"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "</body>"
    "</html>";

}  // namespace

namespace content {

// This class listens for terminated latency info events. It listens
// for both the mouse event ack and the gpu swap buffers event since
// the event could occur in either.
class TracingRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  TracingRenderWidgetHost(FrameTree* frame_tree,
                          RenderWidgetHostDelegate* delegate,
                          viz::FrameSinkId frame_sink_id,
                          base::SafeRef<SiteInstanceGroup> site_instance_group,
                          int32_t routing_id,
                          bool hidden,
                          bool renderer_initiated_creation)
      : RenderWidgetHostImpl(frame_tree,
                             /*self_owned=*/false,
                             frame_sink_id,
                             delegate,
                             std::move(site_instance_group),
                             routing_id,
                             hidden,
                             renderer_initiated_creation,
                             std::make_unique<FrameTokenMessageQueue>()) {}

  void OnMouseEventAck(
      const input::MouseEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override {
    RenderWidgetHostImpl::OnMouseEventAck(event, ack_source, ack_result);
  }
};

class TracingRenderWidgetHostFactory : public RenderWidgetHostFactory {
 public:
  TracingRenderWidgetHostFactory() {
    RenderWidgetHostFactory::RegisterFactory(this);
  }

  TracingRenderWidgetHostFactory(const TracingRenderWidgetHostFactory&) =
      delete;
  TracingRenderWidgetHostFactory& operator=(
      const TracingRenderWidgetHostFactory&) = delete;

  ~TracingRenderWidgetHostFactory() override {
    RenderWidgetHostFactory::UnregisterFactory();
  }

  std::unique_ptr<RenderWidgetHostImpl> CreateRenderWidgetHost(
      FrameTree* frame_tree,
      RenderWidgetHostDelegate* delegate,
      viz::FrameSinkId frame_sink_id,
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t routing_id,
      bool hidden,
      bool renderer_initiated_creation) override {
    return std::make_unique<TracingRenderWidgetHost>(
        frame_tree, delegate, frame_sink_id, std::move(site_instance_group),
        routing_id, hidden, renderer_initiated_creation);
  }
};

class MouseLatencyBrowserTest : public ContentBrowserTest {
 public:
  MouseLatencyBrowserTest() {}

  MouseLatencyBrowserTest(const MouseLatencyBrowserTest&) = delete;
  MouseLatencyBrowserTest& operator=(const MouseLatencyBrowserTest&) = delete;

  ~MouseLatencyBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    runner_->Quit();
  }

  void OnTraceDataCollected(std::unique_ptr<std::string> trace_data_string) {
    trace_data_ = base::test::ParseJson(*trace_data_string);
    runner_->Quit();
  }

 protected:
  void LoadURL() {
    const GURL data_url(kDataURL);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));
  }

  // Generate mouse events for a synthetic click at |point|.
  void DoSyncClick(const gfx::PointF& position) {
    SyntheticTapGestureParams params;
    params.gesture_source_type = content::mojom::GestureSourceType::kMouseInput;
    params.position = position;
    params.duration_ms = 100;
    std::unique_ptr<SyntheticTapGesture> gesture(
        new SyntheticTapGesture(params));

    GetWidgetHost()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(&MouseLatencyBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    runner_ = std::make_unique<base::RunLoop>();
    runner_->Run();
  }

  // Generate mouse events drag from |position|.
  void DoSyncCoalescedMoves(const gfx::PointF position,
                            const gfx::Vector2dF& delta1,
                            const gfx::Vector2dF& delta2) {
    SyntheticSmoothMoveGestureParams params;
    params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
    params.start_point.SetPoint(position.x(), position.y());
    params.distances.push_back(delta1);
    params.distances.push_back(delta2);

    std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
        new SyntheticSmoothMoveGesture(params));

    GetWidgetHost()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(&MouseLatencyBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    runner_ = std::make_unique<base::RunLoop>();
    runner_->Run();
  }

  // Generate mouse wheel scroll.
  void DoSyncCoalescedMouseWheel(const gfx::PointF position,
                                 const gfx::Vector2dF& delta) {
    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = content::mojom::GestureSourceType::kMouseInput;
    params.anchor = position;
    params.distances.push_back(delta);

    GetWidgetHost()->QueueSyntheticGesture(
        std::make_unique<SyntheticSmoothScrollGesture>(params),
        base::BindOnce(&MouseLatencyBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    runner_ = std::make_unique<base::RunLoop>();
    runner_->Run();
  }

  void StartTracing() {
    base::trace_event::TraceConfig trace_config(
        "{"
        "\"enable_argument_filter\":false,"
        "\"enable_systrace\":false,"
        "\"included_categories\":["
        "\"latencyInfo\""
        "],"
        "\"record_mode\":\"record-until-full\""
        "}");

    base::RunLoop run_loop;
    ASSERT_TRUE(TracingController::GetInstance()->StartTracing(
        trace_config, run_loop.QuitClosure()));
    run_loop.Run();
  }

  const base::Value& StopTracing() {
    bool success = TracingController::GetInstance()->StopTracing(
        TracingController::CreateStringEndpoint(
            base::BindOnce(&MouseLatencyBrowserTest::OnTraceDataCollected,
                           base::Unretained(this))));
    EXPECT_TRUE(success);

    // Runs until we get the OnTraceDataCollected callback, which populates
    // trace_data_;
    runner_ = std::make_unique<base::RunLoop>();
    runner_->Run();
    return trace_data_;
  }

  std::string ShowTraceEventsWithId(const std::string& id_to_show,
                                    const base::Value::List* traceEvents) {
    std::stringstream stream;
    for (const base::Value& traceEvent_value : *traceEvents) {
      if (!traceEvent_value.is_dict())
        continue;
      const base::Value::Dict& traceEvent = traceEvent_value.GetDict();

      const std::string* id = traceEvent.FindString("id");
      if (!id)
        continue;

      if (*id == id_to_show)
        stream << traceEvent;
    }
    return stream.str();
  }

  void AssertTraceIdsBeginAndEnd(const base::Value& trace_data,
                                 const std::string& trace_event_name) {
    const base::Value::Dict* trace_data_dict = trace_data.GetIfDict();
    ASSERT_TRUE(trace_data_dict);

    const base::Value::List* traceEvents =
        trace_data_dict->FindList("traceEvents");
    ASSERT_TRUE(traceEvents);

    std::map<std::string, int> trace_ids;

    for (const base::Value& traceEvent_value : *traceEvents) {
      ASSERT_TRUE(traceEvent_value.is_dict());
      const base::Value::Dict& traceEvent = traceEvent_value.GetDict();

      const std::string* name = traceEvent.FindString("name");
      ASSERT_TRUE(name);

      if (*name != trace_event_name)
        continue;

      const std::string* id = traceEvent.FindString("id");
      if (id)
        ++trace_ids[*id];
    }

    for (auto i : trace_ids) {
      // Each trace id should show up once for the begin, and once for the end.
      EXPECT_EQ(2, i.second) << ShowTraceEventsWithId(i.first, traceEvents);
    }
  }

 private:
  std::unique_ptr<base::RunLoop> runner_;
  base::Value trace_data_;
  TracingRenderWidgetHostFactory widget_factory_;
};

// Ensures that LatencyInfo async slices are reported correctly for MouseUp and
// MouseDown events in the case where no swap is generated.
// Disabled on Android because we don't support synthetic mouse input on
// Android (crbug.com/723618).
// Disabled on due to flakiness (https://crbug.com/800303, https://crbug.com/815363).
IN_PROC_BROWSER_TEST_F(MouseLatencyBrowserTest,
                       DISABLED_MouseDownAndUpRecordedWithoutSwap) {
  LoadURL();

  auto filter = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::Type::kMouseUp);
  StartTracing();
  DoSyncClick(gfx::PointF(100, 100));
  EXPECT_EQ(blink::mojom::InputEventResultState::kNotConsumed,
            filter->GetAckStateWaitIfNecessary());
  const base::Value& trace_data = StopTracing();

  const base::Value::Dict* trace_data_dict = trace_data.GetIfDict();
  ASSERT_TRUE(trace_data_dict);

  const base::Value::List* traceEvents =
      trace_data_dict->FindList("traceEvents");
  ASSERT_TRUE(traceEvents);

  std::vector<std::string> trace_event_names;

  for (const base::Value& traceEvent_value : *traceEvents) {
    ASSERT_TRUE(traceEvent_value.is_dict());
    const base::Value::Dict& traceEvent = traceEvent_value.GetDict();

    const std::string* name = traceEvent.FindString("name");
    ASSERT_TRUE(name);

    if (*name != "InputLatency::MouseUp" && *name != "InputLatency::MouseDown")
      continue;
    trace_event_names.push_back(*name);
  }

  // We see two events per async slice, a begin and an end.
  EXPECT_THAT(trace_event_names,
              testing::UnorderedElementsAre(
                  "InputLatency::MouseDown", "InputLatency::MouseDown",
                  "InputLatency::MouseUp", "InputLatency::MouseUp"));
}

// Ensures that LatencyInfo async slices are reported correctly for MouseMove
// events in the case where events are coalesced. (crbug.com/771165).
// Disabled on Android because we don't support synthetic mouse input on Android
// (crbug.com/723618).
// http://crbug.com/801629 : Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(MouseLatencyBrowserTest,
                       DISABLED_CoalescedMouseMovesCorrectlyTerminated) {
  LoadURL();

  StartTracing();
  DoSyncCoalescedMoves(gfx::PointF(100, 100), gfx::Vector2dF(150, 150),
                       gfx::Vector2dF(250, 250));
  // The following wait is the upper bound for gpu swap completed callback. It
  // is two frames to account for double buffering.
  MainThreadFrameObserver observer(GetWidgetHost());
  observer.Wait();
  observer.Wait();

  const base::Value& trace_data = StopTracing();

  AssertTraceIdsBeginAndEnd(trace_data, "InputLatency::MouseMove");
}

// TODO(crbug.com/41436535): This is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(MouseLatencyBrowserTest,
                       DISABLED_CoalescedMouseWheelsCorrectlyTerminated) {
  LoadURL();

  StartTracing();
  DoSyncCoalescedMouseWheel(gfx::PointF(100, 100), gfx::Vector2dF(0, -100));
  const base::Value& trace_data = StopTracing();

  AssertTraceIdsBeginAndEnd(trace_data, "InputLatency::MouseWheel");
}

}  // namespace content
