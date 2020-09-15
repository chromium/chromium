// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/display_cutout/display_cutout_constants.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"

namespace content {

namespace {

#if defined(OS_ANDROID)

// These inset and flags simulate when we are not extending into the cutout.
const gfx::Insets kNoCutoutInsets = gfx::Insets();
const int kNoCutoutInsetsExpectedFlags = DisplayCutoutSafeArea::kEmpty;

// These inset and flags simulate when the we are extending into the cutout.
const gfx::Insets kCutoutInsets = gfx::Insets(1, 0, 1, 0);
const int kCutoutInsetsExpectedFlags =
    DisplayCutoutSafeArea::kTop | DisplayCutoutSafeArea::kBottom;

// These inset and flags simulate when we are extending into the cutout and have
// rotated the device so that the cutout is on the other sides.
const gfx::Insets kRotatedCutoutInsets = gfx::Insets(0, 1, 0, 1);
const int kRotatedCutoutInsetsExpectedFlags =
    DisplayCutoutSafeArea::kLeft | DisplayCutoutSafeArea::kRight;

#endif

class TestWebContentsObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  // WebContentsObserver override.
  void ViewportFitChanged(blink::mojom::ViewportFit value) override {
    value_ = value;

    if (value_ == wanted_value_)
      run_loop_.Quit();
  }

  bool has_value() const { return value_.has_value(); }

  void WaitForWantedValue(blink::mojom::ViewportFit wanted_value) {
    if (value_.has_value()) {
      EXPECT_EQ(wanted_value, value_);
      return;
    }

    wanted_value_ = wanted_value;
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  base::Optional<blink::mojom::ViewportFit> value_;
  blink::mojom::ViewportFit wanted_value_ = blink::mojom::ViewportFit::kAuto;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

// Used for forcing a specific |blink::mojom::DisplayMode| during a test.
class DisplayCutoutWebContentsDelegate : public WebContentsDelegate {
 public:
  blink::mojom::DisplayMode GetDisplayMode(
      const WebContents* web_contents) override {
    return display_mode_;
  }

  void SetDisplayMode(blink::mojom::DisplayMode display_mode) {
    display_mode_ = display_mode;
  }

 private:
  blink::mojom::DisplayMode display_mode_ = blink::mojom::DisplayMode::kBrowser;
};

const char kTestHTML[] =
    "<!DOCTYPE html>"
    "<style>"
    "  #target {"
    "    margin-top: env(safe-area-inset-top);"
    "    margin-left: env(safe-area-inset-left);"
    "    margin-bottom: env(safe-area-inset-bottom);"
    "    margin-right: env(safe-area-inset-right);"
    "  }"
    "</style>"
    "<div id=target></div>";

}  // namespace

class DisplayCutoutBrowserTest : public ContentBrowserTest {
 public:
  DisplayCutoutBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "DisplayCutoutAPI");
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    embedded_test_server()->ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

  void LoadTestPageWithViewportFitFromMeta(const std::string& value) {
    LoadTestPageWithData(
        "<!DOCTYPE html>"
        "<meta name='viewport' content='viewport-fit=" +
        value + "'><iframe></iframe>");
  }

  void LoadSubFrameWithViewportFitMetaValue(const std::string& value) {
    const std::string data =
        "data:text/html;charset=utf-8,<!DOCTYPE html>"
        "<meta name='viewport' content='viewport-fit=" +
        value + "'>";

    FrameTreeNode* root = web_contents_impl()->GetFrameTree()->root();
    FrameTreeNode* child = root->child_at(0);

    ASSERT_TRUE(NavigateToURLFromRenderer(child, GURL(data)));
    web_contents_impl()->Focus();
  }

  bool ClearViewportFitTag() {
    return ExecuteScript(
        web_contents_impl(),
        "document.getElementsByTagName('meta')[0].content = ''");
  }

  void SendSafeAreaToFrame(int top, int left, int bottom, int right) {
    mojo::AssociatedRemote<blink::mojom::DisplayCutoutClient> client;
    MainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
        client.BindNewEndpointAndPassReceiver());
    client->SetSafeArea(
        blink::mojom::DisplayCutoutSafeArea::New(top, left, bottom, right));
  }

  std::string GetCurrentSafeAreaValue(const std::string& name) {
    std::string value;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        MainFrame(),
        "(() => {"
        "const e = document.getElementById('target');"
        "const style = window.getComputedStyle(e, null);"
        "window.domAutomationController.send("
        "  style.getPropertyValue('margin-" +
            name +
            "'));"
            "})();",
        &value));
    return value;
  }

  void LoadTestPageWithData(const std::string& data) {
    // Write |data| to a temporary file that can be later reached at
    // http://127.0.0.1/test_file_*.html.
    static int s_test_file_number = 1;
    base::FilePath file_path = temp_dir_.GetPath().AppendASCII(
        base::StringPrintf("test_file_%d.html", s_test_file_number++));
    {
      base::ScopedAllowBlockingForTesting allow_temp_file_writing;
      ASSERT_TRUE(base::WriteFile(file_path, data));
    }
    GURL url = embedded_test_server()->GetURL(
        "/" + file_path.BaseName().AsUTF8Unsafe());

    // Reset UKM and navigate to the html file created above.
    ResetUKM();
    ASSERT_TRUE(NavigateToURL(shell(), url));
  }

  void SimulateFullscreenStateChanged(RenderFrameHost* frame,
                                      bool is_fullscreen) {
    web_contents_impl()->FullscreenStateChanged(frame, is_fullscreen);
  }

  void SimulateFullscreenExit() {
    web_contents_impl()->ExitFullscreenMode(true);
  }

  RenderFrameHost* MainFrame() { return web_contents_impl()->GetMainFrame(); }

  RenderFrameHost* ChildFrame() {
    FrameTreeNode* root = web_contents_impl()->GetFrameTree()->root();
    return root->child_at(0)->current_frame_host();
  }

  WebContentsImpl* web_contents_impl() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  unsigned GetUKMEntryCount() const {
    using Entry = ukm::builders::Layout_DisplayCutout_StateChanged;
    auto ukm_entries = test_ukm_recorder_->GetEntriesByName(Entry::kEntryName);
    return ukm_entries.size();
  }

  void ExpectUKMEntry(int index,
                      ukm::SourceId source_id,
                      bool is_main_frame,
                      blink::mojom::ViewportFit applied_value,
                      blink::mojom::ViewportFit supplied_value,
                      int ignored_reason,
                      int safe_areas_present) {
    using Entry = ukm::builders::Layout_DisplayCutout_StateChanged;
    auto ukm_entries = test_ukm_recorder_->GetEntriesByName(Entry::kEntryName);

    EXPECT_EQ(source_id, ukm_entries[index]->source_id);
    EXPECT_EQ(is_main_frame, *test_ukm_recorder_->GetEntryMetric(
                                 ukm_entries[index], Entry::kIsMainFrameName));
    EXPECT_EQ(static_cast<int>(applied_value),
              *test_ukm_recorder_->GetEntryMetric(
                  ukm_entries[index], Entry::kViewportFit_AppliedName));
    EXPECT_EQ(static_cast<int>(supplied_value),
              *test_ukm_recorder_->GetEntryMetric(
                  ukm_entries[index], Entry::kViewportFit_SuppliedName));
    EXPECT_EQ(ignored_reason,
              *test_ukm_recorder_->GetEntryMetric(
                  ukm_entries[index], Entry::kViewportFit_IgnoredReasonName));
    EXPECT_EQ(safe_areas_present,
              *test_ukm_recorder_->GetEntryMetric(
                  ukm_entries[index], Entry::kSafeAreasPresentName));
  }

 private:
  void ResetUKM() {
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(DisplayCutoutBrowserTest);
};

// The viewport meta tag is only enabled on Android.
#if defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Fullscreen) {
  LoadTestPageWithViewportFitFromMeta("cover");
  LoadSubFrameWithViewportFitMetaValue("contain");

  {
    TestWebContentsObserver observer(web_contents_impl());
    SimulateFullscreenStateChanged(MainFrame(), true);
    observer.WaitForWantedValue(blink::mojom::ViewportFit::kCover);
    web_contents_impl()->SetDisplayCutoutSafeArea(kCutoutInsets);
  }

  {
    TestWebContentsObserver observer(web_contents_impl());
    SimulateFullscreenStateChanged(ChildFrame(), true);
    observer.WaitForWantedValue(blink::mojom::ViewportFit::kContain);
    web_contents_impl()->SetDisplayCutoutSafeArea(kNoCutoutInsets);
  }

  {
    TestWebContentsObserver observer(web_contents_impl());
    SimulateFullscreenStateChanged(ChildFrame(), false);
    observer.WaitForWantedValue(blink::mojom::ViewportFit::kCover);

    // This simulates the user rotating the device.
    web_contents_impl()->SetDisplayCutoutSafeArea(kCutoutInsets);
    web_contents_impl()->SetDisplayCutoutSafeArea(kRotatedCutoutInsets);
  }

  {
    TestWebContentsObserver observer(web_contents_impl());
    SimulateFullscreenStateChanged(MainFrame(), false);
    SimulateFullscreenExit();
    observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
    web_contents_impl()->SetDisplayCutoutSafeArea(kNoCutoutInsets);
  }

  // Get the source id for the page and close the |shell|. This will flush any
  // unrecorded UKM metrics.
  ukm::SourceId source_id =
      web_contents_impl()->GetMainFrame()->GetPageUkmSourceId();
  shell()->Close();

  // Check UKM metrics are recorded. The first two entries are from loading the
  // frame and the subframe with a viewport fit attribute.
  EXPECT_EQ(5u, GetUKMEntryCount());
  ExpectUKMEntry(0, source_id, true, blink::mojom::ViewportFit::kAuto,
                 blink::mojom::ViewportFit::kCover,
                 DisplayCutoutIgnoredReason::kWebContentsNotFullscreen,
                 kNoCutoutInsetsExpectedFlags);
  ExpectUKMEntry(1, source_id, false, blink::mojom::ViewportFit::kAuto,
                 blink::mojom::ViewportFit::kContain,
                 DisplayCutoutIgnoredReason::kWebContentsNotFullscreen,
                 kNoCutoutInsetsExpectedFlags);

  // This is when we take the main frame fullscreen.
  ExpectUKMEntry(2, source_id, true, blink::mojom::ViewportFit::kCover,
                 blink::mojom::ViewportFit::kCover,
                 DisplayCutoutIgnoredReason::kAllowed,
                 kCutoutInsetsExpectedFlags);

  // This is when we take the subframe fullscreen.
  ExpectUKMEntry(3, source_id, false, blink::mojom::ViewportFit::kContain,
                 blink::mojom::ViewportFit::kContain,
                 DisplayCutoutIgnoredReason::kAllowed,
                 kNoCutoutInsetsExpectedFlags);

  // These is when the subframe exits fullscreen.
  ExpectUKMEntry(4, source_id, true, blink::mojom::ViewportFit::kCover,
                 blink::mojom::ViewportFit::kCover,
                 DisplayCutoutIgnoredReason::kAllowed,
                 kRotatedCutoutInsetsExpectedFlags);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest,
                       ViewportFit_Fullscreen_Update) {
  LoadTestPageWithViewportFitFromMeta("cover");

  {
    TestWebContentsObserver observer(web_contents_impl());
    SimulateFullscreenStateChanged(MainFrame(), true);
    observer.WaitForWantedValue(blink::mojom::ViewportFit::kCover);
    web_contents_impl()->SetDisplayCutoutSafeArea(kNoCutoutInsets);
  }

  {
    TestWebContentsObserver observer(web_contents_impl());
    EXPECT_TRUE(ClearViewportFitTag());
    observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
    web_contents_impl()->SetDisplayCutoutSafeArea(kNoCutoutInsets);
  }

  // Get the source id for the page and close the |shell|. This will flush any
  // unrecorded UKM metrics.
  ukm::SourceId source_id =
      web_contents_impl()->GetMainFrame()->GetPageUkmSourceId();
  shell()->Close();

  // Check UKM metrics are recorded.
  EXPECT_EQ(2u, GetUKMEntryCount());
  ExpectUKMEntry(0, source_id, true, blink::mojom::ViewportFit::kAuto,
                 blink::mojom::ViewportFit::kCover,
                 DisplayCutoutIgnoredReason::kWebContentsNotFullscreen,
                 kNoCutoutInsetsExpectedFlags);
  ExpectUKMEntry(1, source_id, true, blink::mojom::ViewportFit::kCover,
                 blink::mojom::ViewportFit::kCover,
                 DisplayCutoutIgnoredReason::kAllowed,
                 kNoCutoutInsetsExpectedFlags);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Noop_Navigate) {
  {
    TestWebContentsObserver observer(web_contents_impl());
    LoadTestPageWithViewportFitFromMeta("cover");
    EXPECT_FALSE(observer.has_value());
  }

  ukm::SourceId source_id =
      web_contents_impl()->GetMainFrame()->GetPageUkmSourceId();
  LoadTestPageWithData("");

  // Check UKM metrics are recorded.
  EXPECT_EQ(1u, GetUKMEntryCount());
  ExpectUKMEntry(0, source_id, true, blink::mojom::ViewportFit::kAuto,
                 blink::mojom::ViewportFit::kCover,
                 DisplayCutoutIgnoredReason::kWebContentsNotFullscreen,
                 kNoCutoutInsetsExpectedFlags);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest,
                       ViewportFit_Noop_WebContentsDestroyed) {
  {
    TestWebContentsObserver observer(web_contents_impl());
    LoadTestPageWithViewportFitFromMeta("cover");
    EXPECT_FALSE(observer.has_value());
  }

  ukm::SourceId source_id =
      web_contents_impl()->GetMainFrame()->GetPageUkmSourceId();
  shell()->Close();

  // Check UKM metrics are recorded.
  EXPECT_EQ(1u, GetUKMEntryCount());
  ExpectUKMEntry(0, source_id, true, blink::mojom::ViewportFit::kAuto,
                 blink::mojom::ViewportFit::kCover,
                 DisplayCutoutIgnoredReason::kWebContentsNotFullscreen,
                 kNoCutoutInsetsExpectedFlags);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, WebDisplayMode) {
  // Inject the custom delegate used for this test.
  std::unique_ptr<DisplayCutoutWebContentsDelegate> delegate(
      new DisplayCutoutWebContentsDelegate());
  web_contents_impl()->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), web_contents_impl()->GetDelegate());

  {
    TestWebContentsObserver observer(web_contents_impl());
    LoadTestPageWithViewportFitFromMeta("cover");
    EXPECT_FALSE(observer.has_value());
  }
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, WebDisplayMode_Fullscreen) {
  // Inject the custom delegate used for this test.
  std::unique_ptr<DisplayCutoutWebContentsDelegate> delegate(
      new DisplayCutoutWebContentsDelegate());
  delegate->SetDisplayMode(blink::mojom::DisplayMode::kFullscreen);
  web_contents_impl()->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), web_contents_impl()->GetDelegate());

  {
    TestWebContentsObserver observer(web_contents_impl());
    LoadTestPageWithViewportFitFromMeta("cover");
    observer.WaitForWantedValue(blink::mojom::ViewportFit::kCover);
  }
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, WebDisplayMode_Standalone) {
  // Inject the custom delegate used for this test.
  std::unique_ptr<DisplayCutoutWebContentsDelegate> delegate(
      new DisplayCutoutWebContentsDelegate());
  delegate->SetDisplayMode(blink::mojom::DisplayMode::kStandalone);
  web_contents_impl()->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), web_contents_impl()->GetDelegate());

  {
    TestWebContentsObserver observer(web_contents_impl());
    LoadTestPageWithViewportFitFromMeta("cover");
    EXPECT_FALSE(observer.has_value());
  }
}

#endif

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, PublishSafeAreaVariables) {
  LoadTestPageWithData(kTestHTML);

  // Make sure all the safe areas are currently zero.
  EXPECT_EQ("0px", GetCurrentSafeAreaValue("top"));
  EXPECT_EQ("0px", GetCurrentSafeAreaValue("left"));
  EXPECT_EQ("0px", GetCurrentSafeAreaValue("bottom"));
  EXPECT_EQ("0px", GetCurrentSafeAreaValue("right"));

  SendSafeAreaToFrame(1, 2, 3, 4);

  // Make sure all the safe ares are correctly set.
  EXPECT_EQ("1px", GetCurrentSafeAreaValue("top"));
  EXPECT_EQ("2px", GetCurrentSafeAreaValue("left"));
  EXPECT_EQ("3px", GetCurrentSafeAreaValue("bottom"));
  EXPECT_EQ("4px", GetCurrentSafeAreaValue("right"));
}

}  //  namespace content
