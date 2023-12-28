// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kBaseDataDir[] = "content/test/data/compute_pressure";

class ComputePressureOriginTrialBrowserTest : public ContentBrowserTest {
 public:
  ~ComputePressureOriginTrialBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We need to use URLLoaderInterceptor (rather than a EmbeddedTestServer),
    // because origin trial token is associated with a fixed origin, whereas
    // EmbeddedTestServer serves content on a random port.
    interceptor_ = URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
        kBaseDataDir, GURL("https://example.test/"));
  }

  void TearDownOnMainThread() override {
    interceptor_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  bool HasComputePressureApi() {
    return EvalJs(shell(), "'PressureObserver' in window").ExtractBool();
  }

 protected:
  const GURL kValidTokenUrl{"https://example.test/valid_token.html"};
  const GURL kNoTokenUrl{"https://example.test/no_token.html"};

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_F(ComputePressureOriginTrialBrowserTest,
                       ValidOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kValidTokenUrl));
  EXPECT_TRUE(HasComputePressureApi());
}

IN_PROC_BROWSER_TEST_F(ComputePressureOriginTrialBrowserTest,
                       ValidThirdPartyOriginTrialToken) {
  // In this test, we use an EmbeddedTestServer because we need two
  // different origins to test the third-party OT token mechanism for
  // ComputePressure.
  // We use the URL provided by |https_server| for the main frame because we do
  // not care about the port number EmbeddedTestServer gives us. The page we
  // navigate to then loads a script served via |interceptor_|, as we need a
  // specific origin.
  embedded_https_test_server().ServeFilesFromSourceDirectory(
      GetTestDataFilePath());
  ASSERT_TRUE(embedded_https_test_server().Start());

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_https_test_server().GetURL(
                                 "/compute_pressure/third_party_token.html")));
  EXPECT_FALSE(HasComputePressureApi());
  ASSERT_TRUE(ExecJs(shell(), "insert3rdPartyToken()"));
  EXPECT_TRUE(HasComputePressureApi());
}

IN_PROC_BROWSER_TEST_F(ComputePressureOriginTrialBrowserTest,
                       NoOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kNoTokenUrl));
  EXPECT_FALSE(HasComputePressureApi());
}

class ComputePressureOriginTrialKillSwitchBrowserTest
    : public ComputePressureOriginTrialBrowserTest {
 public:
  ComputePressureOriginTrialKillSwitchBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kComputePressure);
  }
};

IN_PROC_BROWSER_TEST_F(ComputePressureOriginTrialKillSwitchBrowserTest,
                       ValidOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kValidTokenUrl));
  EXPECT_FALSE(HasComputePressureApi());
}

IN_PROC_BROWSER_TEST_F(ComputePressureOriginTrialKillSwitchBrowserTest,
                       NoOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kNoTokenUrl));
  EXPECT_FALSE(HasComputePressureApi());
}

}  // namespace

}  // namespace content
