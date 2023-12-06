// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fence_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_fenceevent_string.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

blink::FencedFrame::ReportingDestination ToPublicDestination(
    const V8FenceReportingDestination& destination) {
  switch (destination.AsEnum()) {
    case V8FenceReportingDestination::Enum::kBuyer:
      return blink::FencedFrame::ReportingDestination::kBuyer;
    case V8FenceReportingDestination::Enum::kSeller:
      return blink::FencedFrame::ReportingDestination::kSeller;
    case V8FenceReportingDestination::Enum::kComponentSeller:
      return blink::FencedFrame::ReportingDestination::kComponentSeller;
    case V8FenceReportingDestination::Enum::kDirectSeller:
      return blink::FencedFrame::ReportingDestination::kDirectSeller;
    case V8FenceReportingDestination::Enum::kSharedStorageSelectUrl:
      return blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl;
  }
}

absl::optional<mojom::blink::AutomaticBeaconType> GetAutomaticBeaconType(
    const WTF::String& input) {
  if (input == blink::kDeprecatedFencedFrameTopNavigationBeaconType) {
    return mojom::blink::AutomaticBeaconType::kDeprecatedTopNavigation;
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kFencedFramesM120FeaturesPart2)) {
    if (input == blink::kFencedFrameTopNavigationStartBeaconType) {
      return mojom::blink::AutomaticBeaconType::kTopNavigationStart;
    }
    if (input == blink::kFencedFrameTopNavigationCommitBeaconType) {
      return mojom::blink::AutomaticBeaconType::kTopNavigationCommit;
    }
  }
  return absl::nullopt;
}

}  // namespace

Fence::Fence(LocalDOMWindow& window) : ExecutionContextClient(&window) {}

void Fence::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void Fence::reportEvent(ScriptState* script_state,
                        const V8UnionFenceEventOrString* event,
                        ExceptionState& exception_state) {
  switch (event->GetContentType()) {
    case V8UnionFenceEventOrString::ContentType::kString:
      reportPrivateAggregationEvent(script_state, event->GetAsString(),
                                    exception_state);
      return;
    case V8UnionFenceEventOrString::ContentType::kFenceEvent:
      reportEvent(script_state, event->GetAsFenceEvent(), exception_state);
      return;
  }
}

void Fence::reportEvent(ScriptState* script_state,
                        const FenceEvent* event,
                        ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a Fence object associated with a Document that is not "
        "fully active");
    return;
  }

  if (event->hasDestinationURL() &&
      base::FeatureList::IsEnabled(
          blink::features::kAdAuctionReportingWithMacroApi)) {
    reportEventToDestinationURL(script_state, event, exception_state);
  } else {
    reportEventToDestinationEnum(script_state, event, exception_state);
  }
}

void Fence::reportEventToDestinationEnum(ScriptState* script_state,
                                         const FenceEvent* event,
                                         ExceptionState& exception_state) {
  if (!event->hasDestination()) {
    exception_state.ThrowTypeError("Missing required 'destination' property.");
    return;
  }
  if (!event->hasEventType()) {
    exception_state.ThrowTypeError("Missing required 'eventType' property.");
    return;
  }

  if (event->hasEventData() &&
      event->eventData().length() > blink::kFencedFrameMaxBeaconLength) {
    exception_state.ThrowSecurityError(
        "The data provided to reportEvent() exceeds the maximum length, which "
        "is 64KB.");
    return;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());
  bool has_fenced_frame_reporting =
      frame->GetDocument()->Loader()->FencedFrameProperties().has_value() &&
      frame->GetDocument()
          ->Loader()
          ->FencedFrameProperties()
          ->has_fenced_frame_reporting();
  if (!has_fenced_frame_reporting) {
    AddConsoleMessage("This frame did not register reporting metadata.");
    return;
  }

  WTF::Vector<blink::FencedFrame::ReportingDestination> destinations;
  destinations.reserve(event->destination().size());
  base::ranges::transform(event->destination(),
                          std::back_inserter(destinations),
                          ToPublicDestination);

  network::AttributionReportingRuntimeFeatures
      attribution_reporting_runtime_features;
  if (AttributionSrcLoader* attribution_src_loader =
          frame->GetAttributionSrcLoader()) {
    attribution_reporting_runtime_features =
        attribution_src_loader->GetRuntimeFeatures();
  }
  frame->GetLocalFrameHostRemote().SendFencedFrameReportingBeacon(
      event->getEventDataOr(String{""}), event->eventType(), destinations,
      attribution_reporting_runtime_features);
}

void Fence::reportEventToDestinationURL(ScriptState* script_state,
                                        const FenceEvent* event,
                                        ExceptionState& exception_state) {
  if (event->hasEventType()) {
    exception_state.ThrowTypeError(
        "When reporting to a custom destination URL, 'eventType' is not "
        "allowed.");
    return;
  }
  if (event->hasEventData()) {
    exception_state.ThrowTypeError(
        "When reporting to a custom destination URL, 'eventData' is not "
        "allowed.");
    return;
  }
  if (event->hasDestination()) {
    exception_state.ThrowTypeError(
        "When reporting to a custom destination URL, 'destination' is not "
        "allowed.");
    return;
  }
  if (event->destinationURL().length() > blink::kFencedFrameMaxBeaconLength) {
    exception_state.ThrowSecurityError(
        "The destination URL provided to reportEvent() exceeds the maximum "
        "length, which is 64KB.");
    return;
  }

  KURL destinationURL(event->destinationURL());
  if (!destinationURL.IsValid()) {
    exception_state.ThrowTypeError(
        "The destination URL provided to reportEvent() is not a valid URL.");
    return;
  }
  if (!destinationURL.ProtocolIs(url::kHttpsScheme)) {
    exception_state.ThrowTypeError(
        "The destination URL provided to reportEvent() does not have the "
        "required scheme (https).");
    return;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());
  bool has_fenced_frame_reporting =
      frame->GetDocument()->Loader()->FencedFrameProperties().has_value() &&
      frame->GetDocument()
          ->Loader()
          ->FencedFrameProperties()
          ->has_fenced_frame_reporting();
  if (!has_fenced_frame_reporting) {
    AddConsoleMessage("This frame did not register reporting metadata.");
    return;
  }

  network::AttributionReportingRuntimeFeatures
      attribution_reporting_runtime_features;
  if (AttributionSrcLoader* attribution_src_loader =
          frame->GetAttributionSrcLoader()) {
    attribution_reporting_runtime_features =
        attribution_src_loader->GetRuntimeFeatures();
  }
  frame->GetLocalFrameHostRemote().SendFencedFrameReportingBeaconToCustomURL(
      destinationURL, attribution_reporting_runtime_features);
}

void Fence::setReportEventDataForAutomaticBeacons(
    ScriptState* script_state,
    const FenceEvent* event,
    ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a Fence object associated with a Document that is not "
        "fully active");
    return;
  }
  if (!event->hasDestination()) {
    exception_state.ThrowTypeError("Missing required 'destination' property.");
    return;
  }
  if (!event->hasEventType()) {
    exception_state.ThrowTypeError("Missing required 'eventType' property.");
    return;
  }
  absl::optional<mojom::blink::AutomaticBeaconType> beacon_type =
      GetAutomaticBeaconType(event->eventType());
  if (!beacon_type.has_value()) {
    AddConsoleMessage(event->eventType() +
                      " is not a valid automatic beacon event type.");
    return;
  }
  if (event->hasEventData() &&
      event->eventData().length() > blink::kFencedFrameMaxBeaconLength) {
    exception_state.ThrowSecurityError(
        "The data provided to setReportEventDataForAutomaticBeacons() exceeds "
        "the maximum length, which is 64KB.");
    return;
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kFencedFramesM120FeaturesPart2) &&
      event->eventType() ==
          blink::kDeprecatedFencedFrameTopNavigationBeaconType) {
    AddConsoleMessage(event->eventType() + " is deprecated in favor of " +
                          kFencedFrameTopNavigationCommitBeaconType + ".",
                      mojom::blink::ConsoleMessageLevel::kWarning);
  }
  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());
  bool has_fenced_frame_reporting =
      frame->GetDocument()->Loader()->FencedFrameProperties().has_value() &&
      frame->GetDocument()
          ->Loader()
          ->FencedFrameProperties()
          ->has_fenced_frame_reporting();
  if (!has_fenced_frame_reporting) {
    AddConsoleMessage("This frame did not register reporting metadata.");
    return;
  }

  WTF::Vector<blink::FencedFrame::ReportingDestination> destinations;
  destinations.reserve(event->destination().size());
  base::ranges::transform(event->destination(),
                          std::back_inserter(destinations),
                          ToPublicDestination);

  // TODO(crbug.com/1485222) The attribution reporting runtime features should
  // be sent to the browser separately from the automatic beacon data, since
  // it's not guaranteed that any beacon data will be set before an automatic
  // beacon is sent out.
  network::AttributionReportingRuntimeFeatures
      attribution_reporting_runtime_features;
  if (AttributionSrcLoader* attribution_src_loader =
          frame->GetAttributionSrcLoader()) {
    attribution_reporting_runtime_features =
        attribution_src_loader->GetRuntimeFeatures();
  }
  frame->GetLocalFrameHostRemote().SetFencedFrameAutomaticBeaconReportEventData(
      beacon_type.value(), event->getEventDataOr(String{""}), destinations,
      attribution_reporting_runtime_features, event->once(),
      event->crossOriginExposed());
}

HeapVector<Member<FencedFrameConfig>> Fence::getNestedConfigs(
    ExceptionState& exception_state) {
  HeapVector<Member<FencedFrameConfig>> out;
  const absl::optional<FencedFrame::RedactedFencedFrameProperties>&
      fenced_frame_properties =
          DomWindow()->document()->Loader()->FencedFrameProperties();
  if (fenced_frame_properties.has_value() &&
      fenced_frame_properties.value().nested_urn_config_pairs() &&
      fenced_frame_properties.value()
          .nested_urn_config_pairs()
          ->potentially_opaque_value) {
    for (const std::pair<GURL, FencedFrame::RedactedFencedFrameConfig>&
             config_pair : fenced_frame_properties.value()
                               .nested_urn_config_pairs()
                               ->potentially_opaque_value.value()) {
      FencedFrame::RedactedFencedFrameConfig config = config_pair.second;
      out.push_back(FencedFrameConfig::From(config));
    }
  }
  return out;
}

void Fence::reportPrivateAggregationEvent(ScriptState* script_state,
                                          const String& event,
                                          ExceptionState& exception_state) {
  if (!base::FeatureList::IsEnabled(blink::features::kPrivateAggregationApi) ||
      !blink::features::kPrivateAggregationApiEnabledInProtectedAudience
           .Get() ||
      !blink::features::kPrivateAggregationApiProtectedAudienceExtensionsEnabled
           .Get()) {
    exception_state.ThrowSecurityError(
        "FLEDGE extensions must be enabled to use reportEvent() for private "
        "aggregation events.");
    return;
  }
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a Fence object associated with a Document that is not "
        "fully active");
    return;
  }

  if (event.StartsWith(blink::kFencedFrameReservedPAEventPrefix)) {
    AddConsoleMessage("Reserved events cannot be triggered manually.");
    return;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());

  bool has_fenced_frame_reporting =
      frame->GetDocument()->Loader()->FencedFrameProperties().has_value() &&
      frame->GetDocument()
          ->Loader()
          ->FencedFrameProperties()
          ->has_fenced_frame_reporting();
  if (!has_fenced_frame_reporting) {
    AddConsoleMessage("This frame did not register reporting metadata.");
    return;
  }

  frame->GetLocalFrameHostRemote()
      .SendPrivateAggregationRequestsForFencedFrameEvent(event);
}

void Fence::AddConsoleMessage(const String& message,
                              mojom::blink::ConsoleMessageLevel level) {
  DCHECK(DomWindow());
  DomWindow()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript, level, message));
}

}  // namespace blink
