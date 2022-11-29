// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"

#include "base/containers/contains.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_navigation_timing_activation_start.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

bool PassesSameOriginCheck(const ResourceResponse& response,
                           const SecurityOrigin& initiator_security_origin) {
  const KURL& response_url = response.ResponseUrl();
  scoped_refptr<const SecurityOrigin> resource_origin =
      SecurityOrigin::Create(response_url);
  return resource_origin->IsSameOriginWith(&initiator_security_origin);
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
    LocalDOMWindow* window,
    ResourceTimingInfo* info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    HeapVector<Member<PerformanceServerTiming>> server_timing,
    network::mojom::NavigationDeliveryType navigation_delivery_type)
    : PerformanceResourceTiming(
          info ? AtomicString(
                     info->FinalResponse().CurrentRequestUrl().GetString())
               : g_empty_atom,
          time_origin,
          cross_origin_isolated_capability,
          info->CacheState(),
          base::Contains(url::GetSecureSchemes(),
                         window->Url().Protocol().Ascii()),
          std::move(server_timing),
          window,
          navigation_delivery_type),
      ExecutionContextClient(window),
      resource_timing_info_(info) {
  DCHECK(window);
  DCHECK(info);
}

PerformanceNavigationTiming::~PerformanceNavigationTiming() = default;

const AtomicString& PerformanceNavigationTiming::entryType() const {
  return performance_entry_names::kNavigation;
}

PerformanceEntryType PerformanceNavigationTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kNavigation;
}

void PerformanceNavigationTiming::Trace(Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
  PerformanceResourceTiming::Trace(visitor);
}

DocumentLoadTiming* PerformanceNavigationTiming::GetDocumentLoadTiming() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return nullptr;

  return &loader->GetTiming();
}

DocumentLoader* PerformanceNavigationTiming::GetDocumentLoader() const {
  return DomWindow() ? DomWindow()->document()->Loader() : nullptr;
}

const DocumentTiming* PerformanceNavigationTiming::GetDocumentTiming() const {
  return DomWindow() ? &DomWindow()->document()->GetTiming() : nullptr;
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
  return PerformanceResourceTiming::GetTransferSize(
      resource_timing_info_->FinalResponse().EncodedBodyLength(), CacheState());
}

uint64_t PerformanceNavigationTiming::GetEncodedBodySize() const {
  return resource_timing_info_->FinalResponse().EncodedBodyLength();
}

uint64_t PerformanceNavigationTiming::GetDecodedBodySize() const {
  return resource_timing_info_->FinalResponse().DecodedBodyLength();
}

AtomicString PerformanceNavigationTiming::GetNavigationType(
    WebNavigationType type) {
  switch (type) {
    case kWebNavigationTypeReload:
    case kWebNavigationTypeFormResubmittedReload:
      return "reload";
    case kWebNavigationTypeBackForward:
    case kWebNavigationTypeFormResubmittedBackForward:
      return "back_forward";
    case kWebNavigationTypeLinkClicked:
    case kWebNavigationTypeFormSubmitted:
    case kWebNavigationTypeOther:
      return "navigate";
  }
  NOTREACHED();
  return "navigate";
}

AtomicString PerformanceNavigationTiming::initiatorType() const {
  return performance_entry_names::kNavigation;
}

bool PerformanceNavigationTiming::AllowRedirectDetails() const {
  if (!GetExecutionContext())
    return false;
  // TODO(sunjian): Think about how to make this flag deterministic.
  // crbug/693183.
  const blink::SecurityOrigin* security_origin =
      GetExecutionContext()->GetSecurityOrigin();
  return AllowNavigationTimingRedirect(resource_timing_info_->RedirectChain(),
                                       resource_timing_info_->FinalResponse(),
                                       *security_origin);
}
bool PerformanceNavigationTiming::AllowNegativeValue() const {
  return false;
}

AtomicString PerformanceNavigationTiming::AlpnNegotiatedProtocol() const {
  return resource_timing_info_->FinalResponse().AlpnNegotiatedProtocol();
}

AtomicString PerformanceNavigationTiming::ConnectionInfo() const {
  return resource_timing_info_->FinalResponse().ConnectionInfoString();
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventStart() const {
  bool allow_redirect_details = AllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();

  if (!allow_redirect_details || !timing ||
      !timing->CanRequestFromPreviousDocument())
    return 0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventStart(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventEnd() const {
  bool allow_redirect_details = AllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();

  if (!allow_redirect_details || !timing ||
      !timing->CanRequestFromPreviousDocument())
    return 0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventEnd(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domInteractive() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomInteractive(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventStart()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventStart(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventEnd()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventEnd(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domComplete() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomComplete(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventStart(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventEnd(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

AtomicString PerformanceNavigationTiming::type() const {
  if (DomWindow()) {
    return GetNavigationType(GetDocumentLoader()->GetNavigationType());
  }
  return "navigate";
}

uint16_t PerformanceNavigationTiming::redirectCount() const {
  bool allow_redirect_details = AllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return timing->RedirectCount();
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectStart() const {
  bool allow_redirect_details = AllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectStart(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectEnd() const {
  bool allow_redirect_details = AllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectEnd(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::fetchStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->FetchStart(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::responseEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->ResponseEnd(), AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

// Overriding PerformanceEntry's attributes.
DOMHighResTimeStamp PerformanceNavigationTiming::duration() const {
  return loadEventEnd();
}

ScriptValue PerformanceNavigationTiming::notRestoredReasons(
    ScriptState* script_state) const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader || !loader->GetFrame()->IsOutermostMainFrame())
    return ScriptValue::CreateNull(script_state->GetIsolate());

  // TODO(crbug.com/1370954): Save NotRestoredReasons in Document instead of
  // Frame.
  return NotRestoredReasonsBuilder(script_state,
                                   loader->GetFrame()->GetNotRestoredReasons());
}

ScriptValue PerformanceNavigationTiming::NotRestoredReasonsBuilder(
    ScriptState* script_state,
    const mojom::blink::BackForwardCacheNotRestoredReasonsPtr& reasons) const {
  if (!reasons)
    return ScriptValue::CreateNull(script_state->GetIsolate());
  V8ObjectBuilder builder(script_state);
  switch (reasons->blocked) {
    case mojom::blink::BFCacheBlocked::kYes:
    case mojom::blink::BFCacheBlocked::kNo:
      builder.AddBoolean(
          "blocked", reasons->blocked == mojom::blink::BFCacheBlocked::kYes);
      break;
    case mojom::blink::BFCacheBlocked::kMasked:
      // |blocked| can be null when masking the value.
      builder.AddNull("blocked");
      break;
  }
  builder.AddString("url", AtomicString(reasons->same_origin_details
                                            ? reasons->same_origin_details->url
                                            : ""));
  builder.AddString("src", AtomicString(reasons->same_origin_details
                                            ? reasons->same_origin_details->src
                                            : ""));
  builder.AddString("id", AtomicString(reasons->same_origin_details
                                           ? reasons->same_origin_details->id
                                           : ""));
  builder.AddString("name",
                    AtomicString(reasons->same_origin_details
                                     ? reasons->same_origin_details->name
                                     : ""));
  Vector<AtomicString> reason_strings;
  Vector<v8::Local<v8::Value>> children_result;
  if (reasons->same_origin_details) {
    for (const auto& reason : reasons->same_origin_details->reasons) {
      reason_strings.push_back(reason);
    }
    for (const auto& child : reasons->same_origin_details->children) {
      children_result.push_back(
          NotRestoredReasonsBuilder(script_state, child).V8Value());
    }
  }
  builder.Add("reasons", reason_strings);
  builder.Add("children", children_result);
  return builder.GetScriptValue();
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

  if (RuntimeEnabledFeatures::Prerender2RelatedFeaturesEnabled(
          ExecutionContext::From(builder.GetScriptState()))) {
    builder.AddNumber(
        "activationStart",
        PerformanceNavigationTimingActivationStart::activationStart(*this));
  }

  if (RuntimeEnabledFeatures::BackForwardCacheNotRestoredReasonsEnabled(
          ExecutionContext::From(builder.GetScriptState()))) {
    builder.Add("notRestoredReasons",
                notRestoredReasons(builder.GetScriptState()));
    ExecutionContext::From(builder.GetScriptState())
        ->CountUse(WebFeature::kBackForwardCacheNotRestoredReasons);
  }
}

}  // namespace blink
