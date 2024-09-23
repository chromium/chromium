// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// These integration tests cover the interaction between the Trust Token API's
// Fetch and iframe surfaces and various configuration requiring Origin Trial
// tokens to execute some or all of the Trust Tokens operations (issuance,
// redemption, and signing).
//
// There are two configuration modes:
// - "third-party origin trial": all Trust Tokens operations require an origin
// trial token to execute and, if a token is missing, the Trust Tokens interface
// disppears so that attempts to execute operations will silently no-op. This is
// because the Trust Tokens interface manifests itself as an additional argument
// in fetch's RequestInit dictionary, which does not throw errors when
// unexpected arguments are provided.
// - "standard origin trial": only Trust Tokens issuance requires an origin
// trial token to execute and, if a token is missing, issuance will fail.
//
// As an example, consider
//
//    fetch("https://chromium.org", {
//        privateToken: {
//            version: 1,
//            operation: 'token-request'}})
//
// a representative fetch with an associated Trust Tokens issuance operation.
// When Trust Tokens is completely disabled (e.g. "third-party origin trial"
// mode with no token), the trustToken argument will be ignored. On the other
// hand, when Trust Tokens is enabled but issuance is forbidden ("standard
// origin trial" mode with no token), this will reject with an exception.

namespace content {

namespace {

using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

// Trust Tokens has three interfaces: fetch, XHR, and iframe. However, the XHR
// and fetch interfaces use essentially identical code paths, so we exclude the
// XHR interface in order to save some test duration.
enum class Interface {
  kFetch,
  kIframe,
};

// Prints a string representation to use for generating test names.
std::string ToString(Interface interface) {
  switch (interface) {
    case Interface::kFetch:
      return "Fetch";
    case Interface::kIframe:
      return "Iframe";
  }
}

using Op = network::mojom::TrustTokenOperationType;

enum class Outcome {
  // A request with Trust Tokens parameters should reach the network stack.
  kSuccess,
  // A request without Trust Tokens parameters should reach the network stack.
  kSuccessWithoutTrustTokenParams,
  // The Trust Tokens operation should error out. For the Fetch interface, this
  // means an exception gets thrown; for the iframe interface, it means we
  // continue the request with no Trust Tokens parameters (i.e., it's equivalent
  // to kSuccessWithoutTrustTokenParams).
  kFailure,
};

enum class TrialEnabled {
  // The Trust Tokens operation at hand will be executed from a context with an
  // origin trial token.
  kEnabled,
  // The Trust Tokens operation at hand will be executed from a context lacking
  // an origin trial token.
  kDisabled,
};

// Prints a string representation to use for generating test names.
std::string ToString(TrialEnabled trial_enabled) {
  switch (trial_enabled) {
    case TrialEnabled::kEnabled:
      return "TrialEnabled";
    case TrialEnabled::kDisabled:
      return "TrialDisabled";
  }
}

using TrialType = network::features::TrustTokenOriginTrialSpec;

// Prints a string representation to use for generating test names.
std::string ToString(TrialType trial_type) {
  switch (trial_type) {
    case TrialType::kAllOperationsRequireOriginTrial:
      return "AllOpsNeedTrial";
    case TrialType::kOnlyIssuanceRequiresOriginTrial:
      return "OnlyIssuanceNeedsTrial";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

struct TestDescription {
  Op op;
  Outcome outcome;
  TrialType trial_type;
  TrialEnabled trial_enabled;
};

class TrustTokenOriginTrialBrowsertest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<Interface, TestDescription>> {
 public:
  TrustTokenOriginTrialBrowsertest() {
    auto& field_trial_param =
        network::features::kTrustTokenOperationsRequiringOriginTrial;
    // kPrivateStateTokens ignores origin trial params
    features_.InitWithFeaturesAndParameters(
        {{network::features::kFledgePst,
          {{field_trial_param.name,
            field_trial_param.GetName(std::get<1>(GetParam()).trial_type)}}}},
        {network::features::kPrivateStateTokens});
  }

  // kPageWithOriginTrialToken is a landing page from which we execute Trust
  // Tokens operations in test cases that require an origin trial token to be
  // present. We use a deterministic port and swap in the landing page with
  // URLLoaderInterceptor, rather than serving the page from
  // EmbeddedTestServer, because the token is generated offline and bound to a
  // specific origin.
  const GURL kPageWithOriginTrialToken{"http://localhost:5555"};

  // kTrustTokenUrl is the destination URL of the executed Trust Tokens
  // operations. It's arbitrary, since the tests just need to intercept
  // requests en route to check if they bear Trust Tokens parameters.
  const GURL kTrustTokenUrl{kPageWithOriginTrialToken.Resolve("/trust-token")};

 protected:
  // OnRequest is a URLLoaderInterceptor callback. It:
  // - serves the Origin Trials token when navigating to the landing page
  // - quits the run loop, and stores the obtained request, when receiving a
  // request to the Trust Tokens URL
  // - declines to intercept otherwise (e.g. on favicon load)
  //
  // For the reasons discussed in |kPageWithOriginTrialToken|'s member comment,
  // we need to use an interceptor to serve the landing page. Since we're
  // already stuck with having an interceptor around, we use the same
  // interceptor---instead of, say, registering an EmbeddedTestServer
  // callback---to verify that requests sent to the Trust Tokens endpoint bear
  // (or omit) Trust Tokens parameters. (This also lets us avoid having to set
  // up all of the server-side logic necessary for executing a Trust Tokens
  // operation end to end.)
  bool OnRequest(URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url == kPageWithOriginTrialToken) {
      // Origin Trials key generated with:
      //
      // tools/origin_trials/generate_token.py --expire-days 5000 --version 3 \
      // http://localhost:5555 TrustTokens
      //
      // Note that you can't have an origin trial token with expiry more than
      // 2^31-1 seconds past the epoch, so (for instance) --expire-days 10000
      // would not have generated a valid token.
      URLLoaderInterceptor::WriteResponse(
          base::ReplaceStringPlaceholders(
              "HTTP/1.1 200 OK\n"
              "Content-type: text/html\n"
              "Origin-Trial: $1\n\n",
              {"A220DaFwmOb78vs8TojpryN1mfL9+zHjNDdo+rJTwRcaPkCIzU4/"
               "vP9pnSHyI2ye8WsoxToBprvd7YH+"
               "SdR0FgAAAABTeyJvcmlnaW4iOiAiaHR0cDovL2xvY2FsaG9zdDo1NTU1IiwgImZ"
               "lYXR1cmUiOiAiVHJ1c3RUb2tlbnMiLCAiZXhwaXJ5IjogMjAyNTQ1OTI0MX0="},
              /*offsets=*/nullptr),
          /*body=*/"", params->client.get());
      return true;
    }

    if (params->url_request.url == kTrustTokenUrl) {
      {
        base::AutoLock lock(mutex_);
        CHECK(!trust_token_request_)
            << "Unexpected second Trust Tokens request";
        trust_token_request_ = params->url_request;
      }

      // Write a response here so that the request doesn't fail: this is
      // necessary so that tests expecting kFailure do not erroneously pass in
      // cases where the request does not error out.
      URLLoaderInterceptor::WriteResponse(
          "HTTP/1.1 200 OK\nContent-type: text/html\n\n", /*body=*/"",
          params->client.get());

      base::OnceClosure done;
      {
        base::AutoLock lock(mutex_);
        done = std::move(on_received_request_);
      }

      std::move(done).Run();

      return true;
    }

    return false;
  }

  base::test::ScopedFeatureList features_;

  // The request data is written on the IO sequence and read on the main
  // sequence.
  base::Lock mutex_;

  // |on_received_request_| is called once a request arrives at
  // |kTrustTokenUrl|; the request is then placed in |trust_token_request_|.
  base::OnceClosure on_received_request_ GUARDED_BY(mutex_);
  std::optional<network::ResourceRequest> trust_token_request_
      GUARDED_BY(mutex_);
};

const TestDescription kTestDescriptions[] = {
    {Op::kIssuance, Outcome::kSuccess,
     TrialType::kOnlyIssuanceRequiresOriginTrial, TrialEnabled::kEnabled},

    {Op::kRedemption, Outcome::kSuccess,
     TrialType::kOnlyIssuanceRequiresOriginTrial, TrialEnabled::kEnabled},

    {Op::kRedemption, Outcome::kSuccess,
     TrialType::kOnlyIssuanceRequiresOriginTrial, TrialEnabled::kDisabled},

    {Op::kIssuance, Outcome::kSuccess,
     TrialType::kAllOperationsRequireOriginTrial, TrialEnabled::kEnabled},

    {Op::kIssuance, Outcome::kSuccessWithoutTrustTokenParams,
     TrialType::kAllOperationsRequireOriginTrial, TrialEnabled::kDisabled},

    {Op::kRedemption, Outcome::kSuccess,
     TrialType::kAllOperationsRequireOriginTrial, TrialEnabled::kEnabled},

    {Op::kRedemption, Outcome::kSuccessWithoutTrustTokenParams,
     TrialType::kAllOperationsRequireOriginTrial, TrialEnabled::kDisabled},
};

// Prints a string representation to use for generating test names.
std::string ToString(Op op) {
  switch (op) {
    case Op::kIssuance:
      return "Issuance";
    case Op::kRedemption:
      return "Redemption";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

std::string TestParamToString(
    const ::testing::TestParamInfo<std::tuple<Interface, TestDescription>>&
        info) {
  Interface interface = std::get<0>(info.param);
  const TestDescription& test_description = std::get<1>(info.param);

  return base::ReplaceStringPlaceholders(
      "$1_$2_$3_$4",
      {ToString(interface), ToString(test_description.op),
       ToString(test_description.trial_type),
       ToString(test_description.trial_enabled)},
      nullptr);
}

}  // namespace

// Each parameter has to be a valid JSON encoding of a TrustToken JS object
// *and* valid to directly substitute into JS: this is because the iframe API
// requires a JSON encoding of the parameters object, while the Fetch and XHR
// APIs require actual objects.
INSTANTIATE_TEST_SUITE_P(ExecutingAllOperations,
                         TrustTokenOriginTrialBrowsertest,
                         Combine(Values(Interface::kFetch, Interface::kIframe),
                                 ValuesIn(kTestDescriptions)),
                         &TestParamToString);

// Test that a Trust Tokens request passes parameters to the network stack
// only when permitted by the origin trials framework (either because
// configuration specifies that no origin trial token is required, or because an
// origin trial token is present in the executing context).
IN_PROC_BROWSER_TEST_P(TrustTokenOriginTrialBrowsertest,
                       ProvidesParamsOnlyWhenAllowed) {
  TestDescription test_description = std::get<1>(GetParam());
  Interface interface = std::get<0>(GetParam());

  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [this](URLLoaderInterceptor::RequestParams* params) {
        return OnRequest(params);
      }));

  switch (test_description.trial_enabled) {
    case TrialEnabled::kEnabled:
      ASSERT_TRUE(NavigateToURL(shell(), kPageWithOriginTrialToken));
      break;
    case TrialEnabled::kDisabled:
      ASSERT_TRUE(embedded_test_server()->Start());
      ASSERT_TRUE(NavigateToURL(
          shell(), embedded_test_server()->GetURL("/title1.html")));
      break;
  }

  base::RunLoop run_loop;

  {
    base::AutoLock lock(mutex_);
    on_received_request_ = run_loop.QuitClosure();
  }

  network::TrustTokenTestParameters trust_token_params(
      1, test_description.op, std::nullopt, std::nullopt);

  network::TrustTokenParametersAndSerialization
      expected_params_and_serialization =
          network::SerializeTrustTokenParametersAndConstructExpectation(
              trust_token_params);

  std::string command;
  switch (interface) {
    case Interface::kFetch:
      command = JsReplace("fetch($1, {privateToken: ", kTrustTokenUrl) +
                expected_params_and_serialization.serialized_params + "});";
      break;
    case Interface::kIframe:
      if (test_description.op != Op::kSigning) {
        return;
      }
      command = JsReplace(
          "let iframe = document.createElement('iframe');"
          "iframe.src = $1;"
          "iframe.trustToken = $2;"
          "document.body.appendChild(iframe);",
          kTrustTokenUrl, expected_params_and_serialization.serialized_params);

      // When a Trust Tokens operation fails via the iframe interface, the
      // request itself will still execute, just without an associated Trust
      // Tokens operation.
      if (test_description.outcome == Outcome::kFailure)
        test_description.outcome = Outcome::kSuccessWithoutTrustTokenParams;
      break;
  }

  if (test_description.outcome == Outcome::kFailure) {
    // Use EvalJs here to wait for promises to resolve.
    EXPECT_THAT(EvalJs(shell(), command), EvalJsResult::IsError());
    return;
  }

  ASSERT_TRUE(ExecJs(shell(), command));

  run_loop.Run();

  // URLLoaderInterceptor writes to trust_token_request_ on the IO sequence.
  base::AutoLock lock(mutex_);

  ASSERT_TRUE(trust_token_request_);

  switch (test_description.outcome) {
    case Outcome::kSuccess:
      EXPECT_TRUE(trust_token_request_->trust_token_params);
      break;
    case Outcome::kSuccessWithoutTrustTokenParams:
      EXPECT_FALSE(trust_token_request_->trust_token_params);
      break;
    case Outcome::kFailure:
      NOTREACHED_IN_MIGRATION();  // Handled earlier.
  }
}

}  // namespace content
