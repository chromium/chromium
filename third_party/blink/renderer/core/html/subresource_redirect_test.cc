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
    : public ::testing::WithParamInterface<std::tuple<bool, bool, bool>>,
      public SimTest {
 protected:
  SubresourceRedirectSimTest()
      : scoped_lazy_image_loading_for_test_(is_lazyload_image_enabled()),
        scoped_automatic_lazy_image_loading_for_test_(
            is_lazyload_image_enabled()) {
    if (is_subresource_redirect_enabled())
      scoped_feature_list_.InitAndEnableFeature(features::kSubresourceRedirect);
    GetNetworkStateNotifier().SetSaveDataEnabled(is_save_data_enabled());
  }

  bool is_subresource_redirect_enabled() { return std::get<0>(GetParam()); }

  bool is_lazyload_image_enabled() { return std::get<1>(GetParam()); }

  bool is_save_data_enabled() { return std::get<2>(GetParam()); }

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

  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

// This test verifies subresource redirect previews state based on different
// states of SaveData, LazyLoad, SubresourceRedirect features.
TEST_P(SubresourceRedirectSimTest, CSSBackgroundImage) {
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

TEST_P(SubresourceRedirectSimTest, ImgElement) {
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

TEST_P(SubresourceRedirectSimTest, JavascriptCreatedSameOriginImage) {
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

TEST_P(SubresourceRedirectSimTest, JavascriptCreatedCrossOriginImage) {
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

  VerifySubresourceRedirectPreviewsState("https://differentorigin.com/img.png",
                                         false);

  if (is_save_data_enabled() && is_subresource_redirect_enabled()) {
    histogram_tester_.ExpectUniqueSample(
        "SubresourceRedirect.Blink.Ineligibility",
        BlinkSubresourceRedirectIneligibility::kJavascriptCreatedCrossOrigin,
        is_lazyload_image_enabled() ? 2 : 1);
  } else {
    histogram_tester_.ExpectTotalCount(
        "SubresourceRedirect.Blink.Ineligibility", 0);
  }
}

TEST_P(SubresourceRedirectSimTest, ImgElementWithCrossOriginAttribute) {
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
       RestrictedByContentSecurityPolicyDefaultSrc) {
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

TEST_P(SubresourceRedirectSimTest, RestrictedByContentSecurityPolicyImgSrc) {
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
    ::testing::Combine(::testing::Bool(), /* is_subresource_redirect_enabled */
                       ::testing::Bool(), /* is_lazyload_image_enabled */
                       ::testing::Bool()) /* is_save_data_enabled_*/);

}  // namespace

}  // namespace blink
