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

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
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
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class Beacon {
  STACK_ALLOCATED();

 public:
  virtual void Serialize(ResourceRequest&) const = 0;
  virtual uint64_t size() const = 0;
  virtual const AtomicString GetContentType() const = 0;
};

class BeaconString final : public Beacon {
 public:
  explicit BeaconString(const String& data) : data_(data) {}

  uint64_t size() const override { return data_.CharactersSizeInBytes(); }

  void Serialize(ResourceRequest& request) const override {
    scoped_refptr<EncodedFormData> entity_body =
        EncodedFormData::Create(data_.Utf8());
    request.SetHttpBody(entity_body);
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

  uint64_t size() const override { return data_->size(); }

  void Serialize(ResourceRequest& request) const override {
    DCHECK(data_);

    scoped_refptr<EncodedFormData> entity_body = EncodedFormData::Create();
    if (data_->HasBackingFile()) {
      entity_body->AppendFile(To<File>(data_)->GetPath(),
                              To<File>(data_)->LastModifiedTime());
    } else {
      entity_body->AppendBlob(data_->Uuid(), data_->GetBlobDataHandle());
    }

    request.SetHttpBody(std::move(entity_body));

    if (!content_type_.IsEmpty()) {
      if (!cors::IsCorsSafelistedContentType(content_type_)) {
        request.SetMode(network::mojom::blink::RequestMode::kCors);
      }
      request.SetHTTPContentType(content_type_);
    }
  }

  const AtomicString GetContentType() const override { return content_type_; }

 private:
  Blob* const data_;
  AtomicString content_type_;
};

class BeaconDOMArrayBufferView final : public Beacon {
 public:
  explicit BeaconDOMArrayBufferView(DOMArrayBufferView* data) : data_(data) {
    CHECK(base::CheckedNumeric<wtf_size_t>(data->byteLengthAsSizeT()).IsValid())
        << "EncodedFormData::Create cannot deal with huge ArrayBuffers.";
  }

  uint64_t size() const override { return data_->byteLengthAsSizeT(); }

  void Serialize(ResourceRequest& request) const override {
    DCHECK(data_);

    scoped_refptr<EncodedFormData> entity_body = EncodedFormData::Create(
        data_->BaseAddress(),
        base::checked_cast<wtf_size_t>(data_->byteLengthAsSizeT()));
    request.SetHttpBody(std::move(entity_body));
  }

  const AtomicString GetContentType() const override { return g_null_atom; }

 private:
  DOMArrayBufferView* const data_;
};

class BeaconDOMArrayBuffer final : public Beacon {
 public:
  explicit BeaconDOMArrayBuffer(DOMArrayBuffer* data) : data_(data) {
    CHECK(base::CheckedNumeric<wtf_size_t>(data->ByteLengthAsSizeT()).IsValid())
        << "EncodedFormData::Create cannot deal with huge ArrayBuffers.";
  }

  uint64_t size() const override { return data_->ByteLengthAsSizeT(); }

  void Serialize(ResourceRequest& request) const override {
    DCHECK(data_);

    scoped_refptr<EncodedFormData> entity_body = EncodedFormData::Create(
        data_->Data(),
        base::checked_cast<wtf_size_t>(data_->ByteLengthAsSizeT()));
    request.SetHttpBody(std::move(entity_body));
  }

  const AtomicString GetContentType() const override { return g_null_atom; }

 private:
  DOMArrayBuffer* const data_;
};

class BeaconURLSearchParams final : public Beacon {
 public:
  explicit BeaconURLSearchParams(URLSearchParams* data) : data_(data) {}

  uint64_t size() const override {
    return data_->toString().CharactersSizeInBytes();
  }

  void Serialize(ResourceRequest& request) const override {
    DCHECK(data_);

    request.SetHttpBody(data_->ToEncodedFormData());
    request.SetHTTPContentType(GetContentType());
  }

  const AtomicString GetContentType() const override {
    return AtomicString("application/x-www-form-urlencoded;charset=UTF-8");
  }

 private:
  URLSearchParams* const data_;
};

class BeaconFormData final : public Beacon {
 public:
  explicit BeaconFormData(FormData* data)
      : data_(data), entity_body_(data_->EncodeMultiPartFormData()) {
    content_type_ = AtomicString("multipart/form-data; boundary=") +
                    entity_body_->Boundary().data();
  }

  uint64_t size() const override { return entity_body_->SizeInBytes(); }

  void Serialize(ResourceRequest& request) const override {
    request.SetHttpBody(entity_body_.get());
    request.SetHTTPContentType(content_type_);
  }

  const AtomicString GetContentType() const override { return content_type_; }

 private:
  FormData* const data_;
  scoped_refptr<EncodedFormData> entity_body_;
  AtomicString content_type_;
};

bool SendBeaconCommon(const ScriptState& state,
                      LocalFrame* frame,
                      const KURL& url,
                      const Beacon& beacon) {
  if (!frame->DomWindow()
           ->GetContentSecurityPolicyForWorld(&state.World())
           ->AllowConnectToSource(url, url, RedirectStatus::kNoRedirect)) {
    // We're simulating a network failure here, so we return 'true'.
    return true;
  }

  ResourceRequest request(url);
  request.SetHttpMethod(http_names::kPOST);
  request.SetKeepalive(true);
  request.SetRequestContext(mojom::blink::RequestContextType::BEACON);
  beacon.Serialize(request);
  FetchParameters params(std::move(request), &state.World());
  // The spec says:
  //  - If mimeType is not null:
  //   - If mimeType value is a CORS-safelisted request-header value for the
  //     Content-Type header, set corsMode to "no-cors".
  // As we don't support requests with non CORS-safelisted Content-Type, the
  // mode should always be "no-cors".
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kBeacon;

  frame->Client()->DidDispatchPingLoader(url);
  Resource* resource =
      RawResource::Fetch(params, frame->DomWindow()->Fetcher(), nullptr);
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
  request.SetHttpMethod(http_names::kPOST);
  request.SetHTTPContentType("text/ping");
  request.SetHttpBody(EncodedFormData::Create("PING"));
  request.SetHttpHeaderField(http_names::kCacheControl, "max-age=0");
  request.SetHttpHeaderField(http_names::kPingTo,
                             AtomicString(destination_url.GetString()));
  scoped_refptr<const SecurityOrigin> ping_origin =
      SecurityOrigin::Create(ping_url);
  if (ProtocolIs(frame->DomWindow()->Url().GetString(), "http") ||
      frame->DomWindow()->GetSecurityOrigin()->CanAccess(ping_origin.get())) {
    request.SetHttpHeaderField(
        http_names::kPingFrom,
        AtomicString(frame->DomWindow()->Url().GetString()));
  }

  request.SetKeepalive(true);
  request.SetReferrerString(Referrer::NoReferrer());
  request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  request.SetRequestContext(mojom::blink::RequestContextType::PING);
  FetchParameters params(std::move(request),
                         frame->DomWindow()->GetCurrentWorld());
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kPing;

  frame->Client()->DidDispatchPingLoader(ping_url);
  RawResource::Fetch(params, frame->DomWindow()->Fetcher(), nullptr);
}

void PingLoader::SendViolationReport(LocalFrame* frame,
                                     const KURL& report_url,
                                     scoped_refptr<EncodedFormData> report) {
  ResourceRequest request(report_url);
  request.SetHttpMethod(http_names::kPOST);
  request.SetHTTPContentType("application/csp-report");
  request.SetKeepalive(true);
  request.SetHttpBody(std::move(report));
  request.SetCredentialsMode(network::mojom::CredentialsMode::kSameOrigin);
  request.SetRequestContext(mojom::blink::RequestContextType::CSP_REPORT);
  request.SetRequestDestination(network::mojom::RequestDestination::kReport);
  request.SetRequestorOrigin(frame->DomWindow()->GetSecurityOrigin());
  request.SetRedirectMode(network::mojom::RedirectMode::kError);
  FetchParameters params(std::move(request),
                         frame->DomWindow()->GetCurrentWorld());
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kViolationreport;

  frame->Client()->DidDispatchPingLoader(report_url);
  RawResource::Fetch(params, frame->DomWindow()->Fetcher(), nullptr);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            const String& data) {
  BeaconString beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            DOMArrayBufferView* data) {
  BeaconDOMArrayBufferView beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            DOMArrayBuffer* data) {
  BeaconDOMArrayBuffer beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            URLSearchParams* data) {
  BeaconURLSearchParams beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            FormData* data) {
  BeaconFormData beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

bool PingLoader::SendBeacon(const ScriptState& state,
                            LocalFrame* frame,
                            const KURL& beacon_url,
                            Blob* data) {
  BeaconBlob beacon(data);
  return SendBeaconCommon(state, frame, beacon_url, beacon);
}

}  // namespace blink
