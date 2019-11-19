// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_NETWORK_URL_LOADER_FACTORY_H_
#define CONTENT_TEST_FAKE_NETWORK_URL_LOADER_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// A configurable URLLoaderFactory:
// 1. By default, it returns 200 OK with a simple body to any request:
//    If request path ends in ".js", body is:
//    "/*this body came from the network*/". Otherwise, body is:
//    "this body came from the network".
// 2. The default response can be overridden through the non-default
//    constructor.
// 3. Call SetResponse() to set specific response for a url.
//
// TODO(falken): Simplify/refactor this to be based on FakeNetwork as
// they currently share a lot of code. The idea is that tests that want to
// customize network activity should use URLLoaderInterceptor with FakeNetwork
// instead.
class FakeNetworkURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  // If this constructor is used:
  // A default response is used for any url request that is not configured
  // through SetResponse().
  FakeNetworkURLLoaderFactory();

  // If this constructor is used:
  // The provided response is used for any url request that is not configured
  // through SetResponse().
  FakeNetworkURLLoaderFactory(const std::string& headers,
                              const std::string& body,
                              bool network_accessed,
                              net::Error error_code);
  ~FakeNetworkURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  // Sets the response for a specific url. CreateLoaderAndStart() uses this
  // response instead of the default.
  void SetResponse(const GURL& url,
                   const std::string& headers,
                   const std::string& body,
                   bool network_accessed,
                   net::Error error_code);

 private:
  class ResponseInfo {
   public:
    ResponseInfo();
    ResponseInfo(ResponseInfo&& info);
    ResponseInfo(const std::string& headers,
                 const std::string& body,
                 bool network_accessed,
                 net::Error error_code);
    ~ResponseInfo();
    ResponseInfo& operator=(ResponseInfo&& info);

    GURL url;
    std::string headers;
    std::string body;
    bool network_accessed = true;
    net::Error error_code = net::OK;
  };

  // Returns the ResponseInfo for the |url|, it follows the order:
  // 1. Returns the matching entry in |response_info_map_| if exists.
  // 2. Returns |user_defined_default_response_info_| if it's set.
  // 3. Returns default response info (defined inside this method).
  const ResponseInfo& FindResponseInfo(const GURL& url) const;

  // This is user-defined default response info, it overrides the default
  // response info.
  std::unique_ptr<ResponseInfo> user_defined_default_response_info_;

  // User-defined URL => ResponseInfo map.
  base::flat_map<GURL, ResponseInfo> response_info_map_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  DISALLOW_COPY_AND_ASSIGN(FakeNetworkURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_NETWORK_URL_LOADER_FACTORY_H_
