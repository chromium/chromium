// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_REQUEST_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_REQUEST_DATA_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class FetchHeaderList;
class SecurityOrigin;
class ScriptState;
class WebServiceWorkerRequest;

class FetchRequestData final
    : public GarbageCollectedFinalized<FetchRequestData> {
 public:
  enum Tainting { kBasicTainting, kCORSTainting, kOpaqueTainting };

  static FetchRequestData* Create();
  static FetchRequestData* Create(ScriptState*, const WebServiceWorkerRequest&);
  // Call Request::refreshBody() after calling clone() or pass().
  FetchRequestData* Clone(ScriptState*, ExceptionState&);
  FetchRequestData* Pass(ScriptState*, ExceptionState&);
  ~FetchRequestData();

  void SetMethod(AtomicString method) { method_ = method; }
  const AtomicString& Method() const { return method_; }
  void SetURL(const KURL& url) { url_ = url; }
  const KURL& Url() const { return url_; }
  mojom::RequestContextType Context() const { return context_; }
  void SetContext(mojom::RequestContextType context) { context_ = context; }
  scoped_refptr<const SecurityOrigin> Origin() { return origin_; }
  void SetOrigin(scoped_refptr<const SecurityOrigin> origin) {
    origin_ = std::move(origin);
  }
  bool SameOriginDataURLFlag() { return same_origin_data_url_flag_; }
  void SetSameOriginDataURLFlag(bool flag) {
    same_origin_data_url_flag_ = flag;
  }
  const AtomicString& ReferrerString() const { return referrer_string_; }
  void SetReferrerString(const AtomicString& s) { referrer_string_ = s; }
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }
  void SetReferrerPolicy(ReferrerPolicy p) { referrer_policy_ = p; }
  void SetMode(network::mojom::FetchRequestMode mode) { mode_ = mode; }
  network::mojom::FetchRequestMode Mode() const { return mode_; }
  void SetCredentials(network::mojom::FetchCredentialsMode credentials) {
    credentials_ = credentials;
  }
  network::mojom::FetchCredentialsMode Credentials() const {
    return credentials_;
  }
  void SetCacheMode(mojom::FetchCacheMode cache_mode) {
    cache_mode_ = cache_mode;
  }
  mojom::FetchCacheMode CacheMode() const { return cache_mode_; }
  void SetRedirect(network::mojom::FetchRedirectMode redirect) {
    redirect_ = redirect;
  }
  network::mojom::FetchRedirectMode Redirect() const { return redirect_; }
  void SetImportance(mojom::FetchImportanceMode importance) {
    importance_ = importance;
  }
  mojom::FetchImportanceMode Importance() const { return importance_; }
  void SetResponseTainting(Tainting tainting) { response_tainting_ = tainting; }
  Tainting ResponseTainting() const { return response_tainting_; }
  FetchHeaderList* HeaderList() const { return header_list_.Get(); }
  void SetHeaderList(FetchHeaderList* header_list) {
    header_list_ = header_list;
  }
  BodyStreamBuffer* Buffer() const { return buffer_; }
  // Call Request::refreshBody() after calling setBuffer().
  void SetBuffer(BodyStreamBuffer* buffer) { buffer_ = buffer; }
  String MimeType() const { return mime_type_; }
  void SetMIMEType(const String& type) { mime_type_ = type; }
  String Integrity() const { return integrity_; }
  void SetIntegrity(const String& integrity) { integrity_ = integrity; }
  ResourceLoadPriority Priority() const { return priority_; }
  void SetPriority(ResourceLoadPriority priority) { priority_ = priority; }
  bool Keepalive() const { return keepalive_; }
  void SetKeepalive(bool b) { keepalive_ = b; }
  bool IsHistoryNavigation() const { return is_history_navigation_; }
  void SetIsHistoryNavigation(bool b) { is_history_navigation_ = b; }

  network::mojom::blink::URLLoaderFactory* URLLoaderFactory() const {
    return url_loader_factory_.get();
  }
  void SetURLLoaderFactory(network::mojom::blink::URLLoaderFactoryPtr factory) {
    url_loader_factory_ = std::move(factory);
  }

  void Trace(blink::Visitor*);

 private:
  FetchRequestData();

  FetchRequestData* CloneExceptBody();

  AtomicString method_;
  KURL url_;
  Member<FetchHeaderList> header_list_;
  // FIXME: Support m_skipServiceWorkerFlag;
  mojom::RequestContextType context_;
  scoped_refptr<const SecurityOrigin> origin_;
  // FIXME: Support m_forceOriginHeaderFlag;
  bool same_origin_data_url_flag_;
  AtomicString referrer_string_;
  ReferrerPolicy referrer_policy_;
  // FIXME: Support m_authenticationFlag;
  // FIXME: Support m_synchronousFlag;
  network::mojom::FetchRequestMode mode_;
  network::mojom::FetchCredentialsMode credentials_;
  // TODO(yiyix): |cache_mode_| is exposed but does not yet affect fetch
  // behavior. We must transfer the mode to the network layer and service
  // worker.
  mojom::FetchCacheMode cache_mode_;
  network::mojom::FetchRedirectMode redirect_;
  mojom::FetchImportanceMode importance_;
  // FIXME: Support m_useURLCredentialsFlag;
  // FIXME: Support m_redirectCount;
  Tainting response_tainting_;
  TraceWrapperMember<BodyStreamBuffer> buffer_;
  String mime_type_;
  String integrity_;
  ResourceLoadPriority priority_;
  bool keepalive_;
  bool is_history_navigation_ = false;
  // A specific factory that should be used for this request instead of whatever
  // the system would otherwise decide to use to load this request.
  // Currently used for blob: URLs, to ensure they can still be loaded even if
  // the URL got revoked after creating the request.
  network::mojom::blink::URLLoaderFactoryPtr url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(FetchRequestData);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_REQUEST_DATA_H_
