// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/events/base_event_utils.h"

namespace content {

namespace {

using WebUIImplBrowserTest = ContentBrowserTest;

// TODO(crbug.com/154571): Shared workers are not available on Android.
#if !defined(OS_ANDROID)
const char kLoadSharedWorkerScript[] = R"(
    new Promise((resolve) => {
      const sharedWorker = new SharedWorker($1);
      sharedWorker.port.onmessage = (event) => {
        resolve(event.data === 'pong');
      };
      sharedWorker.port.postMessage('ping');
    });
  )";
#endif  // !defined(OS_ANDROID)

class TestWebUIMessageHandler : public WebUIMessageHandler {
 public:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "messageRequiringGesture",
        base::BindRepeating(&TestWebUIMessageHandler::OnMessageRequiringGesture,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "notifyFinish",
        base::BindRepeating(&TestWebUIMessageHandler::OnNotifyFinish,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "sendMessage",
        base::BindRepeating(&TestWebUIMessageHandler::OnSendMessase,
                            base::Unretained(this)));
  }

  void set_finish_closure(base::RepeatingClosure closure) {
    finish_closure_ = std::move(closure);
  }

  int message_requiring_gesture_count() const {
    return message_requiring_gesture_count_;
  }

  void set_send_message_closure(base::OnceClosure closure) {
    send_message_closure_ = std::move(closure);
  }

 private:
  void OnMessageRequiringGesture(const base::ListValue* args) {
    ++message_requiring_gesture_count_;
  }

  void OnNotifyFinish(const base::ListValue* args) {
    if (finish_closure_)
      finish_closure_.Run();
  }

  void OnSendMessase(const base::ListValue* args) {
    // This message will be invoked when WebContents changes the main RFH
    // and the old main RFH is still alive during navigating from WebUI page
    // to cross-site. WebUI message should be handled with old main RFH.

    if (send_message_closure_)
      std::move(send_message_closure_).Run();

    // AllowJavascript should not have a CHECK crash.
    AllowJavascript();

    // WebUI::CallJavascriptFunctionUnsafe should be run with old main RFH.
    web_ui()->CallJavascriptFunctionUnsafe("test");

    if (finish_closure_)
      std::move(finish_closure_).Run();
  }

  int message_requiring_gesture_count_ = 0;
  base::RepeatingClosure finish_closure_;
  base::OnceClosure send_message_closure_;
};

class WebUIRequiringGestureBrowserTest : public ContentBrowserTest {
 public:
  WebUIRequiringGestureBrowserTest() {
    clock_.SetNowTicks(base::TimeTicks::Now());
    ui::SetEventTickClockForTesting(&clock_);
  }

  ~WebUIRequiringGestureBrowserTest() override {
    ui::SetEventTickClockForTesting(nullptr);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(NavigateToURL(web_contents(), GetWebUIURL(kChromeUIGpuHost)));
    test_handler_ = new TestWebUIMessageHandler();
    web_contents()->GetWebUI()->AddMessageHandler(
        base::WrapUnique(test_handler_));
  }

 protected:
  void SendMessageAndWaitForFinish() {
    main_rfh()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("chrome.send('messageRequiringGesture');"
                           "chrome.send('notifyFinish');"),
        base::NullCallback());
    base::RunLoop run_loop;
    test_handler()->set_finish_closure(run_loop.QuitClosure());
    run_loop.Run();
  }

  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

  WebContents* web_contents() { return shell()->web_contents(); }
  RenderFrameHost* main_rfh() { return web_contents()->GetMainFrame(); }

  TestWebUIMessageHandler* test_handler() { return test_handler_; }

 private:
  base::SimpleTestTickClock clock_;

  // Owned by the WebUI associated with the WebContents.
  TestWebUIMessageHandler* test_handler_ = nullptr;
};

}  // namespace

// Tests that navigating between WebUIs of different types results in
// SiteInstance swap when running in process-per-tab process model.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnDifferenteWebUITypes) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerTab);
  WebContents* web_contents = shell()->web_contents();

  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(ContentWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  int32_t orig_browsing_instance_id =
      orig_site_instance->GetBrowsingInstanceId();

  // Navigate to a different WebUI type and ensure that the SiteInstance
  // has changed and the new process also has WebUI bindings.
  const GURL web_ui_url2(GetWebUIURL(kChromeUIGpuHost));
  EXPECT_TRUE(ContentWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url2));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url2));
  auto* new_site_instance = web_contents->GetSiteInstance();
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_NE(orig_browsing_instance_id,
            new_site_instance->GetBrowsingInstanceId());
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));
}

// Tests that a WebUI page will use a separate SiteInstance when we navigated to
// it from the initial blank page.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest,
                       ForceBrowsingInstanceSwapOnFirstNavigation) {
  WebContents* web_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  // Navigate from the initial blank page to the WebUI URL.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(ContentWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));

  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));
  auto* new_site_instance = web_contents->GetSiteInstance();
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_FALSE(orig_site_instance->IsRelatedSiteInstance(new_site_instance));
}

// Tests that navigating from chrome:// to chrome-untrusted:// results in
// SiteInstance swap.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnFromChromeToUntrusted) {
  WebContents* web_contents = shell()->web_contents();
  AddUntrustedDataSource(web_contents->GetBrowserContext(), "test-host");

  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(ContentWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));

  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  int32_t orig_browsing_instance_id =
      orig_site_instance->GetBrowsingInstanceId();

  // Navigate to chrome-untrusted:// and ensure that the SiteInstance
  // has changed and the new process has no WebUI bindings.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            GetChromeUntrustedUIURL("test-host/title1.html")));
  auto* new_site_instance = web_contents->GetSiteInstance();
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_NE(orig_browsing_instance_id,
            new_site_instance->GetBrowsingInstanceId());
  EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));
}

// Tests that navigating from chrome-untrusted:// to chrome:// results in
// SiteInstance swap.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, ForceSwapOnFromUntrustedToChrome) {
  WebContents* web_contents = shell()->web_contents();
  AddUntrustedDataSource(web_contents->GetBrowserContext(), "test-host");

  ASSERT_TRUE(NavigateToURL(web_contents,
                            GetChromeUntrustedUIURL("test-host/title1.html")));
  EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));

  // Capture the SiteInstance before navigating for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      web_contents->GetSiteInstance());
  int32_t orig_browsing_instance_id =
      orig_site_instance->GetBrowsingInstanceId();

  // Navigate to a WebUI and ensure that the SiteInstance has changed and the
  // new process has WebUI bindings.
  const GURL web_ui_url(GetWebUIURL(kChromeUIHistogramHost));
  EXPECT_TRUE(ContentWebUIControllerFactory::GetInstance()->UseWebUIForURL(
      web_contents->GetBrowserContext(), web_ui_url));

  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));
  auto* new_site_instance = web_contents->GetSiteInstance();
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_NE(orig_browsing_instance_id,
            new_site_instance->GetBrowsingInstanceId());
  EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
      web_contents->GetMainFrame()->GetProcess()->GetID()));
}

IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, SameDocumentNavigationsAndReload) {
  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, GetWebUIURL(kChromeUIHistogramHost)));

  WebUIMessageHandler* test_handler = new TestWebUIMessageHandler;
  web_contents->GetWebUI()->AddMessageHandler(base::WrapUnique(test_handler));
  test_handler->AllowJavascriptForTesting();

  // Push onto window.history. Back should now be an in-page navigation.
  ASSERT_TRUE(ExecuteScript(web_contents,
                            "window.history.pushState({}, '', 'foo.html')"));
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Test handler should still have JavaScript allowed after in-page navigation.
  EXPECT_TRUE(test_handler->IsJavascriptAllowed());

  shell()->Reload();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Verify that after a reload, the test handler has been disallowed.
  EXPECT_FALSE(test_handler->IsJavascriptAllowed());
}

// A WebUI message that should require a user gesture is ignored if there is no
// recent input event.
IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoredIfNoGesture) {
  SendMessageAndWaitForFinish();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoresRendererOnlyGesture) {
  // Note: this doesn't use SendMessageAndWaitForFinish() since this test needs
  // to use a test-only helper to instantiate a scoped user gesture in the
  // renderer.
  main_rfh()->ExecuteJavaScriptWithUserGestureForTests(
      base::ASCIIToUTF16("chrome.send('messageRequiringGesture');"
                         "chrome.send('notifyFinish');"));
  base::RunLoop run_loop;
  test_handler()->set_finish_closure(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureIgnoresNonInteractiveEvents) {
  // Mouse enter / mouse move / mouse leave should not be considered input
  // events that interact with the page.
  content::SimulateMouseEvent(web_contents(),
                              blink::WebInputEvent::Type::kMouseEnter,
                              gfx::Point(50, 50));
  content::SimulateMouseEvent(web_contents(),
                              blink::WebInputEvent::Type::kMouseMove,
                              gfx::Point(50, 50));
  content::SimulateMouseEvent(web_contents(),
                              blink::WebInputEvent::Type::kMouseLeave,
                              gfx::Point(50, 50));
  // Nor should mouse wheel.
  content::SimulateMouseWheelEvent(web_contents(), gfx::Point(50, 50),
                                   gfx::Vector2d(0, 100),
                                   blink::WebMouseWheelEvent::kPhaseBegan);
  SendMessageAndWaitForFinish();
  EXPECT_EQ(0, test_handler()->message_requiring_gesture_count());
}

IN_PROC_BROWSER_TEST_F(WebUIRequiringGestureBrowserTest,
                       MessageRequiringGestureAllowedWithInteractiveEvent) {
  // Simulate a click at Now.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebMouseEvent::Button::kLeft);

  // Now+0 should be allowed.
  SendMessageAndWaitForFinish();
  EXPECT_EQ(1, test_handler()->message_requiring_gesture_count());

  // Now+5 seconds should be allowed.
  AdvanceClock(base::TimeDelta::FromSeconds(5));
  SendMessageAndWaitForFinish();
  EXPECT_EQ(2, test_handler()->message_requiring_gesture_count());

  // Anything after that should be disallowed though.
  AdvanceClock(base::TimeDelta::FromMicroseconds(1));
  SendMessageAndWaitForFinish();
  EXPECT_EQ(2, test_handler()->message_requiring_gesture_count());
}

// Verify that we can successfully navigate to a chrome-untrusted:// URL.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, UntrustedSchemeLoads) {
  auto* web_contents = shell()->web_contents();
  AddUntrustedDataSource(web_contents->GetBrowserContext(), "test-host");

  const GURL untrusted_url(GetChromeUntrustedUIURL("test-host/title2.html"));
  EXPECT_TRUE(NavigateToURL(web_contents, untrusted_url));
  EXPECT_EQ(base::ASCIIToUTF16("Title Of Awesomeness"),
            web_contents->GetTitle());
}

// Verify that we can successfully navigate to a chrome-untrusted:// URL
// without a crash while WebUI::Send is being performed.
IN_PROC_BROWSER_TEST_F(WebUIImplBrowserTest, NavigateWhileWebUISend) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, GetWebUIURL(kChromeUIGpuHost)));

  auto* test_handler = new TestWebUIMessageHandler;
  web_contents->GetWebUI()->AddMessageHandler(base::WrapUnique(test_handler));

  auto* webui = static_cast<WebUIImpl*>(web_contents->GetWebUI());
  EXPECT_EQ(web_contents->GetMainFrame(), webui->frame_host_for_test());

  test_handler->set_finish_closure(base::BindLambdaForTesting([&]() {
    EXPECT_NE(web_contents->GetMainFrame(), webui->frame_host_for_test());
  }));

  bool received_send_message = false;
  test_handler->set_send_message_closure(
      base::BindLambdaForTesting([&]() { received_send_message = true; }));

  base::RunLoop run_loop;
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("onunload=function() { chrome.send('sendMessage')}"),
      base::BindOnce([](base::OnceClosure callback,
                        base::Value) { std::move(callback).Run(); },
                     run_loop.QuitClosure()));
  run_loop.Run();

  RenderFrameDeletedObserver delete_observer(web_contents->GetMainFrame());
  EXPECT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("/simple_page.html")));
  delete_observer.WaitUntilDeleted();

  EXPECT_TRUE(received_send_message);
}

class WebUIRequestSchemesTest : public ContentBrowserTest {
 public:
  WebUIRequestSchemesTest() {
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  ~WebUIRequestSchemesTest() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  WebUIRequestSchemesTest(const WebUIRequestSchemesTest&) = delete;

  WebUIRequestSchemesTest& operator=(const WebUIRequestSchemesTest&) = delete;

  TestWebUIControllerFactory* factory() { return &factory_; }

 private:
  TestWebUIControllerFactory factory_;
};

// Verify that by default WebUI's child process security policy can request
// default schemes such as chrome.
//
// ChildProcessSecurityPolicy::CanRequestURL() always returns true for the
// following schemes, but in practice there are other checks that stop WebUIs
// from accessing these schemes.
IN_PROC_BROWSER_TEST_F(WebUIRequestSchemesTest, DefaultSchemesCanBeRequested) {
  auto* web_contents = shell()->web_contents();

  std::string host_and_path = "test-host/title2.html";
  const GURL chrome_url(GetWebUIURL(host_and_path));
  GURL url;

  std::vector<std::string> requestable_schemes = {
      // WebSafe Schemes:
      "feed", url::kHttpScheme, url::kHttpsScheme, url::kFtpScheme,
      url::kDataScheme, url::kWsScheme, url::kWssScheme,
      // Default added as requestable schemes:
      url::kFileScheme, kChromeUIScheme};

  std::vector<std::string> unrequestable_schemes = {
      kChromeDevToolsScheme, url::kBlobScheme, kChromeUIUntrustedScheme,
      base::StrCat({url::kFileSystemScheme, ":", kChromeUIUntrustedScheme})};

  ASSERT_TRUE(NavigateToURL(web_contents, chrome_url));

  for (const auto& requestable_scheme : requestable_schemes) {
    url = GURL(base::StrCat(
        {requestable_scheme, url::kStandardSchemeSeparator, host_and_path}));
    EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
        web_contents->GetMainFrame()->GetProcess()->GetID(), url));
  }

  for (const auto& unrequestable_scheme : unrequestable_schemes) {
    url = GURL(base::StrCat(
        {unrequestable_scheme, url::kStandardSchemeSeparator, host_and_path}));
    EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
        web_contents->GetMainFrame()->GetProcess()->GetID(), url));
  }
}

// Verify that we can successfully allow non-default URL schemes to
// be requested by the WebUI's child process security policy.
IN_PROC_BROWSER_TEST_F(WebUIRequestSchemesTest,
                       AllowAdditionalSchemesToBeRequested) {
  auto* web_contents = shell()->web_contents();

  std::string host_and_path = "test-host/title2.html";
  GURL url;

  // All URLs with a web safe scheme, or with a scheme not
  // handled by ContentBrowserClient are requestable. All other schemes are
  // not requestable.
  std::vector<std::string> requestable_schemes = {
      // WebSafe schemes:
      "feed",
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kFtpScheme,
      url::kDataScheme,
      url::kWsScheme,
      url::kWssScheme,
      // Default added as requestable schemes:
      "file",
      kChromeUIScheme,
      // Schemes given requestable access:
      kChromeUIUntrustedScheme,
      base::StrCat({url::kFileSystemScheme, ":", kChromeUIUntrustedScheme}),
  };
  std::vector<std::string> unrequestable_schemes = {
      kChromeDevToolsScheme, url::kBlobScheme,
      base::StrCat({url::kFileSystemScheme, ":", kChromeDevToolsScheme})};

  const GURL chrome_ui_url = GetWebUIURL(base::StrCat(
      {host_and_path, "?requestableschemes=", kChromeUIUntrustedScheme, ",",
       url::kWsScheme}));

  ASSERT_TRUE(NavigateToURL(web_contents, chrome_ui_url));

  for (const auto& requestable_scheme : requestable_schemes) {
    url = GURL(base::StrCat(
        {requestable_scheme, url::kStandardSchemeSeparator, host_and_path}));
    EXPECT_TRUE(ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
        web_contents->GetMainFrame()->GetProcess()->GetID(), url));
  }

  for (const auto& unrequestable_scheme : unrequestable_schemes) {
    url = GURL(base::StrCat(
        {unrequestable_scheme, url::kStandardSchemeSeparator, host_and_path}));
    EXPECT_FALSE(ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
        web_contents->GetMainFrame()->GetProcess()->GetID(), url));
  }
}

class WebUIWorkerTest : public ContentBrowserTest {
 public:
  WebUIWorkerTest() { WebUIControllerFactory::RegisterFactory(&factory_); }

  ~WebUIWorkerTest() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  WebUIWorkerTest(const WebUIWorkerTest&) = delete;

  WebUIWorkerTest& operator=(const WebUIWorkerTest&) = delete;

 private:
  TestWebUIControllerFactory factory_;
};

// TODO(crbug.com/154571): Shared workers are not available on Android.
#if !defined(OS_ANDROID)
// Verify that we can create SharedWorker with scheme "chrome://" under
// WebUI page.
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest, CanCreateWebUISharedWorkerForWebUI) {
  const GURL web_ui_url =
      GURL(GetWebUIURL("test-host/title2.html?notrustedtypes=true"));
  const GURL web_ui_worker_url =
      GURL(GetWebUIURL("test-host/web_ui_shared_worker.js"));

  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, web_ui_url));

  EXPECT_EQ(true, EvalJs(web_contents,
                         JsReplace(kLoadSharedWorkerScript,
                                   web_ui_worker_url.spec().c_str()),
                         EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
}

// Verify that pages with scheme other than "chrome://" cannot create
// SharedWorker with scheme "chrome://".
IN_PROC_BROWSER_TEST_F(WebUIWorkerTest,
                       CannotCreateWebUISharedWorkerForNonWebUI) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL non_web_ui_url =
      GURL(embedded_test_server()->GetURL("/title1.html?notrustedtypes=true"));
  const GURL web_ui_worker_url =
      GURL(GetWebUIURL("test-host/web_ui_shared_worker.js"));

  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents, non_web_ui_url));

  auto result = EvalJs(
      web_contents,
      JsReplace(kLoadSharedWorkerScript, web_ui_worker_url.spec().c_str()),
      EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */);
  std::string expected_failure = R"(a JavaScript error:
Error: Failed to construct 'SharedWorker')";
  EXPECT_THAT(result.error, ::testing::StartsWith(expected_failure));
}
#endif  // !defined(OS_ANDROID)

}  // namespace content
