// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browsing_data_remover.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
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

  void RemoveAndWait(int remove_mask) {
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
// set.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       PreserveTransportSecurityState) {
  IssueRequestThatSetsHsts();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
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

}  // namespace content
