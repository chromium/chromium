// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fence_event.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

mojom::blink::ReportingDestination ToMojom(
    const V8FenceReportingDestination& destination) {
  switch (destination.AsEnum()) {
    case V8FenceReportingDestination::Enum::kBuyer:
      return mojom::blink::ReportingDestination::kBuyer;
    case V8FenceReportingDestination::Enum::kSeller:
      return mojom::blink::ReportingDestination::kSeller;
    case V8FenceReportingDestination::Enum::kComponentSeller:
      return mojom::blink::ReportingDestination::kComponentSeller;
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
  DCHECK(frame->IsInFencedFrameTree());

  if (frame->GetFencedFrameMode() !=
      mojom::blink::FencedFrameMode::kOpaqueAds) {
    AddConsoleMessage(
        "fence.reportEvent is only available in the 'opaque-ads' mode.");
    return;
  }

  LocalFrame* fenced_frame = DynamicTo<LocalFrame>(
      DomWindow()->GetFrame()->Top(FrameTreeBoundary::kFenced));
  DCHECK(fenced_frame);
  DCHECK(fenced_frame->GetDocument());

  const mojom::blink::FencedFrameReportingPtr& fenced_frame_reporting =
      fenced_frame->GetDocument()->Loader()->FencedFrameReporting();
  if (!fenced_frame_reporting) {
    AddConsoleMessage("This frame did not register reporting metadata.");
    return;
  }

  for (const V8FenceReportingDestination& web_destination :
       event->destination()) {
    mojom::blink::ReportingDestination destination = ToMojom(web_destination);

    const auto metadata_iter =
        fenced_frame_reporting->metadata.find(destination);
    if (metadata_iter == fenced_frame_reporting->metadata.end()) {
      AddConsoleMessage(
          "This frame did not register reporting metadata for "
          "destination '" +
          web_destination.AsString() + "'.");
      continue;
    }

    const auto url_iter = metadata_iter->value.find(event->eventType());
    if (url_iter == metadata_iter->value.end()) {
      AddConsoleMessage(
          "This frame did not register reporting url for "
          "destination '" +
          web_destination.AsString() + "' and event_type '" +
          event->eventType() + "'.");
      continue;
    }

    KURL url = url_iter->value;
    if (!url.IsValid() || !url.ProtocolIsInHTTPFamily()) {
      AddConsoleMessage(
          "This frame registered invalid reporting url for "
          "destination '" +
          web_destination.AsString() + "' and event_type '" +
          event->eventType() + "'.");
      continue;
    }

    PingLoader::SendBeacon(*script_state, frame, url, event->eventData());
  }
}

void Fence::AddConsoleMessage(const String& message) {
  DCHECK(DomWindow());
  DomWindow()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning, message));
}

}  // namespace blink
