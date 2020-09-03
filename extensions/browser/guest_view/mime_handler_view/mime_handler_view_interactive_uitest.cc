// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"

using guest_view::GuestViewManager;
using guest_view::GuestViewManagerDelegate;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

namespace extensions {

// The test extension id is set by the key value in the manifest.
const char kExtensionId[] = "oickdpebdnfbgkcaoklfcdhjniefkcji";

// Counts the number of URL requests made for a given URL.
class MimeHandlerViewTest : public ExtensionApiTest {
 public:
  MimeHandlerViewTest() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  ~MimeHandlerViewTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("mime_handler_view"));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // TODO(paulmeyer): This function is implemented over and over by the
  // different GuestView test classes. It really needs to be refactored out to
  // some kind of GuestViewTest base class.
  TestGuestViewManager* GetGuestViewManager() {
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

  const Extension* LoadTestExtension() {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("mime_handler_view"));
    if (!extension)
      return nullptr;

    CHECK_EQ(std::string(kExtensionId), extension->id());

    return extension;
  }

  void RunTestWithUrl(const GURL& url) {
    // Use the testing subclass of MimeHandlerViewGuest.
    GetGuestViewManager()->RegisterTestGuestViewType<MimeHandlerViewGuest>(
        base::BindRepeating(&TestMimeHandlerViewGuest::Create));

    const Extension* extension = LoadTestExtension();
    ASSERT_TRUE(extension);

    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(), url);

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }

  void RunTest(const std::string& path) {
    RunTestWithUrl(embedded_test_server()->GetURL("/" + path));
  }

 private:
  TestGuestViewManagerFactory factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test is flaky on Linux.  https://crbug.com/877627
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_Fullscreen DISABLED_Fullscreen
#else
#define MAYBE_Fullscreen Fullscreen
#endif
IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, MAYBE_Fullscreen) {
  RunTest("testFullscreen.csv");
}

namespace {

void WaitForFullscreenAnimation() {
#if defined(OS_MAC)
  const int delay_in_ms = 1500;
#else
  const int delay_in_ms = 100;
#endif
  // Wait for Mac OS fullscreen animation.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(delay_in_ms));
  run_loop.Run();
}

}  // namespace

// TODO(1119576): Flaky under Lacros.
#if BUILDFLAG(IS_LACROS)
#define MAYBE_EscapeExitsFullscreen DISABLED_EscapeExitsFullscreen
#else
#define MAYBE_EscapeExitsFullscreen EscapeExitsFullscreen
#endif
IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, MAYBE_EscapeExitsFullscreen) {
  // Use the testing subclass of MimeHandlerViewGuest.
  GetGuestViewManager()->RegisterTestGuestViewType<MimeHandlerViewGuest>(
      base::BindRepeating(&TestMimeHandlerViewGuest::Create));

  const Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  ResultCatcher catcher;

  // Set observer to watch for fullscreen.
  FullscreenNotificationObserver fullscreen_waiter(browser());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/testFullscreenEscape.csv"));

  // Make sure we have a guestviewmanager.
  auto* embedder_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  auto* guest_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  auto* guest_rwh =
      guest_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();

  // Wait for fullscreen mode.
  fullscreen_waiter.Wait();
  WaitForFullscreenAnimation();

  // Send a touch to focus the guest. We can't directly test that the correct
  // RenderWidgetHost got focus, but the wait seems to work.
  SimulateMouseClick(guest_contents, 0, blink::WebMouseEvent::Button::kLeft);
  while (!IsRenderWidgetHostFocused(guest_rwh)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  EXPECT_EQ(guest_contents, content::GetFocusedWebContents(embedder_contents));

  // Send <esc> to exit fullscreen.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  WaitForFullscreenAnimation();

  // Now wait for the test to succeed, or timeout.
  if (!catcher.GetNextResult())
    FAIL() << catcher.message();
}

}  // namespace extensions
