// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_url_data.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

const char kImageUrl[] = "https://example.com/a.png";
const char kFontUrl[] = "https://example.com/a.woff2";

class TestFontResourceClient final
    : public GarbageCollected<TestFontResourceClient>,
      public FontResourceClient {
 public:
  String DebugName() const override { return "TestFontResourceClient"; }
};

// A frame client that routes subresource loads through the URLLoaderMockFactory
// so that image/font fetches can be driven to a deterministic completion (in
// particular, an error) via url_test_helpers::ServeAsynchronousRequests().
class MockLoaderFrameClient final : public EmptyLocalFrameClient {
 public:
  std::unique_ptr<URLLoader> CreateURLLoaderForTesting() override {
    return URLLoaderMockFactory::GetSingletonInstance()->CreateURLLoader();
  }
};

void ConfigureSettings(Settings& settings) {
  // Images are otherwise deferred and never actually loaded, which would
  // prevent us from observing a completed (failed) load.
  settings.SetLoadsImagesAutomatically(true);
}

}  // namespace

// Tests for the caching behavior of style classes that use the ResourceFetcher
// as an identifier for their caching.
class StyleResourceFetcherIdentityTest : public testing::Test {
 protected:
  void SetUp() override {
    holder_a_ = std::make_unique<DummyPageHolder>(
        gfx::Size(), /*chrome_client=*/nullptr,
        MakeGarbageCollected<MockLoaderFrameClient>(),
        BindOnce(&ConfigureSettings));
    holder_b_ = std::make_unique<DummyPageHolder>(
        gfx::Size(), /*chrome_client=*/nullptr,
        MakeGarbageCollected<MockLoaderFrameClient>(),
        BindOnce(&ConfigureSettings));
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  Document& DocA() const { return holder_a_->GetDocument(); }
  Document& DocB() const { return holder_b_->GetDocument(); }
  ResourceFetcher* FetcherA() const { return DocA().Fetcher(); }
  ResourceFetcher* FetcherB() const { return DocB().Fetcher(); }

  CSSImageValue* MakeImageValue(StyleImage* fetcher_agnostic_image = nullptr) {
    auto* url_data = MakeGarbageCollected<CSSUrlData>(AtomicString(kImageUrl));
    return MakeGarbageCollected<CSSImageValue>(*url_data,
                                               fetcher_agnostic_image);
  }

  // Builds a concrete StyleImage backed by a not-yet-started
  // ImageResourceContent. Its CachedImage() is non-null and reports no
  // failure, so it represents a "cached, healthy" resource.
  StyleImage* MakeStyleImage() {
    auto* content = ImageResourceContent::CreateNotStarted();
    auto* url_data = MakeGarbageCollected<CSSUrlData>(AtomicString(kImageUrl));
    return MakeGarbageCollected<StyleFetchedImage>(content, *url_data, DocA(),
                                                   KURL(kImageUrl));
  }

  CSSFontFaceSrcValue* MakeFontValue() {
    auto* url_data = MakeGarbageCollected<CSSUrlData>(AtomicString(kFontUrl));
    auto* uri_value = MakeGarbageCollected<cssvalue::CSSURIValue>(*url_data);
    const DOMWrapperWorld* world =
        MakeGarbageCollected<CSSParserContext>(DocA())->JavascriptWorld();
    return CSSFontFaceSrcValue::Create(uri_value, world);
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> holder_a_;
  std::unique_ptr<DummyPageHolder> holder_b_;
};

TEST_F(StyleResourceFetcherIdentityTest, CSSImageValueCaching) {
  url_test_helpers::RegisterMockedErrorURLLoad(
      url_test_helpers::ToKURL(kImageUrl));
  CSSImageValue* value = MakeImageValue();

  EXPECT_TRUE(value->IsCachePending(FetcherA()));
  EXPECT_TRUE(value->IsCachePending(FetcherB()));
  EXPECT_TRUE(value->IsCachePending(nullptr));
  EXPECT_EQ(value->CachedImage(FetcherA()), nullptr);
  EXPECT_EQ(value->CachedImage(FetcherB()), nullptr);
  EXPECT_EQ(value->CachedImage(nullptr), nullptr);
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  StyleImage* cached_a = value->CacheImage(DocA());
  ASSERT_TRUE(cached_a);

  EXPECT_FALSE(value->IsCachePending(FetcherA()));
  EXPECT_TRUE(value->IsCachePending(FetcherB()));
  EXPECT_TRUE(value->IsCachePending(nullptr));
  EXPECT_EQ(value->CachedImage(FetcherA()), cached_a);
  EXPECT_EQ(value->CachedImage(FetcherB()), nullptr);
  EXPECT_EQ(value->CachedImage(nullptr), nullptr);
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  url_test_helpers::ServeAsynchronousRequests();
  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  StyleImage* cached_b = value->CacheImage(DocB());
  ASSERT_TRUE(cached_b);
  EXPECT_NE(cached_a, cached_b);
  EXPECT_FALSE(value->IsCachePending(FetcherB()));
  EXPECT_EQ(value->CachedImage(FetcherA()), cached_a);
  EXPECT_EQ(value->CachedImage(FetcherB()), cached_b);
  EXPECT_EQ(value->CachedImage(nullptr), nullptr);

  url_test_helpers::ServeAsynchronousRequests();
  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  EXPECT_EQ(value->CacheImage(DocA()), cached_a);
}

TEST_F(StyleResourceFetcherIdentityTest,
       CSSImageValueCachingWithFetcherAgnosticImage) {
  url_test_helpers::RegisterMockedErrorURLLoad(
      url_test_helpers::ToKURL(kImageUrl));
  StyleImage* fetcher_agnostic_image = MakeStyleImage();
  CSSImageValue* value = MakeImageValue(fetcher_agnostic_image);

  // All contexts can use the fetcher_agnostic_image
  EXPECT_FALSE(value->IsCachePending(FetcherA()));
  EXPECT_FALSE(value->IsCachePending(FetcherB()));
  EXPECT_FALSE(value->IsCachePending(nullptr));
  EXPECT_EQ(value->CachedImage(FetcherA()), fetcher_agnostic_image);
  EXPECT_EQ(value->CachedImage(FetcherB()), fetcher_agnostic_image);
  EXPECT_EQ(value->CachedImage(nullptr), fetcher_agnostic_image);
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  // A per-fetcher fetch takes precedence over the fetcher_agnostic_image
  StyleImage* cached_a = value->CacheImage(DocA());
  ASSERT_TRUE(cached_a);
  EXPECT_NE(cached_a, fetcher_agnostic_image);
  EXPECT_EQ(value->CachedImage(FetcherA()), cached_a);
  EXPECT_EQ(value->CachedImage(FetcherB()), fetcher_agnostic_image);
  EXPECT_EQ(value->CachedImage(nullptr), fetcher_agnostic_image);

  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));
}

TEST_F(StyleResourceFetcherIdentityTest,
       CSSImageValueHasFailedFetcherAgnosticImage) {
  url_test_helpers::RegisterMockedErrorURLLoad(
      url_test_helpers::ToKURL(kImageUrl));

  // Produce a StyleImage in a failed state
  CSSImageValue* source = MakeImageValue();
  StyleImage* failed_agnostic_image = source->CacheImage(DocA());
  ASSERT_TRUE(failed_agnostic_image);
  url_test_helpers::ServeAsynchronousRequests();
  ASSERT_TRUE(failed_agnostic_image->ErrorOccurred());

  CSSImageValue* value = MakeImageValue(failed_agnostic_image);

  // All contexts can use the (failed) fetcher_agnostic_image
  EXPECT_FALSE(value->IsCachePending(FetcherA()));
  EXPECT_FALSE(value->IsCachePending(FetcherB()));
  EXPECT_FALSE(value->IsCachePending(nullptr));
  EXPECT_EQ(value->CachedImage(FetcherA()), failed_agnostic_image);
  EXPECT_EQ(value->CachedImage(FetcherB()), failed_agnostic_image);
  EXPECT_EQ(value->CachedImage(nullptr), failed_agnostic_image);
  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(nullptr));
}

TEST_F(StyleResourceFetcherIdentityTest, CSSImageSetValueCaching) {
  auto* value = MakeGarbageCollected<CSSImageSetValue>();

  EXPECT_TRUE(value->IsCachePending(FetcherA(), 1.0f));
  EXPECT_TRUE(value->IsCachePending(FetcherB(), 1.0f));
  EXPECT_TRUE(value->IsCachePending(nullptr, 1.0f));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  StyleImage* image_a = MakeStyleImage();
  StyleImage* cached_a = value->CacheImage(FetcherA(), image_a, 1.0f);
  ASSERT_TRUE(cached_a);

  // CachedImage() is only queried where IsCachePending() is false, since it
  // DCHECKs the entry exists.
  EXPECT_FALSE(value->IsCachePending(FetcherA(), 1.0f));
  EXPECT_TRUE(value->IsCachePending(FetcherA(), 2.0f));
  EXPECT_TRUE(value->IsCachePending(FetcherB(), 1.0f));
  EXPECT_TRUE(value->IsCachePending(nullptr, 1.0f));
  EXPECT_EQ(value->CachedImage(FetcherA(), 1.0f), cached_a);
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  StyleImage* image_null = MakeStyleImage();
  StyleImage* cached_null = value->CacheImage(nullptr, image_null, 1.0f);
  ASSERT_TRUE(cached_null);
  EXPECT_NE(cached_a, cached_null);
  EXPECT_FALSE(value->IsCachePending(FetcherA(), 1.0f));
  EXPECT_TRUE(value->IsCachePending(FetcherB(), 1.0f));
  EXPECT_FALSE(value->IsCachePending(nullptr, 1.0f));
  EXPECT_EQ(value->CachedImage(FetcherA(), 1.0f), cached_a);
  EXPECT_EQ(value->CachedImage(nullptr, 1.0f), cached_null);
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  StyleImage* image_b = MakeStyleImage();
  StyleImage* cached_b = value->CacheImage(FetcherB(), image_b, 1.0f);
  ASSERT_TRUE(cached_b);
  EXPECT_NE(cached_a, cached_b);
  EXPECT_FALSE(value->IsCachePending(FetcherB(), 1.0f));
  EXPECT_EQ(value->CachedImage(FetcherA(), 1.0f), cached_a);
  EXPECT_EQ(value->CachedImage(FetcherB(), 1.0f), cached_b);
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  // Re-caching FetcherA at a new DPR replaces its single entry.
  StyleImage* image_a_2 = MakeStyleImage();
  StyleImage* cached_a_2 = value->CacheImage(FetcherA(), image_a_2, 2.0f);
  ASSERT_TRUE(cached_a_2);
  EXPECT_NE(cached_a, cached_a_2);
  EXPECT_FALSE(value->IsCachePending(FetcherA(), 2.0f));
  EXPECT_EQ(value->CachedImage(FetcherA(), 2.0f), cached_a_2);

  StyleImage* cached_a_3 =
      value->CacheImage(FetcherA(), /*style_image=*/nullptr, 2.0f);
  ASSERT_TRUE(cached_a_3);
  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));
}

TEST_F(StyleResourceFetcherIdentityTest, CSSFontFaceSrcValueCaching) {
  url_test_helpers::RegisterMockedErrorURLLoad(
      url_test_helpers::ToKURL(kFontUrl));
  CSSFontFaceSrcValue* value = MakeFontValue();
  auto* client = MakeGarbageCollected<TestFontResourceClient>();

  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  FontResource& first = value->Fetch(DocA().GetExecutionContext(), client);
  FontResource& again = value->Fetch(DocA().GetExecutionContext(), client);
  EXPECT_EQ(&first, &again);

  FetcherA()->StartLoad(&first);
  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));

  FontResource& from_b = value->Fetch(DocB().GetExecutionContext(), client);
  EXPECT_EQ(&value->Fetch(DocB().GetExecutionContext(), client), &from_b);
  FetcherB()->StartLoad(&from_b);
  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherA()));
  EXPECT_TRUE(value->HasFailedOrCanceledSubresources(FetcherB()));
  EXPECT_FALSE(value->HasFailedOrCanceledSubresources(nullptr));
}

}  // namespace blink
