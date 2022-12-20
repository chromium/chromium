// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
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

void Fence::reportEvent(ScriptState* script_state,
                        const FenceEvent* event,
                        ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "May not use a Fence object associated with a Document that is not "
        "fully active");
    return;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame);

  LocalFrame* fenced_frame = nullptr;
  if (blink::features::IsAllowURNsInIframeEnabled() &&
      !frame->IsInFencedFrameTree()) {
    // The only way to get a Fence outside a fenced frame is from
    // LocalDOMWindow::fence(), when both:
    // - blink::features::IsAllowURNsInIframeEnabled() is true
    // - the Document itself was loaded from a urn:uuid
    // In that case, pretend that the frame is a fenced frame root for this
    // temporary experiment.
    // TODO(crbug.com/1123606): Disable window.fence.reportEvent in iframes.
    // In order to disable, run the else branch unconditionally.
    // Also remove the features.h include above.
    fenced_frame = frame;
  } else {
    DCHECK(frame->IsInFencedFrameTree());

    if (frame->GetFencedFrameMode() !=
        mojom::blink::FencedFrameMode::kOpaqueAds) {
      AddConsoleMessage(
          "fence.reportEvent is only available in the 'opaque-ads' mode.");
      return;
    }

    Frame* possibly_remote_fenced_frame = DomWindow()->GetFrame()->Top();
    if (!frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            possibly_remote_fenced_frame->GetSecurityContext()
                ->GetSecurityOrigin())) {
      AddConsoleMessage(
          "fence.reportEvent is only available in same-origin subframes.");
      return;
    }
    fenced_frame = DynamicTo<LocalFrame>(possibly_remote_fenced_frame);
  }

  DCHECK(fenced_frame);
  DCHECK(fenced_frame->GetDocument());

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
