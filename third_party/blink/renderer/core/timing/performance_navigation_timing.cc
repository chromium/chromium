// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"

#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
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

PerformanceNavigationTiming::PerformanceNavigationTiming(
    LocalDOMWindow& window,
    ResourceTimingInfo& info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    HeapVector<Member<PerformanceServerTiming>> server_timing)
    : PerformanceResourceTiming(info,
                                performance_entry_names::kNavigation,
                                time_origin,
                                cross_origin_isolated_capability,
                                std::move(server_timing),
                                window),
      ExecutionContextClient(&window) {
  Info()->SetAllowRedirectDetails(
      !GetDocumentLoadTiming()->HasCrossOriginRedirect());
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
  if (!loader) {
    return nullptr;
  }

  return &loader->GetTiming();
}

DocumentLoader* PerformanceNavigationTiming::GetDocumentLoader() const {
  return DomWindow() ? DomWindow()->document()->Loader() : nullptr;
}

const DocumentTiming* PerformanceNavigationTiming::GetDocumentTiming() const {
  return DomWindow() ? &DomWindow()->document()->GetTiming() : nullptr;
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

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!Info()->AllowRedirectDetails() || !timing ||
      !timing->CanRequestFromPreviousDocument()) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventStart(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();

  if (!Info()->AllowRedirectDetails() || !timing ||
      !timing->CanRequestFromPreviousDocument()) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventEnd(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domInteractive() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomInteractive(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventStart()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventStart(),
      Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventEnd()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventEnd(),
      Info()->AllowNegativeValue(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domComplete() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomComplete(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventStart(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventEnd(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

AtomicString PerformanceNavigationTiming::type() const {
  if (DomWindow()) {
    return GetNavigationType(GetDocumentLoader()->GetNavigationType());
  }
  return "navigate";
}

uint16_t PerformanceNavigationTiming::redirectCount() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!Info()->AllowRedirectDetails() || !timing) {
    return 0;
  }
  return timing->RedirectCount();
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!Info()->AllowRedirectDetails() || !timing) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectStart(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!Info()->AllowRedirectDetails() || !timing) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectEnd(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::fetchStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->FetchStart(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::responseEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->ResponseEnd(), Info()->AllowNegativeValue(),
      CrossOriginIsolatedCapability());
}

// Overriding PerformanceEntry's attributes.
DOMHighResTimeStamp PerformanceNavigationTiming::duration() const {
  return loadEventEnd();
}

ScriptValue PerformanceNavigationTiming::notRestoredReasons(
    ScriptState* script_state) const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader || !loader->GetFrame()->IsOutermostMainFrame()) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  // TODO(crbug.com/1370954): Save NotRestoredReasons in Document instead of
  // Frame.
  return NotRestoredReasonsBuilder(script_state,
                                   loader->GetFrame()->GetNotRestoredReasons());
}

ScriptValue PerformanceNavigationTiming::NotRestoredReasonsBuilder(
    ScriptState* script_state,
    const mojom::blink::BackForwardCacheNotRestoredReasonsPtr& reasons) const {
  if (!reasons) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
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
  builder.AddStringOrNull("src", AtomicString(reasons->src));
  builder.AddStringOrNull("id", AtomicString(reasons->id));
  builder.AddStringOrNull("name", AtomicString(reasons->name));
  Vector<AtomicString> reason_strings;
  Vector<v8::Local<v8::Value>> children_result;
  if (reasons->same_origin_details) {
    builder.AddString("url", AtomicString(reasons->same_origin_details->url));
    for (const auto& reason : reasons->same_origin_details->reasons) {
      reason_strings.push_back(reason);
    }
    for (const auto& child : reasons->same_origin_details->children) {
      children_result.push_back(
          NotRestoredReasonsBuilder(script_state, child).V8Value());
    }
  } else {
    // For cross-origin iframes, url should always be null.
    builder.AddNull("url");
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
