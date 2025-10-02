// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"

using content::URLLoaderInterceptor;

namespace {
constexpr char kBaseDataDir[] = "headless/test/data/";

}  // namespace

namespace headless {

class HeadlessOriginTrialsBrowserTest : public HeadlessBrowserTest {
 public:
  HeadlessOriginTrialsBrowserTest() = default;

  HeadlessOriginTrialsBrowserTest(const HeadlessOriginTrialsBrowserTest&) =
      delete;
  HeadlessOriginTrialsBrowserTest& operator=(
      const HeadlessOriginTrialsBrowserTest&) = delete;

  ~HeadlessOriginTrialsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    HeadlessBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());
              return true;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    HeadlessBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(HeadlessOriginTrialsBrowserTest,
                       TrialsDisabledByDefault) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("https://example.test/no_origin_trial.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  // Ensures that createShadowRoot() is not defined, as no token is provided to
  // enable the WebComponents V0 origin trial.
  // TODO(crbug.com/40673000): Implement a permanent, sample trial so this test
  // doesn't rely on WebComponents V0, which will eventually go away.
  EXPECT_THAT(
      EvaluateScript(web_contents,
                     "'createShadowRoot' in document.createElement('div')"),
      DictHasValue("result.result.value", false));
}

IN_PROC_BROWSER_TEST_F(HeadlessOriginTrialsBrowserTest,
                       DelegateAvailableOnContext) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();
  HeadlessBrowserContextImpl* context_impl =
      HeadlessBrowserContextImpl::From(browser_context);

  EXPECT_TRUE(context_impl->GetOriginTrialsControllerDelegate())
      << "Headless browser should have an OriginTrialsControllerDelegate";
}

}  // namespace headless
