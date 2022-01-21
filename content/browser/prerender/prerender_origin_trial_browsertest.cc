// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
namespace {

// Generate token with the command:
// generate_token.py https://prerender2.test:443 Prerender2
// --expire-timestamp=2000000000
base::StringPiece origin_trial_token =
    "A+QUuynQlwucleswdxzQ90sAEAG5wKtEUvwrsTLkdNK33ywwb9E0YRcwSu6vG9RG4kNeTPBZBL"
    "sGVzAnKxsNPQ8AAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly9wcmVyZW5kZXIyLnRlc3Q6NDQzIiwg"
    "ImZlYXR1cmUiOiAiUHJlcmVuZGVyMiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

constexpr char kAddSpeculationRuleScript[] = R"({
    const script = document.createElement('script');
    script.type = 'speculationrules';
    script.text = `{
      "prerender": [{
        "source": "list",
        "urls": [$1]
      }]
    }`;
    document.head.appendChild(script);
  })";

enum class FeatureEnabledType {
  kDisabled,
  kEnabled,
  kDefault,
};

std::string FeatureEnabledTypeToString(const FeatureEnabledType& type) {
  switch (type) {
    case FeatureEnabledType::kDisabled:
      return "FeatureDisabled";
    case FeatureEnabledType::kEnabled:
      return "FeatureEnabled";
    case FeatureEnabledType::kDefault:
      return "FeatureDefault";
  }
}

enum class BlinkFeatureEnabledType {
  kDisabled,
  kEnabled,
  kDefault,
};

std::string BlinkFeatureEnabledTypeToString(
    const BlinkFeatureEnabledType& type) {
  switch (type) {
    case BlinkFeatureEnabledType::kDisabled:
      return "BlinkFeatureDisabled";
    case BlinkFeatureEnabledType::kEnabled:
      return "BlinkFeatureEnabled";
    case BlinkFeatureEnabledType::kDefault:
      return "BlinkFeatureDefault";
  }
}

std::string EnabledTypesToString(
    const testing::TestParamInfo<
        testing::tuple<FeatureEnabledType, BlinkFeatureEnabledType>>& info) {
  return FeatureEnabledTypeToString(testing::get<0>(info.param)) + "_" +
         BlinkFeatureEnabledTypeToString(testing::get<1>(info.param));
}

class PrerenderOriginTrialBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          testing::tuple<FeatureEnabledType, BlinkFeatureEnabledType>> {
 protected:
  PrerenderOriginTrialBrowserTest() {
    switch (testing::get<0>(GetParam())) {
      case FeatureEnabledType::kDisabled:
        feature_list_.InitAndDisableFeature(blink::features::kPrerender2);
        break;
      case FeatureEnabledType::kEnabled:
        // Enable prerendering with no physical memory requirement so the test
        // can run on any bot.
        feature_list_.InitWithFeatures(
            {blink::features::kPrerender2},
            {blink::features::kPrerender2MemoryControls});
        break;
      case FeatureEnabledType::kDefault:
        // Keep the default state for blink::features::kPrerender2, but disable
        // the physical memory requirement so the test can run on any bot.
        feature_list_.InitAndDisableFeature(
            blink::features::kPrerender2MemoryControls);
        break;
    }
  }
  ~PrerenderOriginTrialBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    switch (testing::get<1>(GetParam())) {
      case BlinkFeatureEnabledType::kDisabled:
        command_line->AppendSwitchASCII(switches::kDisableBlinkFeatures,
                                        "Prerender2");
        break;
      case BlinkFeatureEnabledType::kEnabled:
        command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                        "Prerender2");
        break;
      case BlinkFeatureEnabledType::kDefault:
        break;
    }
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    url_loader_interceptor_.emplace(
        base::BindRepeating(&PrerenderOriginTrialBrowserTest::InterceptRequest,
                            base::Unretained(this)));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void LoadPageWithoutTrialToken() {
    ASSERT_TRUE(NavigateToURL(web_contents(), GetUrl("/")));
  }

  void LoadPageWithTrialToken() {
    ASSERT_TRUE(NavigateToURL(web_contents(), GetUrl("/with_token")));
  }

  void CheckFeatureDisabled() {
    EXPECT_EQ(false, EvalJs(web_contents(), "'prerendering' in document"));
    EXPECT_EQ(false,
              EvalJs(web_contents(), "'onprerenderingchange' in document"));
    AddSpeculationRuleScript();
    CheckPrerenderRequestIsNotSent();
  }

  // Check the Prerender2 feature is enabled on the page.
  // |enabled_on_prerendered_page| indicates whether the Prerender2 feature
  // should be enabled on the prerendered page or not.
  void CheckFeatureEnabled(bool enabled_on_prerendered_page) {
    EXPECT_EQ(true, EvalJs(web_contents(), "'prerendering' in document"));
    EXPECT_EQ(true,
              EvalJs(web_contents(), "'onprerenderingchange' in document"));
    AddSpeculationRuleScript();
    test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
        *web_contents(), GetUrl("/prerender"));
    EXPECT_TRUE(prerender_requested_);

    std::unique_ptr<WebContentsConsoleObserver> console_observer;
    if (!enabled_on_prerendered_page) {
      const char* kConsolePattern =
          "Failed to dispatch 'prerenderingchange' event: Prerender2 feature "
          "is not enabled on the document.";
      console_observer =
          std::make_unique<WebContentsConsoleObserver>(web_contents());
      console_observer->SetPattern(kConsolePattern);
    }

    test::PrerenderTestHelper::NavigatePrimaryPage(*web_contents(),
                                                   GetUrl("/prerender"));
    EXPECT_EQ(enabled_on_prerendered_page,
              EvalJs(web_contents(), "'prerendering' in document"));
    EXPECT_EQ(enabled_on_prerendered_page,
              EvalJs(web_contents(), "'onprerenderingchange' in document"));
    if (enabled_on_prerendered_page) {
      EXPECT_EQ(true, EvalJs(web_contents(),
                             "onprerenderingchange_observed_promise"));
    } else {
      console_observer->Wait();
      EXPECT_EQ(1u, console_observer->messages().size());
      EXPECT_EQ(false, EvalJs(web_contents(), "onprerenderingchange_observed"));
    }
  }

 private:
  static GURL GetUrl(const std::string& path) {
    return GURL("https://prerender2.test/").Resolve(path);
  }

  // URLLoaderInterceptor callback
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    // Construct and send the response.
    std::string headers =
        "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
    if (params->url_request.url.path() == "/with_token")
      base::StrAppend(&headers, {"Origin-Trial: ", origin_trial_token, "\n"});
    headers += '\n';
    std::string body = R"(
        <script>
        let onprerenderingchange_observed = false;
        const onprerenderingchange_observed_promise = new Promise(resolve => {
          document.addEventListener('prerenderingchange', () => {
            onprerenderingchange_observed = true;
            resolve(true);
          });
        });
        </script>
    )";
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());

    if (params->url_request.url.path() == "/prerender") {
      prerender_requested_ = true;
    }
    return true;
  }

  void CheckPrerenderRequestIsNotSent() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
    run_loop.Run();
    EXPECT_FALSE(prerender_requested_);
  }

  void AddSpeculationRuleScript() {
    EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_TRUE(ExecJs(web_contents(), JsReplace(kAddSpeculationRuleScript,
                                                 GetUrl("/prerender"))));
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  absl::optional<URLLoaderInterceptor> url_loader_interceptor_;
  bool prerender_requested_ = false;
  base::test::ScopedFeatureList feature_list_;
};

// We basically use blink::features::kPrerender2 to enable Prerender 2 related
// features (eg: chrome://flags/#enable-prerender2, Field Trial Testing Config).
// But it is still posiible to change the availability this feature by enabling
// (/disabling) Blink runtime-enabled features (REF) with
// --enable(/disable)-blink-features=Prerender2. So we test all the combinations
// of them here.
INSTANTIATE_TEST_SUITE_P(
    All,
    PrerenderOriginTrialBrowserTest,
    testing::Combine(testing::Values(FeatureEnabledType::kDisabled,
                                     FeatureEnabledType::kEnabled,
                                     FeatureEnabledType::kDefault),
                     testing::Values(BlinkFeatureEnabledType::kDisabled,
                                     BlinkFeatureEnabledType::kEnabled,
                                     BlinkFeatureEnabledType::kDefault)),
    EnabledTypesToString);

// Check the availability of Prerender 2 related APIs on a page without a valid
// Origin Trial token. The following table shows the expected availability.
//                |    blink::features::kPrerender2   |
//                | disabled  | enabled   | default   |
// ---------------|-----------|-----------|-----------|
//     | disabled | false     | false     | false     |
// REF | enabled  | false     | true      | IsAndroid |
//     | default  | false     | true      | false     |
IN_PROC_BROWSER_TEST_P(PrerenderOriginTrialBrowserTest, WithoutTrialToken) {
  LoadPageWithoutTrialToken();
  switch (testing::get<0>(GetParam())) {
    case FeatureEnabledType::kDisabled:
      CheckFeatureDisabled();
      break;
    case FeatureEnabledType::kEnabled:
      if (testing::get<1>(GetParam()) == BlinkFeatureEnabledType::kDisabled) {
        CheckFeatureDisabled();
      } else {
        CheckFeatureEnabled(true);
      }
      break;
    case FeatureEnabledType::kDefault:
      // Currently blink::features::kPrerender2 is default-enabled only on
      // Android.
#if BUILDFLAG(IS_ANDROID)
      if (testing::get<1>(GetParam()) == BlinkFeatureEnabledType::kEnabled) {
        CheckFeatureEnabled(true);
      } else {
        CheckFeatureDisabled();
      }
#else   // BUILDFLAG(IS_ANDROID)
      CheckFeatureDisabled();
#endif  // BUILDFLAG(IS_ANDROID)
      break;
  }
}

// Check the availability of Prerender 2 related APIs on a page with a valid
// Origin Trial token. The following table shows the expected availability:
// (The expected availability on the prerendered page should be same as the
// availability on a page without a valid Origin Trial token listed above.)
//                |    blink::features::kPrerender2   |
//                | disabled  | enabled   | default   |
// ---------------|-----------|-----------|-----------|
//     | disabled | false     | IsAndroid | IsAndroid |
// REF | enabled  | false     | true      | IsAndroid |
//     | default  | false     | true      | IsAndroid |
IN_PROC_BROWSER_TEST_P(PrerenderOriginTrialBrowserTest, WithTrialToken) {
  LoadPageWithTrialToken();
  switch (testing::get<0>(GetParam())) {
    case FeatureEnabledType::kDisabled:
      CheckFeatureDisabled();
      break;
    case FeatureEnabledType::kEnabled:
      if (testing::get<1>(GetParam()) == BlinkFeatureEnabledType::kDisabled) {
        // Currently Origin Trial for Prerender2 is available only on Android.
#if BUILDFLAG(IS_ANDROID)
        CheckFeatureEnabled(false);
#else   // BUILDFLAG(IS_ANDROID)
        CheckFeatureDisabled();
#endif  // BUILDFLAG(IS_ANDROID)
      } else {
        CheckFeatureEnabled(true);
      }
      break;
    case FeatureEnabledType::kDefault:
      // Currently Origin Trial for Prerender2 is available only on Android.
#if BUILDFLAG(IS_ANDROID)
      if (testing::get<1>(GetParam()) == BlinkFeatureEnabledType::kEnabled) {
        CheckFeatureEnabled(true);
      } else {
        CheckFeatureEnabled(false);
      }
#else   // BUILDFLAG(IS_ANDROID)
      CheckFeatureDisabled();
#endif  // BUILDFLAG(IS_ANDROID)
      break;
  }
}

}  // namespace
}  // namespace content
