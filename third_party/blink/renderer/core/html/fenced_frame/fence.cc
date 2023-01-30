// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fence_event.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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

LocalFrame* Fence::GetAssociatedFencedFrameForReporting() {
  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame);

  if (blink::features::IsAllowURNsInIframeEnabled() &&
      !frame->IsInFencedFrameTree()) {
    DCHECK(frame->GetDocument());
    return frame;
  }

  // We can only reach this point if we are in a fenced frame tree. If we were
  // not in a fenced frame tree and `IsAllowURNsInIframeEnabled()` were
  // disabled, then `this` would not have been constructed in the first place.
  DCHECK(frame->IsInFencedFrameTree());

  if (frame->GetFencedFrameMode() !=
      mojom::blink::FencedFrameMode::kOpaqueAds) {
    AddConsoleMessage(
        "Fenced event reporting is only available in the 'opaque-ads' mode.");
    return nullptr;
  }

  Frame* top_frame = frame->Top();
  if (!frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          top_frame->GetSecurityContext()->GetSecurityOrigin())) {
    AddConsoleMessage(
        "Fenced event reporting is only available in same-origin subframes.");
    return nullptr;
  }

  LocalFrame* fenced_frame = DynamicTo<LocalFrame>(top_frame);
  DCHECK(fenced_frame);
  return fenced_frame;
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

  LocalFrame* fenced_frame = GetAssociatedFencedFrameForReporting();
  if (!fenced_frame) {
    return;
  }

  bool has_fenced_frame_reporting =
      fenced_frame->GetDocument()->Loader()->HasFencedFrameReporting();
  if (!has_fenced_frame_reporting) {
    AddConsoleMessage("This frame did not register reporting metadata.");
    return;
  }

  for (const V8FenceReportingDestination& web_destination :
       event->destination()) {
    fenced_frame->GetLocalFrameHostRemote().SendFencedFrameReportingBeacon(
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
  LocalFrame* fenced_frame = GetAssociatedFencedFrameForReporting();
  if (!fenced_frame) {
    return;
  }
  if (event->eventType() != "reserved.top_navigation") {
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
  bool has_fenced_frame_reporting =
      fenced_frame->GetDocument()->Loader()->HasFencedFrameReporting();
  if (!has_fenced_frame_reporting) {
    AddConsoleMessage("This frame did not register reporting metadata.");
    return;
  }
  WTF::Vector<blink::FencedFrame::ReportingDestination> destination_vector;
  for (const V8FenceReportingDestination& web_destination :
       event->destination()) {
    destination_vector.push_back(ToPublicDestination(web_destination));
  }
  fenced_frame->GetLocalFrameHostRemote()
      .SetFencedFrameAutomaticBeaconReportEventData(event->eventData(),
                                                    destination_vector);
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

void Fence::AddConsoleMessage(const String& message) {
  DCHECK(DomWindow());
  DomWindow()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kError, message));
}

}  // namespace blink
