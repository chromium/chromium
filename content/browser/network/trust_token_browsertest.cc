// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/trust_token_browsertest.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_server_handler_registration.h"
#include "services/network/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon_stdstring.h"

namespace content {

namespace {

using network::test::TrustTokenRequestHandler;
using SignedRequest = network::test::TrustTokenSignedRequest;
using ::testing::AllOf;
using ::testing::DescribeMatcher;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsSubsetOf;
using ::testing::Not;
using ::testing::Optional;
using ::testing::StrEq;
using ::testing::Truly;

MATCHER_P(HasHeader, name, base::StringPrintf("Has header %s", name)) {
  if (!arg.headers.HasHeader(name)) {
    *result_listener << base::StringPrintf("%s wasn't present", name);
    return false;
  }
  *result_listener << base::StringPrintf("%s was present", name);
  return true;
}

MATCHER_P2(HasHeader,
           name,
           other_matcher,
           "has header " + std::string(name) + " that " +
               DescribeMatcher<std::string>(other_matcher)) {
  std::optional<std::string> header = arg.headers.GetHeader(name);
  if (!header) {
    *result_listener << base::StringPrintf("%s wasn't present", name);
    return false;
  }
  return ExplainMatchResult(other_matcher, *header, result_listener);
}

MATCHER(
    ReflectsSigningFailure,
    "The given signed request reflects a client-side signing failure, having "
    "an empty redemption record and no other related headers.") {
  return ExplainMatchResult(
      AllOf(HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord,
                      StrEq("")),
            Not(HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))),
      arg, result_listener);
}

}  // namespace

TrustTokenBrowsertest::TrustTokenBrowsertest() {
  auto& field_trial_param =
      network::features::kTrustTokenOperationsRequiringOriginTrial;
  features_.InitAndEnableFeatureWithParameters(
      network::features::kPrivateStateTokens,
      {{field_trial_param.name,
        field_trial_param.GetName(network::features::TrustTokenOriginTrialSpec::
                                      kOriginTrialNotRequired)}});
}

void TrustTokenBrowsertest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");

  server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  server_.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));

  SetupCrossSiteRedirector(embedded_test_server());
  SetupCrossSiteRedirector(&server_);

  network::test::RegisterTrustTokenTestHandlers(&server_, &request_handler_);

  TrustTokenBrowsertest::Observe(shell()->web_contents());

  ASSERT_TRUE(server_.Start());
}

void TrustTokenBrowsertest::ProvideRequestHandlerKeyCommitmentsToNetworkService(
    std::vector<std::string_view> hosts) {
  base::flat_map<url::Origin, std::string_view> origins_and_commitments;
  std::string key_commitments = request_handler_.GetKeyCommitmentRecord();

  // TODO(davidvc): This could be extended to make the request handler aware
  // of different origins, which would allow using different key commitments
  // per origin.
  for (std::string_view host : hosts) {
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    origins_and_commitments.insert_or_assign(
        url::Origin::Create(server_.base_url().ReplaceComponents(replacements)),
        key_commitments);
  }

  if (origins_and_commitments.empty()) {
    origins_and_commitments = {
        {url::Origin::Create(server_.base_url()), key_commitments}};
  }

  base::RunLoop run_loop;
  GetNetworkService()->SetTrustTokenKeyCommitments(
      network::WrapKeyCommitmentsForIssuers(std::move(origins_and_commitments)),
      run_loop.QuitClosure());
  run_loop.Run();
}

std::string TrustTokenBrowsertest::IssuanceOriginFromHost(
    const std::string& host) const {
  auto ret = url::Origin::Create(server_.GetURL(host, "/")).Serialize();
  return ret;
}

void TrustTokenBrowsertest::OnTrustTokensAccessed(
    RenderFrameHost* render_frame_host,
    const TrustTokenAccessDetails& details) {
  access_count_++;
}

void TrustTokenBrowsertest::OnTrustTokensAccessed(
    NavigationHandle* navigation_handle,
    const TrustTokenAccessDetails& details) {
  access_count_++;
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, FetchEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch("/redeem", {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    await fetch("/sign", {privateToken: {version: 1,
                                       operation: 'send-redemption-record',
                                       issuers: [$1]}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))));

  // Expect three accesses, one for issue, redeem, and sign.
  EXPECT_EQ(3, access_count_);
}

// Fetch is called directly from top level (a.test), issuer origin (b.test)
// is different from top frame origin.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, FetchEndToEndThirdParty) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"b.test"});

  const GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch($1, {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch($2, {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    await fetch($3, {privateToken: {version: 1,
                                       operation: 'send-redemption-record',
                                       issuers: [$4]}});
    return "Success"; })(); )";

  const std::string issuer_origin = IssuanceOriginFromHost("b.test");
  const std::string issuance_url = server_.GetURL("b.test", "/issue").spec();
  const std::string redemption_url = server_.GetURL("b.test", "/redeem").spec();
  const std::string signature_url = server_.GetURL("b.test", "/sign").spec();

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(command, issuance_url, redemption_url,
                                      signature_url, issuer_origin)));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))));

  // Expect three accesses, one for issue, redeem, and sign.
  EXPECT_EQ(3, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, XhrEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // If this isn't idiomatic JS, I don't know what is.
  std::string command = R"(
  (async () => {
    let request = new XMLHttpRequest();
    request.open('GET', '/issue');
    request.setPrivateToken({
      version: 1,
      operation: 'token-request'
    });
    let promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;

    request = new XMLHttpRequest();
    request.open('GET', '/redeem');
    request.setPrivateToken({
      version: 1,
      operation: 'token-redemption'
    });
    promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;

    request = new XMLHttpRequest();
    request.open('GET', '/sign');
    request.setPrivateToken({
      version: 1,
      operation: 'send-redemption-record',
      issuers: [$1]
    });
    promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;
    return "Success";
    })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))));

  // Expect three accesses, one for issue, redeem, and sign.
  EXPECT_EQ(3, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, IframeSendRedemptionRecord) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  std::string command = R"(
  (async () => {
    await fetch("/issue", {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch("/redeem", {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    return "Success";
  })())";

  GURL start_url = server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  EXPECT_EQ("Success", EvalJs(shell(), command));

  auto execute_op_via_iframe = [&](std::string_view path,
                                   std::string_view trust_token) {
    // It's important to set the trust token arguments before updating src, as
    // the latter triggers a load.
    EXPECT_TRUE(ExecJs(
        shell(), JsReplace(
                     R"( const myFrame = document.getElementById("test_iframe");
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

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))));

  // Expect three accesses, one for issue, redeem, and sign.
  EXPECT_EQ(3, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       IframeCanOnlySendRedemptionRecord) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  auto fail_to_execute_op_via_iframe = [&](std::string_view path,
                                           std::string_view trust_token) {
    // It's important to set the trust token arguments before updating src, as
    // the latter triggers a load.
    EXPECT_TRUE(ExecJs(
        shell(), JsReplace(
                     R"( const myFrame = document.getElementById("test_iframe");
                         myFrame.trustToken = $1;
                         myFrame.src = $2;)",
                     trust_token, path)));
    TestNavigationObserver load_observer(shell()->web_contents());
    load_observer.WaitForNavigationFinished();
  };

  fail_to_execute_op_via_iframe("/issue", R"({"type": "token-request"})");
  std::string command = JsReplace(R"(
  (async () => {
    return await document.hasPrivateToken($1);
  })();)",
                                  IssuanceOriginFromHost("a.test"));

  EXPECT_EQ(false, EvalJs(shell(), command));

  fail_to_execute_op_via_iframe("/redeem", R"({"type": "token-redemption"})");
  command = JsReplace(R"(
  (async () => {
    return document.hasRedemptionRecord($1);
  })();)",
                      IssuanceOriginFromHost("a.test"));
  EXPECT_EQ(false, EvalJs(shell(), command));

  fail_to_execute_op_via_iframe("/bad", R"({"type": "bad-type"})");
  command = JsReplace(R"(
  (async () => {
    return await document.hasPrivateToken($1)
    || document.hasRedemptionRecord($1);
  })();)",
                      IssuanceOriginFromHost("a.test"));
  EXPECT_EQ(false, EvalJs(shell(), command));

  // Expect zero accesses.
  EXPECT_EQ(0, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, HasTrustTokenAfterIssuance) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = JsReplace(R"(
  (async () => {
    await fetch("/issue", {privateToken: {version: 1,
                                        operation: 'token-request'}});
    return await document.hasPrivateToken($1);
  })();)",
                                  IssuanceOriginFromHost("a.test"));

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  //
  // Note: EvalJs's EXPECT_EQ type-conversion magic only supports the
  // "Yoda-style" EXPECT_EQ(expected, actual).
  EXPECT_EQ(true, EvalJs(shell(), command));

  // Expect one access for issue.
  EXPECT_EQ(1, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SigningWithNoRedemptionRecordDoesntCancelRequest) {
  TrustTokenRequestHandler::Options options;
  request_handler_.UpdateOptions(std::move(options));

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // This sign operation will fail, because we don't have a redemption record in
  // storage, a prerequisite. However, the failure shouldn't be fatal.
  std::string command = JsReplace(R"((async () => {
      await fetch("/sign", {privateToken: {version: 1,
                                         operation: 'send-redemption-record',
                                         issuers: [$1]}});
      return "Success";
      })(); )",
                                  IssuanceOriginFromHost("a.test"));

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));

  // Expect one access for sign.
  EXPECT_EQ(1, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, FetchEndToEndInIsolatedWorld) {
  // Ensure an isolated world can execute Trust Tokens operations when its
  // window's main world can. In particular, this ensures that the
  // redemtion-and-signing permissions policy is appropriately propagated by the
  // browser process.

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch("/redeem", {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    await fetch("/sign", {privateToken: {version: 1,
                                       operation: 'send-redemption-record',
                                  issuers: [$1]}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test")),
             EXECUTE_SCRIPT_DEFAULT_OPTIONS,
             /*world_id=*/30));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))));

  // Expect three accesses, one for issue, redeem, and sign.
  EXPECT_EQ(3, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RecordsTimers) {
  base::HistogramTester histograms;

  // |completion_waiter| adds a synchronization point so that we can
  // safely fetch all of the relevant histograms from the network process.
  //
  // Without this, there's a race between the fetch() promises resolving and the
  // NetErrorForTrustTokenOperation histogram being logged. This likely has no
  // practical impact during normal operation, but it makes this test flake: see
  // https://crbug.com/1165862.
  //
  // The URLLoaderInterceptor's completion callback receives its
  // URLLoaderCompletionStatus from URLLoaderClient::OnComplete, which happens
  // after CorsURLLoader::NotifyCompleted, which records the final histogram.
  base::RunLoop run_loop;
  content::URLLoaderInterceptor completion_waiter(
      base::BindRepeating([](URLLoaderInterceptor::RequestParams*) {
        return false;  // Don't intercept outbound requests.
      }),
      base::BindLambdaForTesting(
          [&run_loop](const GURL& url,
                      const network::URLLoaderCompletionStatus& status) {
            if (url.spec().find("sign") != std::string::npos)
              run_loop.Quit();
          }),
      /*ready_callback=*/base::NullCallback());

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch("/redeem", {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    await fetch("/sign", {privateToken: {version: 1,
                                       operation: 'send-redemption-record',
                                  issuers: [$1]}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  run_loop.Run();
  content::FetchHistogramsFromChildProcesses();

  // Just check that the timers were populated: since we can't mock a clock in
  // this browser test, it's hard to check the recorded values for
  // reasonableness.
  for (const std::string& op : {"Issuance", "Redemption", "Signing"}) {
    histograms.ExpectTotalCount(
        "Net.TrustTokens.OperationBeginTime.Success." + op, 1);
    histograms.ExpectTotalCount(
        "Net.TrustTokens.OperationTotalTime.Success." + op, 1);
    histograms.ExpectTotalCount(
        "Net.TrustTokens.OperationServerTime.Success." + op, 1);
    histograms.ExpectTotalCount(
        "Net.TrustTokens.OperationFinalizeTime.Success." + op, 1);
    histograms.ExpectUniqueSample(
        "Net.TrustTokens.NetErrorForTrustTokenOperation.Success." + op, net::OK,
        1);
  }

  // Expect three accesses, one for issue, redeem, and sign.
  EXPECT_EQ(3, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RecordsNetErrorCodes) {
  // Verify that the Net.TrustTokens.NetErrorForTrustTokenOperation.* metrics
  // record successfully by testing two "success" cases where there's an
  // unrelated net stack error and one case where the Trust Tokens operation
  // itself fails.
  base::HistogramTester histograms;

  ProvideRequestHandlerKeyCommitmentsToNetworkService(
      {"no-cert-for-this.domain"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  EXPECT_THAT(
      EvalJs(shell(), JsReplace(
                          R"(fetch($1, {privateToken: {
                                            version: 1,
                                            operation: 'token-request'}})
                   .then(() => "Unexpected success!")
                   .catch(err => err.message);)",
                          IssuanceOriginFromHost("no-cert-for-this.domain")))
          .ExtractString(),
      HasSubstr("Failed to fetch"));

  EXPECT_THAT(
      EvalJs(shell(), JsReplace(
                          R"(fetch($1, {privateToken: {
                                         version: 1,
                                         operation: 'send-redemption-record',
                 issuers: ['https://nonexistent-issuer.example']}})
                   .then(() => "Unexpected success!")
                   .catch(err => err.message);)",
                          IssuanceOriginFromHost("no-cert-for-this.domain")))
          .ExtractString(),
      HasSubstr("Failed to fetch"));

  content::FetchHistogramsFromChildProcesses();

  // "Success" since we executed the outbound half of the Trust Tokens
  // operation without issue:
  histograms.ExpectUniqueSample(
      "Net.TrustTokens.NetErrorForTrustTokenOperation.Success.Issuance",
      net::ERR_CERT_COMMON_NAME_INVALID, 1);

  // "Success" since signing can't fail:
  histograms.ExpectUniqueSample(
      "Net.TrustTokens.NetErrorForTrustTokenOperation.Success.Signing",
      net::ERR_CERT_COMMON_NAME_INVALID, 1);

  // Attempt a redemption against 'a.test'; we don't have a token for this
  // domain, so it should fail.
  EXPECT_EQ("InvalidStateError",
            EvalJs(shell(), JsReplace(
                                R"(fetch($1, {privateToken: {
                                            version: 1,
                                            operation: 'token-redemption'}})
                   .then(() => "Unexpected success!")
                   .catch(err => err.name);)",
                                IssuanceOriginFromHost("a.test"))));

  content::FetchHistogramsFromChildProcesses();

  histograms.ExpectUniqueSample(
      "Net.TrustTokens.NetErrorForTrustTokenOperation.Failure.Redemption",
      net::ERR_TRUST_TOKEN_OPERATION_FAILED, 1);

  // Expect three accesses, one for issue, redeem, and sign.
  EXPECT_EQ(3, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RecordsFetchFailureReasons) {
  // Verify that the Net.TrustTokens.NetErrorForFetchFailure.* metrics
  // record successfully by testing one case with a blocked resource, one case
  // with a generic net-stack failure, and one case with a Trust Tokens
  // operation failure.
  base::HistogramTester histograms;

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test", "b.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // This fetch will fail because we set `redirect: 'error'` and the
  // /cross-site/ URL will redirect the request.
  EXPECT_EQ("TypeError", EvalJs(shell(),
                                R"(fetch("/cross-site/b.test/issue", {
                                     redirect: 'error',
                                     privateToken: {version: 1,
                                                  operation: 'token-request'}
                                   })
                                   .then(() => "Unexpected success!")
                                   .catch(err => err.name);)"));

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectUniqueSample(
      "Net.TrustTokens.NetErrorForFetchFailure.Issuance", net::ERR_FAILED,
      /*expected_count=*/1);

  // Since issuance failed, there should be no tokens to redeem, so redemption
  // should fail:
  EXPECT_EQ("OperationError", EvalJs(shell(),
                                     R"(fetch("/redeem", {privateToken: {
                                            version: 1,
                                            operation: 'token-redemption'}})
                   .then(() => "Unexpected success!")
                   .catch(err => err.name);)"));

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectUniqueSample(
      "Net.TrustTokens.NetErrorForFetchFailure.Redemption",
      net::ERR_TRUST_TOKEN_OPERATION_FAILED,
      /*expected_count=*/1);

  // Execute a cross-site b.test -> a.test issuance that would succeed, were it
  // not for site b requiring CORP headers and none being present on the a.test
  // issuance response:
  ASSERT_TRUE(NavigateToURL(
      shell(),
      server_.GetURL("b.test",
                     "/cross-origin-opener-policy_redirect_final.html")));
  GURL site_a_issuance_url =
      GURL(IssuanceOriginFromHost("a.test")).Resolve("/issue");
  EXPECT_THAT(EvalJs(shell(), JsReplace(R"(fetch($1, {
  mode: 'no-cors',
                  privateToken: {version: 1,
                               operation: 'token-request'}})
                   .then(() => "Unexpected success!")
                   .catch(err => err.message);)",
                                        site_a_issuance_url))
                  .ExtractString(),
              HasSubstr("Failed to fetch"));

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectBucketCount(
      "Net.TrustTokens.NetErrorForFetchFailure.Issuance",
      net::ERR_BLOCKED_BY_RESPONSE,
      /*expected_count=*/1);

  // Expect three accesses, two for issue and one for redeem.
  EXPECT_EQ(3, access_count_);
}

// Trust Tokens should require that their executing contexts be secure.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, OperationsRequireSecureContext) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url =
      embedded_test_server()->GetURL("insecure.test", "/page_with_iframe.html");
  // Make sure that we are, in fact, using an insecure page.
  ASSERT_FALSE(network::IsUrlPotentiallyTrustworthy(start_url));

  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 1. Confirm that the Fetch interface doesn't work:
  std::string command =
      R"(fetch("/issue", {privateToken: {version: 1,
                                       operation: 'token-request'}})
           .catch(error => error.message);)";
  EXPECT_THAT(EvalJs(shell(), command).ExtractString(),
              HasSubstr("secure context"));

  // 2. Confirm that the XHR interface isn't present:
  EXPECT_EQ(false, EvalJs(shell(), "'setTrustToken' in (new XMLHttpRequest);"));

  // 3. Confirm that the iframe interface doesn't work by verifying that no
  // Trust Tokens operation gets executed.
  GURL issuance_url = server_.GetURL("/issue");
  URLLoaderMonitor monitor({issuance_url});
  // It's important to set the trust token arguments before updating src, as
  // the latter triggers a load.
  EXPECT_TRUE(ExecJs(
      shell(), JsReplace(
                   R"( const myFrame = document.getElementById("test_iframe");
                       myFrame.trustToken = $1;
                       myFrame.src = $2;)",
                   R"({"operation": "token-request"})", issuance_url)));
  monitor.WaitForUrls();
  EXPECT_THAT(monitor.GetRequestInfo(issuance_url),
              Optional(Field(&network::ResourceRequest::trust_token_params,
                             IsFalse())));

  // Expect zero accesses.
  EXPECT_EQ(0, access_count_);
}

// Issuance should fail if we don't have keys for the issuer at hand.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, IssuanceRequiresKeys) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService(
      {"not-the-right-server.example"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
    fetch('/issue', {privateToken: {version: 1,
                                  operation: 'token-request'}})
    .then(() => 'Success').catch(err => err.name); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("InvalidStateError", EvalJs(shell(), command));

  // Expect one access of issue.
  EXPECT_EQ(1, access_count_);
}

// When the server rejects issuance, the client-side issuance operation should
// fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       CorrectlyReportsServerErrorDuringIssuance) {
  TrustTokenRequestHandler::Options options;
  options.issuance_outcome =
      TrustTokenRequestHandler::ServerOperationOutcome::kUnconditionalFailure;
  request_handler_.UpdateOptions(std::move(options));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  EXPECT_EQ("OperationError", EvalJs(shell(), R"(fetch('/issue',
        { privateToken: { version: 1, operation: 'token-request' } })
        .then(()=>'Success').catch(err => err.name); )"));

  // Expect one access of issue.
  EXPECT_EQ(1, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, CrossOriginIssuanceWorks) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"sub1.b.test"});

  GURL start_url = server_.GetURL("sub2.b.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // Using GetURL to generate the issuance location is important
  // because it sets the port correctly.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(R"(
            fetch($1, { privateToken: { version: 1,
                                      operation: 'token-request' } })
            .then(()=>'Success'); )",
                                server_.GetURL("sub1.b.test", "/issue"))));

  // Expect one access of issue.
  EXPECT_EQ(1, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, CrossSiteIssuanceWorks) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("b.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // Using GetURL to generate the issuance location is important
  // because it sets the port correctly.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
            fetch($1, { privateToken: { version: 1,
                                      operation: 'token-request' } })
            .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  // Expect one access of issue.
  EXPECT_EQ(1, access_count_);
}

// Issuance should succeed only if the number of issuers associated with the
// requesting context's top frame origin is less than the limit on the number of
// such issuers.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       IssuanceRespectsAssociatedIssuersCap) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  static_assert(
      network::kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers < 10,
      "Consider rewriting this test for performance's sake if the "
      "number-of-issuers limit gets too large.");

  // Each hasPrivateStateToken call adds the provided issuer to the calling
  // context's list of associated issuers.
  for (int i = 0;
       i < network::kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers; ++i) {
    ASSERT_EQ("Success", EvalJs(shell(), "document.hasPrivateToken('https://a" +
                                             base::NumberToString(i) +
                                             ".test').then(()=>'Success');"));
  }

  EXPECT_EQ("OperationError", EvalJs(shell(), R"(
            fetch('/issue', { privateToken: { version: 1,
                                            operation: 'token-request' } })
            .then(() => 'Success').catch(error => error.name); )"));

  // Expect one access for issue.
  EXPECT_EQ(1, access_count_);
}

// When an issuance request is made in cors mode, a cross-origin redirect from
// issuer A to issuer B should result in a new issuance request to issuer B,
// obtaining issuer B tokens on success.
//
// Note: For more on the interaction between Trust Tokens and redirects, see the
// "Handling redirects" section in the design doc
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.5erfr3uo012t
IN_PROC_BROWSER_TEST_F(
    TrustTokenBrowsertest,
    CorsModeCrossOriginRedirectIssuanceUsesNewOriginAsIssuer) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test", "b.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(fetch($1, {privateToken: {
                                          version: 1,
                                          operation: 'token-request'}})
                             .then(() => "Success")
                             .catch(error => error.name);)";

  EXPECT_EQ(
      "Success",
      EvalJs(shell(),
             JsReplace(command,
                       server_.GetURL("a.test", "/cross-site/b.test/issue"))));

  EXPECT_EQ(true, EvalJs(shell(), JsReplace("document.hasPrivateToken($1);",
                                            IssuanceOriginFromHost("b.test"))));
  EXPECT_EQ(false,
            EvalJs(shell(), JsReplace("document.hasPrivateToken($1);",
                                      IssuanceOriginFromHost("a.test"))));

  // Expect two accesses for issues.
  EXPECT_EQ(2, access_count_);
}

// When an issuance request is made in no-cors mode, a cross-origin redirect
// from issuer A to issuer B should result in recycling the original issuance
// request, obtaining issuer A tokens on success.
//
// Note: For more on the interaction between Trust Tokens and redirects, see the
// "Handling redirects" section in the design doc
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.5erfr3uo012t
IN_PROC_BROWSER_TEST_F(
    TrustTokenBrowsertest,
    NoCorsModeCrossOriginRedirectIssuanceUsesOriginalOriginAsIssuer) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(fetch($1, {mode: 'no-cors',
                                      privateToken: {
                                          version: 1,
                                          operation: 'token-request'}})
                             .then(() => "Success")
                             .catch(error => error.name);)";

  EXPECT_EQ(
      "Success",
      EvalJs(shell(),
             JsReplace(command,
                       server_.GetURL("a.test", "/cross-site/b.test/issue"))));

  EXPECT_EQ(true, EvalJs(shell(), JsReplace("document.hasPrivateToken($1);",
                                            IssuanceOriginFromHost("a.test"))));
  EXPECT_EQ(false,
            EvalJs(shell(), JsReplace("document.hasPrivateToken($1);",
                                      IssuanceOriginFromHost("b.test"))));

  // Expect one access for issue.
  EXPECT_EQ(1, access_count_);
}

// Issuance from a context with a secure-but-non-HTTP/S top frame origin
// should fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       IssuanceRequiresSuitableTopFrameOrigin) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService();

  GURL file_url = GetTestUrl(/*dir=*/nullptr, "title1.html");
  ASSERT_TRUE(file_url.SchemeIsFile());

  ASSERT_TRUE(NavigateToURL(shell(), file_url));

  std::string command =
      R"(fetch($1, {privateToken: {version: 1,
                                 operation: 'token-request'}})
           .catch(error => error.name);)";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("InvalidStateError",
            EvalJs(shell(), JsReplace(command, server_.GetURL("/issue"))));

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));
  EXPECT_EQ(
      false,
      EvalJs(shell(),
             JsReplace("document.hasPrivateToken($1);",
                       url::Origin::Create(server_.base_url()).Serialize())));

  // Expect one access for issue.
  EXPECT_EQ(1, access_count_);
}

// Redemption from a secure-but-non-HTTP(S) top frame origin should fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RedemptionRequiresSuitableTopFrameOrigin) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command =
      R"(fetch("/issue", {privateToken: {version: 1,
                                       operation: 'token-request'}})
                             .then(() => "Success")
                             .catch(error => error.name);)";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  GURL file_url = GetTestUrl(/*dir=*/nullptr, "title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), file_url));

  // Redemption from a page with a file:// top frame origin should fail.
  command = R"(fetch($1, {privateToken: {version: 1,
                                       operation: 'token-redemption'}})
                 .catch(error => error.name);)";
  EXPECT_EQ(
      "InvalidStateError",
      EvalJs(shell(), JsReplace(command, server_.GetURL("a.test", "/redeem"))));

  // Expect two accesses, one for issue and one for redemption.
  EXPECT_EQ(2, access_count_);
}

// hasPrivateToken from a context with a secure-but-non-HTTP/S top frame
// origin should fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       HasTrustTokenRequiresSuitableTopFrameOrigin) {
  GURL file_url = GetTestUrl(/*dir=*/nullptr, "title1.html");
  ASSERT_TRUE(file_url.SchemeIsFile());
  ASSERT_TRUE(NavigateToURL(shell(), file_url));

  EXPECT_EQ("NotAllowedError",
            EvalJs(shell(),
                   R"(document.hasPrivateToken('https://issuer.example')
                              .catch(error => error.name);)"));

  EXPECT_EQ(0, access_count_);
}

// A hasPrivateToken call initiated from a secure context should succeed
// even if the initiating frame's origin is opaque (e.g. from a sandboxed
// iframe).
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       HasTrustTokenFromSecureSubframeWithOpaqueOrigin) {
  ASSERT_TRUE(NavigateToURL(
      shell(), server_.GetURL("a.test", "/page_with_sandboxed_iframe.html")));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ("Success",
            EvalJs(root->child_at(0)->current_frame_host(),
                   R"(document.hasPrivateToken('https://davids.website')
                              .then(()=>'Success');)"));

  EXPECT_EQ(0, access_count_);
}

// An operation initiated from a secure context should succeed even if the
// operation's associated request's initiator is opaque (e.g. from a sandboxed
// iframe with the right Permissions Policy).
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       OperationFromSecureSubframeWithOpaqueOrigin) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  ASSERT_TRUE(NavigateToURL(
      shell(), server_.GetURL("a.test", "/page_with_sandboxed_iframe.html")));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ("Success", EvalJs(root->child_at(0)->current_frame_host(),
                              JsReplace(R"(
                              fetch($1, {mode: 'no-cors',
                                         privateToken: {
                                             version: 1,
                                             operation: 'token-request'}
                                         }).then(()=>'Success');)",
                                        server_.GetURL("a.test", "/issue"))));

  // Expect one access for issue.
  EXPECT_EQ(1, access_count_);
}

// If a server issues with a key not present in the client's collection of key
// commitments, the issuance operation should fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, IssuanceWithAbsentKeyFails) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // Reset the handler, so that the client's valid keys disagree with the
  // server's keys. (This is theoretically flaky, but the chance of the client's
  // random keys colliding with the server's random keys is negligible.)
  request_handler_.UpdateOptions(TrustTokenRequestHandler::Options());

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command =
      R"(fetch($1, {privateToken: {version: 1,
                                                   operation: 'token-request'}})
                             .then(() => "Success")
                             .catch(error => error.name);)";
  EXPECT_EQ(
      "OperationError",
      EvalJs(shell(), JsReplace(command, server_.GetURL("a.test", "/issue"))));

  // Expect one access for issue.
  EXPECT_EQ(1, access_count_);
}

// This regression test for crbug.com/1111735 ensures it's possible to execute
// redemption from a nested same-origin frame that hasn't committed a
// navigation.
//
// How it works: The main frame embeds a same-origin iframe that does not
// commit a navigation (here, specifically because of an HTTP 204 return). From
// this iframe, we execute a Trust Tokens redemption operation via the iframe
// interface (in other words, the Trust Tokens operation executes during the
// process of navigating to a grandchild frame).  The grandchild frame's load
// will result in a renderer kill without the fix for the bug applied.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SignFromFrameLackingACommittedNavigation) {
  GURL start_url = server_.GetURL(
      "a.test", "/page-executing-trust-token-signing-from-204-subframe.html");

  // Execute a signing operation from a child iframe that has not committed a
  // navigation (see the html source).
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // For good measure, make sure the analogous signing operation works from
  // fetch, too, even though it wasn't broken by the same bug.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ("Success", EvalJs(root->child_at(0)->current_frame_host(),
                              JsReplace(R"(
                              fetch($1, {mode: 'no-cors',
                                         privateToken: {
                                             version: 1,
                                             operation: 'send-redemption-record',
                                             issuers: [
                                                 'https://issuer.example'
                                             ]}
                                         }).then(()=>'Success');)",
                                        server_.GetURL("a.test", "/issue"))));

  // Expect one access for sign.
  EXPECT_EQ(1, access_count_);
}

// Redemption should fail when there are no keys for the issuer.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RedemptionRequiresKeys) {
  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("InvalidStateError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // Expect one access for redemption.
  EXPECT_EQ(1, access_count_);
}

// Redemption should fail when there are no tokens to redeem.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RedemptionRequiresTokens) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("OperationError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // Expect one access for redemption.
  EXPECT_EQ(1, access_count_);
}

// When we have tokens for one issuer A, redemption against a different issuer B
// should still fail if we don't have any tokens for B.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RedemptionWithoutTokensForDesiredIssuerFails) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test", "b.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("OperationError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )",
                                      server_.GetURL("b.test", "/redeem"))));

  // Expect two accesses, one for issuance and one for redemption.
  EXPECT_EQ(2, access_count_);
}

// When the server rejects redemption, the client-side redemption operation
// should fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       CorrectlyReportsServerErrorDuringRedemption) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  EXPECT_EQ("Success", EvalJs(shell(), R"(fetch('/issue',
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )"));

  // Send a redemption request to the issuance endpoint, which should error out
  // for the obvious reason that it isn't an issuance request:
  EXPECT_EQ("OperationError", EvalJs(shell(), R"(fetch('/issue',
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )"));

  // Expect two accesses, one for issuance and one for redemption.
  EXPECT_EQ(2, access_count_);
}

// After a successful issuance and redemption, a subsequent redemption against
// the same issuer should hit the redemption record cache.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RedemptionHitsRedemptionRecordCache) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  EXPECT_EQ("NoModificationAllowedError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // Expect three accesses, one for issuance and two for redemption.
  EXPECT_EQ(3, access_count_);
}

// Redemption with `refresh-policy: 'refresh'` from an issuer context should
// succeed, overwriting the existing redemption record.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RefreshPolicyRefreshWorksInIssuerContext) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption',
                        refreshPolicy: 'refresh' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // Expect three accesses, one for issuance and two for redemption.
  EXPECT_EQ(3, access_count_);
}

// Redemption with `refresh-policy: 'refresh'` from a non-issuer context should
// still work.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RefreshPolicyRefreshRequiresIssuerContext) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"b.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  // Execute the operations against issuer https://b.test:<port> from a
  // different context; attempting to use refreshPolicy: 'refresh' should still
  // succeed.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("b.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("b.test", "/redeem"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption',
                        refreshPolicy: 'refresh' } })
        .then(()=>'Success').catch(err => err.name); )",
                                      server_.GetURL("b.test", "/redeem"))));

  // Expect three accesses, one for issuance and two for redemption.
  EXPECT_EQ(3, access_count_);
}

// When a redemption request is made in cors mode, a cross-origin redirect from
// issuer A to issuer B should result in a new redemption request to issuer B,
// failing if there are no issuer B tokens.
//
// Note: For more on the interaction between Trust Tokens and redirects, see the
// "Handling redirects" section in the design doc
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.5erfr3uo012t
IN_PROC_BROWSER_TEST_F(
    TrustTokenBrowsertest,
    CorsModeCrossOriginRedirectRedemptionUsesNewOriginAsIssuer) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test", "b.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  // Obtain both https://a.test:<PORT> and https://b.test:<PORT> tokens, the
  // former for the initial redemption request to https://a.test:<PORT> and the
  // latter for the fresh post-redirect redemption request to
  // https://b.test:<PORT>.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("b.test", "/issue"))));

  // On the redemption request, `mode: 'cors'` (the default) has the effect that
  // that redirecting a request will renew the request's Trust Tokens state.
  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/cross-site/b.test/redeem',
        { privateToken: { mode: 'cors',
                        version: 1,
                        operation: 'token-redemption' } })
        .then(()=>'Success'); )"));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { privateToken: { version: 1,
                        operation: 'send-redemption-record',
                        issuers: [$1],
        } }).then(()=>'Success');)",
                                      IssuanceOriginFromHost("b.test"))));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))));

  // When a signing operation fails, it isn't fatal, so the requests
  // should always get sent successfully.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { privateToken: { version: 1,
                        operation: 'send-redemption-record',
                        issuers: [$1],
        } }).then(()=>'Success');)",
                                      IssuanceOriginFromHost("a.test"))));

  // There shouldn't have been an a.test redemption record attached to the
  // request.
  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));

  // Expect six accesses, four for issuance and two for redemption.
  EXPECT_EQ(6, access_count_);
}

// When a redemption request is made in no-cors mode, a cross-origin redirect
// from issuer A to issuer B should result in recycling the original redemption
// request, obtaining an issuer A redemption record on success.
//
// Note: This isn't necessarily the behavior we'll end up wanting here; the test
// serves to document how redemption and redirects currently interact.  For more
// on the interaction between Trust Tokens and redirects, see the "Handling
// redirects" section in the design doc
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.5erfr3uo012t
IN_PROC_BROWSER_TEST_F(
    TrustTokenBrowsertest,
    NoCorsModeCrossOriginRedirectRedemptionUsesOriginalOriginAsIssuer) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/issue',
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )"));

  // `mode: 'no-cors'` on redemption has the effect that that redirecting a
  // request will maintain the request's Trust Tokens state.
  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/cross-site/b.test/redeem',
        { mode: 'no-cors',
          privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(()=>'Success'); )"));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { privateToken: { version: 1,
                        operation: 'send-redemption-record',
                        issuers: [$1]
        } })
        .then(()=>'Success'); )",
                                      IssuanceOriginFromHost("a.test"))));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))));

  // When a signing operation fails, it isn't fatal, so the requests
  // should always get sent successfully.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { privateToken: { version: 1,
                        operation: 'send-redemption-record',
                        issuers: [$1]
        } })
            .then(()=>'Success'); )",
                                      IssuanceOriginFromHost("b.test"))));

  // There shouldn't have been a b.test redemption record attached to the
  // request.
  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));

  // Expect four accesses, two for issuance and two for redemption.
  EXPECT_EQ(4, access_count_);
}

// When a redemption request is made in no-cors mode, a cross-origin redirect
// from issuer A to issuer B should result in recycling the original redemption
// request and, in particular, sending the same token.
//
// Note: This isn't necessarily the behavior we'll end up wanting here; the test
// serves to document how redemption and redirects currently interact.
IN_PROC_BROWSER_TEST_F(
    TrustTokenBrowsertest,
    NoCorsModeCrossOriginRedirectRedemptionRecyclesSameRedemptionRequest) {
  // Have issuance provide only a single token so that, if the redemption logic
  // searches for a new token after redirect, the redemption will fail.
  TrustTokenRequestHandler::Options options;
  options.batch_size = 1;
  request_handler_.UpdateOptions(std::move(options));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/issue',
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )"));

  // The redemption should succeed after the redirect, yielding an a.test
  // redemption record (the redemption record correctly corresponding to a.test
  // is covered by a prior test case).
  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/cross-site/b.test/redeem',
        { mode: 'no-cors',
          privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(()=>'Success'); )"));

  // Expect two accesses, one for issuance and one for redemption.
  EXPECT_EQ(2, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SigningRequiresRedemptionRecordInStorage) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {privateToken: {version: 1,
                                          operation: 'token-request'}});
      await fetch("/redeem", {privateToken: {version: 1,
                                           operation: 'token-redemption'}});
      await fetch("/sign", {privateToken: {
        version: 1,
        operation: 'send-redemption-record',
        issuers: [$1]}  // b.test, set below
      });
      return "Requests succeeded";
    } catch (err) {
      return "Requests failed unexpectedly";
    }
  })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  //
  // When a signing operation fails, it isn't fatal, so the requests
  // should always get sent successfully.
  EXPECT_EQ(
      "Requests succeeded",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("b.test"))));

  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));

  // Expect three access, one for issue, redeem, and sign.
  EXPECT_EQ(3, access_count_);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, FetchEndToEndWithServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const char* const hostname = "a.test";
  ProvideRequestHandlerKeyCommitmentsToNetworkService({hostname});
  const std::string origin = IssuanceOriginFromHost(hostname);
  const GURL create_sw_url =
      server_.GetURL(hostname, "/service_worker/create_service_worker.html");
  EXPECT_TRUE(NavigateToURL(shell(), create_sw_url));
  // call register function defined in create_sw_url with the service worker js
  // file path
  EXPECT_EQ("DONE",
            EvalJs(shell(), "register('fetch_event_respond_with_fetch.js');"));
  // Following navigate to empty html page makes fetch requests go through
  // service worker. Requests do not go through service workers when commented
  // out.
  const GURL empty_page_url =
      server_.GetURL(hostname, "/service_worker/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), empty_page_url));
  const std::string trust_token_fetch_snippet = R"(
  (async () => {
    if (navigator.serviceWorker.controller === null) return "NotServiceWorker";
    await fetch("/issue", {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch("/redeem", {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    await fetch("/sign", {privateToken: {version: 1,
                                       operation: 'send-redemption-record',
                                  issuers: [$1]}});
    return "TTSuccess"; })(); )";

  EXPECT_EQ("TTSuccess",
            EvalJs(shell(), JsReplace(trust_token_fetch_snippet, origin)));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader))));

  // Expect three accesses, one for issue and one for redeem and one for sign.
  EXPECT_EQ(3, access_count_);
}

// Test redemption limit. Make three refreshing redemption calls back to back
// and test whether the third one fails. This test does not mock time. It
// assumes time (in network process) elapsed between the first and the last
// redemption call is less than the hard coded limit (currently 48 hours).
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RedemptionLimit) {
  // set request handler options batch size to more than 3
  TrustTokenRequestHandler::Options options;
  options.batch_size = 10;
  request_handler_.UpdateOptions(std::move(options));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});
  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  // issue options.batch_size many tokens
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption',
                        refreshPolicy: 'refresh' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));
  // third redemption should fail
  EXPECT_EQ("Error",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { privateToken: { version: 1,
                        operation: 'token-redemption',
                        refreshPolicy: 'refresh' } })
        .then(()=>'Success')
        .catch(()=>'Error'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // Expect four accesses, one for issuance, one for redemption, and two for
  // sign.
  EXPECT_EQ(4, access_count_);
}

// Check whether depreciated fetch API where 'type' refers to operation
// type fails.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, CheckDepreciatedTypeField) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(fetch(
    "/issue", {privateToken: {type: 'token-request'}})
    .then(()=>'Success')
    .catch(error => error.message); )";

  EXPECT_THAT(EvalJs(shell(), command).ExtractString(),
              HasSubstr("Failed to read the 'operation'\
 property from 'PrivateToken': Required member is undefined."));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SendRedemptionRequestWithEmptyIssuers) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {privateToken: {version: 1,
                                        operation: 'token-request'}});
    await fetch("/redeem", {privateToken: {version: 1,
                                         operation: 'token-redemption'}});
    return "Success"; })(); )";
  ASSERT_EQ("Success", EvalJs(shell(), command));

  command = R"(
    fetch("/sign", {privateToken: {version: 1,
                                 operation: 'send-redemption-record',
                                 issuers: []}})
    .then(() => 'Success')
    .catch(error => error.message); )";

  // fetch should throw due to empty issuer field
  EXPECT_THAT(EvalJs(shell(), command).ExtractString(),
              HasSubstr("Failed to execute 'fetch' on 'Window':\
 privateToken: operation type 'send-redemption-record' requires that\
 the 'issuers' field be present and contain at least one secure,\
 HTTP(S) URL, but it was missing or empty."));
}

}  // namespace content
