// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"

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
  WheelEventListenerBrowserTest() = default;

  WheelEventListenerBrowserTest(const WheelEventListenerBrowserTest&) = delete;
  WheelEventListenerBrowserTest& operator=(
      const WheelEventListenerBrowserTest&) = delete;

  ~WheelEventListenerBrowserTest() override = default;

 protected:
  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  void LoadURL(const std::string& page_data) {
    const GURL data_url("data:text/html," + page_data);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    std::u16string ready_title(u"ready");
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    std::ignore = watcher.WaitAndGetTitle();

    MainThreadFrameObserver main_thread_sync(host);
    main_thread_sync.Wait();
  }

  void ScrollByMouseWheel() {
    // Send a wheel event and wait for its ack.
    auto wheel_msg_watcher = std::make_unique<InputMsgWatcher>(
        GetWidgetHost(), blink::WebInputEvent::Type::kMouseWheel);
    double x = 10;
    double y = 10;
    blink::WebMouseWheelEvent wheel_event =
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            x, y, x, y, -20, -20, 0,
            ui::ScrollGranularity::kScrollByPrecisePixel);
    wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
    GetWidgetHost()->ForwardWheelEvent(wheel_event);
    EXPECT_EQ(blink::mojom::InputEventResultState::kSetNonBlocking,
              wheel_msg_watcher->WaitForAck());
  }

  void WaitForScroll() {
    RenderFrameSubmissionObserver observer(
        GetWidgetHost()->render_frame_metadata_provider());
    gfx::PointF default_scroll_offset;
    while (observer.LastRenderFrameMetadata()
               .root_scroll_offset.value_or(default_scroll_offset)
               .y() <= 0) {
      observer.WaitForMetadataChange();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
