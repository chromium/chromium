// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/same_site_data_remover_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/browsing_data/browsing_data_browsertest_utils.h"
#include "content/browser/browsing_data/browsing_data_test_utils.h"
#include "content/public/browser/same_site_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;

namespace content {

class SameSiteDataRemoverBrowserTest : public ContentBrowserTest {
 public:
  SameSiteDataRemoverBrowserTest() {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Set up HTTP and HTTPS test servers that handle all hosts.
    host_resolver()->AddRule("*", "127.0.0.1");

    if (IsOutOfProcessNetworkService())
      browsing_data_browsertest_utils::SetUpMockCertVerifier(net::OK);

    https_server_.reset(new net::EmbeddedTestServer(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS));
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
    return BrowserContext::GetDefaultStoragePartition(GetBrowserContext());
  }

  net::EmbeddedTestServer* GetHttpsServer() { return https_server_.get(); }

  void ClearData(bool clear_storage) {
    base::RunLoop run_loop;
    ClearSameSiteNoneData(run_loop.QuitClosure(), GetBrowserContext(),
                          clear_storage);
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

  DISALLOW_COPY_AND_ASSIGN(SameSiteDataRemoverBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SameSiteDataRemoverBrowserTest,
                       TestClearDataWithStorageRemoval) {
  StoragePartition* storage_partition = GetStoragePartition();
  CreateCookieForTest("TestCookie", "www.google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
                      true /* is_cookie_secure */, GetBrowserContext());
  browsing_data_browsertest_utils::AddServiceWorker(
      "www.google.com", storage_partition, GetHttpsServer());

  ClearData(/* clear_storage= */ true);

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
                       TestClearDataWithoutStorageRemoval) {
  StoragePartition* storage_partition = GetStoragePartition();
  CreateCookieForTest("TestCookie", "www.google.com",
                      net::CookieSameSite::NO_RESTRICTION,
                      net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
                      true /* is_cookie_secure */, GetBrowserContext());
  browsing_data_browsertest_utils::AddServiceWorker(
      "www.google.com", storage_partition, GetHttpsServer());

  ClearData(/* clear_storage= */ false);

  // Check that cookies were deleted.
  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(GetBrowserContext());
  EXPECT_THAT(cookies, IsEmpty());

  // Storage partition data should NOT have been cleared.
  std::vector<StorageUsageInfo> service_workers =
      browsing_data_browsertest_utils::GetServiceWorkers(storage_partition);
  ASSERT_EQ(1u, service_workers.size());
  EXPECT_EQ(service_workers[0].origin.GetURL(),
            GetHttpsServer()->GetURL("www.google.com", "/"));
}

}  // namespace content
