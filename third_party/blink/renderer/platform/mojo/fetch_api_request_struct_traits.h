// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FETCH_API_REQUEST_STRUCT_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FETCH_API_REQUEST_STRUCT_TRAITS_H_

#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class KURL;
}

namespace mojo {

template <>
struct StructTraits<::blink::mojom::FetchAPIRequestDataView,
                    ::blink::WebServiceWorkerRequest> {
  static ::network::mojom::FetchRequestMode mode(
      const ::blink::WebServiceWorkerRequest& request) {
    return request.Mode();
  }

  static bool is_main_resource_load(
      const ::blink::WebServiceWorkerRequest& request) {
    return request.IsMainResourceLoad();
  }

  static ::blink::mojom::RequestContextType request_context_type(
      const ::blink::WebServiceWorkerRequest& request) {
    return request.GetRequestContext();
  }

  static ::network::mojom::RequestContextFrameType frame_type(
      const ::blink::WebServiceWorkerRequest& request) {
    return request.GetFrameType();
  }

  static ::blink::KURL url(const ::blink::WebServiceWorkerRequest&);

  static WTF::String method(const ::blink::WebServiceWorkerRequest&);

  static WTF::HashMap<WTF::String, WTF::String> headers(
      const ::blink::WebServiceWorkerRequest&);

  static scoped_refptr<::blink::BlobDataHandle> blob(
      const ::blink::WebServiceWorkerRequest&);

  static const ::blink::Referrer& referrer(
      const ::blink::WebServiceWorkerRequest&);

  static ::network::mojom::FetchCredentialsMode credentials_mode(
      const ::blink::WebServiceWorkerRequest& request) {
    return request.CredentialsMode();
  }

  static ::blink::mojom::FetchCacheMode cache_mode(
      const ::blink::WebServiceWorkerRequest& request) {
    return request.CacheMode();
  }

  static ::network::mojom::FetchRedirectMode redirect_mode(
      const ::blink::WebServiceWorkerRequest& request) {
    return request.RedirectMode();
  }

  static WTF::String integrity(const ::blink::WebServiceWorkerRequest&);
  static bool keepalive(const ::blink::WebServiceWorkerRequest& request) {
    return request.Keepalive();
  }
  static WTF::String client_id(const ::blink::WebServiceWorkerRequest&);

  static bool is_reload(const ::blink::WebServiceWorkerRequest& request) {
    return request.IsReload();
  }

  static bool is_history_navigation(
      const ::blink::WebServiceWorkerRequest& request) {
    return request.IsHistoryNavigation();
  }

  static bool Read(::blink::mojom::FetchAPIRequestDataView,
                   ::blink::WebServiceWorkerRequest* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FETCH_API_REQUEST_STRUCT_TRAITS_H_
