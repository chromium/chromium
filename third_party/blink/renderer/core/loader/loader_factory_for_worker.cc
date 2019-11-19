// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/loader_factory_for_worker.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

void LoaderFactoryForWorker::Trace(Visitor* visitor) {
  visitor->Trace(global_scope_);
  LoaderFactory::Trace(visitor);
}

std::unique_ptr<WebURLLoader> LoaderFactoryForWorker::CreateURLLoader(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  WrappedResourceRequest wrapped(request);

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
  if (request.Url().ProtocolIs("blob") && !url_loader_factory) {
    global_scope_->GetPublicURLManager().Resolve(
        request.Url(), url_loader_factory.InitWithNewPipeAndPassReceiver());
  }

  if (url_loader_factory) {
    return web_context_->WrapURLLoaderFactory(url_loader_factory.PassPipe())
        ->CreateURLLoader(wrapped, CreateTaskRunnerHandle(task_runner));
  }

  // If |global_scope_| is a service worker, use |script_loader_factory_| for
  // the following request contexts.
  // - SERVICE_WORKER for a classic main script, a module main script, or a
  //   module imported script.
  // - SCRIPT for a classic imported script.
  //
  // Other workers (dedicated workers, shared workers, and worklets) don't have
  // a loader specific to script loading.
  if (global_scope_->IsServiceWorkerGlobalScope()) {
    if (request.GetRequestContext() ==
            mojom::RequestContextType::SERVICE_WORKER ||
        request.GetRequestContext() == mojom::RequestContextType::SCRIPT) {
      // GetScriptLoaderFactory() may return nullptr in tests even for service
      // workers.
      if (web_context_->GetScriptLoaderFactory()) {
        return web_context_->GetScriptLoaderFactory()->CreateURLLoader(
            wrapped, CreateTaskRunnerHandle(task_runner));
      }
    }
  } else {
    DCHECK(!web_context_->GetScriptLoaderFactory());
  }

  return web_context_->GetURLLoaderFactory()->CreateURLLoader(
      wrapped, CreateTaskRunnerHandle(task_runner));
}

std::unique_ptr<CodeCacheLoader>
LoaderFactoryForWorker::CreateCodeCacheLoader() {
  return web_context_->CreateCodeCacheLoader();
}

// TODO(altimin): This is used when creating a URLLoader, and
// ResourceFetcher::GetTaskRunner is used whenever asynchronous tasks around
// resource loading are posted. Modify the code so that all the tasks related to
// loading a resource use the resource loader handle's task runner.
std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
LoaderFactoryForWorker::CreateTaskRunnerHandle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return scheduler::WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
      std::move(task_runner));
}

}  // namespace blink
