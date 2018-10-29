// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_RESPONSE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_RESPONSE_H_

#include "base/time/time.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-shared.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/wtf/forward.h"   // nogncheck
#include "third_party/blink/renderer/platform/wtf/hash_map.h"  // nogncheck
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"  // nogncheck
#endif

namespace blink {

class BlobDataHandle;
class HTTPHeaderMap;
class WebHTTPHeaderVisitor;
class WebServiceWorkerResponsePrivate;

// Represents a response to a fetch operation. ServiceWorker uses this to
// respond to a FetchEvent dispatched by the browser. The plan is for the Cache
// and fetch() API to also use it.
class BLINK_PLATFORM_EXPORT WebServiceWorkerResponse {
 public:
  ~WebServiceWorkerResponse() { Reset(); }
  WebServiceWorkerResponse();
  WebServiceWorkerResponse(const WebServiceWorkerResponse& other) {
    Assign(other);
  }
  WebServiceWorkerResponse& operator=(const WebServiceWorkerResponse& other) {
    Assign(other);
    return *this;
  }

  void Reset();
  void Assign(const WebServiceWorkerResponse&);

  void SetURLList(const WebVector<WebURL>&);
  const WebVector<WebURL>& UrlList() const;

  void SetStatus(unsigned short);
  unsigned short Status() const;

  void SetStatusText(const WebString&);
  const WebString& StatusText() const;

  void SetResponseType(network::mojom::FetchResponseType);
  network::mojom::FetchResponseType ResponseType() const;

  void SetHeader(const WebString& key, const WebString& value);

  // If the key already exists, appends the value to the same key (comma
  // delimited) else creates a new entry.
  void AppendHeader(const WebString& key, const WebString& value);

  WebVector<WebString> GetHeaderKeys() const;
  WebString GetHeader(const WebString& key) const;
  void VisitHTTPHeaderFields(WebHTTPHeaderVisitor*) const;

  void SetBlob(const WebString& uuid,
               uint64_t size,
               mojo::ScopedMessagePipeHandle);
  WebString BlobUUID() const;
  uint64_t BlobSize() const;

  mojo::ScopedMessagePipeHandle CloneBlobPtr() const;

  WebString SideDataBlobUUID() const;
  uint64_t SideDataBlobSize() const;
  mojo::ScopedMessagePipeHandle CloneSideDataBlobPtr() const;

  // Provides a more detailed error when status() is zero.
  void SetError(mojom::ServiceWorkerResponseError);
  mojom::ServiceWorkerResponseError GetError() const;

  void SetResponseTime(base::Time);
  base::Time ResponseTime() const;

  void SetCacheStorageCacheName(const WebString&);
  const WebString& CacheStorageCacheName() const;

  void SetCorsExposedHeaderNames(const WebVector<WebString>&);
  const WebVector<WebString>& CorsExposedHeaderNames() const;

#if INSIDE_BLINK
  const HTTPHeaderMap& Headers() const;

  void SetBlobDataHandle(scoped_refptr<BlobDataHandle>);
  scoped_refptr<BlobDataHandle> GetBlobDataHandle() const;

  void SetSideDataBlobDataHandle(scoped_refptr<BlobDataHandle>);
#endif

 private:
  WebPrivatePtr<WebServiceWorkerResponsePrivate> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_RESPONSE_H_
