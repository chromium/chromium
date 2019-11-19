// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include <tuple>

#include "base/optional.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const char* kLazyLoadEventsDeferredMessage =
    "Images loaded lazily and replaced with placeholders. Load events are "
    "deferred. See https://crbug.com/954323";

const char* kLazyLoadMissingDimensionMessage =
    "An <img> element was lazyloaded with loading=lazy, but had no dimensions "
    "specified. Specifying dimensions improves performance. See "
    "https://crbug.com/954323";

Vector<char> ReadTestImage() {
  return test::ReadFromFile(test::CoreTestDataPath("notifications/500x500.png"))
      ->CopyAs<Vector<char>>();
}

class LazyLoadImagesSimTest : public ::testing::WithParamInterface<bool>,
                              public SimTest {
 protected:
  LazyLoadImagesSimTest()
      : scoped_lazy_image_loading_for_test_(GetParam()),
        scoped_automatic_lazy_image_loading_for_test_(GetParam()),
        scoped_lazy_image_loading_metadata_fetch_for_test_(GetParam()) {}

  void SetLazyLoadEnabled(bool enabled) {
    WebView().GetPage()->GetSettings().SetLazyLoadEnabled(enabled);
  }

  void LoadMainResource(const String& html_body) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(html_body);
    GetDocument().UpdateStyleAndLayoutTree();
  }

  const ComputedStyle* GetElementComputedStyle(const Element& element,
                                               PseudoId pseudo_id) {
    if (pseudo_id == kPseudoIdNone)
      return element.GetComputedStyle();
    return element.GetPseudoElement(pseudo_id)->GetComputedStyle();
  }

  void ExpectCSSBackgroundImageDeferredState(const char* element_id,
                                             PseudoId pseudo_id,
                                             bool deferred) {
    const ComputedStyle* deferred_image_style = GetElementComputedStyle(
        *GetDocument().getElementById(element_id), pseudo_id);
    EXPECT_TRUE(deferred_image_style->HasBackgroundImage());
    bool is_background_image_found = false;
    for (const FillLayer* background_layer =
             &deferred_image_style->BackgroundLayers();
         background_layer; background_layer = background_layer->Next()) {
      if (StyleImage* deferred_image = background_layer->GetImage()) {
        EXPECT_TRUE(deferred_image->IsImageResource());
        EXPECT_EQ(deferred, deferred_image->IsLazyloadPossiblyDeferred());
        EXPECT_NE(deferred, deferred_image->IsLoaded());
        is_background_image_found = true;
      }
    }
    EXPECT_TRUE(is_background_image_found);
    EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
    EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

  void VerifyCSSBackgroundImageInPseudoStyleDeferred(
      const char* style,
      const char* deferred_div_classes,
      const Vector<PseudoId>& background_pseudo_ids) {
    bool is_lazyload_image_enabled = GetParam();
    SetLazyLoadEnabled(is_lazyload_image_enabled);
    SimRequest image_resource("https://example.com/img.png", "image/png");
    LoadMainResource(String::Format(R"HTML(
      <html>
      <head>
      <style>
      %s
      </style>
      </head>
      <body>
      <div style='height:10000px;'></div>
      <div id="deferred_image" class="%s"></div>
      </body>
      </html>
    )HTML",
                                    style, deferred_div_classes));

    if (!is_lazyload_image_enabled)
      image_resource.Complete(ReadTestImage());

    Compositor().BeginFrame();
    test::RunPendingTasks();
    for (const auto& pseudo_id : background_pseudo_ids) {
      ExpectCSSBackgroundImageDeferredState("deferred_image", pseudo_id,
                                            is_lazyload_image_enabled);
    }
    if (is_lazyload_image_enabled) {
      // Scroll down until the background image is visible.
      GetDocument().View()->LayoutViewport()->SetScrollOffset(
          ScrollOffset(0, 10000), kProgrammaticScroll);
      Compositor().BeginFrame();
      test::RunPendingTasks();
      image_resource.Complete(ReadTestImage());
      for (const auto& pseudo_id : background_pseudo_ids) {
        ExpectCSSBackgroundImageDeferredState("deferred_image", pseudo_id,
                                              false);
      }
    }
    EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
    EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

  void VerifyImageElementWithDimensionDeferred(const char* img_attribute) {
    bool is_lazyload_image_enabled = GetParam();
    SetLazyLoadEnabled(is_lazyload_image_enabled);
    SimRequest image_resource("https://example.com/img.png", "image/png");
    LoadMainResource(String::Format(R"HTML(
        <body onload='console.log("main body onload");'>
          <div style='height:10000px;'></div>
          <img src="img.png" %s
               onload= 'console.log("deferred_image onload");'>
        </body>)HTML",
                                    img_attribute));

    if (!is_lazyload_image_enabled)
      image_resource.Complete(ReadTestImage());

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    if (!is_lazyload_image_enabled)
      EXPECT_TRUE(ConsoleMessages().Contains("deferred_image onload"));

    if (is_lazyload_image_enabled) {
      // Scroll down until the image element is visible.
      GetDocument().View()->LayoutViewport()->SetScrollOffset(
          ScrollOffset(0, 10000), kProgrammaticScroll);
      Compositor().BeginFrame();
      test::RunPendingTasks();
      image_resource.Complete(ReadTestImage());
      test::RunPendingTasks();
      EXPECT_TRUE(ConsoleMessages().Contains("deferred_image onload"));
    }
    EXPECT_EQ(is_lazyload_image_enabled,
              ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
    EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

 private:
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test_;
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test_ = true;
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_ =
          false;
};

TEST_P(LazyLoadImagesSimTest, CSSBackgroundImage) {
  bool is_lazyload_image_enabled = GetParam();
  SetLazyLoadEnabled(is_lazyload_image_enabled);
  SimRequest image_resource("https://example.com/img.png", "image/png");
  LoadMainResource(String::Format(R"HTML(
        <style>
        #deferred_image {
          height:200px;
          background-image: url('img.png');
        }
        </style>
        <div style='height:10000px;'></div>
        <div id="deferred_image"></div>
      )HTML"));

  if (!is_lazyload_image_enabled)
    image_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ExpectCSSBackgroundImageDeferredState("deferred_image", kPseudoIdNone,
                                        is_lazyload_image_enabled);

  if (is_lazyload_image_enabled) {
    // Scroll down until the background image is visible.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, 10000), kProgrammaticScroll);
    Compositor().BeginFrame();
    test::RunPendingTasks();
    image_resource.Complete(ReadTestImage());
    ExpectCSSBackgroundImageDeferredState("deferred_image", kPseudoIdNone,
                                          false);
  }
}

TEST_P(LazyLoadImagesSimTest, CSSBackgroundImagePseudoStyleBefore) {
  VerifyCSSBackgroundImageInPseudoStyleDeferred(R"HTML(
    .pseudo-element::before {
      content: '';
      height: 50px;
      background-image: url('img.png');
    })HTML",
                                                "pseudo-element",
                                                {kPseudoIdBefore});
}

TEST_P(LazyLoadImagesSimTest, CSSBackgroundImagePseudoStyleAfter) {
  VerifyCSSBackgroundImageInPseudoStyleDeferred(R"HTML(
    .pseudo-element::after {
      content: '';
      height: 50px;
      background-image: url('img.png');
    })HTML",
                                                "pseudo-element",
                                                {kPseudoIdAfter});
}

TEST_P(LazyLoadImagesSimTest, CSSBackgroundImagePseudoStyleBeforeBlock) {
  VerifyCSSBackgroundImageInPseudoStyleDeferred(R"HTML(
    .pseudo-element::before {
      content: '';
      display: block;
      height: 50px;
      width: 50px;
      background-image: url('img.png');
    })HTML",
                                                "pseudo-element",
                                                {kPseudoIdBefore});
}

TEST_P(LazyLoadImagesSimTest,
       CSSBackgroundImagePseudoStyleBeforeAndAfterBlock) {
  VerifyCSSBackgroundImageInPseudoStyleDeferred(R"HTML(
    .pseudo-element::before {
      content: '';
      display: block;
      height: 50px;
      width: 50px;
      background-image: url('img.png');
    })HTML",
                                                "pseudo-element",
                                                {kPseudoIdBefore});
}

TEST_P(LazyLoadImagesSimTest, LargeImageHeight100Width100) {
  VerifyImageElementWithDimensionDeferred("height='100px' width='100px'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageHeight1Width100) {
  VerifyImageElementWithDimensionDeferred("height='1px' width='100px'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageHeight100Width1) {
  VerifyImageElementWithDimensionDeferred("height='100px' width='1px'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageStyleHeight100Width100) {
  VerifyImageElementWithDimensionDeferred(
      "style='height: 100px; width: 100px;'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageStyleHeight100Width1) {
  VerifyImageElementWithDimensionDeferred("style='height: 100px; width: 1px;'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageStyleHeight1Width100) {
  VerifyImageElementWithDimensionDeferred("style='height: 1px; width: 100px;'");
}

INSTANTIATE_TEST_SUITE_P(,
                         LazyLoadImagesSimTest,
                         ::testing::Bool() /*is_lazyload_image_enabled*/);

SimRequestBase::Params BuildRequestParamsForRangeResponse() {
  SimRequestBase::Params params;
  params.response_http_headers = {{"content-range", "bytes 0-2047/9311"}};
  params.response_http_status = 206;
  return params;
}

void ExpectResourceIsFullImage(Resource* resource) {
  EXPECT_TRUE(resource);
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_FALSE(
      resource->GetResourceRequest().HttpHeaderFields().Contains("range"));
  EXPECT_EQ(ResourceType::kImage, resource->GetType());
  EXPECT_FALSE(ToImageResource(resource)->ShouldShowLazyImagePlaceholder());
}

void ExpectResourceIsLazyImagePlaceholder(Resource* resource) {
  EXPECT_TRUE(resource);
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_EQ("bytes=0-2047",
            resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_EQ(ResourceType::kImage, resource->GetType());
  EXPECT_TRUE(ToImageResource(resource)->ShouldShowLazyImagePlaceholder());
}

class ScopedDataSaverSetting {
 public:
  explicit ScopedDataSaverSetting(bool is_data_saver_enabled)
      : was_data_saver_previously_enabled_(
            GetNetworkStateNotifier().SaveDataEnabled()) {
    GetNetworkStateNotifier().SetSaveDataEnabledOverride(is_data_saver_enabled);
  }

  ~ScopedDataSaverSetting() {
    GetNetworkStateNotifier().SetSaveDataEnabledOverride(
        was_data_saver_previously_enabled_);
  }

 private:
  const bool was_data_saver_previously_enabled_;
};

enum class LazyImageLoadingFeatureStatus {
  // LazyImageLoading is disabled.
  kDisabled = 0,
  // LazyImageLoading is enabled, but AutomaticLazyImageLoading is disabled.
  kEnabledExplicit,
  // Both LazyImageLoading and AutomaticLazyImageLoading are enabled.
  kEnabledExplicitMetadataFetchDisabled,  // Metadata fetch feature disabled.
  kEnabledAutomatic,
  kEnabledAutomaticMetadataFetchDisabled,  // Metadata fetch feature disabled.
  // LazyImageLoading, AutomaticLazyImageLoading, and
  // RestrictAutomaticLazyImageLoadingToDataSaver are enabled, while data saver
  // is off.
  kEnabledAutomaticRestrictedAndDataSaverOff,
  // LazyImageLoading, AutomaticLazyImageLoading, and
  // RestrictAutomaticLazyImageLoadingToDataSaver are enabled, while data saver
  // is on.
  kEnabledAutomaticRestrictedAndDataSaverOn,
};

class LazyLoadImagesParamsTest : public SimTest,
                                 public ::testing::WithParamInterface<
                                     std::tuple<LazyImageLoadingFeatureStatus,
                                                WebEffectiveConnectionType>> {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  LazyLoadImagesParamsTest()
      : lazy_image_loading_feature_status_(
            std::get<LazyImageLoadingFeatureStatus>(GetParam())),
        scoped_lazy_image_loading_for_test_(
            lazy_image_loading_feature_status_ !=
            LazyImageLoadingFeatureStatus::kDisabled),
        scoped_automatic_lazy_image_loading_for_test_(
            static_cast<int>(lazy_image_loading_feature_status_) >=
            static_cast<int>(LazyImageLoadingFeatureStatus::kEnabledAutomatic)),
        scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_(
            static_cast<int>(lazy_image_loading_feature_status_) >=
            static_cast<int>(LazyImageLoadingFeatureStatus::
                                 kEnabledAutomaticRestrictedAndDataSaverOff)),
        scoped_data_saver_setting_(
            lazy_image_loading_feature_status_ ==
            LazyImageLoadingFeatureStatus::
                kEnabledAutomaticRestrictedAndDataSaverOn),
        scoped_lazy_image_loading_metadata_fetch_for_test_(
            lazy_image_loading_feature_status_ !=
                LazyImageLoadingFeatureStatus::kDisabled &&
            lazy_image_loading_feature_status_ !=
                LazyImageLoadingFeatureStatus::
                    kEnabledExplicitMetadataFetchDisabled &&
            lazy_image_loading_feature_status_ !=
                LazyImageLoadingFeatureStatus::
                    kEnabledAutomaticMetadataFetchDisabled) {}

  void SetUp() override {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi,
        std::get<WebEffectiveConnectionType>(GetParam()),
        1000 /*http_rtt_msec*/, 100 /*max_bandwidth_mbps*/);

    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(
        WebSize(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();

    // These should match the values that would be returned by
    // GetLoadingDistanceThreshold().
    settings.SetLazyImageLoadingDistanceThresholdPxUnknown(200);
    settings.SetLazyImageLoadingDistanceThresholdPxOffline(300);
    settings.SetLazyImageLoadingDistanceThresholdPxSlow2G(400);
    settings.SetLazyImageLoadingDistanceThresholdPx2G(500);
    settings.SetLazyImageLoadingDistanceThresholdPx3G(600);
    settings.SetLazyImageLoadingDistanceThresholdPx4G(700);
    settings.SetLazyLoadEnabled(
        RuntimeEnabledFeatures::LazyImageLoadingEnabled());
    settings.SetLazyImageFirstKFullyLoadUnknown(0);
    settings.SetLazyImageFirstKFullyLoadSlow2G(0);
    settings.SetLazyImageFirstKFullyLoad2G(0);
    settings.SetLazyImageFirstKFullyLoad3G(0);
    settings.SetLazyImageFirstKFullyLoad4G(0);
  }

  int GetLoadingDistanceThreshold() const {
    static constexpr int kDistanceThresholdByEffectiveConnectionType[] = {
        200, 300, 400, 500, 600, 700};
    return kDistanceThresholdByEffectiveConnectionType[static_cast<int>(
        std::get<WebEffectiveConnectionType>(GetParam()))];
  }

  bool IsAutomaticLazyImageLoadingExpected() const {
    switch (lazy_image_loading_feature_status_) {
      case LazyImageLoadingFeatureStatus::kDisabled:
      case LazyImageLoadingFeatureStatus::kEnabledExplicit:
      case LazyImageLoadingFeatureStatus::
          kEnabledAutomaticRestrictedAndDataSaverOff:
      case LazyImageLoadingFeatureStatus::kEnabledExplicitMetadataFetchDisabled:
        return false;
      case LazyImageLoadingFeatureStatus::kEnabledAutomatic:
      case LazyImageLoadingFeatureStatus::
          kEnabledAutomaticRestrictedAndDataSaverOn:
      case LazyImageLoadingFeatureStatus::
          kEnabledAutomaticMetadataFetchDisabled:
        return true;
    }
    NOTREACHED();
    return false;
  }
  bool IsMetadataFetchExpected() {
    switch (lazy_image_loading_feature_status_) {
      case LazyImageLoadingFeatureStatus::kEnabledExplicitMetadataFetchDisabled:
      case LazyImageLoadingFeatureStatus::
          kEnabledAutomaticMetadataFetchDisabled:
        return false;
      default:
        return true;
    }
  }

 private:
  LazyImageLoadingFeatureStatus lazy_image_loading_feature_status_;
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test_;
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_;
  ScopedDataSaverSetting scoped_data_saver_setting_;
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test_;
};

TEST_P(LazyLoadImagesParamsTest, NearViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  base::Optional<SimSubresourceRequest> lazy_resource, auto_resource,
      unset_resource;
  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled() &&
      IsMetadataFetchExpected()) {
    lazy_resource.emplace("https://example.com/lazy.png", "image/png",
                          BuildRequestParamsForRangeResponse());
  } else {
    lazy_resource.emplace("https://example.com/lazy.png", "image/png");
  }
  if (IsAutomaticLazyImageLoadingExpected() && IsMetadataFetchExpected()) {
    auto_resource.emplace("https://example.com/auto.png", "image/png",
                          BuildRequestParamsForRangeResponse());
    unset_resource.emplace("https://example.com/unset.png", "image/png",
                           BuildRequestParamsForRangeResponse());
  } else {
    auto_resource.emplace("https://example.com/auto.png", "image/png");
    unset_resource.emplace("https://example.com/unset.png", "image/png");
  }
  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <head>
          <link rel='stylesheet' href='https://example.com/style.css' />
        </head>
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/eager.png' loading='eager'
             onload='console.log("eager onload");' />
        <img src='https://example.com/lazy.png' loading='lazy'
             onload='console.log("lazy onload");' />
        <img src='https://example.com/auto.png' loading='auto'
             onload='console.log("auto onload");' />
        <img src='https://example.com/unset.png'
             onload='console.log("unset onload");' />
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() - 100));

  css_resource.Complete("img { width: 50px; height: 50px; }");

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled() &&
      IsMetadataFetchExpected()) {
    lazy_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/lazy.png")));
    lazy_resource.emplace("https://example.com/lazy.png", "image/png");
  }

  if (IsAutomaticLazyImageLoadingExpected() && IsMetadataFetchExpected()) {
    auto_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/auto.png")));
    auto_resource.emplace("https://example.com/auto.png", "image/png");

    unset_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/unset.png")));
    unset_resource.emplace("https://example.com/unset.png", "image/png");
  }

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  eager_resource.Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/eager.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  lazy_resource->Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/lazy.png")));

  auto_resource->Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/auto.png")));

  unset_resource->Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/unset.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));

  switch (std::get<LazyImageLoadingFeatureStatus>(GetParam())) {
    case LazyImageLoadingFeatureStatus::kEnabledAutomatic:
    case LazyImageLoadingFeatureStatus::
        kEnabledAutomaticRestrictedAndDataSaverOn:
      EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
      EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
      EXPECT_TRUE(GetDocument().IsUseCounted(
          WebFeature::kLazyLoadImageMissingDimensionsForLazy));
      break;
    default:
      break;
  }
}

TEST_P(LazyLoadImagesParamsTest, FarFromViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  base::Optional<SimSubresourceRequest> lazy_resource, auto_resource,
      unset_resource;
  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled() &&
      IsMetadataFetchExpected()) {
    lazy_resource.emplace("https://example.com/lazy.png", "image/png",
                          BuildRequestParamsForRangeResponse());
  } else {
    lazy_resource.emplace("https://example.com/lazy.png", "image/png");
  }
  if (IsAutomaticLazyImageLoadingExpected() && IsMetadataFetchExpected()) {
    auto_resource.emplace("https://example.com/auto.png", "image/png",
                          BuildRequestParamsForRangeResponse());
    unset_resource.emplace("https://example.com/unset.png", "image/png",
                           BuildRequestParamsForRangeResponse());
  } else {
    auto_resource.emplace("https://example.com/auto.png", "image/png");
    unset_resource.emplace("https://example.com/unset.png", "image/png");
  }

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <head>
          <link rel='stylesheet' href='https://example.com/style.css' />
        </head>
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/eager.png' loading='eager'
             onload='console.log("eager onload");' />
        <img src='https://example.com/lazy.png' loading='lazy'
             onload='console.log("lazy onload");' />
        <img src='https://example.com/auto.png' loading='auto'
             onload='console.log("auto onload");' />
        <img src='https://example.com/unset.png'
             onload='console.log("unset onload");' />
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  css_resource.Complete("img { width: 50px; height: 50px; }");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  eager_resource.Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/eager.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  if (std::get<LazyImageLoadingFeatureStatus>(GetParam()) !=
      LazyImageLoadingFeatureStatus::kEnabledAutomaticMetadataFetchDisabled) {
    EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  }
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
    if (IsMetadataFetchExpected()) {
      lazy_resource->Complete(partial_image);
      ExpectResourceIsLazyImagePlaceholder(
          GetDocument().Fetcher()->CachedResource(
              KURL("https://example.com/lazy.png")));
      lazy_resource.emplace("https://example.com/lazy.png", "image/png");
    }
  } else {
    lazy_resource->Complete(full_image);
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/lazy.png")));
  }

  if (IsAutomaticLazyImageLoadingExpected()) {
    if (IsMetadataFetchExpected()) {
      auto_resource->Complete(partial_image);
      ExpectResourceIsLazyImagePlaceholder(
          GetDocument().Fetcher()->CachedResource(
              KURL("https://example.com/auto.png")));

      unset_resource->Complete(partial_image);
      ExpectResourceIsLazyImagePlaceholder(
          GetDocument().Fetcher()->CachedResource(
              KURL("https://example.com/unset.png")));
      auto_resource.emplace("https://example.com/auto.png", "image/png");
      unset_resource.emplace("https://example.com/unset.png", "image/png");
    }
  } else {
    auto_resource->Complete(full_image);
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/auto.png")));

    unset_resource->Complete(full_image);
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/unset.png")));
  }

  if (std::get<LazyImageLoadingFeatureStatus>(GetParam()) !=
      LazyImageLoadingFeatureStatus::kEnabledAutomaticMetadataFetchDisabled) {
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));

  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
    // Scroll down so that the images are near the viewport.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, 150), kProgrammaticScroll);

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));

    lazy_resource->Complete(full_image);
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/lazy.png")));

    if (IsAutomaticLazyImageLoadingExpected()) {
      EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
      EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

      auto_resource->Complete(full_image);
      ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
          KURL("https://example.com/auto.png")));

      unset_resource->Complete(full_image);
      ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
          KURL("https://example.com/unset.png")));
    }

    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));

  switch (std::get<LazyImageLoadingFeatureStatus>(GetParam())) {
    case LazyImageLoadingFeatureStatus::kEnabledAutomatic:
    case LazyImageLoadingFeatureStatus::
        kEnabledAutomaticRestrictedAndDataSaverOn:
      EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
      EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
      EXPECT_TRUE(GetDocument().IsUseCounted(
          WebFeature::kLazyLoadImageMissingDimensionsForLazy));
      break;
    default:
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    LazyImageLoading,
    LazyLoadImagesParamsTest,
    ::testing::Combine(
        ::testing::Values(LazyImageLoadingFeatureStatus::kDisabled,
                          LazyImageLoadingFeatureStatus::kEnabledExplicit,
                          LazyImageLoadingFeatureStatus::kEnabledAutomatic,
                          LazyImageLoadingFeatureStatus::
                              kEnabledAutomaticRestrictedAndDataSaverOff,
                          LazyImageLoadingFeatureStatus::
                              kEnabledAutomaticRestrictedAndDataSaverOn,
                          LazyImageLoadingFeatureStatus::
                              kEnabledExplicitMetadataFetchDisabled,
                          LazyImageLoadingFeatureStatus::
                              kEnabledAutomaticMetadataFetchDisabled),
        ::testing::Values(WebEffectiveConnectionType::kTypeUnknown,
                          WebEffectiveConnectionType::kTypeOffline,
                          WebEffectiveConnectionType::kTypeSlow2G,
                          WebEffectiveConnectionType::kType2G,
                          WebEffectiveConnectionType::kType3G,
                          WebEffectiveConnectionType::kType4G)));

class LazyLoadAutomaticImagesTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;
  static constexpr int kLoadingDistanceThreshold = 300;

  LazyLoadAutomaticImagesTest()
      : scoped_lazy_image_loading_for_test_(true),
        scoped_automatic_lazy_image_loading_for_test_(true),
        scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_(
            false) {}

  void SetUp() override {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi,
        WebEffectiveConnectionType::kType4G, 1000 /*http_rtt_msec*/,
        100 /*max_bandwidth_mbps*/);
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(
        WebSize(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();
    settings.SetLazyImageLoadingDistanceThresholdPx4G(
        kLoadingDistanceThreshold);
    settings.SetLazyFrameLoadingDistanceThresholdPx4G(
        kLoadingDistanceThreshold);
    settings.SetLazyLoadEnabled(
        RuntimeEnabledFeatures::LazyImageLoadingEnabled());
    settings.SetLazyImageFirstKFullyLoad4G(0);
  }

  void SetLazyImageFirstKFullyLoad(int count) {
    Settings& settings = WebView().GetPage()->GetSettings();
    settings.SetLazyImageFirstKFullyLoad4G(count);
  }

  void LoadMainResourceWithImageFarFromViewport(const char* image_attributes) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(String::Format(
        R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' %s
             onload='console.log("image onload");' />
        </body>)HTML",
        kViewportHeight + kLoadingDistanceThreshold + 100, image_attributes));

    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  void TestLoadImageExpectingLazyLoad(const char* image_attributes) {
    SimSubresourceRequest image_range_resource(
        "https://example.com/image.png", "image/png",
        BuildRequestParamsForRangeResponse());

    LoadMainResourceWithImageFarFromViewport(image_attributes);

    EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

    Vector<char> partial_image = ReadTestImage();
    EXPECT_LT(2048U, partial_image.size());
    partial_image.resize(2048U);

    image_range_resource.Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/image.png")));

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));
  }

  void TestLoadImageExpectingLazyLoadWithoutPlaceholder(
      const char* image_attributes) {
    SimSubresourceRequest full_resource("https://example.com/image.png",
                                        "image/png");

    LoadMainResourceWithImageFarFromViewport(image_attributes);

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

    // Scrolling down should trigger the fetch of the image.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, kLoadingDistanceThreshold + kViewportHeight),
        kProgrammaticScroll);
    Compositor().BeginFrame();
    test::RunPendingTasks();
    full_resource.Complete(ReadTestImage());
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/image.png")));
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
    EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
    EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

  void TestLoadImageExpectingFullImageLoad(const char* image_attributes) {
    SimSubresourceRequest full_resource("https://example.com/image.png",
                                        "image/png");

    LoadMainResourceWithImageFarFromViewport(image_attributes);

    EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

    full_resource.Complete(ReadTestImage());
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/image.png")));

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
    EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
    EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

 private:
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test_;
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_;
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test_ = true;
  ScopedLazyImageVisibleLoadTimeMetricsForTest
      scoped_lazy_image_visible_load_time_metrics_for_test_ = true;
};

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromLazyToEager) {
  TestLoadImageExpectingLazyLoad("id='my_image' loading='lazy'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  full_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
  EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageMissingDimensionsForLazy));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromAutoToEager) {
  TestLoadImageExpectingLazyLoad("id='my_image' loading='auto'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  full_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
  EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageMissingDimensionsForLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromUnsetToEager) {
  TestLoadImageExpectingLazyLoad("id='my_image'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  full_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
  EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadMissingDimensionMessage));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageMissingDimensionsForLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWithLazyAttr) {
  TestLoadImageExpectingLazyLoad("loading='lazy' width='1px' height='1px'");
  EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWithLazyAttr) {
  TestLoadImageExpectingLazyLoad(
      "loading='lazy' style='width:1px;height:1px;'");
  EXPECT_FALSE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth1Height1) {
  TestLoadImageExpectingFullImageLoad("width='1px' height='1px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth10Height10) {
  TestLoadImageExpectingFullImageLoad("width='10px' height='10px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth1Height11) {
  TestLoadImageExpectingLazyLoadWithoutPlaceholder("width='1px' height='11px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth1Height1) {
  TestLoadImageExpectingFullImageLoad("style='width:1px;height:1px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth10Height10) {
  TestLoadImageExpectingFullImageLoad("style='width:10px;height:10px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth11Height1) {
  TestLoadImageExpectingLazyLoadWithoutPlaceholder(
      "style='width:11px;height:1px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, FirstKImagesLoaded) {
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test = false;
  SetLazyImageFirstKFullyLoad(1);

  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest img1("https://example.com/image.png?id=1", "image/png");

  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
    <body onload='console.log("main body onload");'>
    <div style='height: %dpx;'></div>
    <img src='https://example.com/image.png?id=1'
         onload='console.log("image id=1 onload");' />
    <img src='https://example.com/image.png?id=2'
         onload='console.log("image id=2 onload");' />
    </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));
  Compositor().BeginFrame();

  // One image should be loaded fully, even though it is below viewport.
  img1.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png?id=1")));
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image id=1 onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("image id=2 onload"));

  // Scrolling down should trigger the fetch of the second image.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kLoadingDistanceThreshold + kViewportHeight),
      kProgrammaticScroll);

  SimSubresourceRequest img2("https://example.com/image.png?id=2", "image/png");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  img2.Complete(ReadTestImage());

  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png?id=2")));
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image id=1 onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image id=2 onload"));
  EXPECT_TRUE(ConsoleMessages().Contains(kLazyLoadEventsDeferredMessage));
}

TEST_F(LazyLoadAutomaticImagesTest, JavascriptCreatedImageFarFromViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <script>
          var my_image = new Image(50, 50);
          my_image.onload = function() { console.log('my_image onload'); };
          my_image.src = 'https://example.com/image.png';
          document.body.appendChild(my_image);
        </script>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("my_image onload"));

  image_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("my_image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, JavascriptCreatedImageAddedAfterLoad) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <script>
          var my_image = new Image(50, 50);
          my_image.onload = function() {
            console.log('my_image onload');
            document.body.appendChild(this);
          };
          my_image.src = 'https://example.com/image.png';
        </script>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("my_image onload"));

  image_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("my_image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, ImageInsideLazyLoadedFrame) {
  ScopedLazyFrameLoadingForTest scoped_lazy_frame_loading_for_test(true);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://example.com/child_frame.html' loading='lazy'
                id='child_frame' width='300px' height='300px'
                onload='console.log("child frame onload");'></iframe>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame onload"));

  SimRequest child_frame_resource("https://example.com/child_frame.html",
                                  "text/html");

  // Scroll down so that the iframe is near the viewport, but the images within
  // it aren't near the viewport yet.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 150),
                                                          kProgrammaticScroll);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");
  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  SimSubresourceRequest lazy_range_resource(
      "https://example.com/lazy.png", "image/png",
      BuildRequestParamsForRangeResponse());
  SimSubresourceRequest auto_resource("https://example.com/auto.png",
                                      "image/png");
  SimSubresourceRequest unset_resource("https://example.com/unset.png",
                                       "image/png");

  child_frame_resource.Complete(R"HTML(
      <head>
        <link rel='stylesheet' href='https://example.com/style.css' />
      </head>
      <body onload='window.parent.console.log("child body onload");'>
      <div style='height: 100px;'></div>
      <img src='https://example.com/eager.png' loading='eager'
           onload='window.parent.console.log("eager onload");' />
      <img src='https://example.com/lazy.png' loading='lazy'
           onload='window.parent.console.log("lazy onload");' />
      <img src='https://example.com/auto.png' loading='auto'
           onload='window.parent.console.log("auto onload");' />
      <img src='https://example.com/unset.png'
           onload='window.parent.console.log("unset onload");' />
      </body>)HTML");

  css_resource.Complete("img { width: 50px; height: 50px; }");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  Document* child_frame_document =
      To<HTMLIFrameElement>(GetDocument().getElementById("child_frame"))
          ->contentDocument();

  eager_resource.Complete(full_image);
  ExpectResourceIsFullImage(child_frame_document->Fetcher()->CachedResource(
      KURL("https://example.com/eager.png")));

  lazy_range_resource.Complete(partial_image);
  ExpectResourceIsLazyImagePlaceholder(
      child_frame_document->Fetcher()->CachedResource(
          KURL("https://example.com/lazy.png")));

  auto_resource.Complete(full_image);
  ExpectResourceIsFullImage(child_frame_document->Fetcher()->CachedResource(
      KURL("https://example.com/auto.png")));

  unset_resource.Complete(full_image);
  ExpectResourceIsFullImage(child_frame_document->Fetcher()->CachedResource(
      KURL("https://example.com/unset.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));

  SimSubresourceRequest lazy_resource("https://example.com/lazy.png",
                                      "image/png");

  // Scroll down so that all the images in the iframe are near the viewport.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 250),
                                                          kProgrammaticScroll);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  lazy_resource.Complete(full_image);
  ExpectResourceIsFullImage(child_frame_document->Fetcher()->CachedResource(
      KURL("https://example.com/lazy.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, LazyLoadDisabledOnReload) {
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test = false;
  String main_resource_html = String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png'
             width='50px' height='50px'
             onload='console.log("auto image onload");' />
        <img src='https://example.com/image_lazy.png' loading='lazy'
             width='50px' height='50px'
             onload='console.log("auto image onload");' />
        </body>)HTML",
      LazyLoadAutomaticImagesTest::kViewportHeight +
          LazyLoadAutomaticImagesTest::kLoadingDistanceThreshold + 100);

  {
    HistogramTester histogram_tester;
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(main_resource_html);
    Compositor().BeginFrame();
    test::RunPendingTasks();

    // Scrolling down should trigger the fetch both the images, and record
    // visible load time metrics.
    SimSubresourceRequest auto_image("https://example.com/image.png",
                                     "image/png");
    SimSubresourceRequest lazy_image("https://example.com/image_lazy.png",
                                     "image/png");
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, LazyLoadAutomaticImagesTest::kLoadingDistanceThreshold +
                            LazyLoadAutomaticImagesTest::kViewportHeight),
        kProgrammaticScroll);
    Compositor().BeginFrame();
    test::RunPendingTasks();
    auto_image.Complete(ReadTestImage());
    lazy_image.Complete(ReadTestImage());
    test::RunPendingTasks();
    histogram_tester.ExpectTotalCount(
        "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 2);
  }

  // Reloading the page should load the image with loading=auto, but still defer
  // the image with loading=lazy.
  {
    HistogramTester histogram_tester;
    SimRequest main_resource("https://example.com/", "text/html");
    SimSubresourceRequest auto_image("https://example.com/image.png",
                                     "image/png");
    MainFrame().StartReload(WebFrameLoadType::kReload);
    main_resource.Complete(main_resource_html);
    auto_image.Complete(ReadTestImage());

    // Scrolling down should trigger the fetch deferred image, and record
    // visible load time metrics for both images.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, LazyLoadAutomaticImagesTest::kLoadingDistanceThreshold +
                            LazyLoadAutomaticImagesTest::kViewportHeight),
        kProgrammaticScroll);
    SimSubresourceRequest lazy_image("https://example.com/image_lazy.png",
                                     "image/png");
    Compositor().BeginFrame();
    test::RunPendingTasks();
    lazy_image.Complete(ReadTestImage());
    test::RunPendingTasks();
    histogram_tester.ExpectTotalCount(
        "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 2);
  }
}

TEST_F(LazyLoadAutomaticImagesTest, AboveTheFoldImageLoadedBeforeVisible) {
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test = false;
  HistogramTester histogram_tester;

  // Since the image is above the fold, ensure that the image starts loading
  // before it becomes visible. When automatic lazyloading is enabled, the main
  // way that an above-the-fold image that's relevant to automatic lazyload
  // (i.e. would have visible load time metrics recorded for it) can be finshed
  // loading by the time it becomes visible is for it to be fully loaded as one
  // of the first K images seen by the parser.
  SetLazyImageFirstKFullyLoad(1);

  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<body><img src='https://example.com/image.png' /></body>");
  image_resource.Complete(ReadTestImage());

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // visible yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0);
}

TEST_F(LazyLoadAutomaticImagesTest, AboveTheFoldImageVisibleBeforeLoaded) {
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test = false;
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<body><img src='https://example.com/image.png' /></body>");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // VisibleBeforeLoaded should have been recorded immediately when the image
  // became visible.
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // finished loading yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0);

  image_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0);
}

TEST_F(LazyLoadAutomaticImagesTest, BelowTheFoldImageLoadedBeforeVisible) {
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test = false;
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' />
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  // Scroll down such that the image is within kLoadingDistanceThreshold of the
  // viewport, but isn't visible yet.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 200),
                                                          kProgrammaticScroll);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  image_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // visible yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0);

  // Scroll down such that the image is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight + kLoadingDistanceThreshold),
      kProgrammaticScroll);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0, 1);
}

TEST_F(LazyLoadAutomaticImagesTest, BelowTheFoldImageVisibleBeforeLoaded) {
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test = false;
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' />
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  // Scroll down such that the image is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight + kLoadingDistanceThreshold),
      kProgrammaticScroll);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // VisibleBeforeLoaded should have been recorded immediately when the image
  // became visible.
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // finished loading yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0);

  image_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 1);
}

}  // namespace

}  // namespace blink
