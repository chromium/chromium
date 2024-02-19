// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/mem_buffer_util.h"
#include "base/run_loop.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFaviconPageUrl[] = "/favicon.html";

void QuitLoopIfFaviconUpdated(
    base::RepeatingClosure quit_run_loop_closure,
    const fuchsia::web::NavigationState& change,
    fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
        ack_callback) {
  if (change.has_favicon())
    quit_run_loop_closure.Run();
  ack_callback();
}

void RunUntilFaviconUpdated(TestNavigationListener* test_navigation_listener) {
  base::RunLoop run_loop;
  test_navigation_listener->SetBeforeAckHook(
      base::BindRepeating(&QuitLoopIfFaviconUpdated, run_loop.QuitClosure()));
  run_loop.Run();
}

void ValidateFavicon(const fuchsia::web::Favicon& favicon,
                     size_t expected_width,
                     size_t expected_height,
                     size_t check_point_x,
                     size_t check_point_y,
                     uint32_t expected_color) {
  ASSERT_TRUE(favicon.has_width());
  EXPECT_EQ(favicon.width(), expected_width);
  ASSERT_TRUE(favicon.has_height());
  EXPECT_EQ(favicon.height(), expected_height);
  ASSERT_TRUE(favicon.has_data());
  std::optional<std::string> data = base::StringFromMemBuffer(favicon.data());
  ASSERT_TRUE(data.has_value());
  size_t expected_size = expected_width * expected_height * sizeof(uint32_t);
  ASSERT_EQ(data->size(), expected_size);
  size_t offset = check_point_x + check_point_y * expected_width;
  uint32_t color = reinterpret_cast<const uint32_t*>(data->data())[offset];
  EXPECT_EQ(color, expected_color);
}

}  // namespace

class FaviconTest : public WebEngineBrowserTest {
 public:
  FaviconTest() { set_test_server_root(base::FilePath(kTestServerRoot)); }
  ~FaviconTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    WebEngineBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    frame_ = FrameForTest::Create(context(), {});
  }

  void TearDownOnMainThread() override {
    frame_ = {};

    WebEngineBrowserTest::TearDownOnMainThread();
  }

  FrameForTest frame_;
};

// Verify that favicons are not loaded by default.
IN_PROC_BROWSER_TEST_F(FaviconTest, Disabled) {
  GURL url = embedded_test_server()->GetURL(kFaviconPageUrl);
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(), {},
                                       url.spec()));

  frame_.navigation_listener().RunUntilUrlAndTitleEquals(url, "Favicon");

  // Favicon should not be sent.
  EXPECT_FALSE(frame_.navigation_listener().current_state()->has_favicon());
}

// Check that the favicon for the page is sent after the page is loaded. Also
// verify that the icon is reloaded when the page changes.
IN_PROC_BROWSER_TEST_F(FaviconTest, LoadAndUpdate) {
  frame_.CreateAndAttachNavigationListener(
      fuchsia::web::NavigationEventListenerFlags::FAVICON);

  GURL url = embedded_test_server()->GetURL(kFaviconPageUrl);
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(), {},
                                       url.spec()));

  // An empty favicon should be sent first.
  RunUntilFaviconUpdated(&frame_.navigation_listener());
  EXPECT_TRUE(
      frame_.navigation_listener().current_state()->favicon().IsEmpty());

  // The image is sent later.
  RunUntilFaviconUpdated(&frame_.navigation_listener());
  ValidateFavicon(frame_.navigation_listener().current_state()->favicon(), 16,
                  16, 7, 4, 0xA6272536);
  EXPECT_EQ(frame_.navigation_listener().current_state()->url(), url.spec());
  EXPECT_EQ(frame_.navigation_listener().current_state()->title(), "Favicon");

  // Update the icon from the page and verify that it's updated as expected.
  ExecuteJavaScript(frame_.get(),
                    "document.getElementById('favicon').href = 'favicon2.png'");
  RunUntilFaviconUpdated(&frame_.navigation_listener());
  ValidateFavicon(frame_.navigation_listener().current_state()->favicon(), 16,
                  16, 12, 7, 0xB5A39C1A);

  // URL and Title should not change when the favicon is loaded.
  EXPECT_FALSE(frame_.navigation_listener().last_changes()->has_url());
  EXPECT_FALSE(frame_.navigation_listener().last_changes()->has_title());
}

// Check that the favicon updates after a navigation.
IN_PROC_BROWSER_TEST_F(FaviconTest, FaviconNavigations) {
  frame_.CreateAndAttachNavigationListener(
      fuchsia::web::NavigationEventListenerFlags::FAVICON);

  GURL url = embedded_test_server()->GetURL(kFaviconPageUrl);
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(), {},
                                       url.spec()));

  // An empty favicon should be sent first.
  RunUntilFaviconUpdated(&frame_.navigation_listener());
  EXPECT_TRUE(
      frame_.navigation_listener().current_state()->favicon().IsEmpty());

  // The image is sent later.
  RunUntilFaviconUpdated(&frame_.navigation_listener());
  ValidateFavicon(frame_.navigation_listener().current_state()->favicon(), 16,
                  16, 7, 4, 0xA6272536);

  // Reload the same page with a different query string.
  url = embedded_test_server()->GetURL(std::string(kFaviconPageUrl) +
                                       "?favicon=favicon2.png");
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(), {},
                                       url.spec()));

  // An empty icon should be sent when navigating to a new page.
  frame_.navigation_listener().RunUntilUrlEquals(url);
  EXPECT_TRUE(
      frame_.navigation_listener().current_state()->favicon().IsEmpty());

  // The favicon is sent later.
  RunUntilFaviconUpdated(&frame_.navigation_listener());
  ValidateFavicon(frame_.navigation_listener().current_state()->favicon(), 16,
                  16, 12, 7, 0xB5A39C1A);
}
