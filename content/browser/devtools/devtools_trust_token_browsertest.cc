// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/network/trust_token_browsertest.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

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

  // The returned view is only valid until the next |SendCommand| call.
  const base::Value::List& GetTrustTokensViaProtocol() {
    SendCommandSync("Storage.getTrustTokens");
    const base::Value* tokens = result()->Find("tokens");
    CHECK(tokens);
    return tokens->GetList();
  }

  // Asserts that CDP reports |count| number of tokens for |issuerOrigin|.
  void AssertTrustTokensViaProtocol(const std::string& issuerOrigin,
                                    int expectedCount) {
    const base::Value::List& tokens = GetTrustTokensViaProtocol();
    EXPECT_GT(tokens.size(), 0ul);

    for (const auto& token : tokens) {
      const auto& token_dict = token.GetDict();
      const std::string* issuer = token_dict.FindString("issuerOrigin");
      if (*issuer == issuerOrigin) {
        const std::optional<int> actualCount = token_dict.FindInt("count");
        EXPECT_THAT(actualCount, ::testing::Optional(expectedCount));
        return;
      }
    }
    FAIL() << "No trust tokens for issuer " << issuerOrigin;
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
        { privateToken: { version: 1, operation: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1, operation: 'token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommandSync("Network.enable");

  // Make sure there are no existing DevTools events in the queue.
  EXPECT_FALSE(HasExistingNotification());

  // 3) Issue another redemption, and verify its served from cache.
  EXPECT_EQ("NoModificationAllowedError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1, operation: 'token-redemption' } })
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // 4) Verify the request is marked as successful and not as failed.
  WaitForNotification("Network.requestServedFromCache", true);
  WaitForNotification("Network.loadingFinished", true);
  WaitForNotification("Network.trustTokenOperationDone", true);
}

namespace {

bool MatchStatus(const std::string& expected_status,
                 const base::Value::Dict& params) {
  const std::string* actual_status = params.FindString("status");
  return expected_status == *actual_status;
}

base::RepeatingCallback<bool(const base::Value::Dict&)> okStatusMatcher =
    base::BindRepeating(
        &MatchStatus,
        protocol::Network::TrustTokenOperationDone::StatusEnum::Ok);

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest, FetchEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1) Navigate to a test site.
  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommandSync("Network.enable");

  // 3) Request and redeem a token, then use the redeemed token in a Signing
  // request.
  std::string command = R"(
  (async () => {
    await fetch('/issue', {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch('/redeem', {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    await fetch('/sign', {privateToken: {version: 1,
                                       operation: 'send-redemption-record',
                                  issuers: [$1]}});
    return 'Success'; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  // 4) Verify that we received three successful events.
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
}

IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest, IframeEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1) Navigate to a test site.
  GURL start_url = server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommandSync("Network.enable");

  // 3) Request and redeem a token, then use the redeemed token in a Signing
  // request.
  std::string command = R"(
  (async () => {
    await fetch('/issue', {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch('/redeem', {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    return 'Success'; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  // 3) Request and redeem a token, then use the redeemed token in a Signing
  // request.
  auto execute_op_via_iframe = [&](std::string_view path,
                                   std::string_view trust_token) {
    // It's important to set the trust token arguments before updating src, as
    // the latter triggers a load.
    EXPECT_TRUE(ExecJs(
        shell(), JsReplace(
                     R"( const myFrame = document.getElementById('test_iframe');
                         myFrame.privateToken = $1;
                         myFrame.src = $2;)",
                     trust_token, path)));
    TestNavigationObserver load_observer(shell()->web_contents());
    load_observer.WaitForNavigationFinished();
  };

  execute_op_via_iframe("/sign", JsReplace(
                                     R"({"version": 1,
                       "operation": "send-redemption-record",
              "issuers": [$1]})",
                                     IssuanceOriginFromHost("a.test")));

  // 4) Verify that we received three successful events.
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
}

// When the server rejects issuance, DevTools gets a failed notification.
IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest,
                       FailedIssuanceFiresFailedOperationEvent) {
  TrustTokenRequestHandler::Options options;
  options.issuance_outcome =
      TrustTokenRequestHandler::ServerOperationOutcome::kUnconditionalFailure;
  request_handler_.UpdateOptions(std::move(options));

  // 1) Navigate to a test site.
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommandSync("Network.enable");

  // 3) Request some Trust Tokens.
  EXPECT_EQ("OperationError", EvalJs(shell(), R"(fetch('/issue',
        { privateToken: { version: 1, operation: 'token-request' } })
        .then(()=>'Success').catch(err => err.name); )"));

  // 4) Verify that we received an Trust Token operation failed event.
  WaitForMatchingNotification(
      "Network.trustTokenOperationDone",
      base::BindRepeating(
          &MatchStatus,
          protocol::Network::TrustTokenOperationDone::StatusEnum::BadResponse));
}

IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest, GetTrustTokens) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1) Navigate to a test site.
  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools.
  Attach();

  // 3) Call Storage.getTrustTokens and expect none to be there.
  EXPECT_EQ(GetTrustTokensViaProtocol().size(), 0ul);

  // 4) Request some Trust Tokens.
  std::string command = R"(
  (async () => {
    await fetch('/issue', {privateToken: {version: 1,
                                        operation: 'token-request'}});
    return 'Success'; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  // 5) Call Storage.getTrustTokens and expect a Trust Token to be there.
  EXPECT_EQ(GetTrustTokensViaProtocol().size(), 1ul);
}

IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest, ClearTrustTokens) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1) Navigate to a test site.
  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools.
  Attach();

  // 3) Request some Trust Tokens.
  std::string command = R"(
  (async () => {
    await fetch('/issue', {privateToken: {version: 1,
                                        operation: 'token-request'}});
    return 'Success'; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  // 4) Call Storage.getTrustTokens and expect a Trust Token to be there.
  AssertTrustTokensViaProtocol(IssuanceOriginFromHost("a.test"), 10);

  // 5) Call Storage.clearTrustTokens
  base::Value::Dict params;
  params.Set("issuerOrigin", IssuanceOriginFromHost("a.test"));
  auto* result = SendCommandSync("Storage.clearTrustTokens", std::move(params));

  EXPECT_THAT(result->FindBool("didDeleteTokens"), ::testing::Optional(true));

  // 6) Call Storage.getTrustTokens and expect no Trust Tokens to be there.
  //    Note that we still get an entry for our 'issuerOrigin', but the actual
  //    Token count must be 0.
  AssertTrustTokensViaProtocol(IssuanceOriginFromHost("a.test"), 0);
}
}  // namespace content
