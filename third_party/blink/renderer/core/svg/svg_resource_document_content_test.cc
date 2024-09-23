// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_cache.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_observer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

namespace blink {

namespace {

class FakeSVGResourceDocumentObserver final
    : public GarbageCollected<FakeSVGResourceDocumentObserver>,
      public SVGResourceDocumentObserver {
 public:
  void ResourceNotifyFinished(SVGResourceDocumentContent*) override {}
  void ResourceContentChanged(SVGResourceDocumentContent*) override {}
};

}  // namespace

class SVGResourceDocumentContentSimTest : public SimTest {};

TEST_F(SVGResourceDocumentContentSimTest, GetDocumentBeforeLoadComplete) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete("<html><body></body></html>");

  const char kSVGUrl[] = "https://example.com/svg.svg";
  SimSubresourceRequest svg_resource(kSVGUrl, "application/xml");

  // Request a resource from the cache.
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(kSVGUrl), options);
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  auto* entry = SVGResourceDocumentContent::Fetch(params, GetDocument());

  // Write part of the response. The document should not be initialized yet,
  // because the response is not complete. The document would be invalid at this
  // point.
  svg_resource.Start();
  svg_resource.Write("<sv");
  EXPECT_EQ(nullptr, entry->GetDocument());

  // Finish the response, the Document should now be accessible.
  svg_resource.Complete("g xmlns='http://www.w3.org/2000/svg'></svg>");
  EXPECT_NE(nullptr, entry->GetDocument());
}

TEST_F(SVGResourceDocumentContentSimTest, LoadCompleteAfterDispose) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete("<!doctype html><body></body>");

  const char kSVGUrl[] = "https://example.com/svg.svg";
  SimSubresourceRequest svg_resource(kSVGUrl, "application/xml");

  // Request a resource from the cache.
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(kSVGUrl), options);
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  auto* content = SVGResourceDocumentContent::Fetch(params, GetDocument());

  EXPECT_TRUE(content->IsLoading());
  EXPECT_FALSE(content->IsLoaded());
  EXPECT_FALSE(content->ErrorOccurred());

  // Make the GC dispose - and thus lose track of - the content.
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Write part of the response. The document hasn't been created yet, but the
  // cache no longer references it.
  svg_resource.Start();
  svg_resource.Complete("<svg xmlns='http://www.w3.org/2000/svg'></svg>");

  // The cache reference is gone.
  EXPECT_EQ(GetDocument().GetPage()->GetSVGResourceDocumentCache().Get(
                SVGResourceDocumentCache::MakeCacheKey(params)),
            nullptr);

  EXPECT_FALSE(content->IsLoading());
  EXPECT_TRUE(content->IsLoaded());
  EXPECT_TRUE(content->ErrorOccurred());

  content = nullptr;

  // GC the content. Should not crash/DCHECK.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

class SVGResourceDocumentContentTest : public PageTestBase {
 public:
  SVGResourceDocumentContentTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(SVGResourceDocumentContentTest, EmptyDataUrl) {
  const char kEmptySVGImageDataUrl[] = "data:image/svg+xml,";
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(kEmptySVGImageDataUrl), options);
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  auto* content = SVGResourceDocumentContent::Fetch(params, GetDocument());

  EXPECT_TRUE(content->IsLoaded());
  EXPECT_TRUE(content->ErrorOccurred());
}

TEST_F(SVGResourceDocumentContentTest, InvalidDocumentRoot) {
  const char kInvalidSvgImageDataUrl[] = "data:image/svg+xml,<root/>";
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(kInvalidSvgImageDataUrl), options);
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  auto* content = SVGResourceDocumentContent::Fetch(params, GetDocument());

  EXPECT_TRUE(content->IsLoaded());
  EXPECT_FALSE(content->ErrorOccurred());
  EXPECT_EQ(content->GetStatus(), ResourceStatus::kCached);
}

TEST_F(SVGResourceDocumentContentTest, CacheCleanup) {
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;

  const char kImageDataUrl1[] =
      "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg'/>";
  FetchParameters params1(ResourceRequest(kImageDataUrl1), options);
  params1.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  auto* content1 = SVGResourceDocumentContent::Fetch(params1, GetDocument());
  EXPECT_TRUE(content1->IsLoaded());

  const char kImageDataUrl2[] =
      "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' id='two'/>";
  FetchParameters params2(ResourceRequest(kImageDataUrl2), options);
  params2.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  auto* content2 = SVGResourceDocumentContent::Fetch(params2, GetDocument());
  EXPECT_TRUE(content2->IsLoaded());

  Persistent<FakeSVGResourceDocumentObserver> observer =
      MakeGarbageCollected<FakeSVGResourceDocumentObserver>();
  content2->AddObserver(observer);

  auto& cache = GetPage().GetSVGResourceDocumentCache();

  // Both document contents should be in the cache.
  EXPECT_NE(cache.Get(SVGResourceDocumentCache::MakeCacheKey(params1)),
            nullptr);
  EXPECT_NE(cache.Get(SVGResourceDocumentCache::MakeCacheKey(params2)),
            nullptr);

  ThreadState::Current()->CollectAllGarbageForTesting();

  FastForwardUntilNoTasksRemain();

  // Only content2 (from params2) should be in the cache.
  EXPECT_EQ(cache.Get(SVGResourceDocumentCache::MakeCacheKey(params1)),
            nullptr);
  EXPECT_NE(cache.Get(SVGResourceDocumentCache::MakeCacheKey(params2)),
            nullptr);

  content2->RemoveObserver(observer);

  ThreadState::Current()->CollectAllGarbageForTesting();

  FastForwardUntilNoTasksRemain();

  // Neither of the document contents should be in the cache.
  EXPECT_EQ(cache.Get(SVGResourceDocumentCache::MakeCacheKey(params1)),
            nullptr);
  EXPECT_EQ(cache.Get(SVGResourceDocumentCache::MakeCacheKey(params2)),
            nullptr);
}

TEST_F(SVGResourceDocumentContentTest, SecondLoadOfResourceInError) {
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;

  const char kUrl[] = "data:image/svg+xml,a";
  FetchParameters params1(ResourceRequest(kUrl), options);
  params1.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);

  auto* content1 = SVGResourceDocumentContent::Fetch(params1, GetDocument());
  EXPECT_TRUE(content1->IsLoaded());

  // Simulate a later failure.
  content1->UpdateStatus(ResourceStatus::kLoadError);
  EXPECT_TRUE(content1->ErrorOccurred());

  FetchParameters params2(ResourceRequest(kUrl), options);
  params2.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);

  auto* content2 = SVGResourceDocumentContent::Fetch(params2, GetDocument());
  EXPECT_TRUE(content2->IsLoaded());

  ThreadState::Current()->CollectAllGarbageForTesting();
}

}  // namespace blink
