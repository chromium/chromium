// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/fetch_api_request_struct_traits.h"

#include "mojo/public/cpp/bindings/map_traits_wtf_hash_map.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/serialized_blob_struct_traits.h"
#include "third_party/blink/renderer/platform/mojo/kurl_struct_traits.h"
#include "third_party/blink/renderer/platform/mojo/referrer_struct_traits.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"

namespace mojo {

// static
blink::KURL StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    url(const blink::WebServiceWorkerRequest& request) {
  return request.Url();
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    method(const blink::WebServiceWorkerRequest& request) {
  return request.Method();
}

// static
WTF::HashMap<WTF::String, WTF::String>
StructTraits<blink::mojom::FetchAPIRequestDataView,
             blink::WebServiceWorkerRequest>::
    headers(const blink::WebServiceWorkerRequest& request) {
  WTF::HashMap<WTF::String, WTF::String> header_map;
  for (const auto& pair : request.Headers())
    header_map.insert(pair.key, pair.value);
  return header_map;
}

// static
const blink::Referrer& StructTraits<blink::mojom::FetchAPIRequestDataView,
                                    blink::WebServiceWorkerRequest>::
    referrer(const blink::WebServiceWorkerRequest& request) {
  return request.GetReferrer();
}

// static
scoped_refptr<blink::BlobDataHandle> StructTraits<
    blink::mojom::FetchAPIRequestDataView,
    blink::WebServiceWorkerRequest>::blob(const blink::WebServiceWorkerRequest&
                                              request) {
  return request.GetBlobDataHandle();
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    integrity(const blink::WebServiceWorkerRequest& request) {
  return request.Integrity();
}

// static
WTF::String StructTraits<blink::mojom::FetchAPIRequestDataView,
                         blink::WebServiceWorkerRequest>::
    client_id(const blink::WebServiceWorkerRequest& request) {
  return request.ClientId();
}

// static
bool StructTraits<blink::mojom::FetchAPIRequestDataView,
                  blink::WebServiceWorkerRequest>::
    Read(blink::mojom::FetchAPIRequestDataView data,
         blink::WebServiceWorkerRequest* out) {
  network::mojom::FetchRequestMode mode;
  blink::mojom::RequestContextType requestContext;
  network::mojom::RequestContextFrameType frameType;
  blink::KURL url;
  WTF::String method;
  WTF::HashMap<WTF::String, WTF::String> headers;
  scoped_refptr<blink::BlobDataHandle> blob;
  blink::Referrer referrer;
  network::mojom::FetchCredentialsMode credentialsMode;
  network::mojom::FetchRedirectMode redirectMode;
  WTF::String integrity;
  WTF::String clientId;

  if (!data.ReadMode(&mode) || !data.ReadRequestContextType(&requestContext) ||
      !data.ReadFrameType(&frameType) || !data.ReadUrl(&url) ||
      !data.ReadMethod(&method) || !data.ReadHeaders(&headers) ||
      !data.ReadBlob(&blob) || !data.ReadReferrer(&referrer) ||
      !data.ReadCredentialsMode(&credentialsMode) ||
      !data.ReadRedirectMode(&redirectMode) || !data.ReadClientId(&clientId) ||
      !data.ReadIntegrity(&integrity)) {
    return false;
  }

  out->SetMode(mode);
  out->SetIsMainResourceLoad(data.is_main_resource_load());
  out->SetRequestContext(requestContext);
  out->SetFrameType(frameType);
  out->SetURL(url);
  out->SetMethod(method);
  for (const auto& pair : headers)
    out->SetHeader(pair.key, pair.value);
  out->SetBlobDataHandle(blob);
  out->SetReferrer(
      referrer.referrer,
      static_cast<network::mojom::ReferrerPolicy>(referrer.referrer_policy));
  out->SetCredentialsMode(credentialsMode);
  out->SetCacheMode(data.cache_mode());
  out->SetRedirectMode(redirectMode);
  out->SetIntegrity(integrity);
  out->SetKeepalive(data.keepalive());
  out->SetClientId(clientId);
  out->SetIsReload(data.is_reload());
  out->SetIsHistoryNavigation(data.is_history_navigation());
  return true;
}

}  // namespace mojo
