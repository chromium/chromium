// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_CONTAINER_H_

#include <memory>

#include "base/types/pass_key.h"
#include "content/browser/preloading/prefetch/pre_prefetch_service_impl.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace content {

// A class that represents a PrePrefetch request.
// This receives prefetch requests, appropriate `URLLoaderFactory` from
// `PrePrefetchServiceImpl`, and pre-calculated headers associated with the
// request, in order to handle the actual pre-prefetching. This holds
// PrePrefetch's pending URLLoader remote and client receiver for its
// consumption.
//
// Thread/lifetime model:
// - `PrePrefetchContainer` is always created via static function
//   `CreateAndStart()`, which combines ctor and `Start()`. `Start()` is always
//   called from `PrePrefetchServiceCore` sequence owned by
//   `PrePrefetchServiceImpl`.
// - `PrePrefetchContainer` can be passed and destructed on any thread, as all
//   `PrePrefetchContainer` members hold this.
// - After being created, `PrePrefetchContainer` should be owned by
//   `PrePrefetchHandle`, and shouldn't be accessed except for 1) dtor of
//   `PrePrefetchHandle` or 2) PrePrefetch consumption happening on the UI
//   thread, which both terminates the lifetime of `this`.
// TODO(crbug.com/452389538): Implement PrePrefetch consumption interface.
class CONTENT_EXPORT PrePrefetchContainer final {
 public:
  static std::unique_ptr<PrePrefetchContainer> CreateAndStart(
      base::PassKey<PrePrefetchServiceImpl>,
      std::unique_ptr<const PrefetchRequest> prefetch_request,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers,
      const std::vector<PrePrefetchUpdateHeadersCallback>&
          non_ui_thread_update_headers_callbacks);
  static std::unique_ptr<PrePrefetchContainer> CreateAndStartForTesting(
      std::unique_ptr<const PrefetchRequest> prefetch_request,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers,
      const std::vector<PrePrefetchUpdateHeadersCallback>&
          non_ui_thread_update_headers_callbacks);

  // Called only from `CreateAndStartInternal()`.
  PrePrefetchContainer(base::PassKey<PrePrefetchContainer>,
                       std::unique_ptr<const PrefetchRequest> prefetch_request);
  ~PrePrefetchContainer();

  PrePrefetchContainer(const PrePrefetchContainer&) = delete;
  PrePrefetchContainer& operator=(const PrePrefetchContainer&) = delete;

  // Takes associated resources upon `PrePrefetchContainer`'s consumption
  // handled in UI thread `PrefetchService`.
  std::unique_ptr<const PrefetchRequest> TakePrefetchRequestOnUI();
  std::unique_ptr<network::ResourceRequest> TakeResourceRequestOnUI();
  mojo::PendingRemote<network::mojom::URLLoader> TakePendingURLLoaderOnUI();
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
  TakePendingURLLoaderClientReceiverOnUI();

 private:
  // Called only from `CreateAndStart()` or `CreateAndStartForTesting()`.
  static std::unique_ptr<PrePrefetchContainer> CreateAndStartInternal(
      std::unique_ptr<const PrefetchRequest> prefetch_request,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers,
      const std::vector<PrePrefetchUpdateHeadersCallback>&
          non_ui_thread_update_headers_callbacks);

  void Start(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers,
      const std::vector<PrePrefetchUpdateHeadersCallback>&
          non_ui_thread_update_headers_callbacks);

  // `PrefetchRequest` associated to `this`.
  std::unique_ptr<const PrefetchRequest> prefetch_request_;

  // `ResourceRequest` used for PrePrefetch's network request, which should be
  // moved to `PrefetchContainer` when the consumption of `this` happens.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // Pending `URLLoader` remote and `URLLoaderClient` receiver for PrePrefetch's
  // network request, which should be moved to `PrefetchContainer` when the
  // consumption of `this` happens.
  mojo::PendingRemote<network::mojom::URLLoader> url_loader_;
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
      url_loader_client_receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_CONTAINER_H_
