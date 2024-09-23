// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"

#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
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
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/delivery_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

using network::mojom::blink::NavigationDeliveryType;

namespace {

String GetSystemEntropy(DocumentLoader* loader) {
  if (loader) {
    switch (loader->GetTiming().SystemEntropyAtNavigationStart()) {
      case mojom::blink::SystemEntropy::kHigh:
        CHECK(loader->GetFrame()->IsOutermostMainFrame());
        return "high";
      case mojom::blink::SystemEntropy::kNormal:
        CHECK(loader->GetFrame()->IsOutermostMainFrame());
        return "normal";
      case mojom::blink::SystemEntropy::kEmpty:
        CHECK(!loader->GetFrame()->IsOutermostMainFrame());
        return g_empty_string;
    }
  }

  return g_empty_string;
}

}  // namespace

PerformanceNavigationTiming::PerformanceNavigationTiming(
    LocalDOMWindow& window,
    mojom::blink::ResourceTimingInfoPtr resource_timing,
    base::TimeTicks time_origin)
    : PerformanceResourceTiming(std::move(resource_timing),
                                AtomicString("navigation"),
                                time_origin,
                                window.CrossOriginIsolatedCapability(),
                                &window),
      ExecutionContextClient(&window) {}

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

void PerformanceNavigationTiming::OnBodyLoadFinished(
    int64_t encoded_body_size,
    int64_t decoded_body_size) {
  UpdateBodySizes(encoded_body_size, decoded_body_size);
}

bool PerformanceNavigationTiming::AllowRedirectDetails() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  return timing && !timing->HasCrossOriginRedirect();
}

DocumentLoader* PerformanceNavigationTiming::GetDocumentLoader() const {
  return DomWindow() ? DomWindow()->document()->Loader() : nullptr;
}

const DocumentTiming* PerformanceNavigationTiming::GetDocumentTiming() const {
  return DomWindow() ? &DomWindow()->document()->GetTiming() : nullptr;
}

AtomicString PerformanceNavigationTiming::GetNavigationTimingType(
    WebNavigationType type) {
  switch (type) {
    case kWebNavigationTypeReload:
    case kWebNavigationTypeFormResubmittedReload:
      return AtomicString("reload");
    case kWebNavigationTypeBackForward:
    case kWebNavigationTypeFormResubmittedBackForward:
    case kWebNavigationTypeRestore:
      return AtomicString("back_forward");
    case kWebNavigationTypeLinkClicked:
    case kWebNavigationTypeFormSubmitted:
    case kWebNavigationTypeOther:
      return AtomicString("navigate");
  }
  NOTREACHED_IN_MIGRATION();
  return AtomicString("navigate");
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!AllowRedirectDetails() || !timing ||
      !timing->CanRequestFromPreviousDocument()) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventStart(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();

  if (!AllowRedirectDetails() || !timing ||
      !timing->CanRequestFromPreviousDocument()) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventEnd(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domInteractive() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomInteractive(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventStart()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventStart(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventEnd()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventEnd(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domComplete() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomComplete(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventStart(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventEnd(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

AtomicString PerformanceNavigationTiming::type() const {
  if (DomWindow()) {
    return GetNavigationTimingType(GetDocumentLoader()->GetNavigationType());
  }
  return AtomicString("navigate");
}

AtomicString PerformanceNavigationTiming::deliveryType() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader) {
    return GetDeliveryType();
  }

  switch (loader->GetNavigationDeliveryType()) {
    case NavigationDeliveryType::kDefault:
      return GetDeliveryType();
    case NavigationDeliveryType::kNavigationalPrefetch:
      return delivery_type_names::kNavigationalPrefetch;
    default:
      NOTREACHED_IN_MIGRATION();
      return g_empty_atom;
  }
}

uint16_t PerformanceNavigationTiming::redirectCount() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!AllowRedirectDetails() || !timing) {
    return 0;
  }
  return timing->RedirectCount();
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!AllowRedirectDetails() || !timing) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectStart(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!AllowRedirectDetails() || !timing) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectEnd(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::fetchStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->FetchStart(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::responseEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->ResponseEnd(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

// Overriding PerformanceEntry's attributes.
DOMHighResTimeStamp PerformanceNavigationTiming::duration() const {
  return loadEventEnd();
}

NotRestoredReasons* PerformanceNavigationTiming::notRestoredReasons() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader || !loader->GetFrame()->IsOutermostMainFrame()) {
    return nullptr;
  }

  return BuildNotRestoredReasons(loader->GetFrame()->GetNotRestoredReasons());
}

AtomicString PerformanceNavigationTiming::systemEntropy() const {
  if (DomWindow()) {
    blink::UseCounter::Count(DomWindow()->document(),
                             WebFeature::kPerformanceNavigateSystemEntropy);
  }

  return AtomicString(GetSystemEntropy(GetDocumentLoader()));
}

DOMHighResTimeStamp PerformanceNavigationTiming::criticalCHRestart(
    ScriptState* script_state) const {
  ExecutionContext::From(script_state)
      ->CountUse(WebFeature::kCriticalCHRestartNavigationTiming);
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing) {
    return 0.0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->CriticalCHRestart(), AllowNegativeValues(),
      CrossOriginIsolatedCapability());
}

NotRestoredReasons* PerformanceNavigationTiming::BuildNotRestoredReasons(
    const mojom::blink::BackForwardCacheNotRestoredReasonsPtr& nrr) const {
  if (!nrr) {
    return nullptr;
  }

  String url;
  HeapVector<Member<NotRestoredReasonDetails>> reasons;
  HeapVector<Member<NotRestoredReasons>> children;
  for (const auto& reason : nrr->reasons) {
    NotRestoredReasonDetails* detail =
        MakeGarbageCollected<NotRestoredReasonDetails>(reason->name);
    reasons.push_back(detail);
  }
  if (nrr->same_origin_details) {
    url = nrr->same_origin_details->url.GetString();
    for (const auto& child : nrr->same_origin_details->children) {
      NotRestoredReasons* nrr_child = BuildNotRestoredReasons(child);
      // Reasons in children vector should never be null.
      CHECK(nrr_child);
      children.push_back(nrr_child);
    }
  }

  HeapVector<Member<NotRestoredReasonDetails>>* reasons_to_report;
  if (nrr->same_origin_details) {
    // Expose same-origin reasons.
    reasons_to_report = &reasons;
  } else {
    if (reasons.size() == 0) {
      // If cross-origin iframes do not have any reasons, set the reasons to
      // nullptr.
      reasons_to_report = nullptr;
    } else {
      // If cross-origin iframes have reasons, that is "masked" for the randomly
      // selected one. Expose that reason.
      reasons_to_report = &reasons;
    }
  }

  NotRestoredReasons* not_restored_reasons =
      MakeGarbageCollected<NotRestoredReasons>(
          /*src=*/nrr->src,
          /*id=*/nrr->id,
          /*name=*/nrr->name, /*url=*/url,
          /*reasons=*/reasons_to_report,
          nrr->same_origin_details ? &children : nullptr);
  return not_restored_reasons;
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
  builder.AddNumber(
      "activationStart",
      PerformanceNavigationTimingActivationStart::activationStart(*this));
  builder.AddNumber("criticalCHRestart",
                    criticalCHRestart(builder.GetScriptState()));

  if (RuntimeEnabledFeatures::BackForwardCacheNotRestoredReasonsEnabled(
          ExecutionContext::From(builder.GetScriptState()))) {
    if (auto* not_restored_reasons = notRestoredReasons()) {
      builder.Add("notRestoredReasons", not_restored_reasons);
    } else {
      builder.AddNull("notRestoredReasons");
    }
    ExecutionContext::From(builder.GetScriptState())
        ->CountUse(WebFeature::kBackForwardCacheNotRestoredReasons);
  }

  if (RuntimeEnabledFeatures::PerformanceNavigateSystemEntropyEnabled(
          ExecutionContext::From(builder.GetScriptState()))) {
    builder.AddString("systemEntropy", GetSystemEntropy(GetDocumentLoader()));
  }
}

}  // namespace blink
