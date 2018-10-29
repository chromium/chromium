// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"
#include "url/url_constants.h"

using extensions::ExtensionsAPIClient;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using guest_view::GuestViewManager;
using guest_view::GuestViewManagerDelegate;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

// The test extension id is set by the key value in the manifest.
const char kExtensionId[] = "oickdpebdnfbgkcaoklfcdhjniefkcji";

class MimeHandlerViewTest : public extensions::ExtensionApiTest {
 public:
  MimeHandlerViewTest() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  ~MimeHandlerViewTest() override {}

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("mime_handler_view"));
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &MimeHandlerViewTest::MonitorRequest, base::Unretained(this)));
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // TODO(paulmeyer): This function is implemented over and over by the
  // different GuestView test classes. It really needs to be refactored out to
  // some kind of GuestViewTest base class.
  TestGuestViewManager* GetGuestViewManager() const {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated can and will get called
    // before a guest is created. Since GuestViewManager is usually not created
    // until the first guest is created, this means that |manager| will be
    // nullptr if trying to use the manager to wait for the first guest. Because
    // of this, the manager must be created here if it does not already exist.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

  MimeHandlerViewGuest* GetLastGuestView() const {
    return MimeHandlerViewGuest::FromWebContents(
               GetGuestViewManager()->GetLastGuestCreated())
        ->As<MimeHandlerViewGuest>();
  }

  const extensions::Extension* LoadTestExtension() {
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("mime_handler_view"));
    if (!extension)
      return nullptr;

    CHECK_EQ(std::string(kExtensionId), extension->id());

    return extension;
  }

  void RunTestWithUrl(const GURL& url) {
    // Use the testing subclass of MimeHandlerViewGuest.
    GetGuestViewManager()->RegisterTestGuestViewType<MimeHandlerViewGuest>(
        base::Bind(&TestMimeHandlerViewGuest::Create));

    const extensions::Extension* extension = LoadTestExtension();
    ASSERT_TRUE(extension);

    extensions::ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(), url);

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }

  void RunTest(const std::string& path) {
    RunTestWithUrl(embedded_test_server()->GetURL("/" + path));
  }

  int basic_count() const { return basic_count_; }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/testBasic.csv")
      basic_count_++;
  }

  TestGuestViewManagerFactory factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
  int basic_count_ = 0;
};

// The parametric version of the test class which runs the test both on
// BrowserPlugin-based and cross-process-frame-based MimeHandlerView
// implementation. All current browser tests should eventually be moved to this
// and then eventually drop the BrowserPlugin dependency once
// https://crbug.com/659750 is fixed.
class MimeHandlerViewCrossProcessTest
    : public MimeHandlerViewTest,
      public ::testing::WithParamInterface<bool> {
 public:
  MimeHandlerViewCrossProcessTest() : MimeHandlerViewTest() {}
  ~MimeHandlerViewCrossProcessTest() override {}

  void SetUpCommandLine(base::CommandLine* cl) override {
    MimeHandlerViewTest::SetUpCommandLine(cl);
    is_cross_process_mode_ = GetParam();
    if (is_cross_process_mode_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kMimeHandlerViewInCrossProcessFrame);
    }
  }

  bool is_cross_process_mode() const { return is_cross_process_mode_; }

 private:
  bool is_cross_process_mode_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewCrossProcessTest);
};

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        MimeHandlerViewCrossProcessTest,
                        ::testing::Bool());

IN_PROC_BROWSER_TEST_P(MimeHandlerViewCrossProcessTest, Embedded) {
  RunTest("test_embedded.html");
  // Sanity check. Navigate the page and verify the guest goes away.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  auto* gv_manager = GetGuestViewManager();
  gv_manager->WaitForAllGuestsDeleted();
  EXPECT_EQ(1U, gv_manager->num_guests_created());
}

// TODO(crbug.com/897971): Flaky on Windows 7 debug.
#if defined(OS_WIN)
#define MAYBE_EmbedWithInitialCrossOriginFrame \
  DISABLED_EmbedWithInitialCrossOriginFrame
#else
#define MAYBE_EmbedWithInitialCrossOriginFrame EmbedWithInitialCrossOriginFrame
#endif
// This test start with an <object> that has a content frame. Then the content
// frame (plugin frame) is navigated to a cross-origin target page. After the
// navigation is completed, the <object> is set to render MimeHandlerView by
// setting its |data| and |type| attributes accordingly.
IN_PROC_BROWSER_TEST_P(MimeHandlerViewCrossProcessTest,
                       MAYBE_EmbedWithInitialCrossOriginFrame) {
  const std::string kTestName = "test_cross_origin_frame";
  std::string cross_origin_url =
      embedded_test_server()->GetURL("b.com", "/test_page.html").spec();
  auto test_url = embedded_test_server()->GetURL(
      "a.com",
      base::StringPrintf("/test_object_with_frame.html?test_data=%s,%s,%s",
                         kTestName.c_str(), cross_origin_url.c_str(),
                         "testEmbedded.csv"));
  RunTestWithUrl(test_url);
}

// This test verifies that navigations on the plugin frame before setting it to
// load MimeHandlerView do not race with the creation of the guest. The test
// loads a page with an <object> which is first navigated to some cross-origin
// domain and then immediately after load, the page triggers a navigation of its
// own to another cross-origin domain. Meanwhile the embedder sets the <object>
// to load a MimeHandlerView. The test passes if MHV loads. This is to catch the
// potential race between the cross-origin renderer initiated navigation and
// the navigation to "about:blank" started from the browser.
IN_PROC_BROWSER_TEST_P(MimeHandlerViewCrossProcessTest,
                       NavigationRaceFromEmbedder) {
  if (!is_cross_process_mode()) {
    // Note that this test would pass trivially with BrowserPlugin-based guests
    // because loading a plugin is quite independent from navigating a plugin.
    // They do actually coexist at times (for more context see
    // https://crbug.com/776510).
    return;
  }

  const std::string kTestName = "test_navigation_race_embedder";
  auto cross_origin_url =
      embedded_test_server()->GetURL("b.com", "/test_page.html").spec();
  auto test_url = embedded_test_server()->GetURL(
      "a.com",
      base::StringPrintf("/test_object_with_frame.html?test_data=%s,%s,%s",
                         kTestName.c_str(), cross_origin_url.c_str(),
                         "testEmbedded.csv"));
  RunTestWithUrl(test_url);
}

// TODO(crbug.com/897971): Flaky on Windows 7 debug.
#if defined(OS_WIN)
#define MAYBE_NavigationRaceFromCrossProcessRenderer \
  DISABLED_NavigationRaceFromCrossProcessRenderer
#else
#define MAYBE_NavigationRaceFromCrossProcessRenderer \
  NavigationRaceFromCrossProcessRenderer
#endif
// TODO(ekaramad): Without proper handling of navigation to 'about:blank', this
// test would be flaky. Use TestNavigationManager class and possibly break the
// test into more sub-tests for various scenarios (https://crbug.com/659750).
// This test verifies that (almost) concurrent navigations in a cross-process
// frame inside an <embed> which is transitioning to a MimeHandlerView will
// not block creation of MimeHandlerView. The test will load some cross-origin
// content in <object> which right after loading will navigate it self to some
// other cross-origin content. On the embedder side, when the first page loads,
// the <object> loads some text/csv content to create a MimeHandlerViewGuest.
// The test passes if MHV loads.
IN_PROC_BROWSER_TEST_P(MimeHandlerViewCrossProcessTest,
                       MAYBE_NavigationRaceFromCrossProcessRenderer) {
  if (!is_cross_process_mode()) {
    // Note that this test would pass trivially with BrowserPlugin-based guests
    // because loading a plugin is quite independent from navigating a plugin.
    // They do actually coexist at times (for more context see
    // https://crbug.com/776510).
    return;
  }

  const std::string kTestName = "test_navigation_race_cross_origin";
  auto cross_origin_url =
      embedded_test_server()->GetURL("b.com", "/test_page.html").spec();
  auto other_cross_origin_url =
      embedded_test_server()->GetURL("c.com", "/test_page.html").spec();
  auto test_url = embedded_test_server()->GetURL(
      "a.com",
      base::StringPrintf("/test_object_with_frame.html?test_data=%s,%s,%s,%s",
                         kTestName.c_str(), cross_origin_url.c_str(),
                         other_cross_origin_url.c_str(), "testEmbedded.csv"));
  RunTestWithUrl(test_url);
}

// TODO(ekaramad): Somehow canceling a first dialog in a setup similar to the
// test below pops up another dialog. This is likely due to the navigation to
// about:blank from both the browser side and the embedder side in the method
// HTMLPlugInElement::RequestObjectInternal. Find out the issue and add another
// test here where the dialog is dismissed and the guest not created.
// (https://crbug.com/659750).
// This test verifies that transitioning a plugin element from text/html to
// application/pdf respects 'beforeunload'. The test specifically checks that
// 'beforeunload' dialog is shown to the user and if the user decides to
// proceed with the transition, MimeHandlerViewGuest is created.
IN_PROC_BROWSER_TEST_P(MimeHandlerViewCrossProcessTest,
                       EmbedWithInitialFrameAcceptBeforeUnloadDialog) {
  if (!is_cross_process_mode()) {
    // BrowserPlugin-based MimeHandlerView does not care for 'beforeunload' and
    // the guest will always be created. This test would time out due to frame
    // never unloading.
    return;
  }
  // Use the testing subclass of MimeHandlerViewGuest.
  GetGuestViewManager()->RegisterTestGuestViewType<MimeHandlerViewGuest>(
      base::BindRepeating(&TestMimeHandlerViewGuest::Create));
  const extensions::Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("a.com", "/test_object_with_frame.html"));
  auto* main_frame =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetMainFrame();
  auto url_with_beforeunload =
      embedded_test_server()->GetURL("b.com", "/test_page.html?beforeunload");
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      main_frame,
      base::StringPrintf(
          "object.data = '%s';"
          " object.onload = () => window.domAutomationController.send(true);",
          url_with_beforeunload.spec().c_str()),
      &result));
  ASSERT_TRUE(result);
  // Give user gesture to the frame, set the <object> to text/csv resource and
  // handle the "beforeunload" dialog.
  content::PrepContentsForBeforeUnloadTest(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  ASSERT_TRUE(content::ExecuteScript(main_frame,
                                     "object.data = './testEmbedded.csv';"
                                     "object.type = 'text/csv';"));
  app_modal::JavaScriptAppModalDialog* alert =
      ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(alert->is_before_unload_dialog());
  alert->native_dialog()->AcceptAppModalDialog();

  EXPECT_TRUE(GetGuestViewManager()->WaitForSingleGuestCreated());
}
// The following tests will eventually converted into a parametric version which
// will run on both BrowserPlugin-based and cross-process-frame-based
// MimeHandlerView (https://crbug.com/659750).
IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, PostMessage) {
  RunTest("test_postmessage.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Basic) {
  RunTest("testBasic.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Iframe) {
  RunTest("test_iframe.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Abort) {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // With the network service, abortStream isn't needed since we pass a Mojo
    // pipe to the renderer. If the plugin chooses to cancel the main request
    // (e.g. to make range requests instead), we are always guaranteed that the
    // Mojo pipe will be broken which will cancel the request. This is different
    // than without the network service, since stream URLs need to be explicitly
    // closed if they weren't yet opened to avoid leaks.
    // TODO(jam): once the network service is the only path, delete the
    // abortStream mimeHandlerPrivate method and supporting code.
    return;
  }
  RunTest("testAbort.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, NonAsciiHeaders) {
  RunTest("testNonAsciiHeaders.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, DataUrl) {
  const char* kDataUrlCsv = "data:text/csv;base64,Y29udGVudCB0byByZWFkCg==";
  RunTestWithUrl(GURL(kDataUrlCsv));
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, EmbeddedDataUrlObject) {
  RunTest("test_embedded_data_url_object.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, EmbeddedDataUrlEmbed) {
  RunTest("test_embedded_data_url_embed.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, EmbeddedDataUrlLong) {
  RunTest("test_embedded_data_url_long.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, ResizeBeforeAttach) {
  // Delay the creation of the guest's WebContents in order to delay the guest's
  // attachment to the embedder. This will allow us to resize the <object> tag
  // after the guest is created, but before it is attached in
  // "test_resize_before_attach.html".
  TestMimeHandlerViewGuest::DelayNextCreateWebContents(500);
  RunTest("test_resize_before_attach.html");

  // Wait for the guest to attach.
  content::WebContents* guest_web_contents =
      GetGuestViewManager()->WaitForSingleGuestCreated();
  TestMimeHandlerViewGuest* guest = static_cast<TestMimeHandlerViewGuest*>(
      MimeHandlerViewGuest::FromWebContents(guest_web_contents));
  guest->WaitForGuestAttached();

  // Ensure that the guest has the correct size after it has attached.
  auto guest_size = guest->size();
  CHECK_EQ(guest_size.width(), 500);
  CHECK_EQ(guest_size.height(), 400);
}

// Regression test for crbug.com/587709.
IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, SingleRequest) {
  GURL url(embedded_test_server()->GetURL("/testBasic.csv"));
  RunTest("testBasic.csv");
  EXPECT_EQ(1, basic_count());
}

// Test that a mime handler view can keep a background page alive.
IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, BackgroundPage) {
  extensions::ProcessManager::SetEventPageIdleTimeForTesting(1);
  extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  RunTest("testBackgroundPage.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, TargetBlankAnchor) {
  RunTest("testTargetBlankAnchor.csv");
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WaitForLoadStop(browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_EQ(
      GURL("about:blank"),
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, BeforeUnload_NoDialog) {
  ASSERT_NO_FATAL_FAILURE(RunTest("testBeforeUnloadNoDialog.csv"));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  content::PrepContentsForBeforeUnloadTest(web_contents);

  // Wait for a round trip to the outer renderer to ensure any beforeunload
  // toggle IPC has had time to reach the browser.
  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "");

  // Try to navigate away from the page. If the beforeunload listener is
  // triggered and a dialog is shown, this navigation will never complete,
  // causing the test to timeout and fail.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, BeforeUnload_ShowDialog) {
  ASSERT_NO_FATAL_FAILURE(RunTest("testBeforeUnloadShowDialog.csv"));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  content::PrepContentsForBeforeUnloadTest(web_contents);

  // Wait for a round trip to the outer renderer to ensure the beforeunload
  // toggle IPC has had time to reach the browser.
  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "");

  web_contents->GetController().LoadURL(GURL("about:blank"), {},
                                        ui::PAGE_TRANSITION_TYPED, "");

  app_modal::JavaScriptAppModalDialog* before_unload_dialog =
      ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(before_unload_dialog->is_before_unload_dialog());
  EXPECT_FALSE(before_unload_dialog->is_reload());
  before_unload_dialog->OnAccept(base::string16(), false);
}

// TODO(mcnee): These tests are BrowserPlugin specific. Once
// MimeHandlerViewGuest is no longer based on BrowserPlugin, remove these tests.
// (See https://crbug.com/533069 and https://crbug.com/659750). These category
// of tests are solely testing BrowserPlugin features.
class MimeHandlerViewBrowserPluginSpecificTest : public MimeHandlerViewTest {
 public:
  MimeHandlerViewBrowserPluginSpecificTest() {}

  ~MimeHandlerViewBrowserPluginSpecificTest() override {}

 protected:
  // None of these test create new tabs, so the embedder should be the first
  // tab.
  content::WebContents* GetEmbedderWebContents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewBrowserPluginSpecificTest);
};

// This test verifies that when BrowserPlugin-based guest has touch handlers,
// the embedder knows about it.
IN_PROC_BROWSER_TEST_F(MimeHandlerViewBrowserPluginSpecificTest,
                       AcceptTouchEvents) {
  RunTest("testBasic.csv");
  content::RenderViewHost* embedder_rvh =
      GetEmbedderWebContents()->GetRenderViewHost();
  bool embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  EXPECT_FALSE(embedder_has_touch_handler);

  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(ExecuteScript(
      guest_web_contents,
      "document.addEventListener('touchstart', dummyTouchStartHandler);"));
  // Wait until embedder has touch handlers.
  while (!content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  ASSERT_TRUE(ExecuteScript(
      guest_web_contents,
      "document.removeEventListener('touchstart', dummyTouchStartHandler);"));
  // Wait until embedder not longer has any touch handlers.
  while (content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

// Verify that a BrowserPlugin captures mouse input on MouseDown.
IN_PROC_BROWSER_TEST_F(MimeHandlerViewBrowserPluginSpecificTest,
                       MouseCaptureOnMouseDown) {
  RunTest("testBasic.csv");
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  auto* guest_widget = MimeHandlerViewGuest::FromWebContents(guest_web_contents)
                           ->GetOwnerRenderWidgetHost();
  auto* embedder_web_contents = GetEmbedderWebContents();

  SendMouseDownToWidget(guest_widget, 0, blink::WebMouseEvent::Button::kLeft);

  while (!GetMouseCaptureWidget(embedder_web_contents)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  EXPECT_EQ(GetMouseCaptureWidget(embedder_web_contents), guest_widget);
}
