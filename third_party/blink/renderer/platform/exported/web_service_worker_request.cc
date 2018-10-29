// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"

#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class WebServiceWorkerRequestPrivate
    : public RefCounted<WebServiceWorkerRequestPrivate> {
 public:
  WebURL url_;
  WebString method_;
  HTTPHeaderMap headers_;
  scoped_refptr<EncodedFormData> http_body;
  scoped_refptr<BlobDataHandle> blob_data_handle;
  Referrer referrer_;
  network::mojom::FetchRequestMode mode_ =
      network::mojom::FetchRequestMode::kNoCORS;
  bool is_main_resource_load_ = false;
  network::mojom::FetchCredentialsMode credentials_mode_ =
      network::mojom::FetchCredentialsMode::kOmit;
  mojom::FetchCacheMode cache_mode_ = mojom::FetchCacheMode::kDefault;
  network::mojom::FetchRedirectMode redirect_mode_ =
      network::mojom::FetchRedirectMode::kFollow;
  mojom::RequestContextType request_context_ =
      mojom::RequestContextType::UNSPECIFIED;
  network::mojom::RequestContextFrameType frame_type_ =
      network::mojom::RequestContextFrameType::kNone;
  WebString integrity_;
  WebURLRequest::Priority priority_ = WebURLRequest::Priority::kUnresolved;
  bool keepalive_ = false;
  WebString client_id_;
  bool is_reload_ = false;
  bool is_history_navigation_ = false;
};

WebServiceWorkerRequest::WebServiceWorkerRequest()
    : private_(base::AdoptRef(new WebServiceWorkerRequestPrivate)) {}

void WebServiceWorkerRequest::Reset() {
  private_.Reset();
}

void WebServiceWorkerRequest::Assign(const WebServiceWorkerRequest& other) {
  private_ = other.private_;
}

void WebServiceWorkerRequest::SetURL(const WebURL& url) {
  private_->url_ = url;
}

const WebString& WebServiceWorkerRequest::Integrity() const {
  return private_->integrity_;
}

WebURLRequest::Priority WebServiceWorkerRequest::Priority() const {
  return private_->priority_;
}

bool WebServiceWorkerRequest::Keepalive() const {
  return private_->keepalive_;
}

const WebURL& WebServiceWorkerRequest::Url() const {
  return private_->url_;
}

void WebServiceWorkerRequest::SetMethod(const WebString& method) {
  private_->method_ = method;
}

const WebString& WebServiceWorkerRequest::Method() const {
  return private_->method_;
}

void WebServiceWorkerRequest::SetHeader(const WebString& key,
                                        const WebString& value) {
  if (DeprecatedEqualIgnoringCase(key, "referer"))
    return;
  private_->headers_.Set(key, value);
}

void WebServiceWorkerRequest::AppendHeader(const WebString& key,
                                           const WebString& value) {
  if (DeprecatedEqualIgnoringCase(key, "referer"))
    return;
  HTTPHeaderMap::AddResult result = private_->headers_.Add(key, value);
  if (!result.is_new_entry)
    result.stored_value->value =
        result.stored_value->value + ", " + String(value);
}

void WebServiceWorkerRequest::VisitHTTPHeaderFields(
    WebHTTPHeaderVisitor* header_visitor) const {
  for (HTTPHeaderMap::const_iterator i = private_->headers_.begin(),
                                     end = private_->headers_.end();
       i != end; ++i)
    header_visitor->VisitHeader(i->key, i->value);
}

const HTTPHeaderMap& WebServiceWorkerRequest::Headers() const {
  return private_->headers_;
}

void WebServiceWorkerRequest::SetBody(const WebHTTPBody& body) {
  private_->http_body = body;
}

WebHTTPBody WebServiceWorkerRequest::Body() const {
  return private_->http_body;
}

void WebServiceWorkerRequest::SetBlob(const WebString& uuid,
                                      long long size,
                                      mojo::ScopedMessagePipeHandle blob_pipe) {
  SetBlob(uuid, size,
          mojom::blink::BlobPtrInfo(std::move(blob_pipe),
                                    mojom::blink::Blob::Version_));
}

void WebServiceWorkerRequest::SetBlob(const WebString& uuid,
                                      long long size,
                                      mojom::blink::BlobPtrInfo blob_info) {
  private_->blob_data_handle =
      BlobDataHandle::Create(uuid, String(), size, std::move(blob_info));
}

void WebServiceWorkerRequest::SetBlobDataHandle(
    scoped_refptr<BlobDataHandle> blob_data_handle) {
  private_->blob_data_handle = std::move(blob_data_handle);
}

scoped_refptr<BlobDataHandle> WebServiceWorkerRequest::GetBlobDataHandle()
    const {
  return private_->blob_data_handle;
}

void WebServiceWorkerRequest::SetReferrer(
    const WebString& web_referrer,
    network::mojom::ReferrerPolicy referrer_policy) {
  // WebString doesn't have the distinction between empty and null. We use
  // the null WTFString for referrer.
  DCHECK_EQ(Referrer::NoReferrer(), String());
  String referrer =
      web_referrer.IsEmpty() ? Referrer::NoReferrer() : String(web_referrer);
  private_->referrer_ =
      Referrer(referrer, static_cast<ReferrerPolicy>(referrer_policy));
}

WebURL WebServiceWorkerRequest::ReferrerUrl() const {
  return KURL(private_->referrer_.referrer);
}

network::mojom::ReferrerPolicy WebServiceWorkerRequest::GetReferrerPolicy()
    const {
  return static_cast<network::mojom::ReferrerPolicy>(
      private_->referrer_.referrer_policy);
}

const Referrer& WebServiceWorkerRequest::GetReferrer() const {
  return private_->referrer_;
}

void WebServiceWorkerRequest::SetMode(network::mojom::FetchRequestMode mode) {
  private_->mode_ = mode;
}

network::mojom::FetchRequestMode WebServiceWorkerRequest::Mode() const {
  return private_->mode_;
}

void WebServiceWorkerRequest::SetIsMainResourceLoad(
    bool is_main_resource_load) {
  private_->is_main_resource_load_ = is_main_resource_load;
}

bool WebServiceWorkerRequest::IsMainResourceLoad() const {
  return private_->is_main_resource_load_;
}

void WebServiceWorkerRequest::SetCredentialsMode(
    network::mojom::FetchCredentialsMode credentials_mode) {
  private_->credentials_mode_ = credentials_mode;
}

void WebServiceWorkerRequest::SetIntegrity(const WebString& integrity) {
  private_->integrity_ = integrity;
}

void WebServiceWorkerRequest::SetPriority(WebURLRequest::Priority priority) {
  private_->priority_ = priority;
}

void WebServiceWorkerRequest::SetKeepalive(bool keepalive) {
  private_->keepalive_ = keepalive;
}

network::mojom::FetchCredentialsMode WebServiceWorkerRequest::CredentialsMode()
    const {
  return private_->credentials_mode_;
}

void WebServiceWorkerRequest::SetCacheMode(mojom::FetchCacheMode cache_mode) {
  private_->cache_mode_ = cache_mode;
}

mojom::FetchCacheMode WebServiceWorkerRequest::CacheMode() const {
  return private_->cache_mode_;
}

void WebServiceWorkerRequest::SetRedirectMode(
    network::mojom::FetchRedirectMode redirect_mode) {
  private_->redirect_mode_ = redirect_mode;
}

network::mojom::FetchRedirectMode WebServiceWorkerRequest::RedirectMode()
    const {
  return private_->redirect_mode_;
}

void WebServiceWorkerRequest::SetRequestContext(
    mojom::RequestContextType request_context) {
  private_->request_context_ = request_context;
}

mojom::RequestContextType WebServiceWorkerRequest::GetRequestContext() const {
  return private_->request_context_;
}

void WebServiceWorkerRequest::SetFrameType(
    network::mojom::RequestContextFrameType frame_type) {
  private_->frame_type_ = frame_type;
}

network::mojom::RequestContextFrameType WebServiceWorkerRequest::GetFrameType()
    const {
  return private_->frame_type_;
}

void WebServiceWorkerRequest::SetClientId(const WebString& client_id) {
  private_->client_id_ = client_id;
}

const WebString& WebServiceWorkerRequest::ClientId() const {
  return private_->client_id_;
}

void WebServiceWorkerRequest::SetIsReload(bool is_reload) {
  private_->is_reload_ = is_reload;
}

bool WebServiceWorkerRequest::IsReload() const {
  return private_->is_reload_;
}

void WebServiceWorkerRequest::SetIsHistoryNavigation(bool b) {
  private_->is_history_navigation_ = b;
}

bool WebServiceWorkerRequest::IsHistoryNavigation() const {
  return private_->is_history_navigation_;
}

}  // namespace blink
