// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "cc/base/switches.h"
#include "content/common/input/input_event.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

class SyntheticInputTest : public ContentBrowserTest {
 public:
  SyntheticInputTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(cc::switches::kEnableGpuBenchmarking);
  }

  RenderWidgetHost* GetRenderWidgetHost() const {
    RenderWidgetHost* const rwh = shell()
                                      ->web_contents()
                                      ->GetRenderWidgetHostView()
                                      ->GetRenderWidgetHost();
    CHECK(rwh);
    return rwh;
  }
};

class GestureScrollObserver : public RenderWidgetHost::InputEventObserver {
 public:
  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (event.GetType() == blink::WebInputEvent::kGestureScrollBegin)
      gesture_scroll_seen_ = true;
  }
  bool HasSeenGestureScrollBegin() const { return gesture_scroll_seen_; }
  bool gesture_scroll_seen_ = false;
};

// This test checks that we destroying a render widget host with an ongoing
// gesture doesn't cause lifetime issues. Namely, that the gesture
// CompletionCallback isn't destroyed before being called or the Mojo pipe
// being closed.
IN_PROC_BROWSER_TEST_F(SyntheticInputTest, DestroyWidgetWithOngoingGesture) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  WaitForLoadStop(shell()->web_contents());

  GestureScrollObserver gesture_observer;

  GetRenderWidgetHost()->AddInputEventObserver(&gesture_observer);

  // By starting a gesture, there's a Mojo callback that the renderer is
  // waiting on the browser to resolve. If the browser is shutdown before
  // ACKing the callback or closing the channel, we'll DCHECK.
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     "chrome.gpuBenchmarking.smoothScrollBy(10000, ()=>{}, "
                     "100, 100, chrome.gpuBenchmarking.TOUCH_INPUT);"));

  while (!gesture_observer.HasSeenGestureScrollBegin()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  shell()->Close();
}

}  // namespace content
