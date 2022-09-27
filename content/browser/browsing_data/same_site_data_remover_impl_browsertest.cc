// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/same_site_data_remover_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/browsing_data/browsing_data_browsertest_utils.h"
#include "content/browser/browsing_data/browsing_data_test_utils.h"
#include "content/public/browser/same_site_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::IsEmpty;

namespace content {

class SameSiteDataRemoverBrowserTest : public ContentBrowserTest {
 public:
  SameSiteDataRemoverBrowserTest() {}

  SameSiteDataRemoverBrowserTest(const SameSiteDataRemoverBrowserTest&) =
      delete;
  SameSiteDataRemoverBrowserTest& operator=(
      const SameSiteDataRemoverBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Set up HTTP and HTTPS test servers that handle all hosts.
    host_resolver()->AddRule("*", "127.0.0.1");

    if (IsOutOfProcessNetworkService())
      browsing_data_browsertest_utils::SetUpMockCertVerifier(net::OK);

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&SameSiteDataRemoverBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    browsing_data_browsertest_utils::SetIgnoreCertificateErrors(command_line);
  }

  BrowserContext* GetBrowserContext() {
    return shell()->web_contents()->GetBrowserContext();
  }

  StoragePartition* GetStoragePartition() {
    return GetBrowserContext()->GetDefaultStoragePartition();
  }

  net::EmbeddedTestServer* GetHttpsServer() { return https_server_.get(); }

  void ClearData() {
    base::RunLoop run_loop;
    ClearSameSiteNoneData(run_loop.QuitClosure(), GetBrowserContext());
    run_loop.Run();
  }

  void ClearData(std::set<url::Origin> clear_storage_origins) {
    base::RunLoop run_loop;
    ClearSameSiteNoneCookiesAndStorageForOrigins(
        run_loop.QuitClosure(), GetBrowserContext(),
        std::move(clear_storage_origins));
    run_loop.Run();
  }

 private:
  // Handles all requests.
  //
  // Supports the following <key>=<value> query parameters in the url:
  // <key>="file"         responds with the content of file <value>
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response(std::make_unique<net::test_server::BasicHttpResponse>());

    std::string value;
    browsing_data_browsertest_utils::SetResponseContent(request.GetURL(),
                                                        &value, response.get());

    return std::move(response);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(SameSiteDataRemoverBrowserTest, TestClearData) {
  StoragePartition* storage_partition = GetStoragePartition();
  CreateCookieForTest(
      "TestCookie", "www.google.com", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      true /* is_cookie_secure */, GetBrowserContext());
  browsing_data_browsertest_utils::AddServiceWorker(
      "www.google.com", storage_partition, GetHttpsServer());

  ClearData();

  // Check that cookies were deleted.
  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetBrowserContext());
  EXPECT_THAT(cookies, IsEmpty());

  // Check that the service worker for the cookie domain was removed.
  std::vector<StorageUsageInfo> service_workers =
      browsing_data_browsertest_utils::GetServiceWorkers(storage_partition);
  EXPECT_THAT(service_workers, IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SameSiteDataRemoverBrowserTest,
                       TestClearDataWithOrigins) {
  StoragePartition* storage_partition = GetStoragePartition();
  CreateCookieForTest(
      "TestCookie", "www.google.com", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      true /* is_cookie_secure */, GetBrowserContext());
  GURL google_url = browsing_data_browsertest_utils::AddServiceWorker(
      "google.com", storage_partition, GetHttpsServer());
  browsing_data_browsertest_utils::AddServiceWorker(
      "foo.bar.com", storage_partition, GetHttpsServer());

  std::set<url::Origin> clear_origins = {url::Origin::Create(google_url)};
  ClearData(std::move(clear_origins));

  // Check that cookies were deleted.
  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetBrowserContext());
  EXPECT_THAT(cookies, IsEmpty());

  // Check that the service worker for the cookie domain was removed.
  std::vector<StorageUsageInfo> service_workers =
      browsing_data_browsertest_utils::GetServiceWorkers(storage_partition);
  EXPECT_EQ(service_workers.size(), 1u);
  EXPECT_EQ(service_workers[0].storage_key.origin().host(), "foo.bar.com");
}

}  // namespace content
