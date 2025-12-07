// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

class SchedulerPolicyTest : public SimTest {
 protected:
  SchedulerPolicyTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetSettings()->SetJavaScriptEnabled(true);

    // An invalid surface ID causes main frames to be deferred, so initialize
    // this here to avoid this from preventing task deferral policies.
    allocator_.GenerateId();
    Compositor().LayerTreeHost()->SetLocalSurfaceIdFromParent(
        allocator_.GetCurrentLocalSurfaceId());
  }

  int32_t ExtractJsIntValue(const char* expr) {
    v8::HandleScope scope(Window().GetIsolate());
    v8::Local<v8::Value> result =
        ClassicScript::CreateUnspecifiedScript(expr)
            ->RunScriptAndReturnValue(GetDocument().domWindow())
            .GetSuccessValueOrEmpty();
    NonThrowableExceptionState exceptionState;
    return ToInt32(scope.GetIsolate(), result,
                   IntegerConversionConfiguration::kNormalConversion,
                   exceptionState);
  }

  void DispatchKeyEvent() {
    WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::GetStaticTimeStampForTests());
    key_event.dom_key = ui::DomKey::FromCharacter('a');
    key_event.windows_key_code = VKEY_A;
    GetWebFrameWidget().DispatchThroughCcInputHandler(key_event);
  }

  // Scheduling policy changes related to main frames happen in OnTaskCompleted,
  // so run BeginFrame in a task to better simulate BeginMainFrame.
  void ScheduleBeginFrame() {
    Window().GetFrame()->GetFrameScheduler()->CompositorTaskRunner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([&]() { Compositor().BeginFrame(); }));
  }

  bool IsDeferringCommits() {
    return Compositor().LayerTreeHost()->IsDeferringCommits();
  }

  bool IsRenderingPaused() {
    return Compositor().LayerTreeHost()->IsRenderingPaused();
  }

  bool MainFrameUpdatesAreDeferred() {
    // This considers more than `SimCompositor::DeferMainFrameUpdate()`,
    // specifically whether there's a valid surface ID.
    return Compositor().LayerTreeHost()->MainFrameUpdatesAreDeferred();
  }

 private:
  viz::ParentLocalSurfaceIdAllocator allocator_;
};

class DeferRendererTasksAfterInputTest : public SchedulerPolicyTest {};

namespace {
// Initial page contents for DeferRendererTasksAfterInput tests. This installs
// a script that installs a keydown handler that:
//   1. Increments an inputCount counter.
//   2. Forces a SetNeedsAnimate via requestAnimationFrame. The scheduling
//      policy requires a frame-was-requested signal for the policy to take
//      effect. This is not technically needed because the default event
//      handlers typically result in requesting a frame, but we don't want to
//      rely on that.
//   3. Schedules a deferrable task that updates the taskCount counter. If the
//      policy takes effect, this task will be deferred until after the next
//      main frame runs.
const char* kDeferRendererTasksInitialPageContents = R"HTML(
    <!DOCTYPE html>
    <title>Test Page</title>
    <script>
      window.taskCount = 0;
      window.inputCount = 0;

      window.addEventListener('keydown', () => {
        ++window.inputCount;
        requestAnimationFrame(() => {});
        scheduler.postTask(() => {
          ++window.taskCount;
        });
      });
    </script>
  )HTML";
}  // namespace

TEST_F(DeferRendererTasksAfterInputTest, TasksNotDeferredDuringEarlyLoading) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Write(kDeferRendererTasksInitialPageContents);

  // Main frames should be deferred because the head is still being processed.
  // This should prevent tasks from being deferred due to input (see also
  // crbug.com/451389811).
  EXPECT_TRUE(MainFrameUpdatesAreDeferred());
  EXPECT_FALSE(IsDeferringCommits());

  DispatchKeyEvent();
  task_environment().RunUntilIdle();

  EXPECT_EQ(ExtractJsIntValue("window.inputCount;"), 1);
  EXPECT_EQ(ExtractJsIntValue("window.taskCount;"), 1);

  // The <p> should cause unblock main frame updates.
  main_resource.Write("<p>Contents</p>");
  EXPECT_FALSE(MainFrameUpdatesAreDeferred());
  // ...but commits are deferred because of paint holding, which also triggers
  // input suppression. So even though tasks would be deferred, the input is
  // dropped and the counts remain the same.
  EXPECT_TRUE(IsDeferringCommits());
  DispatchKeyEvent();
  task_environment().RunUntilIdle();

  EXPECT_EQ(ExtractJsIntValue("window.inputCount;"), 1);
  EXPECT_EQ(ExtractJsIntValue("window.taskCount;"), 1);

  main_resource.Finish();
}

TEST_F(DeferRendererTasksAfterInputTest, TasksNotDeferredWhileRenderingPaused) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Write(kDeferRendererTasksInitialPageContents);

  // The <p> should cause unblock main frame updates.
  main_resource.Write("<p>Contents</p>");
  EXPECT_FALSE(MainFrameUpdatesAreDeferred());

  // Force a main frame to get past FCP paint holding.
  EXPECT_TRUE(IsDeferringCommits());
  ScheduleBeginFrame();
  task_environment().RunUntilIdle();
  EXPECT_FALSE(IsDeferringCommits());
  EXPECT_FALSE(MainFrameUpdatesAreDeferred());

  // Pausing rendering should prevent task deferral.
  EXPECT_FALSE(IsRenderingPaused());
  std::unique_ptr<cc::ScopedPauseRendering> scoped_pauser =
      Compositor().LayerTreeHost()->PauseRendering();
  EXPECT_TRUE(IsRenderingPaused());
  DispatchKeyEvent();
  task_environment().RunUntilIdle();
  EXPECT_EQ(ExtractJsIntValue("window.inputCount;"), 1);
  EXPECT_EQ(ExtractJsIntValue("window.taskCount;"), 1);

  main_resource.Finish();
}

TEST_F(DeferRendererTasksAfterInputTest, TasksDeferredWhenMainFramesExpected) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Write(kDeferRendererTasksInitialPageContents);

  // The <p> should cause unblock main frame updates.
  main_resource.Write("<p>Contents</p>");
  EXPECT_FALSE(MainFrameUpdatesAreDeferred());

  // Force a main frame to get past FCP paint holding.
  EXPECT_TRUE(IsDeferringCommits());
  ScheduleBeginFrame();
  task_environment().RunUntilIdle();
  EXPECT_FALSE(IsDeferringCommits());
  EXPECT_FALSE(MainFrameUpdatesAreDeferred());

  // Dispatching the key event should now cause tasks to be deferred until the
  // subsequent frame.
  DispatchKeyEvent();
  task_environment().RunUntilIdle();
  EXPECT_EQ(ExtractJsIntValue("window.inputCount;"), 1);
  EXPECT_EQ(ExtractJsIntValue("window.taskCount;"), 0);

  ScheduleBeginFrame();
  task_environment().RunUntilIdle();
  EXPECT_EQ(ExtractJsIntValue("window.inputCount;"), 1);
  EXPECT_EQ(ExtractJsIntValue("window.taskCount;"), 1);

  main_resource.Finish();
}

}  // namespace blink
