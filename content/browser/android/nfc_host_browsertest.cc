// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_nfc_overrider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace content {

class NFCHostBrowserTest : public ContentBrowserTest {
 public:
  NFCHostBrowserTest() {
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  }
  ~NFCHostBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server_.Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  // WebNFC needs HTTPS.
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(NFCHostBrowserTest, FencedFrameCannotCloseNFC) {
  device::ScopedNFCOverrider scoped_nfc_overrider;

  GURL main_url(https_server_.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Initialize NFC in the primary main frame.
  EXPECT_EQ("success", EvalJs(web_contents()->GetPrimaryMainFrame(), R"(
    const ndef = new NDEFReader();
    new Promise(async resolve => {
      try {
        await ndef.write("Hello");
        resolve('success');
      } catch (error) {
        resolve('failure');
      }
    });
  )"));

  // Ensure that fenced frame insertion cannot close the NFC connection.
  GURL inner_url(https_server_.GetURL("/fenced_frames/title1.html"));
  RenderFrameHost* fenced_frame_host = fenced_frame_helper_.CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), inner_url);
  EXPECT_NE(nullptr, fenced_frame_host);
  EXPECT_EQ(true, scoped_nfc_overrider.IsConnected());
}

IN_PROC_BROWSER_TEST_F(NFCHostBrowserTest, OpaqueOriginCannotUseNFC) {
  device::ScopedNFCOverrider scoped_nfc_overrider;

  // Navigate to an HTTPS page with CSP sandbox (opaque origin in a secure
  // context).
  GURL opaque_url(https_server_.GetURL("/sandbox_main_frame_csp.html"));
  EXPECT_TRUE(NavigateToURL(shell(), opaque_url));
  EXPECT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());

  EXPECT_THAT(
      EvalJs(web_contents()->GetPrimaryMainFrame(), R"(
    const ndef = new NDEFReader();
    ndef.write("Hello");
  )"),
      EvalJsResult::ErrorIs(testing::HasSubstr(
          "NotSupportedError: Web NFC is unavailable or permission denied.")));
  EXPECT_FALSE(scoped_nfc_overrider.IsConnected());
}

IN_PROC_BROWSER_TEST_F(NFCHostBrowserTest, IframeCannotUseNFC) {
  device::ScopedNFCOverrider scoped_nfc_overrider;

  GURL main_url(https_server_.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Insert a child iframe.
  GURL iframe_url(https_server_.GetURL("/title2.html"));
  EXPECT_TRUE(
      ExecJs(web_contents()->GetPrimaryMainFrame(), JsReplace(R"(
    const iframe = document.createElement('iframe');
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
                                                              iframe_url)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHost* child_rfh =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_NE(nullptr, child_rfh);

  EXPECT_THAT(EvalJs(child_rfh, R"(
    const ndef = new NDEFReader();
    ndef.write("Hello");
  )"),
              EvalJsResult::ErrorIs(testing::HasSubstr("InvalidStateError")));
  EXPECT_FALSE(scoped_nfc_overrider.IsConnected());
}

}  // namespace content
