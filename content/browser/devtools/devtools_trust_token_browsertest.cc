// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/trust_token_browsertest.h"

namespace content {

class DevToolsTrustTokenBrowsertest : public DevToolsProtocolTest,
                                      public TrustTokenBrowsertest {
 public:
  void SetUpOnMainThread() override {
    TrustTokenBrowsertest::SetUpOnMainThread();
    DevToolsProtocolTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    TrustTokenBrowsertest::TearDownOnMainThread();
    DevToolsProtocolTest::TearDownOnMainThread();
  }
};

// After a successful issuance and redemption, a subsequent redemption against
// the same issuer should hit the redemption record (RR) cache.
IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest,
                       RedemptionRecordCacheHitIsReportedAsLoadingFinished) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1. Navigate to a test site, request and redeem a trust token.
  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommand("Network.enable", std::make_unique<base::DictionaryValue>());

  // Make sure there are no existing DevTools events in the queue.
  EXPECT_EQ(notifications_.size(), 0ul);

  // 3) Issue another redemption, and verify its served from cache.
  EXPECT_EQ("NoModificationAllowedError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // 4) Verify the request is marked as successful and not as failed.
  WaitForNotification("Network.requestServedFromCache", true);
  WaitForNotification("Network.loadingFinished", true);
}

}  // namespace content
