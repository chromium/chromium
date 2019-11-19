// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace content {

namespace {

static const char kCanvasPageString[] =
    "<body>"
    "  <canvas id=\"canvas\" width=\"64\" height=\"64\""
    "    style=\"position:absolute;top:0px;left:0px;width:100%;"
    "    height=100%;margin:0;padding:0;\">"
    "  </canvas>"
    "  <script>"
    "    window.ctx = document.getElementById(\"canvas\").getContext(\"2d\");"
    "    function fillWithColor(color) {"
    "      ctx.fillStyle = color;"
    "      ctx.fillRect(0, 0, 64, 64);"
    "      window.domAutomationController.send(color);"
    "    }"
    "    var offset = 150;"
    "    function openNewWindow() {"
    "      window.open(\"/test\", \"\", "
    "          \"top=\"+offset+\",left=\"+offset+\",width=200,height=200\");"
    "      offset += 50;"
    "      window.domAutomationController.send(true);"
    "    }"
    "    window.document.title = \"Ready\";"
    "  </script>"
    "</body>";
}

class SnapshotBrowserTest : public ContentBrowserTest {
 public:
  SnapshotBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Use a smaller browser window to speed up the snapshots.
    command_line->AppendSwitchASCII(::switches::kContentShellHostWindowSize,
                                    "200x200");
  }

  void SetUp() override {
    // These tests rely on the harness producing pixel output.
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  content::WebContentsImpl* GetWebContents(Shell* browser) {
    return static_cast<content::WebContentsImpl*>(browser->web_contents());
  }

  content::RenderWidgetHostImpl* GetRenderWidgetHostImpl(Shell* browser) {
    return GetWebContents(browser)->GetRenderViewHost()->GetWidget();
  }

  void SetupTestServer() {
    // Use an embedded test server so we can open multiple windows in
    // the same renderer process, all referring to the same origin.
    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &SnapshotBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/test")));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);
    if (absolute_url.path() != "/test")
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(kCanvasPageString);
    http_response->set_content_type("text/html");
    return http_response;
  }

  void WaitForAllWindowsToBeReady() {
    const base::string16 expected_title = base::UTF8ToUTF16("Ready");
    // The subordinate windows may load asynchronously. Wait for all of
    // them to execute their script before proceeding.
    auto browser_list = Shell::windows();
    for (Shell* browser : browser_list) {
      TitleWatcher watcher(GetWebContents(browser), expected_title);
      const base::string16& actual_title = watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
    }
  }

  struct ExpectedColor {
    ExpectedColor() : r(0), g(0), b(0) {}
    bool operator==(const ExpectedColor& other) const {
      return (r == other.r && g == other.g && b == other.b);
    }
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  void PickRandomColor(ExpectedColor* expected) {
    expected->r = static_cast<uint8_t>(base::RandInt(0, 256));
    expected->g = static_cast<uint8_t>(base::RandInt(0, 256));
    expected->b = static_cast<uint8_t>(base::RandInt(0, 256));
  }

  struct SerialSnapshot {
    SerialSnapshot() : host(nullptr) {}

    content::RenderWidgetHost* host;
    ExpectedColor color;
  };
  std::vector<SerialSnapshot> expected_snapshots_;

  void SyncSnapshotCallback(content::RenderWidgetHostImpl* rwhi,
                            const gfx::Image& image) {
    bool found = false;
    for (auto iter = expected_snapshots_.begin();
         iter != expected_snapshots_.end(); ++iter) {
      const SerialSnapshot& expected = *iter;
      if (expected.host == rwhi) {
        found = true;

        const SkBitmap* bitmap = image.ToSkBitmap();
        SkColor color = bitmap->getColor(1, 1);

        EXPECT_EQ(static_cast<int>(SkColorGetR(color)),
                  static_cast<int>(expected.color.r))
            << "Red channels differed";
        EXPECT_EQ(static_cast<int>(SkColorGetG(color)),
                  static_cast<int>(expected.color.g))
            << "Green channels differed";
        EXPECT_EQ(static_cast<int>(SkColorGetB(color)),
                  static_cast<int>(expected.color.b))
            << "Blue channels differed";

        expected_snapshots_.erase(iter);
        break;
      }
    }
  }

  std::map<content::RenderWidgetHost*, std::vector<ExpectedColor>>
      expected_async_snapshots_map_;
  int num_remaining_async_snapshots_ = 0;

  void AsyncSnapshotCallback(content::RenderWidgetHostImpl* rwhi,
                             const gfx::Image& image) {
    --num_remaining_async_snapshots_;
    auto iterator = expected_async_snapshots_map_.find(rwhi);
    ASSERT_NE(iterator, expected_async_snapshots_map_.end());
    std::vector<ExpectedColor>& expected_snapshots = iterator->second;
    const SkBitmap* bitmap = image.ToSkBitmap();
    SkColor color = bitmap->getColor(1, 1);
    bool found = false;
    // Find first instance of this color in the list and clear out all
    // of the entries before that point. If it's not found, report
    // failure.
    for (auto iter = expected_snapshots.begin();
         iter != expected_snapshots.end(); ++iter) {
      const ExpectedColor& expected = *iter;
      if (SkColorGetR(color) == expected.r &&
          SkColorGetG(color) == expected.g &&
          SkColorGetB(color) == expected.b) {
        // Erase everything up to this color, but not this color
        // itself, since it might be returned again later on
        // subsequent snapshot requests.
        expected_snapshots.erase(expected_snapshots.begin(), iter);
        found = true;
        break;
      }
    }

    EXPECT_TRUE(found) << "Did not find color ("
                       << static_cast<int>(SkColorGetR(color)) << ", "
                       << static_cast<int>(SkColorGetG(color)) << ", "
                       << static_cast<int>(SkColorGetB(color))
                       << ") in expected snapshots for RWH 0x" << rwhi;
  }
};

// Even the single-window test doesn't work on Android yet. It's expected
// that the multi-window tests would never work on that platform.
#if !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(SnapshotBrowserTest, SingleWindowTest) {
  SetupTestServer();

  content::RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl(shell());

  for (int i = 0; i < 40; ++i) {
    SerialSnapshot expected;
    expected.host = rwhi;
    PickRandomColor(&expected.color);

    std::string colorString = base::StringPrintf(
        "#%02x%02x%02x", expected.color.r, expected.color.g, expected.color.b);
    std::string script = std::string("fillWithColor(\"") + colorString + "\");";
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(GetWebContents(shell()),
                                                       script, &result));
    EXPECT_EQ(result, colorString);

    expected_snapshots_.push_back(expected);

    // Get the snapshot from the surface rather than the window. The
    // on-screen display path is verified by the GPU tests, and it
    // seems difficult to figure out the colorspace transformation
    // required to make these color comparisons.
    rwhi->GetSnapshotFromBrowser(
        base::Bind(&SnapshotBrowserTest::SyncSnapshotCallback,
                   base::Unretained(this), base::Unretained(rwhi)),
        true);
    while (expected_snapshots_.size() > 0) {
      base::RunLoop().RunUntilIdle();
    }
  }
}

// Timing out either all the time, or infrequently, apparently because
// they're too slow, on the following configurations:
//   Windows Debug
//   Linux Chromium OS ASAN LSAN Tests (1)
//   Linux TSAN Tests
// See crbug.com/771119
#if (defined(OS_WIN) && !defined(NDEBUG)) || (defined(OS_CHROMEOS)) || \
    (defined(OS_LINUX) && defined(THREAD_SANITIZER))
#define MAYBE_SyncMultiWindowTest DISABLED_SyncMultiWindowTest
#define MAYBE_AsyncMultiWindowTest DISABLED_AsyncMultiWindowTest
#else
#define MAYBE_SyncMultiWindowTest SyncMultiWindowTest
#define MAYBE_AsyncMultiWindowTest AsyncMultiWindowTest
#endif

IN_PROC_BROWSER_TEST_F(SnapshotBrowserTest, MAYBE_SyncMultiWindowTest) {
  SetupTestServer();

  for (int i = 0; i < 3; ++i) {
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetWebContents(shell()), "openNewWindow()", &result));
    EXPECT_TRUE(result);
  }

  base::RunLoop().RunUntilIdle();

  WaitForAllWindowsToBeReady();

  auto browser_list = Shell::windows();
  EXPECT_EQ(4u, browser_list.size());

  for (int i = 0; i < 20; ++i) {
    for (int j = 0; j < 4; j++) {
      // Start each iteration by taking a snapshot with a different
      // browser instance.
      int browser_index = (i + j) % 4;
      Shell* browser = browser_list[browser_index];
      content::RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl(browser);

      SerialSnapshot expected;
      expected.host = rwhi;
      PickRandomColor(&expected.color);

      std::string colorString =
          base::StringPrintf("#%02x%02x%02x", expected.color.r,
                             expected.color.g, expected.color.b);
      std::string script =
          std::string("fillWithColor(\"") + colorString + "\");";
      std::string result;
      EXPECT_TRUE(content::ExecuteScriptAndExtractString(
          GetWebContents(browser), script, &result));
      EXPECT_EQ(result, colorString);
      expected_snapshots_.push_back(expected);
      // Get the snapshot from the surface rather than the window. The
      // on-screen display path is verified by the GPU tests, and it
      // seems difficult to figure out the colorspace transformation
      // required to make these color comparisons.
      rwhi->GetSnapshotFromBrowser(
          base::Bind(&SnapshotBrowserTest::SyncSnapshotCallback,
                     base::Unretained(this), base::Unretained(rwhi)),
          true);
    }

    while (expected_snapshots_.size() > 0) {
      base::RunLoop().RunUntilIdle();
    }
  }
}

IN_PROC_BROWSER_TEST_F(SnapshotBrowserTest, MAYBE_AsyncMultiWindowTest) {
  SetupTestServer();

  for (int i = 0; i < 3; ++i) {
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetWebContents(shell()), "openNewWindow()", &result));
    EXPECT_TRUE(result);
  }

  base::RunLoop().RunUntilIdle();

  WaitForAllWindowsToBeReady();

  auto browser_list = Shell::windows();
  EXPECT_EQ(4u, browser_list.size());

  // This many pending snapshots per window will be put on the queue
  // before draining the requests. Anything more than 1 seems to catch
  // bugs which might otherwise be introduced in LatencyInfo's
  // propagation of the BROWSER_SNAPSHOT_FRAME_NUMBER_COMPONENT
  // component type.
  int divisor = 3;

  for (int i = 0; i < 10 * divisor; ++i) {
    for (int j = 0; j < 4; j++) {
      // Start each iteration by taking a snapshot with a different
      // browser instance.
      int browser_index = (i + j) % 4;
      Shell* browser = browser_list[browser_index];
      content::RenderWidgetHostImpl* rwhi = GetRenderWidgetHostImpl(browser);

      std::vector<ExpectedColor>& expected_snapshots =
          expected_async_snapshots_map_[rwhi];

      // Pick a unique random color.
      ExpectedColor expected;
      do {
        PickRandomColor(&expected);
      } while (base::Contains(expected_snapshots, expected));
      expected_snapshots.push_back(expected);

      std::string colorString = base::StringPrintf("#%02x%02x%02x", expected.r,
                                                   expected.g, expected.b);
      std::string script =
          std::string("fillWithColor(\"") + colorString + "\");";
      std::string result;
      EXPECT_TRUE(content::ExecuteScriptAndExtractString(
          GetWebContents(browser), script, &result));
      EXPECT_EQ(result, colorString);
      // Get the snapshot from the surface rather than the window. The
      // on-screen display path is verified by the GPU tests, and it
      // seems difficult to figure out the colorspace transformation
      // required to make these color comparisons.
      rwhi->GetSnapshotFromBrowser(
          base::Bind(&SnapshotBrowserTest::AsyncSnapshotCallback,
                     base::Unretained(this), base::Unretained(rwhi)),
          true);
      ++num_remaining_async_snapshots_;
    }

    // Periodically yield and drain the async snapshot requests.
    if ((i % divisor) == 0) {
      bool drained;
      do {
        drained = true;
        for (auto iter = expected_async_snapshots_map_.begin();
             iter != expected_async_snapshots_map_.end(); ++iter) {
          if (iter->second.size() > 1) {
            drained = false;
            break;
          }
        }
        if (!drained) {
          base::RunLoop().RunUntilIdle();
        }
      } while (!drained);
    }
  }

  // At the end of the test, cooperatively wait for all of the snapshot
  // requests to be drained before exiting. This works around crashes
  // seen when tearing down the compositor while these requests are in
  // flight. Likely, user-visible APIs that use this facility are safe
  // in this regard.
  while (num_remaining_async_snapshots_ > 0) {
    base::RunLoop().RunUntilIdle();
  }
}

#endif  // !defined(OS_ANDROID)

}  // namespace content
