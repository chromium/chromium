// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_image_cache.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {
constexpr char kTestResourceFilename[] = "background_image.png";
constexpr char kTestResourceMimeType[] = "image/png";
}  // namespace

class StyleImageCacheTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  }
  const HeapHashMap<String, WeakMember<ImageResourceContent>>&
  FetchedImageMap() {
    return GetDocument().GetStyleEngine().style_image_cache_.fetched_image_map_;
  }
  ImageResourceContent* CacheImageContent(FetchParameters& params) {
    return GetDocument().GetStyleEngine().style_image_cache_.CacheImageContent(
        GetDocument().Fetcher(), params);
  }
  bool CanReuseImageContent(const ImageResourceContent& content,
                            const FetchParameters& params) {
    return GetDocument()
        .GetStyleEngine()
        .style_image_cache_.CanReuseImageContent(content, params);
  }
};

TEST_F(StyleImageCacheTest, DuplicateBackgroundImageURLs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(url.png) }
      .rule2 { background-image: url(url.png) }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule1_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image);

  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule2_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_EQ(*rule1_image, *rule2_image);
}

TEST_F(StyleImageCacheTest, DifferingFragmentsBackgroundImageURLs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(url.svg#a) }
      .rule2 { background-image: url(url.svg#b) }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule1_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image);

  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule2_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_NE(*rule1_image, *rule2_image);
  EXPECT_EQ(rule1_image->CachedImage(), rule2_image->CachedImage());
}

TEST_F(StyleImageCacheTest, CustomPropertyURL) {
  SetBodyInnerHTML(R"HTML(
    <style>
      :root { --bg: url(url.png) }
      #target { background-image: var(--bg) }
      .green { background-color: green }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetElementById("target");

  StyleImage* initial_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(initial_image);

  target->setAttribute(html_names::kClassAttr, AtomicString("green"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* image_after_recalc =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_EQ(*initial_image, *image_after_recalc);
}

TEST_F(StyleImageCacheTest, ComputedValueRelativePath) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target1 { background-image: url(http://test.com/url.png) }
      #target2 { background-image: url(url.png) }
    </style>
    <div id="target1"></div>
    <div id="target2"></div>
  )HTML");

  Element* target1 = GetElementById("target1");
  Element* target2 = GetElementById("target2");

  // Resolves to the same absolute url. Can share the underlying
  // ImageResourceContent since the computed value is the absolute url.
  EXPECT_EQ(*target1->ComputedStyleRef().BackgroundLayers().GetImage(),
            *target2->ComputedStyleRef().BackgroundLayers().GetImage());

  const CSSProperty& property =
      CSSProperty::Get(CSSPropertyID::kBackgroundImage);
  EXPECT_EQ(property
                .CSSValueFromComputedStyle(target1->ComputedStyleRef(), nullptr,
                                           false, CSSValuePhase::kComputedValue)
                ->CssText(),
            "url(\"http://test.com/url.png\")");
  EXPECT_EQ(property
                .CSSValueFromComputedStyle(target2->ComputedStyleRef(), nullptr,
                                           false, CSSValuePhase::kComputedValue)
                ->CssText(),
            "url(\"http://test.com/url.png\")");
}

TEST_F(StyleImageCacheTest, WeakReferenceGC) {
  SetBodyInnerHTML(R"HTML(
    <style id="sheet">
      #target1 { background-image: url(url.png) }
      #target2 { background-image: url(url2.png) }
    </style>
    <div id="target1"></div>
    <div id="target2"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(FetchedImageMap().Contains("http://test.com/url.png"));
  EXPECT_TRUE(FetchedImageMap().Contains("http://test.com/url2.png"));
  EXPECT_EQ(FetchedImageMap().size(), 2u);

  Element* sheet = GetElementById("sheet");
  ASSERT_TRUE(sheet);
  sheet->remove();
  UpdateAllLifecyclePhasesForTest();
  ThreadState::Current()->CollectAllGarbageForTesting();

  // After the sheet has been removed, the lifecycle update and garbage
  // collection have been run, the weak references in the cache should have been
  // collected.
  EXPECT_FALSE(FetchedImageMap().Contains("http://test.com/url.png"));
  EXPECT_FALSE(FetchedImageMap().Contains("http://test.com/url2.png"));
  EXPECT_EQ(FetchedImageMap().size(), 0u);
}

class StyleImageCacheFrameClientTest : public EmptyLocalFrameClient {
 public:
  std::unique_ptr<URLLoader> CreateURLLoaderForTesting() override {
    return URLLoaderMockFactory::GetSingletonInstance()->CreateURLLoader();
  }
};

class StyleImageCacheWithLoadingTest : public StyleImageCacheTest {
 public:
  StyleImageCacheWithLoadingTest() = default;
  ~StyleImageCacheWithLoadingTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void SetUp() override {
    auto setting_overrider = [](Settings& settings) {
      settings.SetLoadsImagesAutomatically(true);
    };
    PageTestBase::SetupPageWithClients(
        nullptr, MakeGarbageCollected<StyleImageCacheFrameClientTest>(),
        setting_overrider);
  }
};

TEST_F(StyleImageCacheWithLoadingTest, DuplicateBackgroundImageURLs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(http://test.com/background_image.png) }
      .rule2 { background-image: url(http://test.com/background_image.png) }
    </style>
    <div id="target"></div>
  )HTML");
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("http://test.com/background_image.png"),
      test::CoreTestDataPath(kTestResourceFilename), kTestResourceMimeType);
  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();
  StyleImage* rule1_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image);
  EXPECT_FALSE(rule1_image->ErrorOccurred());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();
  StyleImage* rule2_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_EQ(*rule1_image, *rule2_image);
  EXPECT_FALSE(rule2_image->ErrorOccurred());
}

TEST_F(StyleImageCacheWithLoadingTest, LoadFailedBackgroundImageURL) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(http://test.com/background_image.png) }
      .rule2 { background-image: url(http://test.com/background_image.png) }
    </style>
    <div id="target"></div>
  )HTML");
  const auto image_url =
      url_test_helpers::ToKURL("http://test.com/background_image.png");
  url_test_helpers::RegisterMockedErrorURLLoad(image_url);
  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());
  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();
  StyleImage* rule1_image1 =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image1->ErrorOccurred());
  url_test_helpers::RegisterMockedURLUnregister(image_url);
  url_test_helpers::RegisterMockedURLLoad(
      image_url, test::CoreTestDataPath(kTestResourceFilename),
      kTestResourceMimeType);
  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();
  StyleImage* rule1_image2 =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_NE(*rule1_image1, *rule1_image2);
  EXPECT_FALSE(rule1_image2->ErrorOccurred());
  EXPECT_TRUE(FetchedImageMap().Contains(image_url.GetString()));
  EXPECT_EQ(FetchedImageMap().size(), 1u);
}

TEST_F(StyleImageCacheWithLoadingTest, DifferingCORSModesImageURLs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(http://test.com/background_image.png) }
      .rule2 { mask-image: url(http://test.com/background_image.png) }
    </style>
    <div id="target"></div>
  )HTML");

  const auto image_url =
      url_test_helpers::ToKURL("http://test.com/background_image.png");
  WebURLResponse response(image_url);
  response.SetMimeType("image/png");
  response.SetHttpHeaderField(http_names::kContentType, kTestResourceMimeType);
  response.SetHttpStatusCode(200);
  // Mark the response as opaque (cross-origin no-cors) so IsCorsSameOrigin()
  // is false:
  response.SetType(network::mojom::FetchResponseType::kOpaque);
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      image_url, test::CoreTestDataPath(kTestResourceFilename), response);

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();

  ImageResourceContent* rule1_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage()->CachedImage();
  EXPECT_TRUE(rule1_image);

  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();

  ImageResourceContent* rule2_image =
      target->ComputedStyleRef().MaskLayers().GetImage()->CachedImage();
  EXPECT_NE(rule1_image, rule2_image);
}

TEST_F(StyleImageCacheWithLoadingTest, IntegrityMetadataBypassesCache) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .plain { background-image: url(http://test.com/background_image.png); }
      .bad { background-image: url("http://test.com/background_image.png" integrity("sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=")); }
      .good1 { background-image: url("http://test.com/background_image.png" integrity("sha256-5EAqnjfseQ08xWbuV6aD4HwSG6pQuEY9J0W1p6hsWyQ=")); }
      .good2 { background-image: url("http://test.com/background_image.png" integrity("sha256-5EAqnjfseQ08xWbuV6aD4HwSG6pQuEY9J0W1p6hsWyQ=")); }
      .other { background-image: url("http://test.com/background_image.png" integrity("sha256-BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=")); }
    </style>
    <div id="target"></div>
  )HTML");
  const auto image_url =
      url_test_helpers::ToKURL("http://test.com/background_image.png");
  url_test_helpers::RegisterMockedURLLoad(
      image_url, test::CoreTestDataPath(kTestResourceFilename),
      kTestResourceMimeType);

  Element* target = GetElementById("target");
  auto load_with_class = [&](const char* class_name) -> ImageResourceContent* {
    target->setAttribute(html_names::kClassAttr, AtomicString(class_name));
    UpdateAllLifecyclePhasesForTest();
    url_test_helpers::ServeAsynchronousRequests();
    return target->ComputedStyleRef()
        .BackgroundLayers()
        .GetImage()
        ->CachedImage();
  };

  ImageResourceContent* plain = load_with_class("plain");
  ASSERT_TRUE(plain);
  EXPECT_FALSE(plain->ErrorOccurred());

  // Non-matching integrity after a plain load must fail and must not overwrite
  // the valid entry in fetched_image_map_.
  ImageResourceContent* bad = load_with_class("bad");
  EXPECT_NE(plain, bad);
  EXPECT_TRUE(bad->ErrorOccurred());
  EXPECT_EQ(plain, FetchedImageMap().at(image_url.GetString()));

  // Matching integrity loads and is reused for an identical digest, while a
  // different digest is not reused and fails.
  ImageResourceContent* good1 = load_with_class("good1");
  EXPECT_NE(plain, good1);
  EXPECT_FALSE(good1->ErrorOccurred());

  ImageResourceContent* good2 = load_with_class("good2");
  EXPECT_EQ(good1, good2);
  EXPECT_FALSE(good2->ErrorOccurred());

  ImageResourceContent* other = load_with_class("other");
  EXPECT_NE(good1, other);
  EXPECT_TRUE(other->ErrorOccurred());
}

TEST_F(StyleImageCacheWithLoadingTest,
       DifferingModeAndCredentialsNotReusedAndDoesNotClobber) {
  const auto image_url =
      url_test_helpers::ToKURL("http://test.com/cors_mode_image.png");
  WebURLResponse response(image_url);
  response.SetMimeType(kTestResourceMimeType);
  response.SetHttpHeaderField(http_names::kContentType, kTestResourceMimeType);
  response.SetHttpHeaderField(http_names::kCacheControl, "no-store");
  response.SetHttpStatusCode(200);
  response.SetType(network::mojom::FetchResponseType::kBasic);
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      image_url, test::CoreTestDataPath(kTestResourceFilename), response);

  const SecurityOrigin* origin =
      GetDocument().GetExecutionContext()->GetSecurityOrigin();

  FetchParameters no_cors_params(ResourceRequest(image_url),
                                 ResourceLoaderOptions(nullptr));
  ImageResourceContent* no_cors_image = CacheImageContent(no_cors_params);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(no_cors_image);
  EXPECT_FALSE(no_cors_image->ErrorOccurred());
  EXPECT_EQ(no_cors_image, FetchedImageMap().at(image_url.GetString()));

  // A CORS anonymous fetch must not reuse the no-cors entry and must not
  // clobber the cached no-cors entry.
  FetchParameters anon_params(ResourceRequest(image_url),
                              ResourceLoaderOptions(nullptr));
  anon_params.SetCrossOriginAccessControl(origin,
                                          kCrossOriginAttributeAnonymous);
  ImageResourceContent* anon_image = CacheImageContent(anon_params);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(anon_image);
  EXPECT_NE(no_cors_image, anon_image);
  EXPECT_EQ(no_cors_image, FetchedImageMap().at(image_url.GetString()));

  // A fresh URL cached with anonymous must not be reused for use-credentials
  // and must not be clobbered.
  const auto creds_url =
      url_test_helpers::ToKURL("http://test.com/cors_creds_image.png");
  WebURLResponse creds_response(creds_url);
  creds_response.SetMimeType(kTestResourceMimeType);
  creds_response.SetHttpHeaderField(http_names::kContentType,
                                    kTestResourceMimeType);
  creds_response.SetHttpHeaderField(http_names::kCacheControl, "no-store");
  creds_response.SetHttpStatusCode(200);
  creds_response.SetType(network::mojom::FetchResponseType::kBasic);
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      creds_url, test::CoreTestDataPath(kTestResourceFilename), creds_response);

  FetchParameters anon_params2(ResourceRequest(creds_url),
                               ResourceLoaderOptions(nullptr));
  anon_params2.SetCrossOriginAccessControl(origin,
                                           kCrossOriginAttributeAnonymous);
  ImageResourceContent* cached_anon = CacheImageContent(anon_params2);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(cached_anon);
  EXPECT_EQ(cached_anon, FetchedImageMap().at(creds_url.GetString()));

  FetchParameters creds_params(ResourceRequest(creds_url),
                               ResourceLoaderOptions(nullptr));
  creds_params.SetCrossOriginAccessControl(
      origin, kCrossOriginAttributeUseCredentials);
  ImageResourceContent* creds_image = CacheImageContent(creds_params);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(creds_image);
  EXPECT_NE(cached_anon, creds_image);
  EXPECT_EQ(cached_anon, FetchedImageMap().at(creds_url.GetString()));
}

TEST_F(StyleImageCacheWithLoadingTest,
       DifferingReferrerPolicyNotReusedAndDoesNotClobber) {
  const auto image_url =
      url_test_helpers::ToKURL("http://test.com/referrer_policy_image.png");
  WebURLResponse response(image_url);
  response.SetMimeType(kTestResourceMimeType);
  response.SetHttpHeaderField(http_names::kContentType, kTestResourceMimeType);
  response.SetHttpHeaderField(http_names::kCacheControl, "no-store");
  response.SetHttpStatusCode(200);
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      image_url, test::CoreTestDataPath(kTestResourceFilename), response);

  ResourceRequest req1(image_url);
  req1.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  FetchParameters params1(std::move(req1), ResourceLoaderOptions(nullptr));
  ImageResourceContent* image1 = CacheImageContent(params1);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(image1);
  EXPECT_EQ(image1, FetchedImageMap().at(image_url.GetString()));

  ResourceRequest req1_same(image_url);
  req1_same.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  FetchParameters params1_same(std::move(req1_same),
                               ResourceLoaderOptions(nullptr));
  EXPECT_TRUE(CanReuseImageContent(*image1, params1_same));

  ResourceRequest req2(image_url);
  req2.SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
  FetchParameters params2(std::move(req2), ResourceLoaderOptions(nullptr));
  EXPECT_FALSE(CanReuseImageContent(*image1, params2));
  ImageResourceContent* image2 = CacheImageContent(params2);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(image2);
  EXPECT_EQ(image1, FetchedImageMap().at(image_url.GetString()));
}

TEST_F(StyleImageCacheWithLoadingTest,
       IsolatedWorldAndOriginDirtyStyleSheetBypassCache) {
  const auto isolated_url =
      url_test_helpers::ToKURL("http://test.com/isolated_world_image.png");
  const auto dirty_url =
      url_test_helpers::ToKURL("http://test.com/origin_dirty_image.png");
  for (const KURL& url : {isolated_url, dirty_url}) {
    WebURLResponse response(url);
    response.SetMimeType(kTestResourceMimeType);
    response.SetHttpHeaderField(http_names::kContentType, kTestResourceMimeType);
    response.SetHttpHeaderField(http_names::kCacheControl, "no-store");
    response.SetHttpStatusCode(200);
    url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
        url, test::CoreTestDataPath(kTestResourceFilename), response);
  }

  DOMWrapperWorld* isolated_world = DOMWrapperWorld::EnsureIsolatedWorld(
      GetDocument().GetExecutionContext()->GetIsolate(), /*world_id=*/1);
  ASSERT_TRUE(isolated_world);

  // Isolated-world fetch must not populate or reuse StyleImageCache.
  FetchParameters isolated_params{ResourceRequest(isolated_url),
                                  ResourceLoaderOptions(isolated_world)};
  ImageResourceContent* isolated_image = CacheImageContent(isolated_params);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(isolated_image);
  EXPECT_FALSE(FetchedImageMap().Contains(isolated_url.GetString()));

  // Origin-dirty stylesheet fetch must not populate StyleImageCache, nor reuse
  // or clobber an existing main-world entry.
  FetchParameters dirty_params(ResourceRequest(dirty_url),
                               ResourceLoaderOptions(nullptr));
  dirty_params.SetFromOriginDirtyStyleSheet(true);
  ImageResourceContent* dirty_image = CacheImageContent(dirty_params);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(dirty_image);
  EXPECT_FALSE(FetchedImageMap().Contains(dirty_url.GetString()));

  FetchParameters clean_params(ResourceRequest(dirty_url),
                               ResourceLoaderOptions(nullptr));
  ImageResourceContent* clean_image = CacheImageContent(clean_params);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(clean_image);
  EXPECT_EQ(clean_image, FetchedImageMap().at(dirty_url.GetString()));

  FetchParameters isolated_params2{ResourceRequest(dirty_url),
                                   ResourceLoaderOptions(isolated_world)};
  EXPECT_FALSE(CanReuseImageContent(*clean_image, isolated_params2));

  FetchParameters dirty_params2(ResourceRequest(dirty_url),
                                ResourceLoaderOptions(nullptr));
  dirty_params2.SetFromOriginDirtyStyleSheet(true);
  EXPECT_FALSE(CanReuseImageContent(*clean_image, dirty_params2));
  ImageResourceContent* dirty_image2 = CacheImageContent(dirty_params2);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(dirty_image2);
  EXPECT_EQ(clean_image, FetchedImageMap().at(dirty_url.GetString()));
}

}  // namespace blink
