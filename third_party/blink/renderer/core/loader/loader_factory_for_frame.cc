// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/loader_factory_for_frame.h"

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/prefetched_signed_exchange_manager.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

Vector<String>& CorsExemptHeaderList() {
  DEFINE_STATIC_LOCAL(ThreadSpecific<Vector<String>>, cors_exempt_header_list,
                      ());
  return *cors_exempt_header_list;
}

}  // namespace

// static
void LoaderFactoryForFrame::SetCorsExemptHeaderList(
    Vector<String> cors_exempt_header_list) {
  CorsExemptHeaderList() = std::move(cors_exempt_header_list);
}
// static
Vector<String> LoaderFactoryForFrame::GetCorsExemptHeaderList() {
  return CorsExemptHeaderList();
}

LoaderFactoryForFrame::LoaderFactoryForFrame(DocumentLoader& document_loader,
                                             LocalDOMWindow& window)
    : document_loader_(document_loader),
      window_(window),
      prefetched_signed_exchange_manager_(
          document_loader.GetPrefetchedSignedExchangeManager()),
      keep_alive_handle_factory_(&window) {
  window.GetFrame()->GetLocalFrameHostRemote().GetKeepAliveHandleFactory(
      keep_alive_handle_factory_.BindNewPipeAndPassReceiver(
          window.GetTaskRunner(TaskType::kNetworking)));
}

void LoaderFactoryForFrame::Trace(Visitor* visitor) const {
  visitor->Trace(document_loader_);
  visitor->Trace(window_);
  visitor->Trace(prefetched_signed_exchange_manager_);
  visitor->Trace(keep_alive_handle_factory_);
  LoaderFactory::Trace(visitor);
}

std::unique_ptr<URLLoader> LoaderFactoryForFrame::CreateURLLoader(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper) {
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
      request.GetRequestContext() !=
          mojom::blink::RequestContextType::SHARED_WORKER) {
    window_->GetPublicURLManager().Resolve(
        request.Url(), url_loader_factory.InitWithNewPipeAndPassReceiver());
  }
  LocalFrame* frame = window_->GetFrame();
  DCHECK(frame);
  FrameScheduler* frame_scheduler = frame->GetFrameScheduler();
  DCHECK(frame_scheduler);

  if (url_loader_factory) {
    return std::make_unique<URLLoaderFactory>(
               base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
                   CrossVariantMojoRemote<
                       network::mojom::URLLoaderFactoryInterfaceBase>(
                       std::move(url_loader_factory))),
               GetCorsExemptHeaderList(),
               /*terminate_sync_load_event=*/nullptr)
        ->CreateURLLoader(webreq, freezable_task_runner,
                          unfreezable_task_runner,
                          /*keep_alive_handle=*/mojo::NullRemote(),
                          back_forward_cache_loader_helper);
  }

  if (document_loader_->GetServiceWorkerNetworkProvider()) {
    mojo::PendingRemote<mojom::blink::KeepAliveHandle> pending_remote;
    mojo::PendingReceiver<mojom::blink::KeepAliveHandle> pending_receiver =
        pending_remote.InitWithNewPipeAndPassReceiver();
    auto loader_factory = document_loader_->GetServiceWorkerNetworkProvider()
                              ->GetSubresourceLoaderFactory(webreq);
    if (loader_factory) {
      IssueKeepAliveHandleIfRequested(request, frame->GetLocalFrameHostRemote(),
                                      std::move(pending_receiver));
      return std::make_unique<URLLoaderFactory>(
                 std::move(loader_factory), GetCorsExemptHeaderList(),
                 /*terminate_sync_load_event=*/nullptr)
          ->CreateURLLoader(webreq, freezable_task_runner,
                            unfreezable_task_runner, std::move(pending_remote),
                            back_forward_cache_loader_helper);
    }
  }

  if (prefetched_signed_exchange_manager_) {
    auto loader =
        prefetched_signed_exchange_manager_->MaybeCreateURLLoader(webreq);
    if (loader)
      return loader;
  }

  mojo::PendingRemote<mojom::blink::KeepAliveHandle> pending_remote;
  IssueKeepAliveHandleIfRequested(
      request, frame->GetLocalFrameHostRemote(),
      pending_remote.InitWithNewPipeAndPassReceiver());

  auto loader = frame->CreateURLLoaderForTesting();
  if (loader) {
    return loader;
  }

  return std::make_unique<URLLoaderFactory>(
             frame->GetURLLoaderFactory(), GetCorsExemptHeaderList(),
             /*terminate_sync_load_event=*/nullptr)
      ->CreateURLLoader(webreq, freezable_task_runner, unfreezable_task_runner,
                        std::move(pending_remote),
                        back_forward_cache_loader_helper);
}

std::unique_ptr<WebCodeCacheLoader>
LoaderFactoryForFrame::CreateCodeCacheLoader() {
  if (document_loader_->GetCodeCacheHost() == nullptr) {
    return nullptr;
  }
  return blink::WebCodeCacheLoader::Create(
      document_loader_->GetCodeCacheHost());
}

void LoaderFactoryForFrame::IssueKeepAliveHandleIfRequested(
    const ResourceRequest& request,
    mojom::blink::LocalFrameHost& local_frame_host,
    mojo::PendingReceiver<mojom::blink::KeepAliveHandle> pending_receiver) {
  DCHECK(pending_receiver);
  if (request.GetKeepalive() && keep_alive_handle_factory_.is_bound()) {
    keep_alive_handle_factory_->IssueKeepAliveHandle(
        std::move(pending_receiver));
  }

  if (!keep_alive_handle_factory_.is_bound()) {
    // TODO(crbug.com/1188074): Remove this CHECK once the investigation is
    // done.
    CHECK(window_->IsContextDestroyed());
  }
}

}  // namespace blink
