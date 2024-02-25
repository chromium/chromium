// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/loader_factory_for_worker.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle_factory.mojom-blink.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"

namespace blink {

void LoaderFactoryForWorker::Trace(Visitor* visitor) const {
  visitor->Trace(global_scope_);
  LoaderFactory::Trace(visitor);
}

LoaderFactoryForWorker::LoaderFactoryForWorker(
    WorkerOrWorkletGlobalScope& global_scope,
    scoped_refptr<WebWorkerFetchContext> web_context)
    : global_scope_(global_scope), web_context_(std::move(web_context)) {}

std::unique_ptr<URLLoader> LoaderFactoryForWorker::CreateURLLoader(
    const network::ResourceRequest& network_request,
    const ResourceLoaderOptions& options,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
    const std::optional<base::UnguessableToken>&
        service_worker_race_network_request_token,
    bool is_from_origin_dirty_style_sheet) {
  Vector<std::unique_ptr<URLLoaderThrottle>> throttles;
  WebVector<std::unique_ptr<URLLoaderThrottle>> web_throttles =
      web_context_->CreateThrottles(network_request);
  throttles.reserve(base::checked_cast<wtf_size_t>(web_throttles.size()));
  for (auto& throttle : web_throttles) {
    throttles.push_back(std::move(throttle));
  }

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
  if (network_request.url.SchemeIs("blob") && !url_loader_factory) {
    global_scope_->GetPublicURLManager().Resolve(
        KURL(network_request.url),
        url_loader_factory.InitWithNewPipeAndPassReceiver());
  }

  // KeepAlive is not yet supported in web workers.
  mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle =
      mojo::NullRemote();

  if (url_loader_factory) {
    return web_context_->WrapURLLoaderFactory(std::move(url_loader_factory))
        ->CreateURLLoader(network_request, freezable_task_runner,
                          unfreezable_task_runner, std::move(keep_alive_handle),
                          back_forward_cache_loader_helper,
                          std::move(throttles));
  }

  // If |global_scope_| is a service worker, use |script_loader_factory_| for
  // the following request contexts.
  // - kServiceWorker for a classic main script, a module main script, or a
  //   module imported script.
  // - kScript for a classic imported script.
  //
  // Other workers (dedicated workers, shared workers, and worklets) don't have
  // a loader specific to script loading.
  if (global_scope_->IsServiceWorkerGlobalScope()) {
    if (network_request.destination ==
            network::mojom::RequestDestination::kServiceWorker ||
        network_request.destination ==
            network::mojom::RequestDestination::kScript) {
      // GetScriptLoaderFactory() may return nullptr in tests even for service
      // workers.
      if (web_context_->GetScriptLoaderFactory()) {
        return web_context_->GetScriptLoaderFactory()->CreateURLLoader(
            network_request, freezable_task_runner, unfreezable_task_runner,
            std::move(keep_alive_handle), back_forward_cache_loader_helper,
            std::move(throttles));
      }
    }
    // URLLoader for RaceNetworkRequest
    if (service_worker_race_network_request_token.has_value()) {
      auto token = service_worker_race_network_request_token.value();
      std::optional<
          mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>>
          race_network_request_url_loader_factory =
              global_scope_->FindRaceNetworkRequestURLLoaderFactory(token);
      if (race_network_request_url_loader_factory) {
        // DumpWithoutCrashing if the corresponding URLLoaderFactory is found
        // and the request URL protocol is not in the HTTP family. The
        // URLLoaderFactory should be found only when the request is HTTP or
        // HTTPS. The crash seems to be caused by extension resources.
        // TODO(crbug.com/1492640) Remove DumpWithoutCrashing once we collect
        // data and identify the cause.
        static bool has_dumped_without_crashing = false;
        if (!has_dumped_without_crashing &&
            !network_request.url.SchemeIsHTTPOrHTTPS()) {
          has_dumped_without_crashing = true;
          SCOPED_CRASH_KEY_BOOL(
              "SWRace", "loader_factory_has_value",
              race_network_request_url_loader_factory.has_value());
          SCOPED_CRASH_KEY_BOOL(
              "SWRace", "is_valid_loader_factory",
              race_network_request_url_loader_factory->is_valid());
          SCOPED_CRASH_KEY_BOOL("SWRace", "is_empty_token", token.is_empty());
          SCOPED_CRASH_KEY_STRING64("SWRace", "token", token.ToString());
          SCOPED_CRASH_KEY_STRING256("SWRace", "request_url",
                                     network_request.url.spec());
          base::debug::DumpWithoutCrashing();
        }

        return web_context_
            ->WrapURLLoaderFactory(
                std::move(race_network_request_url_loader_factory.value()))
            ->CreateURLLoader(
                network_request, freezable_task_runner, unfreezable_task_runner,
                std::move(keep_alive_handle), back_forward_cache_loader_helper,
                std::move(throttles));
      }
    }
  } else {
    CHECK(!web_context_->GetScriptLoaderFactory());
  }

  return web_context_->GetURLLoaderFactory()->CreateURLLoader(
      network_request, freezable_task_runner, unfreezable_task_runner,
      std::move(keep_alive_handle), back_forward_cache_loader_helper,
      std::move(throttles));
}

CodeCacheHost* LoaderFactoryForWorker::GetCodeCacheHost() {
  return global_scope_->GetCodeCacheHost();
}

}  // namespace blink
