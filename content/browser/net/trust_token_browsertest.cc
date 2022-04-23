// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/trust_token_browsertest.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
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
#include "crypto/sha2.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_server_handler_registration.h"
#include "services/network/test/trust_token_test_util.h"
#include "services/network/trust_tokens/test/signed_request_verification_util.h"
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
  std::string header;
  if (!arg.headers.GetHeader(name, &header)) {
    *result_listener << base::StringPrintf("%s wasn't present", name);
    return false;
  }
  return ExplainMatchResult(other_matcher, header, result_listener);
}

MATCHER(SignaturesAreWellFormedAndVerify,
        "The signed request has a well-formed Sec-Signature header where all "
        "signatures verify.") {
  std::string error;
  if (!network::test::ReconstructSigningDataAndVerifySignatures(
          arg.destination, arg.headers, /*verifier=*/{}, &error)) {
    *result_listener << "Verifying the signed request encountered error "
                     << error;
    return false;
  }
  return true;
}

MATCHER_P(ParsingSignRequestDataParameterYields,
          sign_request_data_expectation,
          "The sign-request-data value in the Sec-Signature header, once "
          "parsed, matches the expectation.") {
  std::map<std::string, std::string> verification_keys_per_issuer;

  std::string error;
  network::mojom::TrustTokenSignRequestData sign_request_data;
  if (!network::test::ReconstructSigningDataAndVerifySignatures(
          arg.destination, arg.headers,
          /*verifier=*/{}, &error, /*verification_keys_out=*/nullptr,
          &sign_request_data)) {
    *result_listener << "Verifying the signed request encountered error "
                     << error;
    return false;
  }

  return ExplainMatchResult(Eq(sign_request_data_expectation),
                            sign_request_data, result_listener);
}

MATCHER_P(
    SecSignatureHeaderKeyHashes,
    other_matcher,
    std::string("The verification key hashes in the Sec-Signature header ") +
        DescribeMatcher<std::set<std::string>>(other_matcher)) {
  std::map<std::string, std::string> verification_keys_per_issuer;

  std::string error;
  if (!network::test::ReconstructSigningDataAndVerifySignatures(
          arg.destination, arg.headers,
          /*verifier=*/{}, &error, &verification_keys_per_issuer)) {
    *result_listener << "Veriying the signed request encountered error "
                     << error;
    return false;
  }

  std::set<std::string> hashed_verification_keys;
  for (const auto& kv : verification_keys_per_issuer)
    hashed_verification_keys.insert(crypto::SHA256HashString(kv.second));

  return ExplainMatchResult(other_matcher, hashed_verification_keys,
                            result_listener);
}

MATCHER(
    ReflectsSigningFailure,
    "The given signed request reflects a client-side signing failure, having "
    "an empty redemption record and no other request-signing headers.") {
  return ExplainMatchResult(
      AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord,
                    StrEq("")),
          Not(HasHeader(network::kTrustTokensSecTrustTokenVersionHeader)),
          Not(HasHeader(network::kTrustTokensRequestHeaderSecTime)),
          Not(HasHeader(
              network::
                  kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData)),
          Not(HasHeader(network::kTrustTokensRequestHeaderSecSignature))),
      arg, result_listener);
}

}  // namespace

TrustTokenBrowsertest::TrustTokenBrowsertest() {
  auto& field_trial_param =
      network::features::kTrustTokenOperationsRequiringOriginTrial;
  features_.InitAndEnableFeatureWithParameters(
      network::features::kTrustTokens,
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

  ASSERT_TRUE(server_.Start());
}

void TrustTokenBrowsertest::ProvideRequestHandlerKeyCommitmentsToNetworkService(
    std::vector<base::StringPiece> hosts) {
  base::flat_map<url::Origin, base::StringPiece> origins_and_commitments;
  std::string key_commitments = request_handler_.GetKeyCommitmentRecord();

  // TODO(davidvc): This could be extended to make the request handler aware
  // of different origins, which would allow using different key commitments
  // per origin.
  for (base::StringPiece host : hosts) {
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

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, FetchEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-redemption-record',
                                  signRequestData: 'include',
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
          Not(HasHeader(network::kTrustTokensRequestHeaderSecTime)),
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader),
          SignaturesAreWellFormedAndVerify(),
          SecSignatureHeaderKeyHashes(IsSubsetOf(
              request_handler_.hashes_of_redemption_bound_public_keys())))));
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
    request.setTrustToken({
      type: 'token-request'
    });
    let promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;

    request = new XMLHttpRequest();
    request.open('GET', '/redeem');
    request.setTrustToken({
      type: 'token-redemption'
    });
    promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;

    request = new XMLHttpRequest();
    request.open('GET', '/sign');
    request.setTrustToken({
      type: 'send-redemption-record',
      signRequestData: 'include',
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
          Not(HasHeader(network::kTrustTokensRequestHeaderSecTime)),
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader),
          SignaturesAreWellFormedAndVerify(),
          SecSignatureHeaderKeyHashes(IsSubsetOf(
              request_handler_.hashes_of_redemption_bound_public_keys())))));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, IframeEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  auto execute_op_via_iframe = [&](base::StringPiece path,
                                   base::StringPiece trust_token) {
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

  execute_op_via_iframe("/issue", R"({"type": "token-request"})");
  execute_op_via_iframe("/redeem", R"({"type": "token-redemption"})");
  execute_op_via_iframe("/sign", JsReplace(
                                     R"({"type": "send-redemption-record",
              "signRequestData": "include", "issuers": [$1]})",
                                     IssuanceOriginFromHost("a.test")));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          Not(HasHeader(network::kTrustTokensRequestHeaderSecTime)),
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader),
          SignaturesAreWellFormedAndVerify(),
          SecSignatureHeaderKeyHashes(IsSubsetOf(
              request_handler_.hashes_of_redemption_bound_public_keys())))));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, HasTrustTokenAfterIssuance) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = JsReplace(R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    return await document.hasTrustToken($1);
  })();)",
                                  IssuanceOriginFromHost("a.test"));

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  //
  // Note: EvalJs's EXPECT_EQ type-conversion magic only supports the
  // "Yoda-style" EXPECT_EQ(expected, actual).
  EXPECT_EQ(true, EvalJs(shell(), command));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SigningWithNoRedemptionRecordDoesntCancelRequest) {
  TrustTokenRequestHandler::Options options;
  options.client_signing_outcome =
      TrustTokenRequestHandler::SigningOutcome::kFailure;
  request_handler_.UpdateOptions(std::move(options));

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // This sign operation will fail, because we don't have a redemption record in
  // storage, a prerequisite. However, the failure shouldn't be fatal.
  std::string command = JsReplace(R"((async () => {
      await fetch("/sign", {trustToken: {type: 'send-redemption-record',
                                         signRequestData: 'include',
                                         issuers: [$1]}});
      return "Success";
      })(); )",
                                  IssuanceOriginFromHost("a.test"));

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));
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
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-redemption-record',
                                  signRequestData: 'include',
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
          Not(HasHeader(network::kTrustTokensRequestHeaderSecTime)),
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          HasHeader(network::kTrustTokensSecTrustTokenVersionHeader),
          SignaturesAreWellFormedAndVerify(),
          SecSignatureHeaderKeyHashes(IsSubsetOf(
              request_handler_.hashes_of_redemption_bound_public_keys())))));
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
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-redemption-record',
                                  signRequestData: 'include',
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
                          R"(fetch($1, {trustToken: {type: 'token-request'}})
                   .then(() => "Unexpected success!")
                   .catch(err => err.message);)",
                          IssuanceOriginFromHost("no-cert-for-this.domain")))
          .ExtractString(),
      HasSubstr("Failed to fetch"));

  EXPECT_THAT(
      EvalJs(shell(),
             JsReplace(
                 R"(fetch($1, {trustToken: {type: 'send-redemption-record',
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
  EXPECT_EQ(
      "InvalidStateError",
      EvalJs(shell(), JsReplace(
                          R"(fetch($1, {trustToken: {type: 'token-redemption'}})
                   .then(() => "Unexpected success!")
                   .catch(err => err.name);)",
                          IssuanceOriginFromHost("a.test"))));

  content::FetchHistogramsFromChildProcesses();

  histograms.ExpectUniqueSample(
      "Net.TrustTokens.NetErrorForTrustTokenOperation.Failure.Redemption",
      net::ERR_TRUST_TOKEN_OPERATION_FAILED, 1);
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
                                     trustToken: {type: 'token-request'}
                                   })
                                   .then(() => "Unexpected success!")
                                   .catch(err => err.name);)"));

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectUniqueSample(
      "Net.TrustTokens.NetErrorForFetchFailure.Issuance", net::ERR_FAILED,
      /*expected_count=*/1);

  // Since issuance failed, there should be no tokens to redeem, so redemption
  // should fail:
  EXPECT_EQ("OperationError",
            EvalJs(shell(),
                   R"(fetch("/redeem", {trustToken: {type: 'token-redemption'}})
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
                  trustToken: {type: 'token-request'}})
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
      R"(fetch("/issue", {trustToken: {type: 'token-request'}})
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
                   R"({"type": "token-request"})", issuance_url)));
  monitor.WaitForUrls();
  EXPECT_THAT(monitor.GetRequestInfo(issuance_url),
              Optional(Field(&network::ResourceRequest::trust_token_params,
                             IsFalse())));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, AdditionalSigningData) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});
  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string cmd = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-redemption-record',
      signRequestData: 'include',
      issuers: [$1],
      additionalSigningData: 'some additional data to sign'}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(cmd, IssuanceOriginFromHost("a.test"))));

  auto has_additional_signing_data = [](std::string header) {
    return base::Contains(
        base::SplitString(header, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_ALL),
        base::ToLowerASCII(
            network::
                kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData));
  };

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(
              network::
                  kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData),
          HasHeader(network::kTrustTokensRequestHeaderSignedHeaders,
                    Truly(has_additional_signing_data)),
          SignaturesAreWellFormedAndVerify())));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, OverlongAdditionalSigningData) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string cmd = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), cmd));

  // Even though this contains fewer than
  // network::kTrustTokenAdditionalSigningDataMaxSizeBytes code units, once it's
  // converted to UTF-8 it will contain more than that many bytes, so we expect
  // that it will get rejected by the network service.
  std::u16string overlong_signing_data(
      network::kTrustTokenAdditionalSigningDataMaxSizeBytes,
      u'€' /* char16 literal */);
  ASSERT_LE(overlong_signing_data.size(),
            network::kTrustTokenAdditionalSigningDataMaxSizeBytes);

  cmd = R"(
    fetch("/sign", {trustToken: {type: 'send-redemption-record',
      signRequestData: 'include',
      issuers: [$1],
      additionalSigningData: $2}}).then(()=>"Success");)";

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(cmd, IssuanceOriginFromHost("a.test"),
                                      overlong_signing_data)));
  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       AdditionalSigningDataNotAValidHeader) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  command = R"(
    fetch("/sign", {trustToken: {type: 'send-redemption-record',
      signRequestData: 'include',
      issuers: [$1],
      additionalSigningData: '\r'}}).then(()=>"Success");)";

  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));
  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));
}

// Issuance should fail if we don't have keys for the issuer at hand.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, IssuanceRequiresKeys) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService(
      {"not-the-right-server.example"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
    fetch('/issue', {trustToken: {type: 'token-request'}})
    .then(() => 'Success').catch(err => err.name); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("InvalidStateError", EvalJs(shell(), command));
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
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success').catch(err => err.name); )"));
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
            fetch($1, { trustToken: { type: 'token-request' } })
            .then(()=>'Success'); )",
                                server_.GetURL("sub1.b.test", "/issue"))));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, CrossSiteIssuanceWorks) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("b.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // Using GetURL to generate the issuance location is important
  // because it sets the port correctly.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
            fetch($1, { trustToken: { type: 'token-request' } })
            .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));
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

  // Each hasTrustToken call adds the provided issuer to the calling context's
  // list of associated issuers.
  for (int i = 0;
       i < network::kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers; ++i) {
    ASSERT_EQ("Success", EvalJs(shell(), "document.hasTrustToken('https://a" +
                                             base::NumberToString(i) +
                                             ".test').then(()=>'Success');"));
  }

  EXPECT_EQ("OperationError", EvalJs(shell(), R"(
            fetch('/issue', { trustToken: { type: 'token-request' } })
            .then(() => 'Success').catch(error => error.name); )"));
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

  std::string command = R"(fetch($1, {trustToken: {type: 'token-request'}})
                             .then(() => "Success")
                             .catch(error => error.name);)";

  EXPECT_EQ(
      "Success",
      EvalJs(shell(),
             JsReplace(command,
                       server_.GetURL("a.test", "/cross-site/b.test/issue"))));

  EXPECT_EQ(true, EvalJs(shell(), JsReplace("document.hasTrustToken($1);",
                                            IssuanceOriginFromHost("b.test"))));
  EXPECT_EQ(false,
            EvalJs(shell(), JsReplace("document.hasTrustToken($1);",
                                      IssuanceOriginFromHost("a.test"))));
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
                                      trustToken: {type: 'token-request'}})
                             .then(() => "Success")
                             .catch(error => error.name);)";

  EXPECT_EQ(
      "Success",
      EvalJs(shell(),
             JsReplace(command,
                       server_.GetURL("a.test", "/cross-site/b.test/issue"))));

  EXPECT_EQ(true, EvalJs(shell(), JsReplace("document.hasTrustToken($1);",
                                            IssuanceOriginFromHost("a.test"))));
  EXPECT_EQ(false,
            EvalJs(shell(), JsReplace("document.hasTrustToken($1);",
                                      IssuanceOriginFromHost("b.test"))));
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
      R"(fetch($1, {trustToken: {type: 'token-request'}})
           .catch(error => error.name);)";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("InvalidStateError",
            EvalJs(shell(), JsReplace(command, server_.GetURL("/issue"))));

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));
  EXPECT_EQ(
      false,
      EvalJs(shell(),
             JsReplace("document.hasTrustToken($1);",
                       url::Origin::Create(server_.base_url()).Serialize())));
}

// Redemption from a secure-but-non-HTTP(S) top frame origin should fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RedemptionRequiresSuitableTopFrameOrigin) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command =
      R"(fetch("/issue", {trustToken: {type: 'token-request'}})
                             .then(() => "Success")
                             .catch(error => error.name);)";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  GURL file_url = GetTestUrl(/*dir=*/nullptr, "title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), file_url));

  // Redemption from a page with a file:// top frame origin should fail.
  command = R"(fetch($1, {trustToken: {type: 'token-redemption'}})
                 .catch(error => error.name);)";
  EXPECT_EQ(
      "InvalidStateError",
      EvalJs(shell(), JsReplace(command, server_.GetURL("a.test", "/redeem"))));
}

// hasTrustToken from a context with a secure-but-non-HTTP/S top frame origin
// should fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       HasTrustTokenRequiresSuitableTopFrameOrigin) {
  GURL file_url = GetTestUrl(/*dir=*/nullptr, "title1.html");
  ASSERT_TRUE(file_url.SchemeIsFile());
  ASSERT_TRUE(NavigateToURL(shell(), file_url));

  EXPECT_EQ("NotAllowedError",
            EvalJs(shell(),
                   R"(document.hasTrustToken('https://issuer.example')
                              .catch(error => error.name);)"));
}

// A hasTrustToken call initiated from a secure context should succeed even if
// the initiating frame's origin is opaque (e.g. from a sandboxed iframe).
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       HasTrustTokenFromSecureSubframeWithOpaqueOrigin) {
  ASSERT_TRUE(NavigateToURL(
      shell(), server_.GetURL("a.test", "/page_with_sandboxed_iframe.html")));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ("Success",
            EvalJs(root->child_at(0)->current_frame_host(),
                   R"(document.hasTrustToken('https://davids.website')
                              .then(()=>'Success');)"));
}

// An operation initiated from a secure context should succeed even if the
// operation's associated request's initiator is opaque (e.g. from a sandboxed
// iframe).
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
                                         trustToken: {type: 'token-request'}
                                         }).then(()=>'Success');)",
                                        server_.GetURL("a.test", "/issue"))));
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

  std::string command = R"(fetch($1, {trustToken: {type: 'token-request'}})
                             .then(() => "Success")
                             .catch(error => error.name);)";
  EXPECT_EQ(
      "OperationError",
      EvalJs(shell(), JsReplace(command, server_.GetURL("a.test", "/issue"))));
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
                                         trustToken: {
                                             type: 'send-redemption-record',
                                             issuers: [
                                                 'https://issuer.example'
                                             ]}
                                         }).then(()=>'Success');)",
                                        server_.GetURL("a.test", "/issue"))));
}

// Redemption should fail when there are no keys for the issuer.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RedemptionRequiresKeys) {
  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("InvalidStateError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));
}

// Redemption should fail when there are no tokens to redeem.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RedemptionRequiresTokens) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("OperationError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));
}

// When we have tokens for one issuer A, redemption against a different issuer B
// should still fail if we don't have any tokens for B.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RedemptionWithoutTokensForDesiredIssuerFails) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test", "b.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("OperationError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )",
                                      server_.GetURL("b.test", "/redeem"))));
}

// When the server rejects redemption, the client-side redemption operation
// should fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       CorrectlyReportsServerErrorDuringRedemption) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  EXPECT_EQ("Success", EvalJs(shell(), R"(fetch('/issue',
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )"));

  // Send a redemption request to the issuance endpoint, which should error out
  // for the obvious reason that it isn't an issuance request:
  EXPECT_EQ("OperationError", EvalJs(shell(), R"(fetch('/issue',
        { trustToken: { type: 'token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )"));
}

// After a successful issuance and redemption, a subsequent redemption against
// the same issuer should hit the redemption record cache.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RedemptionHitsRedemptionRecordCache) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

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

  EXPECT_EQ("NoModificationAllowedError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));
}

// Redemption with `refresh-policy: 'refresh'` from an issuer context should
// succeed, overwriting the existing redemption record.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RefreshPolicyRefreshWorksInIssuerContext) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

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

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption',
                        refreshPolicy: 'refresh' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));
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
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("b.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("b.test", "/redeem"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption',
                        refreshPolicy: 'refresh' } })
        .then(()=>'Success').catch(err => err.name); )",
                                      server_.GetURL("b.test", "/redeem"))));
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
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("b.test", "/issue"))));

  // On the redemption request, `mode: 'cors'` (the default) has the effect that
  // that redirecting a request will renew the request's Trust Tokens state.
  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/cross-site/b.test/redeem',
        { trustToken: { mode: 'cors', type: 'token-redemption' } })
        .then(()=>'Success'); )"));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { trustToken: { type: 'send-redemption-record', issuers: [$1],
          signRequestData: 'headers-only' } }).then(()=>'Success');)",
                                      IssuanceOriginFromHost("b.test"))));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          Not(HasHeader(network::kTrustTokensRequestHeaderSecTime)),
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          SignaturesAreWellFormedAndVerify(),
          SecSignatureHeaderKeyHashes(IsSubsetOf(
              request_handler_.hashes_of_redemption_bound_public_keys())))));

  // When a signing operation fails, it isn't fatal, so the requests
  // should always get sent successfully.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { trustToken: { type: 'send-redemption-record', issuers: [$1],
          signRequestData: 'headers-only' } }).then(()=>'Success');)",
                                      IssuanceOriginFromHost("a.test"))));

  // There shouldn't have been an a.test redemption record attached to the
  // request.
  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));
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
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )"));

  // `mode: 'no-cors'` on redemption has the effect that that redirecting a
  // request will maintain the request's Trust Tokens state.
  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/cross-site/b.test/redeem',
        { mode: 'no-cors',
          trustToken: { type: 'token-redemption' } })
        .then(()=>'Success'); )"));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { trustToken: { type: 'send-redemption-record', issuers: [$1],
          signRequestData: 'headers-only' } })
        .then(()=>'Success'); )",
                                      IssuanceOriginFromHost("a.test"))));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          Not(HasHeader(network::kTrustTokensRequestHeaderSecTime)),
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          SignaturesAreWellFormedAndVerify(),
          SecSignatureHeaderKeyHashes(IsSubsetOf(
              request_handler_.hashes_of_redemption_bound_public_keys())))));

  // When a signing operation fails, it isn't fatal, so the requests
  // should always get sent successfully.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { trustToken: { type: 'send-redemption-record', issuers: [$1],
          signRequestData: 'headers-only' } })
            .then(()=>'Success'); )",
                                      IssuanceOriginFromHost("b.test"))));

  // There shouldn't have been a b.test redemption record attached to the
  // request.
  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));
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
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )"));

  // The redemption should succeed after the redirect, yielding an a.test
  // redemption record (the redemption record correctly corresponding to a.test
  // is covered by a prior test case).
  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/cross-site/b.test/redeem',
        { mode: 'no-cors',
          trustToken: { type: 'token-redemption' } })
        .then(()=>'Success'); )"));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SigningRequiresRedemptionRecordInStorage) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
      await fetch("/sign", {trustToken: {
        type: 'send-redemption-record',
        additionalSignedHeaders: ['Sec-Redemption-Record'],
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
}

// Specifying `includeTimestampHeader: 'true'` for a signing operation should
// make the outgoing request bear a Sec-Time header.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SigningIncludesTimestampWhenSpecified) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url(server_.GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
      await fetch("/sign",
      {trustToken:
        {type: 'send-redemption-record',
         signRequestData: 'include',
         includeTimestampHeader: true,
         additionalSignedHeaders: ['Sec-Time'],
         issuers: [$1]}
      });
      return "Success";
    } catch (err) {
      return "Failure";
    }
  })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(HasHeader(network::kTrustTokensRequestHeaderSecTime)));
}

// Specifying `signRequestData: 'omit'` for a signing operation should make
// the outgoing request bear an SRR but no signature.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SignsCorrectRequestDataWithParameterValueOmit) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
      await fetch("/sign", {trustToken: {
        type: 'send-redemption-record',
        signRequestData: 'omit',
        additionalSignedHeaders: ['Sec-Redemption-Record'],
        issuers: [$1]}
      });
      return "Success";
    } catch (err) {
      return "Failure";
    }
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
          Not(HasHeader(network::kTrustTokensRequestHeaderSecSignature)))));
}

// Specifying `signRequestData: 'headers-only'` for a signing operation should
// make the outgoing request bear a signature over the request's headers.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SignsCorrectRequestDataWithParameterValueHeadersOnly) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
      await fetch("/sign", {trustToken: {
        type: 'send-redemption-record',
        signRequestData: 'headers-only',
        additionalSignedHeaders: ['Sec-Redemption-Record'],
        issuers: [$1]}
      });
      return "Success";
    } catch (err) {
      return "Failure";
    }
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
          SignaturesAreWellFormedAndVerify(),
          ParsingSignRequestDataParameterYields(
              network::mojom::TrustTokenSignRequestData::kHeadersOnly))));
}

// Specifying `signRequestData: 'include'` for a signing operation should make
// the outgoing request bear a signature over the request's headers and
// initiating registrable domain.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SignsCorrectRequestDataWithParameterValueInclude) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
      await fetch("/sign", {trustToken: {
        type: 'send-redemption-record',
        signRequestData: 'include',
        additionalSignedHeaders: ['Sec-Redemption-Record'],
        issuers: [$1]}
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
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  EXPECT_THAT(
      request_handler_.last_incoming_signed_request(),
      Optional(AllOf(
          HasHeader(network::kTrustTokensRequestHeaderSecRedemptionRecord),
          SignaturesAreWellFormedAndVerify(),
          ParsingSignRequestDataParameterYields(
              network::mojom::TrustTokenSignRequestData::kInclude))));
}

// Specifying an unsignable header to sign should make signing fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, ShouldntSignUnsignableHeader) {
  TrustTokenRequestHandler::Options options;
  options.client_signing_outcome =
      TrustTokenRequestHandler::SigningOutcome::kFailure;
  request_handler_.UpdateOptions(std::move(options));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'token-redemption'}});
    await fetch("/sign",
    {trustToken:
      {type: 'send-redemption-record',
       signRequestData: 'include',
       includeTimestampHeader: true,
       additionalSignedHeaders: ['Sec-Certainly-Not-Signable'],
       issuers: [$1]}
    });
    return "Requests succeeded";
  })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Requests succeeded",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  EXPECT_THAT(request_handler_.last_incoming_signed_request(),
              Optional(ReflectsSigningFailure()));
}

class TrustTokenBrowsertestWithPlatformIssuance : public TrustTokenBrowsertest {
 public:
  TrustTokenBrowsertestWithPlatformIssuance() {
    // This assertion helps guard against the brittleness of deserializing
    // "true", in case we refactor the parameter's type.
    static_assert(
        std::is_same<decltype(
                         network::features::kPlatformProvidedTrustTokenIssuance
                             .default_value),
                     const bool>::value,
        "Need to update this initialization logic if the type of the param "
        "changes.");
    features_.InitAndEnableFeatureWithParameters(
        network::features::kTrustTokens,
        {{network::features::kPlatformProvidedTrustTokenIssuance.name,
          "true"}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

#if BUILDFLAG(IS_ANDROID)
HandlerWrappingLocalTrustTokenFulfiller::
    HandlerWrappingLocalTrustTokenFulfiller(TrustTokenRequestHandler& handler)
    : handler_(handler) {
  interface_overrider_.SetBinderForName(
      mojom::LocalTrustTokenFulfiller::Name_,
      base::BindRepeating(&HandlerWrappingLocalTrustTokenFulfiller::Bind,
                          base::Unretained(this)));
}
HandlerWrappingLocalTrustTokenFulfiller::
    ~HandlerWrappingLocalTrustTokenFulfiller() = default;

void HandlerWrappingLocalTrustTokenFulfiller::FulfillTrustTokenIssuance(
    network::mojom::FulfillTrustTokenIssuanceRequestPtr request,
    FulfillTrustTokenIssuanceCallback callback) {
  absl::optional<std::string> maybe_result =
      handler_.Issue(std::move(request->request));
  if (maybe_result) {
    std::move(callback).Run(
        network::mojom::FulfillTrustTokenIssuanceAnswer::New(
            network::mojom::FulfillTrustTokenIssuanceAnswer::Status::kOk,
            std::move(*maybe_result)));
    return;
  }
  std::move(callback).Run(network::mojom::FulfillTrustTokenIssuanceAnswer::New(
      network::mojom::FulfillTrustTokenIssuanceAnswer::Status::kUnknownError,
      ""));
}

void HandlerWrappingLocalTrustTokenFulfiller::Bind(
    mojo::ScopedMessagePipeHandle handle) {
  receiver_.Bind(
      mojo::PendingReceiver<content::mojom::LocalTrustTokenFulfiller>(
          std::move(handle)));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertestWithPlatformIssuance,
                       EndToEndAndroidPlatformIssuance) {
  base::HistogramTester histograms;

  TrustTokenRequestHandler::Options options;
  options.specify_platform_issuance_on = {
      network::mojom::TrustTokenKeyCommitmentResult::Os::kAndroid};
  request_handler_.UpdateOptions(std::move(options));

  HandlerWrappingLocalTrustTokenFulfiller fulfiller(request_handler_);

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // Issuance operations successfully answered locally result in
  // NoModificationAllowedError.
  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      return "Unexpected success";
    } catch (e) {
      if (e.name !== "NoModificationAllowedError") {
        return "Unexpected exception";
      }
      const hasToken = await document.hasTrustToken($1);
      if (!hasToken)
        return "Unexpectedly absent token";
      return "Success";
    }})(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectTotalCount(
      base::StrCat({"Net.TrustTokens.OperationBeginTime.Success.Issuance."
                    "PlatformProvided"}),
      1);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertestWithPlatformIssuance,
                       PlatformIssuanceWithoutEmbedderSupport) {
  base::HistogramTester histograms;

  TrustTokenRequestHandler::Options options;
  options.specify_platform_issuance_on = {
      network::mojom::TrustTokenKeyCommitmentResult::Os::kAndroid};
  options.unavailable_local_operation_fallback =
      network::mojom::TrustTokenKeyCommitmentResult::
          UnavailableLocalOperationFallback::kReturnWithError;
  request_handler_.UpdateOptions(std::move(options));

  service_manager::InterfaceProvider::TestApi interface_overrider(
      content::GetGlobalJavaInterfaces());
  // Instead of using interface_overrider.ClearBinder(name), it's necessary to
  // provide a callback that explicitly closes the pipe, since
  // InterfaceProvider's contract requires that it either bind or close pipes
  // it's given (see its comments in interface_provider.mojom).
  interface_overrider.SetBinderForName(
      mojom::LocalTrustTokenFulfiller::Name_,
      base::BindRepeating([](mojo::ScopedMessagePipeHandle handle) {
        mojo::Close(std::move(handle));
      }));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // Issuance operations diverted locally without embedder support, with
  // "return_with_error" specified in the issuer's key commitments, should
  // result in OperationError.
  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      return "Unexpected success";
    } catch (e) {
      return e.name;
    }})(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("OperationError", EvalJs(shell(), command));

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectTotalCount(
      base::StrCat({"Net.TrustTokens.OperationBeginTime.Failure.Issuance."
                    "PlatformProvided"}),
      1);
}
#endif  // BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(
    TrustTokenBrowsertestWithPlatformIssuance,
    IssuanceOnOsNotSpecifiedInKeyCommitmentsReturnsErrorIfConfiguredToDoSo) {
  TrustTokenRequestHandler::Options options;
  options.specify_platform_issuance_on = {
      network::mojom::TrustTokenKeyCommitmentResult::Os::kAndroid};
  // Since we're not on Android, if the issuer
  // 1) configures that, on Android, we should attempt platform-provided token
  //    issuance,
  // 2) specifies "return_with_error" as the fallback behavior for other OSes,
  // issuance against the host configured for platform-provided issuance should
  // fail.
  options.unavailable_local_operation_fallback =
      network::mojom::TrustTokenKeyCommitmentResult::
          UnavailableLocalOperationFallback::kReturnWithError;
  request_handler_.UpdateOptions(std::move(options));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // Issuance operations attempted on OSes other than those specified in
  // the key commitment's "request_issuance_locally_on" field should result in
  // OperationError returns if the issuer specified "return_with_error" as the
  // fallback behavior.
  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      return "Unexpected success";
    } catch (e) {
      return e.name;
    }})(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("OperationError", EvalJs(shell(), command));
}

IN_PROC_BROWSER_TEST_F(
    TrustTokenBrowsertestWithPlatformIssuance,
    IssuanceOnOsNotSpecifiedInKeyCommitmentsFallsBackToWebIssuanceIfSpecified) {
  base::HistogramTester histograms;

  TrustTokenRequestHandler::Options options;
  options.specify_platform_issuance_on = {
      network::mojom::TrustTokenKeyCommitmentResult::Os::kAndroid};
  // Since we're not on Android, if the issuer
  // 1) configures that, on Android, we should attempt platform-provided token
  //    issuance,
  // 2) specifies "web_issuance" as fallback behavior for other OSes,
  // we should see issuance succeed.
  options.unavailable_local_operation_fallback =
      network::mojom::TrustTokenKeyCommitmentResult::
          UnavailableLocalOperationFallback::kWebIssuance;
  request_handler_.UpdateOptions(std::move(options));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // Issuance operations attempted on OSes other than those specified in
  // the key commitment's "request_issuance_locally_on" field should result in
  // OperationError returns if the issuer specified "return_with_error" as the
  // fallback behavior.
  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      if (await document.hasTrustToken($1))
        return "Success";
      return "Issuance failed unexpectedly";
    } catch (e) {
      return e.name;
    }})(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  content::FetchHistogramsFromChildProcesses();
  histograms.ExpectTotalCount(
      base::StrCat({"Net.TrustTokens.OperationBeginTime.Failure.Issuance."
                    "PlatformProvided"}),
      0);  // No platform-provided operation was attempted.
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
