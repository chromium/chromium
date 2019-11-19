// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "media/base/media_switches.h"

namespace {

// This class intercepts media access request from the embedder. The request
// should be triggered only if the embedder API (from tests) allows the request
// in Javascript.
// We do not issue the actual media request; the fact that the request reached
// embedder's WebContents is good enough for our tests. This is also to make
// the test run successfully on trybots.
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() : requested_(false), checked_(false) {}
  ~MockWebContentsDelegate() override {}

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    requested_ = true;
    if (request_message_loop_runner_.get())
      request_message_loop_runner_->Quit();
  }

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override {
    checked_ = true;
    if (check_message_loop_runner_.get())
      check_message_loop_runner_->Quit();
    return true;
  }

  void WaitForRequestMediaPermission() {
    if (requested_)
      return;
    request_message_loop_runner_ = new content::MessageLoopRunner;
    request_message_loop_runner_->Run();
  }

  void WaitForCheckMediaPermission() {
    if (checked_)
      return;
    check_message_loop_runner_ = new content::MessageLoopRunner;
    check_message_loop_runner_->Run();
  }

 private:
  bool requested_;
  bool checked_;
  scoped_refptr<content::MessageLoopRunner> request_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> check_message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockWebContentsDelegate);
};

}  // namespace

namespace extensions {

class WebViewMediaAccessAPITest : public WebViewAPITest {
 protected:
  WebViewMediaAccessAPITest() {}

  // Runs media_access tests.
  void RunTest(const std::string& test_name) {
    ExtensionTestMessageListener test_run_listener("TEST_PASSED", false);
    test_run_listener.set_failure_message("TEST_FAILED");
    EXPECT_TRUE(content::ExecuteScript(
        embedder_web_contents_,
        base::StringPrintf("runTest('%s');", test_name.c_str())));
    ASSERT_TRUE(test_run_listener.WaitUntilSatisfied());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable fake devices to make sure there is at least one device in the
    // system. Otherwise, this test would fail on machines without physical
    // media devices since getUserMedia fails early in those cases.
    WebViewAPITest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }
};

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllow) {
  std::string app_location = "web_view/media_access/allow";
  StartTestServer(app_location);
  LaunchApp(app_location);

  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllow");

  mock->WaitForRequestMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowAndThenDeny) {
  std::string app_location = "web_view/media_access/allow";
  StartTestServer(app_location);
  LaunchApp(app_location);

  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowAndThenDeny");

  mock->WaitForRequestMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowAsync) {
  std::string app_location = "web_view/media_access/allow";
  StartTestServer(app_location);
  LaunchApp(app_location);

  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowAsync");

  mock->WaitForRequestMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowTwice) {
  std::string app_location = "web_view/media_access/allow";
  StartTestServer(app_location);
  LaunchApp(app_location);

  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowTwice");

  mock->WaitForRequestMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestCheck) {
  std::string app_location = "web_view/media_access/check";
  StartTestServer(app_location);
  LaunchApp(app_location);

  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testCheck");

  mock->WaitForCheckMediaPermission();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDeny) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testDeny");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDenyThenAllowThrows) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testDenyThenAllowThrows");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDenyWithPreventDefault) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testDenyWithPreventDefault");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestNoListenersImplyDeny) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testNoListenersImplyDeny");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest,
                       TestNoPreventDefaultImpliesDeny) {
  std::string app_location = "web_view/media_access/deny";
  StartTestServer(app_location);
  LaunchApp(app_location);

  RunTest("testNoPreventDefaultImpliesDeny");
  StopTestServer();
}

}  // namespace extensions
