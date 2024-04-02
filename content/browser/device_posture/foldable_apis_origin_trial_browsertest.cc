// Copyright 2024 The Chromium Authors
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
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kBaseDataDir[] = "content/test/data/device_posture";

class FoldableAPIsOriginTrialBrowserTest : public ContentBrowserTest {
 public:
  ~FoldableAPIsOriginTrialBrowserTest() override = default;

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

  bool HasDevicePostureApi() {
    return EvalJs(shell(), "'devicePosture' in navigator").ExtractBool();
  }

  bool HasDevicePostureCSSApi() {
    return EvalJs(shell(), "window.matchMedia('(device-posture)').matches")
        .ExtractBool();
  }

  bool HasViewportSegmentsApi() {
    return EvalJs(shell(), "'segments' in window.visualViewport").ExtractBool();
  }

  bool HasViewportSegmentsCSSApi() {
    return EvalJs(shell(),
                  "window.matchMedia('(vertical-viewport-segments)').matches")
        .ExtractBool();
  }

 protected:
  const GURL kValidTokenUrl{"https://example.test/valid_token.html"};
  const GURL kNoTokenUrl{"https://example.test/no_token.html"};

  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_F(FoldableAPIsOriginTrialBrowserTest,
                       ValidOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kValidTokenUrl));
  EXPECT_TRUE(HasDevicePostureApi());
  EXPECT_TRUE(HasDevicePostureCSSApi());
  EXPECT_TRUE(HasViewportSegmentsApi());
  EXPECT_TRUE(HasViewportSegmentsCSSApi());
}

IN_PROC_BROWSER_TEST_F(FoldableAPIsOriginTrialBrowserTest, NoOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kNoTokenUrl));
  EXPECT_FALSE(HasDevicePostureApi());
  EXPECT_FALSE(HasDevicePostureCSSApi());
  EXPECT_FALSE(HasViewportSegmentsApi());
  EXPECT_FALSE(HasViewportSegmentsCSSApi());
}

class FoldableAPIsOriginTrialKillSwitchBrowserTest
    : public FoldableAPIsOriginTrialBrowserTest {
 public:
  FoldableAPIsOriginTrialKillSwitchBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {},
        {blink::features::kDevicePosture, blink::features::kViewportSegments});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FoldableAPIsOriginTrialKillSwitchBrowserTest,
                       ValidOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kValidTokenUrl));
  EXPECT_FALSE(HasDevicePostureApi());
  EXPECT_FALSE(HasDevicePostureCSSApi());
  EXPECT_FALSE(HasViewportSegmentsApi());
  EXPECT_FALSE(HasViewportSegmentsCSSApi());
}

IN_PROC_BROWSER_TEST_F(FoldableAPIsOriginTrialKillSwitchBrowserTest,
                       NoOriginTrialToken) {
  ASSERT_TRUE(NavigateToURL(shell(), kNoTokenUrl));
  EXPECT_FALSE(HasDevicePostureApi());
  EXPECT_FALSE(HasDevicePostureCSSApi());
  EXPECT_FALSE(HasViewportSegmentsApi());
  EXPECT_FALSE(HasViewportSegmentsCSSApi());
}

}  // namespace

}  // namespace content
