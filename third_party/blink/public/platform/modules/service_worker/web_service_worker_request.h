// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_REQUEST_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_REQUEST_H_

#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-shared.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

#if INSIDE_BLINK
#include <utility>
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"  // nogncheck
#include "third_party/blink/renderer/platform/network/http_header_map.h"  // nogncheck
#include "third_party/blink/renderer/platform/weborigin/referrer.h"  // nogncheck
#include "third_party/blink/renderer/platform/wtf/forward.h"  // nogncheck
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"  // nogncheck
#endif

namespace blink {

class BlobDataHandle;
class WebHTTPHeaderVisitor;
class WebServiceWorkerRequestPrivate;

// Represents a request for a web resource.
class BLINK_PLATFORM_EXPORT WebServiceWorkerRequest {
 public:
  ~WebServiceWorkerRequest() { Reset(); }
  WebServiceWorkerRequest();
  WebServiceWorkerRequest(const WebServiceWorkerRequest& other) {
    Assign(other);
  }
  WebServiceWorkerRequest& operator=(const WebServiceWorkerRequest& other) {
    Assign(other);
    return *this;
  }

  void Reset();
  void Assign(const WebServiceWorkerRequest&);

  void SetURL(const WebURL&);
  const WebURL& Url() const;

  void SetMethod(const WebString&);
  const WebString& Method() const;

  void SetHeader(const WebString& key, const WebString& value);

  // If the key already exists, the value is appended to the existing value
  // with a comma delimiter between them.
  void AppendHeader(const WebString& key, const WebString& value);

  void VisitHTTPHeaderFields(WebHTTPHeaderVisitor*) const;

  // There are two ways of representing body: WebHTTPBody or Blob.  Only one
  // should be used.
  void SetBody(const WebHTTPBody&);
  WebHTTPBody Body() const;
  void SetBlob(const WebString& uuid,
               long long size,
               mojo::ScopedMessagePipeHandle);

  void SetReferrer(const WebString&, network::mojom::ReferrerPolicy);
  WebURL ReferrerUrl() const;
  network::mojom::ReferrerPolicy GetReferrerPolicy() const;

  void SetMode(network::mojom::FetchRequestMode);
  network::mojom::FetchRequestMode Mode() const;

  void SetIsMainResourceLoad(bool);
  bool IsMainResourceLoad() const;

  void SetCredentialsMode(network::mojom::FetchCredentialsMode);
  network::mojom::FetchCredentialsMode CredentialsMode() const;

  void SetIntegrity(const WebString&);
  const WebString& Integrity() const;

  void SetPriority(WebURLRequest::Priority);
  WebURLRequest::Priority Priority() const;

  void SetCacheMode(mojom::FetchCacheMode);
  mojom::FetchCacheMode CacheMode() const;

  void SetKeepalive(bool);
  bool Keepalive() const;

  void SetRedirectMode(network::mojom::FetchRedirectMode);
  network::mojom::FetchRedirectMode RedirectMode() const;

  void SetRequestContext(mojom::RequestContextType);
  mojom::RequestContextType GetRequestContext() const;

  void SetFrameType(network::mojom::RequestContextFrameType);
  network::mojom::RequestContextFrameType GetFrameType() const;

  void SetClientId(const WebString&);
  const WebString& ClientId() const;

  void SetIsReload(bool);
  bool IsReload() const;

  void SetIsHistoryNavigation(bool);
  bool IsHistoryNavigation() const;

#if INSIDE_BLINK
  const HTTPHeaderMap& Headers() const;
  void SetBlobDataHandle(scoped_refptr<BlobDataHandle>);
  scoped_refptr<BlobDataHandle> GetBlobDataHandle() const;
  const Referrer& GetReferrer() const;
  void SetBlob(const WebString& uuid,
               long long size,
               mojom::blink::BlobPtrInfo);
#endif

 private:
  WebPrivatePtr<WebServiceWorkerRequestPrivate> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_REQUEST_H_
