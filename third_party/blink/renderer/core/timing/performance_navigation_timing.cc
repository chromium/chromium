// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

bool PassesSameOriginCheck(const ResourceResponse& response,
                           const SecurityOrigin& initiator_security_origin) {
  const KURL& response_url = response.ResponseUrl();
  scoped_refptr<const SecurityOrigin> resource_origin =
      SecurityOrigin::Create(response_url);
  return resource_origin->IsSameSchemeHostPort(&initiator_security_origin);
}

bool AllowNavigationTimingRedirect(
    const Vector<ResourceResponse>& redirect_chain,
    const ResourceResponse& final_response,
    const SecurityOrigin& initiator_security_origin) {
  if (!PassesSameOriginCheck(final_response, initiator_security_origin)) {
    return false;
  }

  for (const ResourceResponse& response : redirect_chain) {
    if (!PassesSameOriginCheck(response, initiator_security_origin))
      return false;
  }

  return true;
}

}  // namespace

PerformanceNavigationTiming::PerformanceNavigationTiming(
    LocalFrame* frame,
    ResourceTimingInfo* info,
    base::TimeTicks time_origin,
    const WebVector<WebServerTimingInfo>& server_timing)
    : PerformanceResourceTiming(
          info ? AtomicString(
                     info->FinalResponse().CurrentRequestUrl().GetString())
               : g_empty_atom,
          time_origin,
          SecurityOrigin::IsSecure(frame->GetDocument()->Url()),
          server_timing),
      ContextClient(frame),
      resource_timing_info_(info) {
  DCHECK(frame);
  DCHECK(frame->GetDocument());
  DCHECK(info);
}

PerformanceNavigationTiming::~PerformanceNavigationTiming() = default;

AtomicString PerformanceNavigationTiming::entryType() const {
  return performance_entry_names::kNavigation;
}

PerformanceEntryType PerformanceNavigationTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kNavigation;
}

void PerformanceNavigationTiming::Trace(blink::Visitor* visitor) {
  ContextClient::Trace(visitor);
  PerformanceResourceTiming::Trace(visitor);
}

DocumentLoadTiming* PerformanceNavigationTiming::GetDocumentLoadTiming() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return nullptr;

  return &loader->GetTiming();
}

DocumentLoader* PerformanceNavigationTiming::GetDocumentLoader() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->Loader().GetDocumentLoader();
}

const DocumentTiming* PerformanceNavigationTiming::GetDocumentTiming() const {
  if (!GetFrame())
    return nullptr;
  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &document->GetTiming();
}

ResourceLoadTiming* PerformanceNavigationTiming::GetResourceLoadTiming() const {
  return resource_timing_info_->FinalResponse().GetResourceLoadTiming();
}

bool PerformanceNavigationTiming::AllowTimingDetails() const {
  return true;
}

bool PerformanceNavigationTiming::DidReuseConnection() const {
  return resource_timing_info_->FinalResponse().ConnectionReused();
}

uint64_t PerformanceNavigationTiming::GetTransferSize() const {
  return resource_timing_info_->TransferSize();
}

uint64_t PerformanceNavigationTiming::GetEncodedBodySize() const {
  return resource_timing_info_->FinalResponse().EncodedBodyLength();
}

uint64_t PerformanceNavigationTiming::GetDecodedBodySize() const {
  return resource_timing_info_->FinalResponse().DecodedBodyLength();
}

AtomicString PerformanceNavigationTiming::GetNavigationType(
    WebNavigationType type,
    const Document* document) {
  switch (type) {
    case kWebNavigationTypeReload:
      return "reload";
    case kWebNavigationTypeBackForward:
      return "back_forward";
    case kWebNavigationTypeLinkClicked:
    case kWebNavigationTypeFormSubmitted:
    case kWebNavigationTypeFormResubmitted:
    case kWebNavigationTypeOther:
      return "navigate";
  }
  NOTREACHED();
  return "navigate";
}

AtomicString PerformanceNavigationTiming::initiatorType() const {
  return performance_entry_names::kNavigation;
}

bool PerformanceNavigationTiming::GetAllowRedirectDetails() const {
  blink::ExecutionContext* context =
      GetFrame() ? GetFrame()->GetDocument() : nullptr;
  const blink::SecurityOrigin* security_origin = nullptr;
  if (context)
    security_origin = context->GetSecurityOrigin();
  if (!security_origin)
    return false;
  // TODO(sunjian): Think about how to make this flag deterministic.
  // crbug/693183.
  return AllowNavigationTimingRedirect(resource_timing_info_->RedirectChain(),
                                       resource_timing_info_->FinalResponse(),
                                       *security_origin);
}

AtomicString PerformanceNavigationTiming::AlpnNegotiatedProtocol() const {
  return resource_timing_info_->FinalResponse().AlpnNegotiatedProtocol();
}

AtomicString PerformanceNavigationTiming::ConnectionInfo() const {
  return resource_timing_info_->FinalResponse().ConnectionInfoString();
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventStart() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();

  if (!allow_redirect_details || !timing ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventStart(),
      false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventEnd() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();

  if (!allow_redirect_details || !timing ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventEnd(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domInteractive() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomInteractive(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventStart()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventStart(),
      false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventEnd()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventEnd(),
      false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domComplete() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomComplete(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventStart(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventEnd(), false /* allow_negative_value */);
}

AtomicString PerformanceNavigationTiming::type() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (GetFrame() && loader)
    return GetNavigationType(loader->GetNavigationType(),
                             GetFrame()->GetDocument());
  return "navigate";
}

uint16_t PerformanceNavigationTiming::redirectCount() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return timing->RedirectCount();
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectStart() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectStart(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectEnd() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectEnd(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::fetchStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->FetchStart(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::responseEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->ResponseEnd(), false /* allow_negative_value */);
}

// Overriding PerformanceEntry's attributes.
DOMHighResTimeStamp PerformanceNavigationTiming::duration() const {
  return loadEventEnd();
}

void PerformanceNavigationTiming::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  PerformanceResourceTiming::BuildJSONValue(builder);
  builder.AddNumber("unloadEventStart", unloadEventStart());
  builder.AddNumber("unloadEventEnd", unloadEventEnd());
  builder.AddNumber("domInteractive", domInteractive());
  builder.AddNumber("domContentLoadedEventStart", domContentLoadedEventStart());
  builder.AddNumber("domContentLoadedEventEnd", domContentLoadedEventEnd());
  builder.AddNumber("domComplete", domComplete());
  builder.AddNumber("loadEventStart", loadEventStart());
  builder.AddNumber("loadEventEnd", loadEventEnd());
  builder.AddString("type", type());
  builder.AddNumber("redirectCount", redirectCount());
}
}
