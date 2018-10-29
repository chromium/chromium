// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_ASSOCIATED_RESOURCE_FETCHER_IMPL_H_
#define CONTENT_RENDERER_FETCHERS_ASSOCIATED_RESOURCE_FETCHER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/public/renderer/associated_resource_fetcher.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"

class GURL;

namespace blink {
class WebAssociatedURLLoader;
class WebLocalFrame;
}

namespace content {

class AssociatedResourceFetcherImpl : public AssociatedResourceFetcher {
 public:
  // AssociatedResourceFetcher implementation:
  void SetSkipServiceWorker(bool skip_service_worker) override;
  void SetCacheMode(blink::mojom::FetchCacheMode mode) override;
  void SetLoaderOptions(
      const blink::WebAssociatedURLLoaderOptions& options) override;
  void Start(blink::WebLocalFrame* frame,
             blink::mojom::RequestContextType request_context,
             network::mojom::FetchRequestMode request_mode,
             network::mojom::FetchCredentialsMode fetch_credentials_mode,
             network::mojom::RequestContextFrameType frame_type,
             const Callback& callback) override;

 private:
  friend class AssociatedResourceFetcher;

  class ClientImpl;

  explicit AssociatedResourceFetcherImpl(const GURL& url);

  ~AssociatedResourceFetcherImpl() override;

  void Cancel() override;

  std::unique_ptr<blink::WebAssociatedURLLoader> loader_;
  std::unique_ptr<ClientImpl> client_;

  // Options to send to the loader.
  blink::WebAssociatedURLLoaderOptions options_;

  // Request to send.
  blink::WebURLRequest request_;

  // Limit how long to wait for the server.
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedResourceFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_ASSOCIATED_RESOURCE_FETCHER_IMPL_H_
