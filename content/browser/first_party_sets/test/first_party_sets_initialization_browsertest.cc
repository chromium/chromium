// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "content/browser/first_party_sets/test/scoped_mock_first_party_sets_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kHostA[] = "a.test";

class FirstPartySetsDeadlockingQueriesBrowserTest
    : public content::ContentBrowserTest {
 public:
  FirstPartySetsDeadlockingQueriesBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{net::features::kWaitForFirstPartySetsInit});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers();
    ASSERT_TRUE(https_server_.Start());
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // `SetUpOnMainThread` and `SetUp` are called too late in the browsertest
    // lifecycle; we have to substitute the FirstPartySetsHandler early, before
    // the browser process is fully started.
    scoped_first_party_sets_handler_ =
        std::make_unique<content::ScopedMockFirstPartySetsHandler>();
    scoped_first_party_sets_handler_->set_should_deadlock(true);
  }

  void SetCrossSiteCookieOnDomain(const std::string& domain) {
    GURL domain_url = https_server_.GetURL(domain, "/");
    std::string cookie = base::StrCat({"cross-site=", domain});
    content::SetCookie(
        shell()->web_contents()->GetBrowserContext(), domain_url,
        base::StrCat({cookie, ";SameSite=None;Secure;Domain=", domain}));
    ASSERT_THAT(content::GetCookies(
                    shell()->web_contents()->GetBrowserContext(), domain_url),
                testing::HasSubstr(cookie));
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    return shell()->web_contents()->GetPrimaryMainFrame();
  }

  net::test_server::EmbeddedTestServer& https_server() { return https_server_; }

  content::ScopedMockFirstPartySetsHandler& handler() {
    return *scoped_first_party_sets_handler_;
  }

 private:
  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::ScopedMockFirstPartySetsHandler>
      scoped_first_party_sets_handler_;
};

IN_PROC_BROWSER_TEST_F(FirstPartySetsDeadlockingQueriesBrowserTest,
                       NoDeadlock) {
  SetCrossSiteCookieOnDomain(kHostA);

  // This test fixture has been set up such that First-Party Sets never responds
  // to any query (even though technically it does finish initializing).
  // However, since `kWaitForFirstPartySetsInit` is disabled, that should not
  // cause a deadlock, and network requests should still finish (even those that
  // require cookies).
  //
  // Note that calls to `document.requestStorageAccess()` will deadlock since
  // they rely on FPS having been initialized.

  ASSERT_TRUE(content::NavigateToURL(
      shell()->web_contents(),
      https_server().GetURL(kHostA, "/echoheader?cookie")));
  EXPECT_EQ("cross-site=a.test", content::EvalJs(GetPrimaryMainFrame(),
                                                 "document.body.textContent"));
}
