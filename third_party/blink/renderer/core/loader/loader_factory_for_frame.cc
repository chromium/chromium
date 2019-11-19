// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/loader_factory_for_frame.h"

#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"
#include "third_party/blink/renderer/core/loader/prefetched_signed_exchange_manager.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

LoaderFactoryForFrame::LoaderFactoryForFrame(
    const FrameOrImportedDocument& frame_or_imported_document)
    : frame_or_imported_document_(frame_or_imported_document),
      prefetched_signed_exchange_manager_(
          frame_or_imported_document_->GetDocumentLoader()
              ? frame_or_imported_document_->GetDocumentLoader()
                    ->GetPrefetchedSignedExchangeManager()
              : nullptr) {}

void LoaderFactoryForFrame::Trace(Visitor* visitor) {
  visitor->Trace(frame_or_imported_document_);
  visitor->Trace(prefetched_signed_exchange_manager_);
  LoaderFactory::Trace(visitor);
}

std::unique_ptr<WebURLLoader> LoaderFactoryForFrame::CreateURLLoader(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  WrappedResourceRequest webreq(request);

  mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
      url_loader_factory;
  if (options.url_loader_factory) {
    mojo::Remote<network::mojom::blink::URLLoaderFactory>
        url_loader_factory_remote(std::move(options.url_loader_factory->data));
    url_loader_factory_remote->Clone(
        url_loader_factory.InitWithNewPipeAndPassReceiver());
  }
  // Resolve any blob: URLs that haven't been resolved yet. The XHR and
  // fetch() API implementations resolve blob URLs earlier because there can
  // be arbitrarily long delays between creating requests with those APIs and
  // actually creating the URL loader here. Other subresource loading will
  // immediately create the URL loader so resolving those blob URLs here is
  // simplest.
  // Don't resolve the URL again if this is a shared worker request though, as
  // in that case the browser process will have already done so and the code
  // here should just go through the normal non-blob specific code path (note
  // that this is only strictly true if NetworkService/S13nSW is enabled, but
  // if that isn't the case we're going to run into race conditions resolving
  // the blob URL anyway so it doesn't matter if the blob URL gets resolved
  // here or later in the browser process, so skipping blob URL resolution
  // here for all shared worker loads is okay even with NetworkService/S13nSW
  // disabled).
  // TODO(mek): Move the RequestContext check to the worker side's relevant
  // callsite when we make Shared Worker loading off-main-thread.
  if (request.Url().ProtocolIs("blob") && !url_loader_factory &&
      request.GetRequestContext() != mojom::RequestContextType::SHARED_WORKER) {
    frame_or_imported_document_->GetDocument().GetPublicURLManager().Resolve(
        request.Url(), url_loader_factory.InitWithNewPipeAndPassReceiver());
  }
  LocalFrame& frame = frame_or_imported_document_->GetFrame();
  FrameScheduler* frame_scheduler = frame.GetFrameScheduler();
  DCHECK(frame_scheduler);

  // TODO(altimin): frame_scheduler->CreateResourceLoadingTaskRunnerHandle is
  // used when creating a URLLoader, and ResourceFetcher::GetTaskRunner is
  // used whenever asynchronous tasks around resource loading are posted. Modify
  // the code so that all the tasks related to loading a resource use the
  // resource loader handle's task runner.
  if (url_loader_factory) {
    return Platform::Current()
        ->WrapURLLoaderFactory(url_loader_factory.PassPipe())
        ->CreateURLLoader(
            webreq, frame_scheduler->CreateResourceLoadingTaskRunnerHandle());
  }

  DocumentLoader& document_loader =
      frame_or_imported_document_->GetMasterDocumentLoader();
  if (document_loader.GetServiceWorkerNetworkProvider()) {
    auto loader =
        document_loader.GetServiceWorkerNetworkProvider()->CreateURLLoader(
            webreq, frame_scheduler->CreateResourceLoadingTaskRunnerHandle());
    if (loader)
      return loader;
  }

  if (prefetched_signed_exchange_manager_) {
    auto loader =
        prefetched_signed_exchange_manager_->MaybeCreateURLLoader(webreq);
    if (loader)
      return loader;
  }
  return frame.GetURLLoaderFactory()->CreateURLLoader(
      webreq, frame_scheduler->CreateResourceLoadingTaskRunnerHandle());
}

std::unique_ptr<CodeCacheLoader>
LoaderFactoryForFrame::CreateCodeCacheLoader() {
  return Platform::Current()->CreateCodeCacheLoader();
}

}  // namespace blink
