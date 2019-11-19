// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fetch/fetch_api_request_proto.h"

#include "content/common/fetch/fetch_api_request.pb.h"
#include "content/public/common/referrer.h"

namespace content {

std::string SerializeFetchRequestToString(
    const blink::mojom::FetchAPIRequest& request) {
  proto::internal::FetchAPIRequest request_proto;

  request_proto.set_url(request.url.spec());
  request_proto.set_method(request.method);
  request_proto.mutable_headers()->insert(request.headers.begin(),
                                          request.headers.end());
  request_proto.mutable_referrer()->set_url(request.referrer->url.spec());
  request_proto.mutable_referrer()->set_policy(
      static_cast<int>(request.referrer->policy));
  request_proto.set_is_reload(request.is_reload);
  request_proto.set_mode(static_cast<int>(request.mode));
  request_proto.set_is_main_resource_load(request.is_main_resource_load);
  request_proto.set_request_context_type(
      static_cast<int>(request.request_context_type));
  request_proto.set_credentials_mode(
      static_cast<int>(request.credentials_mode));
  request_proto.set_cache_mode(static_cast<int>(request.cache_mode));
  request_proto.set_redirect_mode(static_cast<int>(request.redirect_mode));
  if (request.integrity)
    request_proto.set_integrity(request.integrity.value());
  request_proto.set_keepalive(request.keepalive);
  request_proto.set_is_history_navigation(request.is_history_navigation);
  return request_proto.SerializeAsString();
}

blink::mojom::FetchAPIRequestPtr DeserializeFetchRequestFromString(
    const std::string& serialized) {
  proto::internal::FetchAPIRequest request_proto;
  if (!request_proto.ParseFromString(serialized)) {
    return blink::mojom::FetchAPIRequest::New();
  }

  auto request_ptr = blink::mojom::FetchAPIRequest::New();
  request_ptr->mode =
      static_cast<network::mojom::RequestMode>(request_proto.mode());
  request_ptr->is_main_resource_load = request_proto.is_main_resource_load();
  request_ptr->request_context_type =
      static_cast<blink::mojom::RequestContextType>(
          request_proto.request_context_type());
  request_ptr->frame_type = network::mojom::RequestContextFrameType::kNone;
  request_ptr->url = GURL(request_proto.url());
  request_ptr->method = request_proto.method();
  request_ptr->headers = {request_proto.headers().begin(),
                          request_proto.headers().end()};
  request_ptr->referrer = blink::mojom::Referrer::New(
      GURL(request_proto.referrer().url()),

      Referrer::ConvertToPolicy(request_proto.referrer().policy()));
  request_ptr->is_reload = request_proto.is_reload();
  request_ptr->credentials_mode = static_cast<network::mojom::CredentialsMode>(
      request_proto.credentials_mode());
  request_ptr->cache_mode =
      static_cast<blink::mojom::FetchCacheMode>(request_proto.cache_mode());
  request_ptr->redirect_mode =
      static_cast<network::mojom::RedirectMode>(request_proto.redirect_mode());
  if (request_proto.has_integrity())
    request_ptr->integrity = request_proto.integrity();
  request_ptr->keepalive = request_proto.keepalive();
  request_ptr->is_history_navigation = request_proto.is_history_navigation();
  return request_ptr;
}

}  // namespace content
