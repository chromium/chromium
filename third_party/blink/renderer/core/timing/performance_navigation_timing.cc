// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"

#include "services/network/public/mojom/url_response_head.mojom-blink.h"
#include "third_party/blink/public/mojom/confidence_level.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_timing_confidence_value.h"
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

V8PerformanceTimingConfidenceValue::Enum GetNavigationConfidenceString(
    mojom::blink::ConfidenceLevel confidence) {
  return confidence == mojom::blink::ConfidenceLevel::kHigh
             ? V8PerformanceTimingConfidenceValue::Enum::kHigh
             : V8PerformanceTimingConfidenceValue::Enum::kLow;
}

}  // namespace

PerformanceNavigationTiming::PerformanceNavigationTiming(
    LocalDOMWindow& window,
    mojom::blink::ResourceTimingInfoPtr resource_timing,
    base::TimeTicks time_origin,
    uint32_t navigation_id)
    : PerformanceResourceTiming(std::move(resource_timing),
                                AtomicString("navigation"),
                                time_origin,
                                window.CrossOriginIsolatedCapability(),
                                &window,
                                navigation_id),
      ExecutionContextClient(&window),
      navigation_delivery_type_(
          window.document()->Loader()->GetNavigationDeliveryType()),
      navigation_type_(window.document()->Loader()->GetNavigationType()),
      document_timing_values_(
          window.document()->GetTiming().GetDocumentTimingValues()),
      document_load_timing_values_(window.document()
                                       ->Loader()
                                       ->GetTiming()
                                       .GetDocumentLoadTimingValues()) {}

PerformanceNavigationTiming::~PerformanceNavigationTiming() = default;

const AtomicString& PerformanceNavigationTiming::entryType() const {
  return performance_entry_names::kNavigation;
}

PerformanceEntryType PerformanceNavigationTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kNavigation;
}

void PerformanceNavigationTiming::Trace(Visitor* visitor) const {
  visitor->Trace(document_timing_values_);
  visitor->Trace(document_load_timing_values_);
  ExecutionContextClient::Trace(visitor);
  PerformanceResourceTiming::Trace(visitor);
}

void PerformanceNavigationTiming::OnBodyLoadFinished(
    int64_t encoded_body_size,
    int64_t decoded_body_size) {
  UpdateBodySizes(encoded_body_size, decoded_body_size);
}

DocumentLoader* PerformanceNavigationTiming::GetDocumentLoader() const {
  return DomWindow() ? DomWindow()->document()->Loader() : nullptr;
}

V8NavigationTimingType::Enum
PerformanceNavigationTiming::GetNavigationTimingType(WebNavigationType type) {
  switch (type) {
    case kWebNavigationTypeReload:
    case kWebNavigationTypeFormResubmittedReload:
      return V8NavigationTimingType::Enum::kReload;
    case kWebNavigationTypeBackForward:
    case kWebNavigationTypeFormResubmittedBackForward:
    case kWebNavigationTypeRestore:
      return V8NavigationTimingType::Enum::kBackForward;
    case kWebNavigationTypeLinkClicked:
    case kWebNavigationTypeFormSubmitted:
    case kWebNavigationTypeOther:
      return V8NavigationTimingType::Enum::kNavigate;
  }
  NOTREACHED();
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventStart() const {
  if (document_load_timing_values_->has_cross_origin_redirect ||
      !document_load_timing_values_->can_request_from_previous_document) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->unload_event_start,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventEnd() const {
  if (document_load_timing_values_->has_cross_origin_redirect ||
      !document_load_timing_values_->can_request_from_previous_document) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->unload_event_end,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domInteractive() const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_timing_values_->dom_interactive,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventStart()
    const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_timing_values_->dom_content_loaded_event_start,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventEnd()
    const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_timing_values_->dom_content_loaded_event_end,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domComplete() const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_timing_values_->dom_complete,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventStart() const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->load_event_start,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventEnd() const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->load_event_end,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

V8NavigationTimingType PerformanceNavigationTiming::type() const {
  return V8NavigationTimingType(GetNavigationTimingType(navigation_type_));
}

AtomicString PerformanceNavigationTiming::deliveryType() const {
  switch (navigation_delivery_type_) {
    case NavigationDeliveryType::kDefault:
      return GetDeliveryType();
    case NavigationDeliveryType::kNavigationalPrefetch:
      return delivery_type_names::kNavigationalPrefetch;
    default:
      NOTREACHED();
  }
}

uint16_t PerformanceNavigationTiming::redirectCount() const {
  if (document_load_timing_values_->has_cross_origin_redirect) {
    return 0;
  }

  return document_load_timing_values_->redirect_count;
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectStart() const {
  if (document_load_timing_values_->has_cross_origin_redirect) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->redirect_start,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectEnd() const {
  if (document_load_timing_values_->has_cross_origin_redirect) {
    return 0;
  }
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->redirect_end,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::fetchStart() const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->fetch_start,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp PerformanceNavigationTiming::responseEnd() const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->response_end,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
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

PerformanceTimingConfidence* PerformanceNavigationTiming::confidence() const {
  if (DomWindow()) {
    blink::UseCounter::Count(
        DomWindow()->document(),
        WebFeature::kPerformanceNavigationTimingConfidence);
  }

  return GetConfidence();
}

PerformanceTimingConfidence* PerformanceNavigationTiming::GetConfidence()
    const {
  std::optional<RandomizedConfidenceValue> confidence =
      document_load_timing_values_->randomized_confidence;
  if (!confidence) {
    return nullptr;
  }

  return MakeGarbageCollected<PerformanceTimingConfidence>(
      confidence->first,
      V8PerformanceTimingConfidenceValue(
          GetNavigationConfidenceString(confidence->second)));
}

DOMHighResTimeStamp PerformanceNavigationTiming::criticalCHRestart() const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), document_load_timing_values_->critical_ch_restart,
      AllowNegativeValues(), CrossOriginIsolatedCapability());
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
  builder.AddString("type", type().AsStringView());
  builder.AddNumber("redirectCount", redirectCount());
  builder.AddNumber(
      "activationStart",
      PerformanceNavigationTimingActivationStart::activationStart(*this));
  builder.AddNumber("criticalCHRestart", criticalCHRestart());

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

  if (RuntimeEnabledFeatures::PerformanceNavigationTimingConfidenceEnabled(
          ExecutionContext::From(builder.GetScriptState()))) {
    if (auto* confidence_value = GetConfidence()) {
      builder.Add("confidence", confidence_value);
    } else {
      builder.AddNull("confidence");
    }
  }
}

}  // namespace blink
