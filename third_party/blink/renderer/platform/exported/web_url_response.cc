/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_url_response.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_http_load_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

class URLResponseExtraDataContainer : public ResourceResponse::ExtraData {
 public:
  static scoped_refptr<URLResponseExtraDataContainer> Create(
      WebURLResponse::ExtraData* extra_data) {
    return base::AdoptRef(new URLResponseExtraDataContainer(extra_data));
  }

  ~URLResponseExtraDataContainer() override = default;

  WebURLResponse::ExtraData* GetExtraData() const { return extra_data_.get(); }

 private:
  explicit URLResponseExtraDataContainer(WebURLResponse::ExtraData* extra_data)
      : extra_data_(base::WrapUnique(extra_data)) {}

  std::unique_ptr<WebURLResponse::ExtraData> extra_data_;
};

}  // namespace

WebURLResponse::~WebURLResponse() = default;

WebURLResponse::WebURLResponse()
    : owned_resource_response_(std::make_unique<ResourceResponse>()),
      resource_response_(owned_resource_response_.get()) {}

WebURLResponse::WebURLResponse(const WebURLResponse& r)
    : owned_resource_response_(
          std::make_unique<ResourceResponse>(*r.resource_response_)),
      resource_response_(owned_resource_response_.get()) {}

WebURLResponse::WebURLResponse(const WebURL& url) : WebURLResponse() {
  SetURL(url);
}

WebURLResponse& WebURLResponse::operator=(const WebURLResponse& r) {
  // Copying subclasses that have different m_resourceResponse ownership
  // semantics via this operator is just not supported.
  DCHECK(owned_resource_response_);
  DCHECK(resource_response_);
  if (&r != this)
    *resource_response_ = *r.resource_response_;
  return *this;
}

bool WebURLResponse::IsNull() const {
  return resource_response_->IsNull();
}

WebURL WebURLResponse::Url() const {
  return resource_response_->Url();
}

void WebURLResponse::SetURL(const WebURL& url) {
  resource_response_->SetURL(url);
}

void WebURLResponse::SetConnectionID(unsigned connection_id) {
  resource_response_->SetConnectionID(connection_id);
}

void WebURLResponse::SetConnectionReused(bool connection_reused) {
  resource_response_->SetConnectionReused(connection_reused);
}

void WebURLResponse::SetLoadTiming(const WebURLLoadTiming& timing) {
  scoped_refptr<ResourceLoadTiming> load_timing =
      scoped_refptr<ResourceLoadTiming>(timing);
  resource_response_->SetResourceLoadTiming(std::move(load_timing));
}

void WebURLResponse::SetHTTPLoadInfo(const WebHTTPLoadInfo& value) {
  resource_response_->SetResourceLoadInfo(value);
}

void WebURLResponse::SetResponseTime(base::Time response_time) {
  resource_response_->SetResponseTime(response_time);
}

WebString WebURLResponse::MimeType() const {
  return resource_response_->MimeType();
}

void WebURLResponse::SetMIMEType(const WebString& mime_type) {
  resource_response_->SetMimeType(mime_type);
}

long long WebURLResponse::ExpectedContentLength() const {
  return resource_response_->ExpectedContentLength();
}

void WebURLResponse::SetExpectedContentLength(
    long long expected_content_length) {
  resource_response_->SetExpectedContentLength(expected_content_length);
}

void WebURLResponse::SetTextEncodingName(const WebString& text_encoding_name) {
  resource_response_->SetTextEncodingName(text_encoding_name);
}

WebURLResponse::HTTPVersion WebURLResponse::HttpVersion() const {
  return static_cast<HTTPVersion>(resource_response_->HttpVersion());
}

void WebURLResponse::SetHTTPVersion(HTTPVersion version) {
  resource_response_->SetHTTPVersion(
      static_cast<ResourceResponse::HTTPVersion>(version));
}

int WebURLResponse::RequestId() const {
  return resource_response_->RequestId();
}

void WebURLResponse::SetRequestId(int request_id) {
  resource_response_->SetRequestId(request_id);
}

int WebURLResponse::HttpStatusCode() const {
  return resource_response_->HttpStatusCode();
}

void WebURLResponse::SetHTTPStatusCode(int http_status_code) {
  resource_response_->SetHTTPStatusCode(http_status_code);
}

WebString WebURLResponse::HttpStatusText() const {
  return resource_response_->HttpStatusText();
}

void WebURLResponse::SetHTTPStatusText(const WebString& http_status_text) {
  resource_response_->SetHTTPStatusText(http_status_text);
}

WebString WebURLResponse::HttpHeaderField(const WebString& name) const {
  return resource_response_->HttpHeaderField(name);
}

void WebURLResponse::SetHTTPHeaderField(const WebString& name,
                                        const WebString& value) {
  resource_response_->SetHTTPHeaderField(name, value);
}

void WebURLResponse::AddHTTPHeaderField(const WebString& name,
                                        const WebString& value) {
  if (name.IsNull() || value.IsNull())
    return;

  resource_response_->AddHTTPHeaderField(name, value);
}

void WebURLResponse::ClearHTTPHeaderField(const WebString& name) {
  resource_response_->ClearHTTPHeaderField(name);
}

void WebURLResponse::VisitHTTPHeaderFields(
    WebHTTPHeaderVisitor* visitor) const {
  const HTTPHeaderMap& map = resource_response_->HttpHeaderFields();
  for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
    visitor->VisitHeader(it->key, it->value);
}

long long WebURLResponse::AppCacheID() const {
  return resource_response_->AppCacheID();
}

void WebURLResponse::SetAppCacheID(long long app_cache_id) {
  resource_response_->SetAppCacheID(app_cache_id);
}

WebURL WebURLResponse::AppCacheManifestURL() const {
  return resource_response_->AppCacheManifestURL();
}

void WebURLResponse::SetAppCacheManifestURL(const WebURL& url) {
  resource_response_->SetAppCacheManifestURL(url);
}

void WebURLResponse::SetHasMajorCertificateErrors(bool value) {
  resource_response_->SetHasMajorCertificateErrors(value);
}

void WebURLResponse::SetCTPolicyCompliance(
    net::ct::CTPolicyCompliance compliance) {
  switch (compliance) {
    case net::ct::CTPolicyCompliance::
        CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE:
    case net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY:
      resource_response_->SetCTPolicyCompliance(
          ResourceResponse::kCTPolicyComplianceDetailsNotAvailable);
      break;
    case net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS:
    case net::ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS:
      resource_response_->SetCTPolicyCompliance(
          ResourceResponse::kCTPolicyDoesNotComply);
      break;
    case net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS:
      resource_response_->SetCTPolicyCompliance(
          ResourceResponse::kCTPolicyComplies);
      break;
    case net::ct::CTPolicyCompliance::CT_POLICY_COUNT:
      NOTREACHED();
      resource_response_->SetCTPolicyCompliance(
          ResourceResponse::kCTPolicyComplianceDetailsNotAvailable);
      break;
  };
}

void WebURLResponse::SetIsLegacySymantecCert(bool value) {
  resource_response_->SetIsLegacySymantecCert(value);
}

void WebURLResponse::SetSecurityStyle(WebSecurityStyle security_style) {
  resource_response_->SetSecurityStyle(
      static_cast<ResourceResponse::SecurityStyle>(security_style));
}

void WebURLResponse::SetSecurityDetails(
    const WebSecurityDetails& web_security_details) {
  ResourceResponse::SignedCertificateTimestampList sct_list;
  for (const auto& iter : web_security_details.sct_list) {
    sct_list.push_back(
        static_cast<ResourceResponse::SignedCertificateTimestamp>(iter));
  }
  Vector<String> san_list;
  san_list.Append(web_security_details.san_list.Data(),
                  web_security_details.san_list.size());
  Vector<AtomicString> certificate;
  for (const auto& iter : web_security_details.certificate) {
    AtomicString cert = iter;
    certificate.push_back(cert);
  }
  resource_response_->SetSecurityDetails(
      web_security_details.protocol, web_security_details.key_exchange,
      web_security_details.key_exchange_group, web_security_details.cipher,
      web_security_details.mac, web_security_details.subject_name, san_list,
      web_security_details.issuer,
      static_cast<time_t>(web_security_details.valid_from),
      static_cast<time_t>(web_security_details.valid_to), certificate,
      sct_list);
}

WebURLResponse::WebSecurityDetails WebURLResponse::SecurityDetailsForTesting() {
  const blink::ResourceResponse::SecurityDetails* security_details =
      resource_response_->GetSecurityDetails();
  std::vector<SignedCertificateTimestamp> sct_list;
  for (const auto& iter : security_details->sct_list) {
    sct_list.push_back(SignedCertificateTimestamp(
        iter.status_, iter.origin_, iter.log_description_, iter.log_id_,
        iter.timestamp_, iter.hash_algorithm_, iter.signature_algorithm_,
        iter.signature_data_));
  }
  return WebSecurityDetails(
      security_details->protocol, security_details->key_exchange,
      security_details->key_exchange_group, security_details->cipher,
      security_details->mac, security_details->subject_name,
      security_details->san_list, security_details->issuer,
      security_details->valid_from, security_details->valid_to,
      security_details->certificate, SignedCertificateTimestampList(sct_list));
}

const ResourceResponse& WebURLResponse::ToResourceResponse() const {
  return *resource_response_;
}

void WebURLResponse::SetWasCached(bool value) {
  resource_response_->SetWasCached(value);
}

void WebURLResponse::SetWasFetchedViaSPDY(bool value) {
  resource_response_->SetWasFetchedViaSPDY(value);
}

bool WebURLResponse::WasFetchedViaServiceWorker() const {
  return resource_response_->WasFetchedViaServiceWorker();
}

void WebURLResponse::SetWasFetchedViaServiceWorker(bool value) {
  resource_response_->SetWasFetchedViaServiceWorker(value);
}

void WebURLResponse::SetWasFallbackRequiredByServiceWorker(bool value) {
  resource_response_->SetWasFallbackRequiredByServiceWorker(value);
}

void WebURLResponse::SetType(network::mojom::FetchResponseType value) {
  resource_response_->SetType(value);
}

network::mojom::FetchResponseType WebURLResponse::GetType() const {
  return resource_response_->GetType();
}

void WebURLResponse::SetURLListViaServiceWorker(
    const WebVector<WebURL>& url_list_via_service_worker) {
  Vector<KURL> url_list(url_list_via_service_worker.size());
  std::transform(url_list_via_service_worker.begin(),
                 url_list_via_service_worker.end(), url_list.begin(),
                 [](const WebURL& url) { return url; });
  resource_response_->SetURLListViaServiceWorker(url_list);
}

WebURL WebURLResponse::OriginalURLViaServiceWorker() const {
  return resource_response_->OriginalURLViaServiceWorker();
}

void WebURLResponse::SetMultipartBoundary(const char* bytes, size_t size) {
  resource_response_->SetMultipartBoundary(bytes, size);
}

void WebURLResponse::SetCacheStorageCacheName(
    const WebString& cache_storage_cache_name) {
  resource_response_->SetCacheStorageCacheName(cache_storage_cache_name);
}

WebVector<WebString> WebURLResponse::CorsExposedHeaderNames() const {
  return resource_response_->CorsExposedHeaderNames();
}

void WebURLResponse::SetCorsExposedHeaderNames(
    const WebVector<WebString>& header_names) {
  Vector<String> exposed_header_names;
  exposed_header_names.Append(header_names.Data(), header_names.size());
  resource_response_->SetCorsExposedHeaderNames(exposed_header_names);
}

void WebURLResponse::SetDidServiceWorkerNavigationPreload(bool value) {
  resource_response_->SetDidServiceWorkerNavigationPreload(value);
}

WebString WebURLResponse::RemoteIPAddress() const {
  return resource_response_->RemoteIPAddress();
}

void WebURLResponse::SetRemoteIPAddress(const WebString& remote_ip_address) {
  resource_response_->SetRemoteIPAddress(remote_ip_address);
}

unsigned short WebURLResponse::RemotePort() const {
  return resource_response_->RemotePort();
}

void WebURLResponse::SetRemotePort(unsigned short remote_port) {
  resource_response_->SetRemotePort(remote_port);
}

void WebURLResponse::SetEncodedDataLength(long long length) {
  resource_response_->SetEncodedDataLength(length);
}

void WebURLResponse::SetIsSignedExchangeInnerResponse(
    bool is_signed_exchange_inner_response) {
  resource_response_->SetIsSignedExchangeInnerResponse(
      is_signed_exchange_inner_response);
}

WebURLResponse::ExtraData* WebURLResponse::GetExtraData() const {
  scoped_refptr<ResourceResponse::ExtraData> data =
      resource_response_->GetExtraData();
  if (!data)
    return nullptr;
  return static_cast<URLResponseExtraDataContainer*>(data.get())
      ->GetExtraData();
}

void WebURLResponse::SetExtraData(WebURLResponse::ExtraData* extra_data) {
  if (extra_data != GetExtraData()) {
    resource_response_->SetExtraData(
        URLResponseExtraDataContainer::Create(extra_data));
  }
}

void WebURLResponse::AppendRedirectResponse(const WebURLResponse& response) {
  resource_response_->AppendRedirectResponse(response.ToResourceResponse());
}

WebString WebURLResponse::AlpnNegotiatedProtocol() const {
  return resource_response_->AlpnNegotiatedProtocol();
}

void WebURLResponse::SetAlpnNegotiatedProtocol(
    const WebString& alpn_negotiated_protocol) {
  resource_response_->SetAlpnNegotiatedProtocol(alpn_negotiated_protocol);
}

net::HttpResponseInfo::ConnectionInfo WebURLResponse::ConnectionInfo() const {
  return resource_response_->ConnectionInfo();
}

void WebURLResponse::SetConnectionInfo(
    net::HttpResponseInfo::ConnectionInfo connection_info) {
  resource_response_->SetConnectionInfo(connection_info);
}

void WebURLResponse::SetAsyncRevalidationRequested(bool requested) {
  resource_response_->SetAsyncRevalidationRequested(requested);
}

WebURLResponse::WebURLResponse(ResourceResponse& r) : resource_response_(&r) {}

}  // namespace blink
