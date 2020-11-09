// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browsing_data_remover.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kHstsPath[] = "/hsts";
const char kHttpAuthPath[] = "/http_auth";
const char kHstsResponseBody[] = "HSTS set";

std::unique_ptr<net::test_server::HttpResponse> HandleHstsRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == kHstsPath) {
    std::unique_ptr<net::test_server::BasicHttpResponse> hsts_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    hsts_response->AddCustomHeader("Strict-Transport-Security",
                                   "max-age=1000000");
    hsts_response->set_content(kHstsResponseBody);
    return hsts_response;
  }
  return nullptr;
}

// Handles |request| to "/http_auth". If "Authorization" header is present,
// responds with a non-empty HTTP 200 page (regardless of auth credentials).
// Otherwise serves a Basic Auth challenge.
std::unique_ptr<net::test_server::HttpResponse> HandleHttpAuthRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kHttpAuthPath)
    return nullptr;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (base::Contains(request.headers, "Authorization")) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("Success!");
  } else {
    http_response->set_code(net::HTTP_UNAUTHORIZED);
    http_response->AddCustomHeader("WWW-Authenticate",
                                   "Basic realm=\"test realm\"");
  }
  return http_response;
}

}  // namespace

namespace content {

class BrowsingDataRemoverImplBrowserTest : public ContentBrowserTest {
 public:
  BrowsingDataRemoverImplBrowserTest()
      : ssl_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    // Use localhost instead of 127.0.0.1, as HSTS isn't allowed on IPs.
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.RegisterRequestHandler(base::BindRepeating(&HandleHstsRequest));
    ssl_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleHttpAuthRequest));
    EXPECT_TRUE(ssl_server_.Start());
  }

  void SetUpOnMainThread() override {}

  void RemoveAndWait(uint64_t remove_mask) {
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(
            shell()->web_contents()->GetBrowserContext());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  void RemoveWithFilterAndWait(
      uint64_t remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter) {
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(
            shell()->web_contents()->GetBrowserContext());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter), &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  // Issues a request for kHstsPath on localhost, and expects it to enable HSTS
  // for the domain.
  void IssueRequestThatSetsHsts() {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = ssl_server_.GetURL("localhost", kHstsPath);

    SimpleURLLoaderTestHelper loader_helper;
    std::unique_ptr<network::SimpleURLLoader> loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory(), loader_helper.GetCallback());
    loader_helper.WaitForCallback();
    ASSERT_TRUE(loader_helper.response_body());
    EXPECT_EQ(kHstsResponseBody, *loader_helper.response_body());

    EXPECT_TRUE(IsHstsSet());
  }

  // Returns true if HSTS is set on localhost.  Does this by issuing an HTTP
  // request to the embedded test server, and expecting it to be redirected from
  // HTTP to HTTPS if HSTS is enabled.  If the request succeeds, it was sent
  // over HTTPS, so HSTS is enabled. If it fails, the request was send using
  // HTTP instead, so HSTS is not enabled for the domain.
  bool IsHstsSet() {
    GURL url = ssl_server_.GetURL("localhost", "/echo");
    GURL::Replacements replacements;
    replacements.SetSchemeStr("http");
    url = url.ReplaceComponents(replacements);
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = url;

    std::unique_ptr<network::SimpleURLLoader> loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    SimpleURLLoaderTestHelper loader_helper;
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory(), loader_helper.GetCallback());
    loader_helper.WaitForCallback();

    // On success, HSTS was enabled for the domain.
    if (loader_helper.response_body()) {
      EXPECT_EQ("Echo", *loader_helper.response_body());
      return true;
    }

    // On failure, the server just hangs up, since it didn't receive an SSL
    // handshake.
    EXPECT_EQ(net::ERR_EMPTY_RESPONSE, loader->NetError());
    return false;
  }

  // Sets HTTP auth cache by making a request with credentials specified in the
  // URL to a page with an auth challenge.
  void IssueRequestThatSetsHttpAuthCache() {
    GURL url = ssl_server_.GetURL(kHttpAuthPath);
    GURL::Replacements replacements;
    replacements.SetUsernameStr("user");
    replacements.SetPasswordStr("password");
    GURL url_with_creds = url.ReplaceComponents(replacements);
    ASSERT_TRUE(NavigateToURL(shell(), url_with_creds));

    ASSERT_TRUE(IsHttpAuthCacheSet());
  }

  // Determines if auth cache is populated by seeing if a request to a page with
  // an auth challenge succeeds.
  bool IsHttpAuthCacheSet() {
    // Set a login request callback to be used instead of a login dialog since
    // such a dialog is difficult to control programmatically and doesn't work
    // on all platforms.
    bool login_requested = false;
    ShellContentBrowserClient::Get()->set_login_request_callback(
        base::BindLambdaForTesting(
            [&](bool is_main_frame /* unused */) { login_requested = true; }));

    GURL url = ssl_server_.GetURL(kHttpAuthPath);
    bool navigation_suceeded = NavigateToURL(shell(), url);

    // Because our login request callback does nothing, navigation should
    // succeed iff login is not needed unless some other unexpected error
    // occurs.
    EXPECT_NE(navigation_suceeded, login_requested);

    return !login_requested && navigation_suceeded;
  }

  network::mojom::URLLoaderFactory* url_loader_factory() {
    return BrowserContext::GetDefaultStoragePartition(
               shell()->web_contents()->GetBrowserContext())
        ->GetURLLoaderFactoryForBrowserProcess()
        .get();
  }

  network::mojom::NetworkContext* network_context() {
    return BrowserContext::GetDefaultStoragePartition(
               shell()->web_contents()->GetBrowserContext())
        ->GetNetworkContext();
  }

 private:
  net::test_server::EmbeddedTestServer ssl_server_;
};

// Verify that TransportSecurityState data is cleared for REMOVE_CACHE.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       ClearTransportSecurityState) {
  IssueRequestThatSetsHsts();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_CACHE);
  EXPECT_FALSE(IsHstsSet());
}

// Verify that TransportSecurityState data is not cleared if REMOVE_CACHE is not
// set or there is a deletelist filter.
// TODO(crbug.com/1040065): Add support for filtered deletions and update test.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       PreserveTransportSecurityState) {
  IssueRequestThatSetsHsts();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  EXPECT_TRUE(IsHstsSet());

  auto filter = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter->AddRegisterableDomain("foobar.com");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_CACHE,
                          std::move(filter));
  EXPECT_TRUE(IsHstsSet());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest, ClearHttpAuthCache) {
  ASSERT_FALSE(IsHttpAuthCacheSet());
  IssueRequestThatSetsHttpAuthCache();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_COOKIES);
  EXPECT_FALSE(IsHttpAuthCacheSet());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       PreserveHttpAuthCache) {
  ASSERT_FALSE(IsHttpAuthCacheSet());
  IssueRequestThatSetsHttpAuthCache();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  EXPECT_TRUE(IsHttpAuthCacheSet());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       ClearHttpAuthCacheWhenEmpty) {
  ASSERT_FALSE(IsHttpAuthCacheSet());

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_COOKIES);
  EXPECT_FALSE(IsHttpAuthCacheSet());
}

namespace {

// Provide BrowsingDataRemoverImplTrustTokenTest the Trust Tokens
// feature as a mixin so that it gets set before the superclass initializes the
// test's NetworkContext, as the NetworkContext's initialization must occur with
// the feature enabled.
class WithTrustTokensEnabled {
 public:
  WithTrustTokensEnabled() {
    feature_list_.InitAndEnableFeature(network::features::kTrustTokens);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests Trust Tokens clearing by calling HasTrustTokensAnswerer::HasTrustTokens
// with a HasTrustTokensAnswerer obtained from the provided NetworkContext.
//
// The Trust Tokens functionality places a cap of 2 distinct arguments to the
// |issuer| argument of
//       HasTrustTokensAnswerer(origin)::HasTrustTokens(issuer)
// for each top-frame origin |origin|. (This limit is recorded in persistent
// storage scoped to the origin |origin| and is not related to the lifetime of
// the specific HasTrustTokensAnswerer object.)
//
// To add an origin, the tester creates a HasTrustTokensAnswerer parameterized
// by |origin| and calls HasTrustTokens with two distinct "priming" issuer
// arguments. This will make the Trust Tokens persistent storage record that
// |origin| is associated with each of these issuers, with the effect that
// (barring a data clear) subsequent HasTrustTokens calls with different issuer
// arguments will fail. To check if an origin is present, the tester calls
//    HasTrustTokensAnswerer(origin)::HasTrustTokens(issuer)
// with an |issuer| argument distinct from the two earlier "priming" issuers.
// This third HasTrustTokens call will error out exactly if |origin| was
// previously added by AddOrigin.
//
// Usage:
//   >= 0 AddOrigin() - origins must be HTTPS
//   (clear data)
//   >= 0 HasOrigin()
class TrustTokensTester {
 public:
  explicit TrustTokensTester(network::mojom::NetworkContext* network_context)
      : network_context_(network_context) {}

  void AddOrigin(const url::Origin& origin) {
    mojo::Remote<network::mojom::HasTrustTokensAnswerer> answerer;
    network_context_->GetHasTrustTokensAnswerer(
        answerer.BindNewPipeAndPassReceiver(), origin);

    // Calling HasTrustTokens will associate the issuer argument with the
    // origin |origin|.
    //
    // Do this until the |origin| is associated with
    // network::kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers many issuers
    // (namely 2; this value is not expected to change frequently).
    //
    // After the limit is reached, subsequent HasTrustToken(origin, issuer)
    // queries will fail for any issuers not in {https://prime0.example,
    // https://prime1.example} --- unless data for |origin| is cleared.
    for (int i = 0; i < 2; ++i) {
      base::RunLoop run_loop;
      answerer->HasTrustTokens(
          url::Origin::Create(
              GURL(base::StringPrintf("https://prime%d.example", i))),
          base::BindLambdaForTesting(
              [&](network::mojom::HasTrustTokensResultPtr) {
                run_loop.Quit();
              }));
      run_loop.Run();
    }
  }

  bool HasOrigin(const url::Origin& origin) {
    mojo::Remote<network::mojom::HasTrustTokensAnswerer> answerer;
    network_context_->GetHasTrustTokensAnswerer(
        answerer.BindNewPipeAndPassReceiver(), origin);

    base::RunLoop run_loop;
    bool has_origin = false;

    // Since https://probe.example is not among the issuer origins previously
    // provided to HasTrustTokens(origin, _) calls in AddOrigin:
    // - If data has not been cleared,
    //     HasTrustToken(origin, https://probe.example)
    //   is expected to fail with kResourceExhausted because |origin| is at its
    //   number-of-associated-issuers limit, so the answerer will refuse to
    //   answer a query for an origin it has not yet seen.
    // - If data has been cleared, the answerer should be able to fulfill the
    //   query.
    answerer->HasTrustTokens(
        url::Origin::Create(GURL("https://probe.example")),
        base::BindLambdaForTesting([&](network::mojom::HasTrustTokensResultPtr
                                           result) {
          // HasTrustTokens will error out with kResourceExhausted exactly
          // when the top-frame origin |origin| was previously added by
          // AddOrigin.
          if (result->status ==
              network::mojom::TrustTokenOperationStatus::kResourceExhausted) {
            has_origin = true;
          }

          run_loop.Quit();
        }));

    run_loop.Run();

    return has_origin;
  }

 private:
  network::mojom::NetworkContext* network_context_;
};

}  // namespace

class BrowsingDataRemoverImplTrustTokenTest
    : public WithTrustTokensEnabled,
      public BrowsingDataRemoverImplBrowserTest {};

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplTrustTokenTest, Remove) {
  TrustTokensTester tester(network_context());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));

  tester.AddOrigin(origin);
  ASSERT_TRUE(tester.HasOrigin(origin));

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS);

  EXPECT_FALSE(tester.HasOrigin(origin));
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplTrustTokenTest, RemoveByDomain) {
  TrustTokensTester tester(network_context());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));
  auto sub_origin = url::Origin::Create(GURL("https://sub.topframe.example"));
  auto another_origin =
      url::Origin::Create(GURL("https://another-topframe.example"));

  tester.AddOrigin(origin);
  tester.AddOrigin(sub_origin);
  tester.AddOrigin(another_origin);

  ASSERT_TRUE(tester.HasOrigin(origin));
  ASSERT_TRUE(tester.HasOrigin(sub_origin));
  ASSERT_TRUE(tester.HasOrigin(another_origin));

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain("topframe.example");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
                          std::move(builder));

  EXPECT_FALSE(tester.HasOrigin(origin));
  EXPECT_FALSE(tester.HasOrigin(sub_origin));
  EXPECT_TRUE(tester.HasOrigin(another_origin));
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplTrustTokenTest,
                       PreserveByDomain) {
  TrustTokensTester tester(network_context());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));
  auto sub_origin = url::Origin::Create(GURL("https://sub.topframe.example"));
  auto another_origin =
      url::Origin::Create(GURL("https://another-topframe.example"));

  tester.AddOrigin(origin);
  tester.AddOrigin(sub_origin);
  tester.AddOrigin(another_origin);
  ASSERT_TRUE(tester.HasOrigin(origin));
  ASSERT_TRUE(tester.HasOrigin(sub_origin));
  ASSERT_TRUE(tester.HasOrigin(another_origin));

  // Delete all data *except* that specified by the filter.
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  builder->AddRegisterableDomain("topframe.example");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
                          std::move(builder));

  EXPECT_TRUE(tester.HasOrigin(origin));
  EXPECT_TRUE(tester.HasOrigin(sub_origin));
  EXPECT_FALSE(tester.HasOrigin(another_origin));
}

}  // namespace content
