// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"

#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fence_event.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

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

  // This is only relevant for fenced frames based on ShadowDOM, since it has
  // to do with the `FramePolicy::is_fenced` bit.
  // TODO(crbug.com/1123606): Handle this case for fenced frames based on
  // MPArch.
  Frame* top = DomWindow()->GetFrame()->Top(FrameTreeBoundary::kFenced);
  DCHECK(top);
  if (top->Owner() && top->Owner()->GetFramePolicy().is_fenced &&
      top->Owner()->GetFramePolicy().fenced_frame_mode !=
          mojom::blink::FencedFrameMode::kOpaqueAds) {
    DomWindow()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "fence.reportEvent is only available in the 'opaque-ads' "
        "mode."));
    return;
  }
}

}  // namespace blink
