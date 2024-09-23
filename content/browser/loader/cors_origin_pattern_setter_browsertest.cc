// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/cors_origin_pattern_setter.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_browser_context.h"
#include "content/shell/browser/shell.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

const auto kAllowSubdomains =
    network::mojom::CorsDomainMatchMode::kAllowSubdomains;
const auto kDisallowSubdomains =
    network::mojom::CorsDomainMatchMode::kDisallowSubdomains;

const char kTestPath[] = "/loader/cors_origin_access_list_test.html";

const char kTestHost[] = "crossorigin.example.com";
const char kTestHostInDifferentCase[] = "CrossOrigin.example.com";
const char kTestSubdomainHost[] = "subdomain.crossorigin.example.com";

// Tests end to end functionality of CORS access origin allow lists.
class CorsOriginPatternSetterBrowserTest : public ContentBrowserTest {
 public:
  CorsOriginPatternSetterBrowserTest(
      const CorsOriginPatternSetterBrowserTest&) = delete;
  CorsOriginPatternSetterBrowserTest& operator=(
      const CorsOriginPatternSetterBrowserTest&) = delete;

 protected:
  CorsOriginPatternSetterBrowserTest() = default;

  std::unique_ptr<TitleWatcher> CreateWatcher() {
    // Register all possible result strings here.
    std::unique_ptr<TitleWatcher> watcher =
        std::make_unique<TitleWatcher>(web_contents(), pass_string());
    watcher->AlsoWaitForTitle(fail_string());
    return watcher;
  }

  std::string GetReason() {
    bool executing = true;
    std::string reason;
    web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        script_,
        base::BindOnce(
            [](bool* flag, std::string* reason, base::Value value) {
              *flag = false;
              DCHECK(value.is_string());
              *reason = value.GetString();
            },
            base::Unretained(&executing), base::Unretained(&reason)),
        ISOLATED_WORLD_ID_GLOBAL);
    while (executing) {
      base::RunLoop loop;
      loop.RunUntilIdle();
    }
    return reason;
  }

  void SetAllowList(const std::string& scheme,
                    const std::string& host,
                    network::mojom::CorsDomainMatchMode mode) {
    {
      std::vector<network::mojom::CorsOriginPatternPtr> list;
      list.push_back(network::mojom::CorsOriginPattern::New(
          scheme, host, /*port=*/0, mode,
          network::mojom::CorsPortMatchMode::kAllowAnyPort,
          network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));

      base::RunLoop run_loop;
      CorsOriginPatternSetter::Set(
          browser_context(),
          url::Origin::Create(
              embedded_test_server()->base_url().DeprecatedGetOriginAsURL()),
          std::move(list), std::vector<network::mojom::CorsOriginPatternPtr>(),
          run_loop.QuitClosure());
      run_loop.Run();
    }

    {
      std::vector<network::mojom::CorsOriginPatternPtr> list;
      list.push_back(network::mojom::CorsOriginPattern::New(
          scheme, host, /*port=*/0, mode,
          network::mojom::CorsPortMatchMode::kAllowAnyPort,
          network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));

      base::RunLoop run_loop;
      CorsOriginPatternSetter::Set(
          browser_context(),
          url::Origin::Create(embedded_test_server()
                                  ->GetURL(kTestHost, "/")
                                  .DeprecatedGetOriginAsURL()),
          std::move(list), std::vector<network::mojom::CorsOriginPatternPtr>(),
          run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  std::string host_ip() { return embedded_test_server()->base_url().host(); }

  const std::u16string& pass_string() const { return pass_string_; }
  const std::u16string& fail_string() const { return fail_string_; }

  virtual WebContents* web_contents() { return shell()->web_contents(); }
  virtual BrowserContext* browser_context() {
    return web_contents()->GetBrowserContext();
  }

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

 private:
  const std::u16string pass_string_ = u"PASS";
  const std::u16string fail_string_ = u"FAIL";
  const std::u16string script_ = u"reason";
};

// Tests if specifying only protocol allows all hosts to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest, AllowAll) {
  SetAllowList("http", "", kAllowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(web_contents(),
                            embedded_test_server()->GetURL(base::StringPrintf(
                                "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if specifying only protocol allows all IP address based hosts to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest, AllowAllForIp) {
  SetAllowList("http", "", kAllowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          kTestHost,
          base::StringPrintf("%s?target=%s", kTestPath, host_ip().c_str()))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows only exactly matched host to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest, AllowExactHost) {
  SetAllowList("http", kTestHost, kDisallowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(web_contents(),
                            embedded_test_server()->GetURL(base::StringPrintf(
                                "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows host that matches exactly, but in
// case insensitive way to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest,
                       AllowExactHostInCaseInsensitive) {
  SetAllowList("http", kTestHost, kDisallowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(
      NavigateToURL(web_contents(),
                    embedded_test_server()->GetURL(base::StringPrintf(
                        "%s?target=%s", kTestPath, kTestHostInDifferentCase))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a host with a different port
// to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest, BlockDifferentPort) {
  SetAllowList("http", kTestHost, kDisallowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(base::StringPrintf(
                          "%s?target=%s&port_diff=1", kTestPath, kTestHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows a subdomain to pass if it is allowed.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest, AllowSubdomain) {
  SetAllowList("http", kTestHost, kAllowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(base::StringPrintf(
                          "%s?target=%s", kTestPath, kTestSubdomainHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a subdomain to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest, BlockSubdomain) {
  SetAllowList("http", kTestHost, kDisallowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(base::StringPrintf(
                          "%s?target=%s", kTestPath, kTestSubdomainHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a host with a different
// protocol to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest,
                       BlockDifferentProtocol) {
  SetAllowList("https", kTestHost, kDisallowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(web_contents(),
                            embedded_test_server()->GetURL(base::StringPrintf(
                                "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if IP address based hosts should not follow subdomain match rules.
IN_PROC_BROWSER_TEST_F(CorsOriginPatternSetterBrowserTest,
                       SubdomainMatchShouldNotBeAppliedForIPAddress) {
  SetAllowList("http", "*.0.0.1", kAllowSubdomains);

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          kTestHost,
          base::StringPrintf("%s?target=%s", kTestPath, host_ip().c_str()))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Assures that the access lists are properly propagated to the storage
// partitions and network contexts created after the lists have been set.
class NewContextCorsOriginPatternSetterBrowserTest
    : public CorsOriginPatternSetterBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    browser_context_ = CreateTestBrowserContext();
    CorsOriginPatternSetterBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    web_contents_.reset();
    CorsOriginPatternSetterBrowserTest::TearDownOnMainThread();
  }

  void TearDown() override {
    GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                          browser_context_.release());
    CorsOriginPatternSetterBrowserTest::TearDown();
  }

  NewContextCorsOriginPatternSetterBrowserTest() = default;

  WebContents* web_contents() override { return web_contents_.get(); }
  BrowserContext* browser_context() override { return browser_context_.get(); }

  void CreateWebContents() {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser_context_.get()));
  }

 private:
  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<WebContents> web_contents_;
};

IN_PROC_BROWSER_TEST_F(NewContextCorsOriginPatternSetterBrowserTest,
                       PatternListPropagation) {
  SetAllowList("http", "", kAllowSubdomains);
  CreateWebContents();

  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          kTestHost,
          base::StringPrintf("%s?target=%s", kTestPath, host_ip().c_str()))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

}  // namespace content
