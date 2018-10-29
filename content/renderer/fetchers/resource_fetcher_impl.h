// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_
#define CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/renderer/resource_fetcher.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/web_url_request.h"

class GURL;

namespace blink {
class WebLocalFrame;
}

namespace content {

class ResourceFetcherImpl : public ResourceFetcher {
 public:
  // ResourceFetcher implementation:
  void SetMethod(const std::string& method) override;
  void SetBody(const std::string& body) override;
  void SetHeader(const std::string& header, const std::string& value) override;
  void Start(blink::WebLocalFrame* frame,
             blink::mojom::RequestContextType request_context,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const net::NetworkTrafficAnnotationTag& annotation_tag,
             Callback callback,
             size_t maximum_download_size) override;
  void SetTimeout(const base::TimeDelta& timeout) override;

 private:
  friend class ResourceFetcher;

  class ClientImpl;

  explicit ResourceFetcherImpl(const GURL& url);

  ~ResourceFetcherImpl() override;

  void OnLoadComplete();
  void OnTimeout();

  std::unique_ptr<ClientImpl> client_;

  // Request to send.
  network::ResourceRequest request_;

  // Limit how long to wait for the server.
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_RESOURCE_FETCHER_IMPL_H_
