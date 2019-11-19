// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/platform/web_input_event.h"

using blink::WebInputEvent;

namespace {

const std::string kWheelEventListenerDataURL = R"HTML(
  <!DOCTYPE html>
  <meta name='viewport' content='width=device-width'/>
  <style>
  html, body {
    margin: 0;
  }
  .spacer { height: 10000px; }
  </style>
  <div class=spacer></div>
  <script>
    window.addEventListener('wheel', () => { while (true); });
    document.title='ready';
  </script>)HTML";

const std::string kMouseWheelEventListenerDataURL = R"HTML(
  <!DOCTYPE html>
  <meta name='viewport' content='width=device-width'/>
  <style>
  html, body {
    margin: 0;
  }
  .spacer { height: 10000px; }
  </style>
  <div class=spacer></div>
  <script>
    document.addEventListener('mousewheel', () => { while (true); });
    document.title='ready';
  </script>)HTML";
}  // namespace

namespace content {

class WheelEventListenerBrowserTest : public ContentBrowserTest {
 public:
  WheelEventListenerBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kPassiveDocumentWheelEventListeners}, {});
  }
  ~WheelEventListenerBrowserTest() override {}

 protected:
  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void LoadURL(const std::string& page_data) {
    const GURL data_url("data:text/html," + page_data);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());

    MainThreadFrameObserver main_thread_sync(host);
    main_thread_sync.Wait();
  }

  void ScrollByMouseWheel() {
    // Send a wheel event and wait for its ack.
    auto wheel_msg_watcher = std::make_unique<InputMsgWatcher>(
        GetWidgetHost(), blink::WebInputEvent::kMouseWheel);
    double x = 10;
    double y = 10;
    blink::WebMouseWheelEvent wheel_event =
        SyntheticWebMouseWheelEventBuilder::Build(
            x, y, x, y, -20, -20, 0,
            ui::input_types::ScrollGranularity::kScrollByPrecisePixel);
    wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
    GetWidgetHost()->ForwardWheelEvent(wheel_event);
    EXPECT_EQ(INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING,
              wheel_msg_watcher->WaitForAck());
  }

  void WaitForScroll() {
    RenderFrameSubmissionObserver observer(
        GetWidgetHost()->render_frame_metadata_provider());
    gfx::Vector2dF default_scroll_offset;
    while (observer.LastRenderFrameMetadata()
               .root_scroll_offset.value_or(default_scroll_offset)
               .y() <= 0) {
      observer.WaitForMetadataChange();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(WheelEventListenerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WheelEventListenerBrowserTest,
                       DocumentWheelEventListenersPassiveByDefault) {
  LoadURL(kWheelEventListenerDataURL);

  // Send a wheel event and wait for its ack.
  ScrollByMouseWheel();

  // Wait for the page to scroll, the test will timeout if the wheel event
  // listener added to window is not treated as passive.
  WaitForScroll();
}

IN_PROC_BROWSER_TEST_F(WheelEventListenerBrowserTest,
                       DocumentMouseWheelEventListenersPassiveByDefault) {
  LoadURL(kMouseWheelEventListenerDataURL);

  // Send a wheel event and wait for its ack.
  ScrollByMouseWheel();

  // Wait for the page to scroll, the test will timeout if the mousewheel event
  // listener added to document is not treated as passive.
  WaitForScroll();
}

}  // namespace content
