// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/code_cache_fetcher.h"

#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/isolated_code_cache_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/webui_bundled_code_cache_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {

namespace {

// Returns true if the code cache for `request` should be serviced from the
// webui bundled code cache.
bool ShouldFetchWebUIBundledCodeCache(const network::ResourceRequest& request) {
  return SchemeRegistry::SchemeSupportsWebUIBundledBytecode(
             String(request.url.GetScheme())) &&
         Platform::Current()->GetWebUIBundledCodeCacheResourceId(request.url);
}

bool ShouldFetchCodeCache(const network::ResourceRequest& request) {
  // Since code cache requests use a per-frame interface, don't fetch cached
  // code for keep-alive requests. These are only used for beaconing and we
  // don't expect code cache to help there.
  if (request.keepalive) {
    return false;
  }

  // Aside from http and https, the only other supported protocols are those
  // listed in the SchemeRegistry as requiring a content equality check. Do not
  // fetch cached code if opted-out by the embedder.
  bool should_use_source_hash =
      SchemeRegistry::SchemeSupportsCodeCacheWithHashing(
          String(request.url.GetScheme())) &&
      Platform::Current()->ShouldUseCodeCacheWithHashing(
          WebURL(KURL(request.url)));
  if (!request.url.SchemeIsHTTPOrHTTPS() && !should_use_source_hash) {
    return false;
  }

  // Supports script resource requests and shared storage worklet module
  // requests.
  // TODO(crbug.com/964467): Currently Chrome doesn't support code cache for
  // dedicated worker, shared worker, audio worklet and paint worklet. For
  // the service worker scripts, Blink receives the code cache via
  // URLLoaderClient::OnReceiveResponse() IPC.
  if (request.destination == network::mojom::RequestDestination::kScript ||
      request.destination ==
          network::mojom::RequestDestination::kSharedStorageWorklet) {
    return true;
  }

  // WebAssembly module request have RequestDestination::kEmpty. Note that
  // we always perform a code fetch for all of these requests because:
  //
  // * It is not easy to distinguish WebAssembly modules from other kEmpty
  //   requests
  // * The fetch might be handled by Service Workers, but we can't still know
  //   if the response comes from the CacheStorage (in such cases its own
  //   code cache will be used) or not.
  //
  // These fetches should be cheap, however, requiring one additional IPC and
  // no browser process disk IO since the cache index is in memory and the
  // resource key should not be present.
  //
  // The only case where it's easy to skip a kEmpty request is when a content
  // equality check is required, because only ScriptResource supports that
  // requirement.
  if (request.destination == network::mojom::RequestDestination::kEmpty) {
    return true;
  }
  return false;
}

mojom::blink::CodeCacheType GetCodeCacheType(
    network::mojom::RequestDestination destination) {
  if (destination == network::mojom::RequestDestination::kEmpty) {
    // For requests initiated by the fetch function, we use code cache for
    // WASM compiled code.
    return mojom::blink::CodeCacheType::kWebAssembly;
  } else {
    // Otherwise, we use code cache for scripting.
    return mojom::blink::CodeCacheType::kJavascript;
  }
}

}  // namespace

// static
scoped_refptr<CodeCacheFetcher> CodeCacheFetcher::TryCreateAndStart(
    const network::ResourceRequest& request,
    CodeCacheHost* code_cache_host,
    scoped_refptr<base::SequencedTaskRunner> host_task_runner,
    base::OnceClosure done_closure) {
  scoped_refptr<CodeCacheFetcher> fetcher;
  if (ShouldFetchWebUIBundledCodeCache(request)) {
    fetcher = base::MakeRefCounted<WebUIBundledCodeCacheFetcher>(
        std::move(host_task_runner),
        Platform::Current()
            ->GetWebUIBundledCodeCacheResourceId(request.url)
            .value(),
        std::move(done_closure));
    fetcher->Start();
  } else if (code_cache_host && ShouldFetchCodeCache(request)) {
    fetcher = base::MakeRefCounted<IsolatedCodeCacheFetcher>(
        *code_cache_host, GetCodeCacheType(request.destination),
        KURL(request.url), std::move(done_closure));
    fetcher->Start();
  }
  return fetcher;
}

}  // namespace blink
