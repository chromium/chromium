// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/svg/svg_document_resource_tracker.h"
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
  if (RuntimeEnabledFeatures::
          SvgPartitionSVGDocumentResourcesInMemoryCacheEnabled()) {
    EXPECT_FALSE(GetDocument()
                     .GetPage()
                     ->GetSVGDocumentResourceTracker()
                     .HasContentForTesting(content));
  } else {
    EXPECT_EQ(GetDocument().GetPage()->GetSVGDocumentResourceTracker().Get(
                  SVGDocumentResourceTracker::MakeCacheKey(params)),
              nullptr);
  }

  EXPECT_FALSE(content->IsLoading());
  EXPECT_TRUE(content->IsLoaded());
  EXPECT_TRUE(content->ErrorOccurred());

  content = nullptr;

  // GC the content. Should not crash/DCHECK.
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST_F(SVGResourceDocumentContentSimTest, AsyncLoadCompleteCallbackRace) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");

  SimSubresourceRequest svg_resource("https://example.com/resource.svg#root",
                                     "application/xml");
  // Write the full page resource, but don't signal it as being complete. This
  // is to avoid flushing layout changes before the external resource has
  // loaded.
  main_resource.Write(
      "<!doctype html><svg><use href='resource.svg#root'/></svg>");

  svg_resource.Start();
  svg_resource.Complete(R"SVG(
    <svg id="root" xmlns="http://www.w3.org/2000/svg">
      <rect width="100" height="100" fill="url(#p)"/>
      <defs>
        <pattern id="p" width="100" height="100">
          <use href="#i"/>
        </pattern>
        <image id="i" href="data:image/gif;base64,R0lGODdhCQAJAKEAAO6C7v8A/6Ag8AAAACwAAAAACQAJAAACFISPaWLhLhh4UNIQG81zswiGIlgAADs="/>
      </defs>
    </svg>)SVG");

  // Flush tasks on the "internal loading" task queue. IsolatedSVGDocumentHost
  // will post/run the async-loading-complete callback on this task queue.
  base::RunLoop run_loop;
  GetDocument()
      .GetTaskRunner(TaskType::kInternalLoading)
      ->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Update layout and paint. This will rebuild the instance tree for the
  // <use>. The rebuilding should not succeed (find the referenced target)
  // because the external resource is not considered fully loaded yet. If it
  // succeeds we may paint with a dirty tree.
  Compositor().BeginFrame();

  main_resource.Complete();
}

TEST_F(SVGResourceDocumentContentSimTest,
       AsyncLoadCompleteCallbackRevalidationRace) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete("<!doctype html>");

  // Setup response headers so that the resource will be revalidated.
  SimRequest::Params revalidate_params;
  revalidate_params.response_http_headers = {{"Cache-Control", "max-age=0"},
                                             {"ETag", "foo"}};
  SimSubresourceRequest svg_resource("https://example.com/resource.svg#root",
                                     "image/svg+xml", revalidate_params);

  // Make an initial request for 'resource.svg'.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<svg><use href='resource.svg#root'/></svg>");

  String svg_resource_content(R"SVG(
    <svg id="root" xmlns="http://www.w3.org/2000/svg">
      <image href="data:image/gif;base64,R0lGODdhCQAJAKEAAO6C7v8A/6Ag8AAAACwAAAAACQAJAAACFISPaWLhLhh4UNIQG81zswiGIlgAADs="/>
    </svg>)SVG");
  svg_resource.Start();
  svg_resource.Complete(svg_resource_content);

  // Flush tasks on the "internal loading" task queue. IsolatedSVGDocumentHost
  // will post/run the async-loading-complete callback on this task queue.
  base::RunLoop run_loop;
  GetDocument()
      .GetTaskRunner(TaskType::kInternalLoading)
      ->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // We don't expect the below to trigger a revalidation, but a new load, so
  // setup the subresource again.
  SimSubresourceRequest svg_resource_second_load(
      "https://example.com/resource.svg#root", "image/svg+xml",
      revalidate_params);

  // Make another request to the same resource.
  GetDocument().body()->firstElementChild()->cloneNode(true);

  svg_resource_second_load.Start();
  svg_resource_second_load.Complete(svg_resource_content);
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

  auto& cache = GetPage().GetSVGDocumentResourceTracker();

  // Both document contents should be in the cache.
  if (RuntimeEnabledFeatures::
          SvgPartitionSVGDocumentResourcesInMemoryCacheEnabled()) {
    EXPECT_TRUE(cache.HasContentForTesting(content1));
    EXPECT_TRUE(cache.HasContentForTesting(content2));
  } else {
    EXPECT_NE(cache.Get(SVGDocumentResourceTracker::MakeCacheKey(params1)),
              nullptr);
    EXPECT_NE(cache.Get(SVGDocumentResourceTracker::MakeCacheKey(params2)),
              nullptr);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  FastForwardUntilNoTasksRemain();

  // Only content2 (from params2) should be in the cache.
  if (RuntimeEnabledFeatures::
          SvgPartitionSVGDocumentResourcesInMemoryCacheEnabled()) {
    EXPECT_FALSE(cache.HasContentForTesting(content1));
    EXPECT_TRUE(cache.HasContentForTesting(content2));
  } else {
    EXPECT_EQ(cache.Get(SVGDocumentResourceTracker::MakeCacheKey(params1)),
              nullptr);
    EXPECT_NE(cache.Get(SVGDocumentResourceTracker::MakeCacheKey(params2)),
              nullptr);
  }

  content2->RemoveObserver(observer);

  ThreadState::Current()->CollectAllGarbageForTesting();

  FastForwardUntilNoTasksRemain();

  // Neither of the document contents should be in the cache.
  if (RuntimeEnabledFeatures::
          SvgPartitionSVGDocumentResourcesInMemoryCacheEnabled()) {
    EXPECT_FALSE(cache.HasContentForTesting(content1));
    EXPECT_FALSE(cache.HasContentForTesting(content2));
  } else {
    EXPECT_EQ(cache.Get(SVGDocumentResourceTracker::MakeCacheKey(params1)),
              nullptr);
    EXPECT_EQ(cache.Get(SVGDocumentResourceTracker::MakeCacheKey(params2)),
              nullptr);
  }
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
