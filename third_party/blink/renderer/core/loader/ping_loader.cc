/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/loader/ping_loader.h"

#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/compiler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class Beacon {
  STACK_ALLOCATED();

 public:
  virtual void Serialize(ResourceRequest&) const = 0;
  virtual unsigned long long size() const = 0;
  virtual const AtomicString GetContentType() const = 0;
};

class BeaconString final : public Beacon {
 public:
  explicit BeaconString(const String& data) : data_(data) {}

  unsigned long long size() const override {
    return data_.CharactersSizeInBytes();
  }

  void Serialize(ResourceRequest& request) const override {
    scoped_refptr<EncodedFormData> entity_body =
        EncodedFormData::Create(data_.Utf8());
    request.SetHTTPBody(entity_body);
    request.SetHTTPContentType(GetContentType());
  }

  const AtomicString GetContentType() const override {
    return AtomicString("text/plain;charset=UTF-8");
  }

 private:
  const String data_;
};

class BeaconBlob final : public Beacon {
 public:
  explicit BeaconBlob(Blob* data) : data_(data) {
    const String& blob_type = data_->type();
    if (!blob_type.IsEmpty() && ParsedContentType(blob_type).IsValid())
      content_type_ = AtomicString(blob_type);
  }

  unsigned long long size() const override { return data_->size(); }

  void Serialize(ResourceRequest& request) const override {
    DCHECK(data_);

    scoped_refptr<EncodedFormData> entity_body = EncodedFormData::Create();
    if (data_->HasBackingFile())
      entity_body->AppendFile(ToFile(data_)->GetPath());
    else
      entity_body->AppendBlob(data_->Uuid(), data_->GetBlobDataHandle());

    request.SetHTTPBody(std::move(entity_body));

    if (!content_type_.IsEmpty())
      request.SetHTTPContentType(content_type_);
  }

  const AtomicString GetContentType() const override { return content_type_; }

 private:
  const Member<Blob> data_;
  AtomicString content_type_;
};

class BeaconDOMArrayBufferView final : public Beacon {
 public:
  explicit BeaconDOMArrayBufferView(DOMArrayBufferView* data) : data_(data) {}

  unsigned long long size() const override { return data_->byteLength(); }

  void Serialize(ResourceRequest& request) const override {
    DCHECK(data_);

    scoped_refptr<EncodedFormData> entity_body =
        EncodedFormData::Create(data_->BaseAddress(), data_->byteLength());
    request.SetHTTPBody(std::move(entity_body));

    // FIXME: a reasonable choice, but not in the spec; should it give a
    // default?
    request.SetHTTPContentType(AtomicString("application/octet-stream"));
  }

  const AtomicString GetContentType() const override { return g_null_atom; }

 private:
  const Member<DOMArrayBufferView> data_;
};

class BeaconFormData final : public Beacon {
 public:
  explicit BeaconFormData(FormData* data)
      : data_(data), entity_body_(data_->EncodeMultiPartFormData()) {
    content_type_ = AtomicString("multipart/form-data; boundary=") +
                    entity_body_->Boundary().data();
  }

  unsigned long long size() const override {
    return entity_body_->SizeInBytes();
  }

  void Serialize(ResourceRequest& request) const override {
    request.SetHTTPBody(entity_body_.get());
    request.SetHTTPContentType(content_type_);
  }

  const AtomicString GetContentType() const override { return content_type_; }

 private:
  const Member<FormData> data_;
  scoped_refptr<EncodedFormData> entity_body_;
  AtomicString content_type_;
};

bool SendBeaconCommon(LocalFrame* frame,
                      const KURL& url,
                      const Beacon& beacon) {
  if (!frame->GetDocument())
    return false;

  if (!ContentSecurityPolicy::ShouldBypassMainWorld(frame->GetDocument()) &&
      !frame->GetDocument()->GetContentSecurityPolicy()->AllowConnectToSource(
          url)) {
    // We're simulating a network failure here, so we return 'true'.
    return true;
  }

  ResourceRequest request(url);
  request.SetHTTPMethod(HTTPNames::POST);
  request.SetKeepalive(true);
  request.SetRequestContext(mojom::RequestContextType::BEACON);
  beacon.Serialize(request);
  FetchParameters params(request);
  // The spec says:
  //  - If mimeType is not null:
  //   - If mimeType value is a CORS-safelisted request-header value for the
  //     Content-Type header, set corsMode to "no-cors".
  // As we don't support requests with non CORS-safelisted Content-Type, the
  // mode should always be "no-cors".
  params.MutableOptions().initiator_info.name = FetchInitiatorTypeNames::beacon;

  frame->Client()->DidDispatchPingLoader(request.Url());
  Resource* resource =
      RawResource::Fetch(params, frame->GetDocument()->Fetcher(), nullptr);
  return resource->GetStatus() != ResourceStatus::kLoadError;
}

}  // namespace

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::SendLinkAuditPing(LocalFrame* frame,
                                   const KURL& ping_url,
                                   const KURL& destination_url) {
  if (!ping_url.ProtocolIsInHTTPFamily())
    return;

  ResourceRequest request(ping_url);
  request.SetHTTPMethod(HTTPNames::POST);
  request.SetHTTPContentType("text/ping");
  request.SetHTTPBody(EncodedFormData::Create("PING"));
  request.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  request.SetHTTPHeaderField(HTTPNames::Ping_To,
                             AtomicString(destination_url.GetString()));
  scoped_refptr<const SecurityOrigin> ping_origin =
      SecurityOrigin::Create(ping_url);
  if (ProtocolIs(frame->GetDocument()->Url().GetString(), "http") ||
      frame->GetDocument()->GetSecurityOrigin()->CanAccess(ping_origin.get())) {
    request.SetHTTPHeaderField(
        HTTPNames::Ping_From,
        AtomicString(frame->GetDocument()->Url().GetString()));
  }

  request.SetKeepalive(true);
  // TODO(domfarolino): Add WPTs ensuring that pings do not have a referrer
  // header.
  request.SetReferrerString(Referrer::NoReferrer());
  request.SetReferrerPolicy(kReferrerPolicyNever);
  request.SetRequestContext(mojom::RequestContextType::PING);
  FetchParameters params(request);
  params.MutableOptions().initiator_info.name = FetchInitiatorTypeNames::ping;

  frame->Client()->DidDispatchPingLoader(request.Url());
  RawResource::Fetch(params, frame->GetDocument()->Fetcher(), nullptr);
}

void PingLoader::SendViolationReport(LocalFrame* frame,
                                     const KURL& report_url,
                                     scoped_refptr<EncodedFormData> report,
                                     ViolationReportType type) {
  ResourceRequest request(report_url);
  request.SetHTTPMethod(HTTPNames::POST);
  switch (type) {
    case kContentSecurityPolicyViolationReport:
      request.SetHTTPContentType("application/csp-report");
      break;
    case kXSSAuditorViolationReport:
      request.SetHTTPContentType("application/xss-auditor-report");
      break;
  }
  request.SetKeepalive(true);
  request.SetHTTPBody(std::move(report));
  request.SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kSameOrigin);
  request.SetRequestContext(mojom::RequestContextType::CSP_REPORT);
  request.SetRequestorOrigin(frame->GetDocument()->GetSecurityOrigin());
  request.SetFetchRedirectMode(network::mojom::FetchRedirectMode::kError);
  FetchParameters params(request);
  params.MutableOptions().initiator_info.name =
      FetchInitiatorTypeNames::violationreport;

  frame->Client()->DidDispatchPingLoader(request.Url());
  RawResource::Fetch(params, frame->GetDocument()->Fetcher(), nullptr);
}

bool PingLoader::SendBeacon(LocalFrame* frame,
                            const KURL& beacon_url,
                            const String& data) {
  BeaconString beacon(data);
  return SendBeaconCommon(frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(LocalFrame* frame,
                            const KURL& beacon_url,
                            DOMArrayBufferView* data) {
  BeaconDOMArrayBufferView beacon(data);
  return SendBeaconCommon(frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(LocalFrame* frame,
                            const KURL& beacon_url,
                            FormData* data) {
  BeaconFormData beacon(data);
  return SendBeaconCommon(frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(LocalFrame* frame,
                            const KURL& beacon_url,
                            Blob* data) {
  BeaconBlob beacon(data);
  return SendBeaconCommon(frame, beacon_url, beacon);
}

}  // namespace blink
