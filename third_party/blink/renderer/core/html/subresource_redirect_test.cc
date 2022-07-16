// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/loader/subresource_redirect_util.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

Vector<char> ReadTestImage() {
  return test::ReadFromFile(test::CoreTestDataPath("notifications/500x500.png"))
      ->CopyAs<Vector<char>>();
}

class SubresourceRedirectSimTest
    : public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>>,
      public SimTest {
 protected:
  SubresourceRedirectSimTest() = default;

  void SetUp() override {
    SimTest::SetUp();

    scoped_lazy_image_loading_for_test_ =
        std::make_unique<ScopedLazyImageLoadingForTest>(
            is_lazyload_image_enabled());
    scoped_automatic_lazy_image_loading_for_test_ =
        std::make_unique<ScopedAutomaticLazyImageLoadingForTest>(
            is_lazyload_image_enabled());

    if (is_subresource_redirect_enabled()) {
      base::FieldTrialParams params;
      if (allow_javascript_crossorigin_images())
        params["allow_javascript_crossorigin_images"] = "true";
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kSubresourceRedirect, params}}, {});
    }
    GetNetworkStateNotifier().SetSaveDataEnabledOverride(
        is_save_data_enabled());
  }

  void TearDown() override {
    GetNetworkStateNotifier().ClearOverride();
    scoped_feature_list_.Reset();
    scoped_automatic_lazy_image_loading_for_test_.reset();
    scoped_lazy_image_loading_for_test_.reset();

    SimTest::TearDown();
  }

  bool is_subresource_redirect_enabled() const {
    return std::get<0>(GetParam());
  }

  bool is_lazyload_image_enabled() const { return std::get<1>(GetParam()); }

  bool is_save_data_enabled() const { return std::get<2>(GetParam()); }

  bool allow_javascript_crossorigin_images() const {
    return std::get<3>(GetParam());
  }

  void LoadMainResource(const String& html_body) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(html_body);
    GetDocument().UpdateStyleAndLayoutTree();
  }

  // Loads the main page resource and then loads the given image in the page.
  void LoadMainResourceAndImage(const String& html_body,
                                const String& img_url) {
    SimRequest image_resource(img_url, "image/png");
    LoadMainResource(html_body);

    if (!is_lazyload_image_enabled())
      image_resource.Complete(ReadTestImage());

    Compositor().BeginFrame();
    test::RunPendingTasks();

    if (is_lazyload_image_enabled()) {
      // Scroll down until the image is visible.
      GetDocument().View()->LayoutViewport()->SetScrollOffset(
          ScrollOffset(0, 10000), mojom::blink::ScrollType::kProgrammatic);
      if (Compositor().NeedsBeginFrame())
        Compositor().BeginFrame();
      test::RunPendingTasks();
      image_resource.Complete(ReadTestImage());
    }
  }

  // Verifies previews state for the fetched request URL.
  void VerifySubresourceRedirectPreviewsState(
      const String& url,
      bool is_subresource_redirect_allowed) {
    PreviewsState previews_state = GetDocument()
                                       .Fetcher()
                                       ->CachedResource(KURL(url))
                                       ->GetResourceRequest()
                                       .GetPreviewsState();
    EXPECT_EQ(is_subresource_redirect_allowed,
              (previews_state & PreviewsTypes::kSubresourceRedirectOn) != 0);
  }

  std::unique_ptr<ScopedLazyImageLoadingForTest>
      scoped_lazy_image_loading_for_test_;
  std::unique_ptr<ScopedAutomaticLazyImageLoadingForTest>
      scoped_automatic_lazy_image_loading_for_test_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

// TODO(crbug/1228072): All tests disabled due to high flakiness. The feature
// experiment has been disabled, all the tests will be re-enabled and deflaked
// before restarting.

// This test verifies subresource redirect previews state based on different
// states of SaveData, LazyLoad, SubresourceRedirect features.
TEST_P(SubresourceRedirectSimTest, DISABLED_CSSBackgroundImage) {
  LoadMainResourceAndImage(R"HTML(
        <style>
        #deferred_image {
          height:200px;
          background-image: url('img.png');
        }
        </style>
        <div style='height:10000px;'></div>
        <div id="deferred_image"></div>
      )HTML",
                           "https://example.com/img.png");

  // Subresource redirect previews bit should be set only if SaveData and
  // SubresourceRedirect feature are enabled.
  VerifySubresourceRedirectPreviewsState(
      "https://example.com/img.png",
      is_save_data_enabled() && is_subresource_redirect_enabled());
}

TEST_P(SubresourceRedirectSimTest, DISABLED_ImgElement) {
  LoadMainResourceAndImage(R"HTML(
        <body>
          <img src='https://example.com/img.png' loading='lazy'/>
        </body>
      )HTML",
                           "https://example.com/img.png");

  // Subresource redirect previews bit should be set only if SaveData and
  // SubresourceRedirect feature are enabled.
  VerifySubresourceRedirectPreviewsState(
      "https://example.com/img.png",
      is_save_data_enabled() && is_subresource_redirect_enabled());

  histogram_tester_.ExpectTotalCount("SubresourceRedirect.Blink.Ineligibility",
                                     0);
}

TEST_P(SubresourceRedirectSimTest, DISABLED_JavascriptCreatedSameOriginImage) {
  LoadMainResourceAndImage(R"HTML(
        <body>
        <div></div>
        <script>
          var img = document.createElement("img");
          img.loading = 'lazy';
          img.src = 'https://example.com/img.png';
          document.getElementsByTagName('div')[0].appendChild(img);
        </script>
        </body>
      )HTML",
                           "https://example.com/img.png");

  VerifySubresourceRedirectPreviewsState("https://example.com/img.png", false);

  if (is_save_data_enabled() && is_subresource_redirect_enabled()) {
    histogram_tester_.ExpectUniqueSample(
        "SubresourceRedirect.Blink.Ineligibility",
        BlinkSubresourceRedirectIneligibility::kJavascriptCreatedSameOrigin,
        is_lazyload_image_enabled() ? 2 : 1);
  } else {
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.Blink.Ineligibility", 0);
  }
}

TEST_P(SubresourceRedirectSimTest, DISABLED_JavascriptCreatedCrossOriginImage) {
  LoadMainResourceAndImage(R"HTML(
        <body>
        <div></div>
        <script>
          var img = document.createElement("img");
          img.loading = 'lazy';
          img.src = 'https://differentorigin.com/img.png';
          document.getElementsByTagName('div')[0].appendChild(img);
        </script>
        </body>
      )HTML",
                           "https://differentorigin.com/img.png");

  VerifySubresourceRedirectPreviewsState(
      "https://differentorigin.com/img.png",
      is_save_data_enabled() && is_subresource_redirect_enabled() &&
          allow_javascript_crossorigin_images());

  if (is_save_data_enabled() && is_subresource_redirect_enabled() &&
      !allow_javascript_crossorigin_images()) {
    histogram_tester_.ExpectUniqueSample(
        "SubresourceRedirect.Blink.Ineligibility",
        BlinkSubresourceRedirectIneligibility::kJavascriptCreatedCrossOrigin,
        is_lazyload_image_enabled() ? 2 : 1);
  } else {
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.Blink.Ineligibility", 0);
  }
}

TEST_P(SubresourceRedirectSimTest,
       DISABLED_ImgElementWithCrossOriginAttribute) {
  LoadMainResourceAndImage(R"HTML(
        <body>
          <img src='https://example.com/img.png' loading='lazy' crossorigin='anonymous'/>
        </body>
      )HTML",
                           "https://example.com/img.png");

  VerifySubresourceRedirectPreviewsState("https://example.com/img.png", false);

  if (is_save_data_enabled() && is_subresource_redirect_enabled()) {
    histogram_tester_.ExpectUniqueSample(
        "SubresourceRedirect.Blink.Ineligibility",
        BlinkSubresourceRedirectIneligibility::kCrossOriginAttributeSet,
        is_lazyload_image_enabled() ? 2 : 1);
  } else {
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.Blink.Ineligibility", 0);
  }
}

TEST_P(SubresourceRedirectSimTest,
       DISABLED_RestrictedByContentSecurityPolicyDefaultSrc) {
  LoadMainResourceAndImage(R"HTML(
        <head>
          <meta http-equiv="Content-Security-Policy" content="default-src 'self'">
        </head>
        <body>
          <img src='https://example.com/img.png' loading='lazy'/>
        </body>
      )HTML",
                           "https://example.com/img.png");

  VerifySubresourceRedirectPreviewsState("https://example.com/img.png", false);

  if (is_save_data_enabled() && is_subresource_redirect_enabled()) {
    histogram_tester_.ExpectUniqueSample(
        "SubresourceRedirect.Blink.Ineligibility",
        BlinkSubresourceRedirectIneligibility::
            kContentSecurityPolicyDefaultSrcRestricted,
        is_lazyload_image_enabled() ? 2 : 1);
  } else {
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.Blink.Ineligibility", 0);
  }
}

// TODO(crbug/1223916): Disabled due to high flakiness and build failures.
TEST_P(SubresourceRedirectSimTest,
       DISABLED_RestrictedByContentSecurityPolicyImgSrc) {
  LoadMainResourceAndImage(R"HTML(
        <head>
          <meta http-equiv="Content-Security-Policy" content="img-src 'self'">
        </head>
        <body>
          <img src='https://example.com/img.png' loading='lazy'/>
        </body>
      )HTML",
                           "https://example.com/img.png");

  VerifySubresourceRedirectPreviewsState("https://example.com/img.png", false);

  if (is_save_data_enabled() && is_subresource_redirect_enabled()) {
    histogram_tester_.ExpectUniqueSample(
        "SubresourceRedirect.Blink.Ineligibility",
        BlinkSubresourceRedirectIneligibility::
            kContentSecurityPolicyImgSrcRestricted,
        is_lazyload_image_enabled() ? 2 : 1);
  } else {
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.Blink.Ineligibility", 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SubresourceRedirectSimTest,
    ::testing::Combine(
        ::testing::Bool(), /* is_subresource_redirect_enabled */
        ::testing::Bool(), /* is_lazyload_image_enabled */
        ::testing::Bool(), /* is_save_data_enabled_*/
        ::testing::Bool() /* allow_javascript_crossorigin_images */));

class SubresourceRedirectCSPSimTest : public ::testing::WithParamInterface<
                                          bool /*allow_csp_restricted_images*/>,
                                      public SimTest {
 protected:
  SubresourceRedirectCSPSimTest() = default;

  void SetUp() override {
    SimTest::SetUp();

    base::FieldTrialParams params;
    params["allow_csp_restricted_images"] =
        allow_csp_restricted_images() ? "true" : "false";
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSubresourceRedirect, params}}, {});

    GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
    WebView().GetPage()->GetSettings().SetLitePageSubresourceRedirectOrigin(
        "https://litepages.googlezip.net");
  }

  void TearDown() override {
    GetNetworkStateNotifier().ClearOverride();
    scoped_feature_list_.Reset();
    SimTest::TearDown();
  }

  bool allow_csp_restricted_images() const { return GetParam(); }

  void LoadMainResource(const String& html_body) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(html_body);
    GetDocument().UpdateStyleAndLayoutTree();
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  void ScrollDownToLoadImage() {
    // Scroll down until the image is visible.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, 10000), mojom::blink::ScrollType::kProgrammatic);
    if (Compositor().NeedsBeginFrame())
      Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  // Verifies previews state for the fetched request URL.
  void VerifySubresourceRedirectPreviewsState(
      const String& url,
      bool is_subresource_redirect_allowed) {
    PreviewsState previews_state = GetDocument()
                                       .Fetcher()
                                       ->CachedResource(KURL(url))
                                       ->GetResourceRequest()
                                       .GetPreviewsState();
    EXPECT_EQ(is_subresource_redirect_allowed,
              (previews_state & PreviewsTypes::kSubresourceRedirectOn) != 0);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

// cross-origin image disallowed by CSP default-src directive should not load.
TEST_P(SubresourceRedirectCSPSimTest, DISABLED_ImageDisallowedByDefaultSrc) {
  LoadMainResource(R"HTML(
        <head>
          <meta http-equiv="Content-Security-Policy" content="default-src 'self'">
        </head>
        <body>
          <img src='https://crossorigin.com/img.png' loading='lazy'/>
        </body>
      )HTML");
  ScrollDownToLoadImage();
  EXPECT_TRUE(std::any_of(ConsoleMessages().begin(), ConsoleMessages().end(),
                          [](const auto& console_message) {
                            return console_message.Contains(
                                "Refused to load the image");
                          }));
}

// cross-origin image disallowed by CSP img-src directive should not load.
TEST_P(SubresourceRedirectCSPSimTest, DISABLED_ImageDisallowedByImgSrc) {
  LoadMainResource(R"HTML(
        <head>
          <meta http-equiv="Content-Security-Policy" content="img-src 'self'">
        </head>
        <body>
          <img src='https://crossorigin.com/img.png' loading='lazy'/>
        </body>
      )HTML");
  ScrollDownToLoadImage();
  EXPECT_TRUE(std::any_of(ConsoleMessages().begin(), ConsoleMessages().end(),
                          [](const auto& console_message) {
                            return console_message.Contains(
                                "Refused to load the image");
                          }));
}

TEST_P(SubresourceRedirectCSPSimTest, DISABLED_RestrictedByDefaultSrc) {
  std::unique_ptr<SimSubresourceRequest> redirected_image;
  WTF::String img_url = "https://example.com/img.png";
  SimRequestBase::Params params;
  if (allow_csp_restricted_images()) {
    // Simulate a redirect to LitePages.
    img_url = "https://litepages.googlezip.net/example.com/img.png";
    params.redirect_url = img_url;
    redirected_image = std::make_unique<SimSubresourceRequest>(
        "https://example.com/img.png", "image/png", params);
  }

  SimSubresourceRequest image_resource(img_url, "image/png");
  LoadMainResource(R"HTML(
        <head>
          <meta http-equiv="Content-Security-Policy" content="default-src 'self'">
        </head>
        <body>
          <img src='https://example.com/img.png' loading='lazy'/>
        </body>
      )HTML");
  ScrollDownToLoadImage();
  image_resource.Complete(ReadTestImage());

  VerifySubresourceRedirectPreviewsState("https://example.com/img.png",
                                         allow_csp_restricted_images());

  if (allow_csp_restricted_images()) {
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.Blink.Ineligibility", 0);
  } else {
    EXPECT_LE(1, histogram_tester_.GetBucketCount(
                     "SubresourceRedirect.Blink.Ineligibility",
                     BlinkSubresourceRedirectIneligibility::
                         kContentSecurityPolicyDefaultSrcRestricted));
  }
  // No CSP error should be reported.
  EXPECT_TRUE(std::none_of(ConsoleMessages().begin(), ConsoleMessages().end(),
                           [](const auto& console_message) {
                             return console_message.Contains(
                                 "Refused to load the image");
                           }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SubresourceRedirectCSPSimTest,
                         /* allow_csp_restricted_images */ ::testing::Bool());

}  // namespace

}  // namespace blink
