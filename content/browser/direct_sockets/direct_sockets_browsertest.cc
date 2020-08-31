// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

class DirectSocketsBrowserTest : public ContentBrowserTest {
 public:
  DirectSocketsBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kDirectSockets);
  }
  ~DirectSocketsBrowserTest() override = default;

  GURL GetTestPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/index.html");
  }

 protected:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_Success) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const blink::mojom::DirectSocketOptions&) { return net::OK; }));

  // TODO(crbug.com/905818): Use port from a listening net::TCPServerSocket.
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d})", 0);

  EXPECT_EQ("openTcp succeeded", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_NotAllowedError) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  // TODO(crbug.com/905818): Use port from a listening net::TCPServerSocket.
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d})", 0);

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_CannotEvadeCors) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  // HTTPS uses port 443.
  const std::string script =
      "openTcp({remoteAddress: '127.0.0.1', remotePort: 443})";

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenUdp_Success) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindLambdaForTesting(
          [&](const blink::mojom::DirectSocketOptions&) { return net::OK; }));

  // TODO(crbug.com/1119620): Use port from a listening net::UDPServerSocket.
  const std::string script = base::StringPrintf(
      "openUdp({remoteAddress: '127.0.0.1', remotePort: %d})", 0);

  EXPECT_EQ("openUdp succeeded", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenUdp_NotAllowedError) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  // TODO(crbug.com/1119620): Use port from a listening net::UDPServerSocket.
  const std::string script = base::StringPrintf(
      "openUdp({remoteAddress: '127.0.0.1', remotePort: %d})", 0);

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenUdp_CannotEvadeCors) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  // QUIC uses port 443.
  const std::string script =
      "openUdp({remoteAddress: '127.0.0.1', remotePort: 443})";

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

}  // namespace content
