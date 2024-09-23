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
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
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
    response.SetMimeType(AtomicString("style/css"));

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
  response.SetMimeType(AtomicString("style/css"));

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

TEST_F(CSSStyleSheetResourceTest, CreateFromCacheWithMediaQueries) {
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
  EXPECT_EQ(1U, contents->ClientSize());
  EXPECT_TRUE(contents->IsReferencedFromResource());
  EXPECT_TRUE(contents->HasRuleSet());

  EXPECT_TRUE(parsed_stylesheet->HasSingleOwnerDocument());
  EXPECT_TRUE(parsed_stylesheet->HasOneClient());
  EXPECT_TRUE(parsed_stylesheet->IsReferencedFromResource());
  EXPECT_TRUE(parsed_stylesheet->HasRuleSet());
}

class CSSStyleSheetResourceSimTest : public SimTest {};

TEST_F(CSSStyleSheetResourceSimTest, CachedWithDifferentMQEval) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame1_resource("https://example.com/frame1.html", "text/html");
  SimRequest frame2_resource("https://example.com/frame2.html", "text/html");

  SimRequest::Params params;
  params.response_http_headers = {{"Cache-Control", "max-age=3600"}};
  SimSubresourceRequest css_resource("https://example.com/frame.css",
                                     "text/css", params);

  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #frame1 {
        width: 200px;
        height: 200px;
      }
      #frame2 {
        width: 400px;
        height: 200px;
      }
    </style>
    <div></div>
    <iframe id="frame1" src="frame1.html"></iframe>
    <iframe id="frame2" src="frame2.html"></iframe>
  )HTML");

  frame1_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <link rel="stylesheet" href="frame.css">
    <div id="target"></div>
  )HTML");

  css_resource.Complete(R"HTML(
    #target { opacity: 0; }
    @media (width > 300px) {
      #target { opacity: 0.3; }
    }
    @media (width > 500px) {
      #target { opacity: 0.5; }
    }
  )HTML");

  test::RunPendingTasks();

  frame2_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <link rel="stylesheet" href="frame.css">
    <div id="target"></div>
  )HTML");

  test::RunPendingTasks();

  Compositor().BeginFrame();

  Document* frame1_doc = To<HTMLIFrameElement>(GetDocument().getElementById(
                                                   AtomicString("frame1")))
                             ->contentDocument();
  Document* frame2_doc = To<HTMLIFrameElement>(GetDocument().getElementById(
                                                   AtomicString("frame2")))
                             ->contentDocument();
  ASSERT_TRUE(frame1_doc);
  ASSERT_TRUE(frame2_doc);
  frame1_doc->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  frame2_doc->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  const ActiveStyleSheetVector& frame1_sheets =
      frame1_doc->GetScopedStyleResolver()->GetActiveStyleSheets();
  const ActiveStyleSheetVector& frame2_sheets =
      frame2_doc->GetScopedStyleResolver()->GetActiveStyleSheets();
  ASSERT_EQ(frame1_sheets.size(), 1u);
  ASSERT_EQ(frame2_sheets.size(), 1u);

  // The two frames should share the same cached StyleSheetContents ...
  EXPECT_EQ(frame1_sheets[0].first->Contents(),
            frame2_sheets[0].first->Contents());

  // ... but have different RuleSets due to different media query evaluation.
  EXPECT_NE(frame1_sheets[0].second, frame2_sheets[0].second);

  // Verify styling based on MQ evaluation.
  Element* target1 = frame1_doc->getElementById(AtomicString("target"));
  ASSERT_TRUE(target1);
  EXPECT_EQ(target1->GetComputedStyle()->Opacity(), 0);
  Element* target2 = frame2_doc->getElementById(AtomicString("target"));
  ASSERT_TRUE(target2);
  EXPECT_EQ(target2->GetComputedStyle()->Opacity(), 0.3f);
}

}  // namespace
}  // namespace blink
