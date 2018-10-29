// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kTestPath[] = "/loader/cors_origin_access_list_test.html";

const char kTestHost[] = "crossorigin.example.com";
const char kTestHostInDifferentCase[] = "CrossOrigin.example.com";
const char kTestSubdomainHost[] = "subdomain.crossorigin.example.com";

enum class TestMode {
  kOutOfBlinkCorsWithServicification,
  kOutOfBlinkCorsWithoutServicification,
};

// Tests end to end functionality of CORS access origin allow lists.
class CorsOriginAccessListBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<TestMode> {
 public:
  CorsOriginAccessListBrowserTest() {
    switch (GetParam()) {
      case TestMode::kOutOfBlinkCorsWithServicification:
        scoped_feature_list_.InitWithFeatures(
            // Enabled features
            {network::features::kOutOfBlinkCORS,
             network::features::kNetworkService,
             blink::features::kServiceWorkerServicification},
            // Disabled features
            {});
        break;
      case TestMode::kOutOfBlinkCorsWithoutServicification:
        scoped_feature_list_.InitWithFeatures(
            // Enabled features
            {network::features::kOutOfBlinkCORS},
            // Disabled features
            {network::features::kNetworkService,
             blink::features::kServiceWorkerServicification});
        break;
    }
  }

 protected:
  std::unique_ptr<TitleWatcher> CreateWatcher() {
    // Register all possible result strings here.
    std::unique_ptr<TitleWatcher> watcher =
        std::make_unique<TitleWatcher>(shell()->web_contents(), pass_string());
    watcher->AlsoWaitForTitle(fail_string());
    return watcher;
  }

  std::string GetReason() {
    bool executing = true;
    std::string reason;
    shell()->web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        script_,
        base::BindRepeating(
            [](bool* flag, std::string* reason, const base::Value* value) {
              *flag = false;
              DCHECK(value);
              DCHECK(value->is_string());
              *reason = value->GetString();
            },
            base::Unretained(&executing), base::Unretained(&reason)));
    while (executing) {
      base::RunLoop loop;
      loop.RunUntilIdle();
    }
    return reason;
  }

  void SetAllowList(const std::string& scheme,
                    const std::string& host,
                    bool allow_subdomains) {
    std::vector<network::mojom::CorsOriginPatternPtr> list1;
    list1.push_back(network::mojom::CorsOriginPattern::New(
        scheme, host, allow_subdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority));
    bool first_list_done = false;
    BrowserContext::SetCorsOriginAccessListsForOrigin(
        shell()->web_contents()->GetBrowserContext(),
        url::Origin::Create(embedded_test_server()->base_url().GetOrigin()),
        std::move(list1), std::vector<network::mojom::CorsOriginPatternPtr>(),
        base::BindOnce([](bool* flag) { *flag = true; },
                       base::Unretained(&first_list_done)));

    std::vector<network::mojom::CorsOriginPatternPtr> list2;
    list2.push_back(network::mojom::CorsOriginPattern::New(
        scheme, host, allow_subdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority));
    bool second_list_done = false;
    BrowserContext::SetCorsOriginAccessListsForOrigin(
        shell()->web_contents()->GetBrowserContext(),
        url::Origin::Create(
            embedded_test_server()->GetURL(kTestHost, "/").GetOrigin()),
        std::move(list2), std::vector<network::mojom::CorsOriginPatternPtr>(),
        base::BindOnce([](bool* flag) { *flag = true; },
                       base::Unretained(&second_list_done)));
    while (!first_list_done || !second_list_done) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  std::string host_ip() { return embedded_test_server()->base_url().host(); }

  const base::string16& pass_string() const { return pass_string_; }
  const base::string16& fail_string() const { return fail_string_; }

 private:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Setup to resolve kTestHost, kTestHostInDifferentCase and
    // kTestSubdomainHost to the 127.0.0.1 that the test server serves.
    host_resolver()->AddRule(kTestHost,
                             embedded_test_server()->host_port_pair().host());
    host_resolver()->AddRule(kTestHostInDifferentCase,
                             embedded_test_server()->host_port_pair().host());
    host_resolver()->AddRule(kTestSubdomainHost,
                             embedded_test_server()->host_port_pair().host());
  }

  const base::string16 pass_string_ = base::ASCIIToUTF16("PASS");
  const base::string16 fail_string_ = base::ASCIIToUTF16("FAIL");
  const base::string16 script_ = base::ASCIIToUTF16("reason");

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CorsOriginAccessListBrowserTest);
};

// Tests if specifying only protocol allows all hosts to pass.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest, AllowAll) {
  SetAllowList("http", "", true);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(base::StringPrintf(
                                 "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if specifying only protocol allows all IP address based hosts to pass.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest, AllowAllForIp) {
  SetAllowList("http", "", true);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   kTestHost, base::StringPrintf("%s?target=%s", kTestPath,
                                                 host_ip().c_str()))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows only exactly matched host to pass.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest, AllowExactHost) {
  SetAllowList("http", kTestHost, false);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(base::StringPrintf(
                                 "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows host that matches exactly, but in
// case insensitive way to pass.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest,
                       AllowExactHostInCaseInsensitive) {
  SetAllowList("http", kTestHost, false);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?target=%s", kTestPath, kTestHostInDifferentCase))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a host with a different port
// to pass.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest, BlockDifferentPort) {
  SetAllowList("http", kTestHost, false);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?target=%s&port_diff=1", kTestPath, kTestHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows a subdomain to pass if it is allowed.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest, AllowSubdomain) {
  SetAllowList("http", kTestHost, true);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?target=%s", kTestPath, kTestSubdomainHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a subdomain to pass.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest, BlockSubdomain) {
  SetAllowList("http", kTestHost, false);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?target=%s", kTestPath, kTestSubdomainHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a host with a different
// protocol to pass.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest,
                       BlockDifferentProtocol) {
  SetAllowList("https", kTestHost, false);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(base::StringPrintf(
                                 "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if IP address based hosts should not follow subdomain match rules.
IN_PROC_BROWSER_TEST_P(CorsOriginAccessListBrowserTest,
                       SubdomainMatchShouldNotBeAppliedForIPAddress) {
  SetAllowList("http", "*.0.0.1", true);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   kTestHost, base::StringPrintf("%s?target=%s", kTestPath,
                                                 host_ip().c_str()))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

INSTANTIATE_TEST_CASE_P(
    OutOfBlinkCorsWithServicification,
    CorsOriginAccessListBrowserTest,
    ::testing::Values(TestMode::kOutOfBlinkCorsWithServicification));

INSTANTIATE_TEST_CASE_P(
    OutOfBlinkCorsWithoutServicification,
    CorsOriginAccessListBrowserTest,
    ::testing::Values(TestMode::kOutOfBlinkCorsWithoutServicification));

// TODO(toyoshim): Instantiates tests for the case kOutOfBlinkCORS is disabled
// and remove relevant LayoutTests if it's possible.

}  // namespace

}  // namespace content
