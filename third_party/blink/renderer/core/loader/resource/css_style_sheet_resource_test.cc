// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/noop_web_url_loader.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;

namespace {

class NoopLoaderFactory final : public ResourceFetcher::LoaderFactory {
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper)
      override {
    return std::make_unique<NoopWebURLLoader>(std::move(freezable_task_runner));
  }
  std::unique_ptr<WebCodeCacheLoader> CreateCodeCacheLoader() override {
    return std::make_unique<CodeCacheLoaderMock>();
  }
};

ResourceFetcher* CreateFetcher() {
  auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
  return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
      properties->MakeDetachable(), MakeGarbageCollected<MockFetchContext>(),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      MakeGarbageCollected<NoopLoaderFactory>(),
      MakeGarbageCollected<MockContextLifecycleNotifier>(),
      nullptr /* back_forward_cache_loader_helper */));
}

// ResourceClient which can wait for the resource load to finish.
class TestResourceClient : public GarbageCollected<TestResourceClient>,
                           public ResourceClient {
 public:
  void NotifyFinished(Resource* resource) override {
    has_finished_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }
  String DebugName() const override { return "TestResourceClient"; }

  void WaitForFinish() {
    if (has_finished_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
  bool has_finished_ = false;
};

class CSSStyleSheetResourceTest : public PageTestBase {
 protected:
  CSSStyleSheetResourceTest() {
    original_memory_cache_ =
        ReplaceMemoryCacheForTesting(MakeGarbageCollected<MemoryCache>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  }

  ~CSSStyleSheetResourceTest() override {
    ReplaceMemoryCacheForTesting(original_memory_cache_.Release());
  }

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    GetDocument().SetURL(KURL("https://localhost/"));
  }

  CSSStyleSheetResource* CreateAndSaveTestStyleSheetResource() {
    const char kUrl[] = "https://localhost/style.css";
    const KURL css_url(kUrl);
    ResourceResponse response(css_url);
    response.SetMimeType("style/css");

    CSSStyleSheetResource* css_resource =
        CSSStyleSheetResource::CreateForTest(css_url, UTF8Encoding());
    css_resource->ResponseReceived(response);
    css_resource->FinishForTest();
    MemoryCache::Get()->Add(css_resource);
    return css_resource;
  }

  Persistent<MemoryCache> original_memory_cache_;
};

TEST_F(CSSStyleSheetResourceTest, DuplicateResourceNotCached) {
  const char kUrl[] = "https://localhost/style.css";
  const KURL image_url(kUrl);
  const KURL css_url(kUrl);
  ResourceResponse response(css_url);
  response.SetMimeType("style/css");

  // Emulate using <img> to do async stylesheet preloads.

  Resource* image_resource = ImageResource::CreateForTest(image_url);
  ASSERT_TRUE(image_resource);
  MemoryCache::Get()->Add(image_resource);
  ASSERT_TRUE(MemoryCache::Get()->Contains(image_resource));

  CSSStyleSheetResource* css_resource =
      CSSStyleSheetResource::CreateForTest(css_url, UTF8Encoding());
  css_resource->ResponseReceived(response);
  css_resource->FinishForTest();

  auto* parser_context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* contents = MakeGarbageCollected<StyleSheetContents>(parser_context);
  auto* sheet = MakeGarbageCollected<CSSStyleSheet>(contents, GetDocument());
  EXPECT_TRUE(sheet);

  contents->CheckLoaded();
  css_resource->SaveParsedStyleSheet(contents);

  // Verify that the cache will have a mapping for |imageResource| at |url|.
  // The underlying |contents| for the stylesheet resource must have a
  // matching reference status.
  EXPECT_TRUE(MemoryCache::Get()->Contains(image_resource));
  EXPECT_FALSE(MemoryCache::Get()->Contains(css_resource));
  EXPECT_FALSE(contents->IsReferencedFromResource());
  EXPECT_FALSE(css_resource->CreateParsedStyleSheetFromCache(parser_context));
}

TEST_F(CSSStyleSheetResourceTest, CreateFromCacheRestoresOriginalSheet) {
  CSSStyleSheetResource* css_resource = CreateAndSaveTestStyleSheetResource();

  auto* parser_context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* contents = MakeGarbageCollected<StyleSheetContents>(parser_context);
  auto* sheet = MakeGarbageCollected<CSSStyleSheet>(contents, GetDocument());
  ASSERT_TRUE(sheet);

  contents->ParseString("div { color: red; }");
  contents->NotifyLoadedSheet(css_resource);
  contents->CheckLoaded();
  EXPECT_TRUE(contents->IsCacheableForResource());

  css_resource->SaveParsedStyleSheet(contents);
  EXPECT_TRUE(MemoryCache::Get()->Contains(css_resource));
  EXPECT_TRUE(contents->IsReferencedFromResource());

  StyleSheetContents* parsed_stylesheet =
      css_resource->CreateParsedStyleSheetFromCache(parser_context);
  ASSERT_EQ(contents, parsed_stylesheet);
}

TEST_F(CSSStyleSheetResourceTest,
       CreateFromCacheWithMediaQueriesCopiesOriginalSheet) {
  CSSStyleSheetResource* css_resource = CreateAndSaveTestStyleSheetResource();

  auto* parser_context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* contents = MakeGarbageCollected<StyleSheetContents>(parser_context);
  auto* sheet = MakeGarbageCollected<CSSStyleSheet>(contents, GetDocument());
  ASSERT_TRUE(sheet);

  contents->ParseString("@media { div { color: red; } }");
  contents->NotifyLoadedSheet(css_resource);
  contents->CheckLoaded();
  EXPECT_TRUE(contents->IsCacheableForResource());

  contents->EnsureRuleSet(MediaQueryEvaluator(GetDocument().GetFrame()),
                          kRuleHasNoSpecialState);
  EXPECT_TRUE(contents->HasRuleSet());

  css_resource->SaveParsedStyleSheet(contents);
  EXPECT_TRUE(MemoryCache::Get()->Contains(css_resource));
  EXPECT_TRUE(contents->IsReferencedFromResource());

  StyleSheetContents* parsed_stylesheet =
      css_resource->CreateParsedStyleSheetFromCache(parser_context);
  ASSERT_TRUE(parsed_stylesheet);

  sheet->ClearOwnerNode();
  sheet = MakeGarbageCollected<CSSStyleSheet>(parsed_stylesheet, GetDocument());
  ASSERT_TRUE(sheet);

  EXPECT_TRUE(contents->HasSingleOwnerDocument());
  EXPECT_EQ(0U, contents->ClientSize());
  EXPECT_TRUE(contents->IsReferencedFromResource());
  EXPECT_TRUE(contents->HasRuleSet());

  EXPECT_TRUE(parsed_stylesheet->HasSingleOwnerDocument());
  EXPECT_TRUE(parsed_stylesheet->HasOneClient());
  EXPECT_FALSE(parsed_stylesheet->IsReferencedFromResource());
  EXPECT_FALSE(parsed_stylesheet->HasRuleSet());
}

TEST_F(CSSStyleSheetResourceTest, TokenizerCreated) {
  base::test::ScopedFeatureList feature_list(features::kPretokenizeCSS);
  auto* fetcher = CreateFetcher();

  KURL url("https://www.example.com/");
  ResourceRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  auto* client = MakeGarbageCollected<TestResourceClient>();
  auto params = FetchParameters::CreateForTest(std::move(request));
  auto* resource = CSSStyleSheetResource::Fetch(params, fetcher, client);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(100, producer, consumer), MOJO_RESULT_OK);

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  ResourceLoader* loader = resource->Loader();
  loader->DidReceiveResponse(WrappedResourceResponse(response));
  loader->DidStartLoadingResponseBody(std::move(consumer));
  loader->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);

  // Send the body in two chunks to make sure this is handled correctly.
  uint32_t num_bytes = 4;
  MojoResult result =
      producer->WriteData(".foo", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(num_bytes, 4u);

  num_bytes = 5;
  result = producer->WriteData("{a:b}", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_EQ(num_bytes, 5u);

  producer.reset();
  client->WaitForFinish();

  // A non-empty tokenizer should be created, and the sheet text will contain
  // the full response body.
  auto tokenizer = resource->TakeTokenizer();
  EXPECT_NE(tokenizer, nullptr);

  // Finish tokenizing and grab the token count.
  while (tokenizer->TokenizeSingle().GetType() != kEOFToken) {
  }
  EXPECT_GT(tokenizer->TokenCount(), 1u);

  EXPECT_EQ(
      resource->SheetText(nullptr, CSSStyleSheetResource::MIMETypeCheck::kLax),
      ".foo{a:b}");
}

}  // namespace
}  // namespace blink
