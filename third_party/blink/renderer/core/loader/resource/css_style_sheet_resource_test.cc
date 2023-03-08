// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;

namespace {

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

  contents->EnsureRuleSet(MediaQueryEvaluator(GetDocument().GetFrame()));
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

}  // namespace
}  // namespace blink
