// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"

#include "base/feature_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fence_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_fenceevent_string.h"
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
  if (event->eventData().length() > blink::kFencedFrameMaxBeaconLength) {
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

  for (const V8FenceReportingDestination& web_destination :
       event->destination()) {
    frame->GetLocalFrameHostRemote().SendFencedFrameReportingBeacon(
        event->eventData(), event->eventType(),
        ToPublicDestination(web_destination));
  }
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
  if (event->eventType() != blink::kFencedFrameTopNavigationBeaconType) {
    AddConsoleMessage(event->eventType() +
                      " is not a valid automatic beacon event type.");
    return;
  }
  if (event->eventData().length() > blink::kFencedFrameMaxBeaconLength) {
    exception_state.ThrowSecurityError(
        "The data provided to setReportEventDataForAutomaticBeacons() exceeds "
        "the maximum length, which is 64KB.");
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
  WTF::Vector<blink::FencedFrame::ReportingDestination> destination_vector;
  for (const V8FenceReportingDestination& web_destination :
       event->destination()) {
    destination_vector.push_back(ToPublicDestination(web_destination));
  }
  frame->GetLocalFrameHostRemote().SetFencedFrameAutomaticBeaconReportEventData(
      event->eventData(), destination_vector);
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
      !blink::features::kPrivateAggregationApiFledgeExtensionsEnabled.Get()) {
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

void Fence::AddConsoleMessage(const String& message) {
  DCHECK(DomWindow());
  DomWindow()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kError, message));
}

}  // namespace blink
