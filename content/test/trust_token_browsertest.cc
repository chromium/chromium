// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/test/test_server_handler_registration.h"
#include "services/network/trust_tokens/test/trust_token_request_handler.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon_stdstring.h"

namespace content {

namespace {

using network::test::TrustTokenRequestHandler;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::Optional;

// TrustTokenBrowsertest is a fixture containing boilerplate for initializing an
// HTTPS test server and passing requests through to an embedded instance of
// network::test::TrustTokenRequestHandler, which contains the guts of the
// "server-side" token issuance and redemption logic as well as some consistency
// checks for subsequent signed requests.
class TrustTokenBrowsertest : public ContentBrowserTest {
 public:
  TrustTokenBrowsertest() {
    auto& field_trial_param =
        network::features::kTrustTokenOperationsRequiringOriginTrial;
    features_.InitAndEnableFeatureWithParameters(
        network::features::kTrustTokens,
        {{field_trial_param.name,
          field_trial_param.GetName(
              network::features::TrustTokenOriginTrialSpec::
                  kOriginTrialNotRequired)}});
  }

  // Registers the following handlers:
  // - default //content/test/data files;
  // - a special "/issue" endpoint executing Trust Tokens issuance;
  // - a special "/redeem" endpoint executing redemption; and
  // - a special "/sign" endpoint that verifies that the received signed request
  // data is correctly structured and that the provided Sec-Signature header's
  // verification key was previously bound to a successful token redemption.
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));

    SetupCrossSiteRedirector(embedded_test_server());
    SetupCrossSiteRedirector(&server_);

    network::test::RegisterTrustTokenTestHandlers(&server_, &request_handler_);

    ASSERT_TRUE(server_.Start());
  }

 protected:
  // Provides the network service key commitments from the internal
  // TrustTokenRequestHandler. All hosts in |hosts| will be provided identical
  // commitments.
  void ProvideRequestHandlerKeyCommitmentsToNetworkService(
      std::vector<base::StringPiece> hosts = {}) {
    base::flat_map<url::Origin, base::StringPiece> origins_and_commitments;
    std::string key_commitments = request_handler_.GetKeyCommitmentRecord();

    // TODO(davidvc): This could be extended to make the request handler aware
    // of different origins, which would allow using different key commitments
    // per origin.
    for (base::StringPiece host : hosts) {
      GURL::Replacements replacements;
      replacements.SetHostStr(host);
      origins_and_commitments.insert_or_assign(
          url::Origin::Create(
              server_.base_url().ReplaceComponents(replacements)),
          key_commitments);
    }

    if (origins_and_commitments.empty()) {
      origins_and_commitments = {
          {url::Origin::Create(server_.base_url()), key_commitments}};
    }

    base::RunLoop run_loop;
    GetNetworkService()->SetTrustTokenKeyCommitments(
        network::WrapKeyCommitmentsForIssuers(
            std::move(origins_and_commitments)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Given a host (e.g. "a.test"), returns the corresponding storage origin
  // for Trust Tokens state. (This adds the correct scheme---probably https---as
  // well as |server_|'s port, which can vary from test to test. There's no
  // ambiguity in the result because the scheme and port are both fixed across
  // all domains.)
  std::string IssuanceOriginFromHost(const std::string& host) const {
    auto ret = url::Origin::Create(server_.GetURL(host, "/")).Serialize();
    return ret;
  }

  base::test::ScopedFeatureList features_;

  // TODO(davidvc): Extend this to support more than one key set.
  TrustTokenRequestHandler request_handler_;

  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, FetchEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'srr-token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-srr',
                                  signRequestData: 'include',
                                  issuers: [$1]}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
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
      type: 'srr-token-redemption'
    });
    promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;

    request = new XMLHttpRequest();
    request.open('GET', '/sign');
    request.setTrustToken({
      type: 'send-srr',
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

  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt)
      << *request_handler_.LastVerificationError();
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
  execute_op_via_iframe("/redeem", R"({"type": "srr-token-redemption"})");
  execute_op_via_iframe(
      "/sign",
      JsReplace(
          R"({"type": "send-srr", "signRequestData": "include", "issuer": $1})",
          IssuanceOriginFromHost("a.test")));
  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
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

  // This sign operation will fail, because we don't have a signed redemption
  // record in storage, a prerequisite. However, the failure shouldn't be fatal.
  std::string command = JsReplace(R"((async () => {
      await fetch("/sign", {trustToken: {type: 'send-srr',
                                         signRequestData: 'include',
                                         issuers: [$1]}});
      return "Success";
      })(); )",
                                  IssuanceOriginFromHost("a.test"));

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));
  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, FetchEndToEndInIsolatedWorld) {
  // Ensure an isolated world can execute Trust Tokens operations when its
  // window's main world can. In particular, this ensures that the
  // redemtion-and-signing feature policy is appropriately propagated by the
  // browser process.

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'srr-token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-srr',
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
  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RecordsTimers) {
  base::HistogramTester histograms;

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'srr-token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-srr',
                                  signRequestData: 'include',
                                  issuers: [$1]}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  // Just check that the timers were populated: since we can't mock a clock in
  // this browser test, it's hard to check the recorded values for
  // reasonableness.
  content::FetchHistogramsFromChildProcesses();
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
      EvalJs(shell(), JsReplace(
                          R"(fetch($1, {trustToken: {type: 'send-srr',
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
      EvalJs(shell(),
             JsReplace(
                 R"(fetch($1, {trustToken: {type: 'srr-token-redemption'}})
                   .then(() => "Unexpected success!")
                   .catch(err => err.name);)",
                 IssuanceOriginFromHost("a.test"))));

  content::FetchHistogramsFromChildProcesses();

  histograms.ExpectUniqueSample(
      "Net.TrustTokens.NetErrorForTrustTokenOperation.Failure.Redemption",
      net::ERR_TRUST_TOKEN_OPERATION_FAILED, 1);
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
    await fetch("/redeem", {trustToken: {type: 'srr-token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-srr',
      signRequestData: 'include',
      issuers: [$1],
      additionalSigningData: 'some additional data to sign'}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(cmd, IssuanceOriginFromHost("a.test"))));

  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, OverlongAdditionalSigningData) {
  TrustTokenRequestHandler::Options options;
  options.client_signing_outcome =
      TrustTokenRequestHandler::SigningOutcome::kFailure;
  request_handler_.UpdateOptions(std::move(options));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string cmd = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'srr-token-redemption'}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), cmd));

  // Even though this contains fewer than
  // network::kTrustTokenAdditionalSigningDataMaxSizeBytes code units, once it's
  // converted to UTF-8 it will contain more than that many bytes, so we expect
  // that it will get rejected by the network service.
  base::string16 overlong_signing_data(
      network::kTrustTokenAdditionalSigningDataMaxSizeBytes,
      u'€' /* char16 literal */);
  ASSERT_LE(overlong_signing_data.size(),
            network::kTrustTokenAdditionalSigningDataMaxSizeBytes);

  cmd = R"(
    fetch("/sign", {trustToken: {type: 'send-srr',
      signRequestData: 'include',
      issuers: [$1],
      additionalSigningData: $2}}).then(()=>"Success");)";

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(cmd, IssuanceOriginFromHost("a.test"),
                                      overlong_signing_data)));
  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       AdditionalSigningDataNotAValidHeader) {
  TrustTokenRequestHandler::Options options;
  options.client_signing_outcome =
      TrustTokenRequestHandler::SigningOutcome::kFailure;
  request_handler_.UpdateOptions(std::move(options));

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  std::string command = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'srr-token-redemption'}});
    return "Success"; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  command = R"(
    fetch("/sign", {trustToken: {type: 'send-srr',
      signRequestData: 'include',
      issuers: [$1],
      additionalSigningData: '\r'}}).then(()=>"Success");)";

  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));
  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
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
  command = R"(fetch($1, {trustToken: {type: 'srr-token-redemption'}})
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
                            ->GetFrameTree()
                            ->root();

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
                            ->GetFrameTree()
                            ->root();

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
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ("Success", EvalJs(root->child_at(0)->current_frame_host(),
                              JsReplace(R"(
                              fetch($1, {mode: 'no-cors',
                                         trustToken: {type: 'send-srr',
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
        { trustToken: { type: 'srr-token-redemption' } })
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
        { trustToken: { type: 'srr-token-redemption' } })
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
        { trustToken: { type: 'srr-token-redemption' } })
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
        { trustToken: { type: 'srr-token-redemption' } })
        .then(() => 'Success')
        .catch(err => err.name); )"));
}

// After a successful issuance and redemption, a subsequent redemption against
// the same issuer should hit the signed redemption record (SRR) cache.
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
        { trustToken: { type: 'srr-token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  EXPECT_EQ("NoModificationAllowedError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'srr-token-redemption' } })
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));
}

// Redemption with `refresh-policy: 'refresh'` from an issuer context should
// succeed, overwriting the existing SRR.
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
        { trustToken: { type: 'srr-token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'srr-token-redemption',
                        refreshPolicy: 'refresh' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));
}

// Redemption with `refresh-policy: 'refresh'` from a non-issuer context should
// fail.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       RefreshPolicyRefreshRequiresIssuerContext) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"b.test"});

  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  // Execute the operations against issuer https://b.test:<port> from a
  // different context; attempting to use refreshPolicy: 'refresh' should error.
  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("b.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'srr-token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("b.test", "/redeem"))));

  EXPECT_EQ("InvalidStateError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'srr-token-redemption',
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
        { trustToken: { mode: 'cors', type: 'srr-token-redemption' } })
        .then(()=>'Success'); )"));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { trustToken: { type: 'send-srr', issuers: [$1],
          signRequestData: 'headers-only' } }).then(()=>'Success');)",
                                      IssuanceOriginFromHost("b.test"))));

  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { trustToken: { type: 'send-srr', issuers: [$1],
          signRequestData: 'headers-only' } }).then(()=>'Success');)",
                                      IssuanceOriginFromHost("a.test"))));

  // There shouldn't have been an a.test SRR attached to the request.
  EXPECT_TRUE(request_handler_.LastVerificationError());
}

// When a redemption request is made in no-cors mode, a cross-origin redirect
// from issuer A to issuer B should result in recycling the original redemption
// request, obtaining an issuer A SRR on success.
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
          trustToken: { type: 'srr-token-redemption' } })
        .then(()=>'Success'); )"));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { trustToken: { type: 'send-srr', issuers: [$1],
          signRequestData: 'headers-only' } })
        .then(()=>'Success'); )",
                                      IssuanceOriginFromHost("a.test"))));

  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(
      fetch('/sign',
        { trustToken: { type: 'send-srr', issuers: [$1],
          signRequestData: 'headers-only' } })
        .then(()=>'Success'); )",
                                      IssuanceOriginFromHost("b.test"))));

  // There shouldn't have been an a.test SRR attached to the request.
  EXPECT_TRUE(request_handler_.LastVerificationError());
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

  // The redemption should succeed after the redirect, yielding an a.test SRR
  // (the SRR correctly corresponding to a.test is covered by a prior test
  // case).
  EXPECT_EQ("Success", EvalJs(shell(), R"(
      fetch('/cross-site/b.test/redeem',
        { mode: 'no-cors',
          trustToken: { type: 'srr-token-redemption' } })
        .then(()=>'Success'); )"));
}

}  // namespace content
