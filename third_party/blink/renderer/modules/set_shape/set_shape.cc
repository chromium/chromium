// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/set_shape/set_shape.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

const char SetShape::kSupplementName[] = "SetShape";

// static
SetShape& SetShape::From(LocalDOMWindow& window) {
  SetShape* supplement = Supplement<LocalDOMWindow>::From<SetShape>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<SetShape>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

SetShape::SetShape(LocalDOMWindow& window)
    : Supplement(window), remote_(&window) {}

HeapMojoRemote<mojom::blink::SetShapeService>& SetShape::GetRemote() {
  if (!remote_.is_bound()) {
    LocalDOMWindow* window = GetSupplementable();
    window->GetBrowserInterfaceBroker().GetInterface(
        remote_.BindNewPipeAndPassReceiver(
            window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return remote_;
}

ScriptPromise<IDLUndefined> SetShape::setShape(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const HeapVector<Member<DOMRectReadOnly>>& rects,
    ExceptionState& exception_state) {
  if (window.GetFrame()) {
    MediaValues* media_values =
        MediaValues::CreateDynamicIfFrameExists(window.GetFrame());
    if (media_values) {
      mojom::blink::DisplayMode display_mode = media_values->DisplayMode();
      if (display_mode != mojom::blink::DisplayMode::kUnframed) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "setShape requires the window to be in unframed display mode.");
        return EmptyPromise();
      }
    }
  }

  if (rects.size() > mojom::blink::kMaxSetShapeRects) {
    exception_state.ThrowTypeError("Invalid number of rectangles.");
    return EmptyPromise();
  }

  Vector<gfx::Rect> converted_rects;
  converted_rects.reserve(rects.size());
  const gfx::Rect window_rect(0, 0, std::max(0, window.innerWidth()),
                              std::max(0, window.innerHeight()));
  bool has_minimum_size_rect = false;
  for (const auto& rect : rects) {
    if (!std::isfinite(rect->x()) || !std::isfinite(rect->y()) ||
        !std::isfinite(rect->width()) || !std::isfinite(rect->height())) {
      exception_state.ThrowTypeError(
          "Rectangle coordinates and dimensions must be finite.");
      return EmptyPromise();
    }

    if (rect->width() < 0 || rect->height() < 0) {
      exception_state.ThrowTypeError(
          "Rectangle dimensions must be non-negative.");
      return EmptyPromise();
    }

    gfx::Rect converted_rect(base::saturated_cast<int>(rect->x()),
                             base::saturated_cast<int>(rect->y()),
                             base::saturated_cast<int>(rect->width()),
                             base::saturated_cast<int>(rect->height()));

    gfx::Rect visible_rect = gfx::IntersectRects(converted_rect, window_rect);
    if (visible_rect.width() >= mojom::blink::kMinimumIwaSetShapeSize &&
        visible_rect.height() >= mojom::blink::kMinimumIwaSetShapeSize) {
      has_minimum_size_rect = true;
    }
    converted_rects.push_back(converted_rect);
  }

  if (!rects.empty() && !has_minimum_size_rect) {
    exception_state.ThrowTypeError(
        Format("At least one rectangle must be {}x{} or larger.",
               mojom::blink::kMinimumIwaSetShapeSize,
               mojom::blink::kMinimumIwaSetShapeSize));
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  From(window).GetRemote()->SetShape(
      std::move(converted_rects),
      resolver->WrapCallbackInScriptScope(
          BindOnce([](ScriptPromiseResolver<IDLUndefined>* resolver,
                      mojom::blink::SetShapeResult result) {
            switch (result) {
              case mojom::blink::SetShapeResult::kSuccess:
                resolver->Resolve();
                break;
              case mojom::blink::SetShapeResult::kNoWindow:
                resolver->RejectWithDOMException(
                    DOMExceptionCode::kInvalidStateError,
                    "The window could not be found.");
                break;
              case mojom::blink::SetShapeResult::kNotUnframed:
                resolver->RejectWithDOMException(
                    DOMExceptionCode::kInvalidStateError,
                    "setShape requires the window to be in unframed display "
                    "mode.");
                break;
              case mojom::blink::SetShapeResult::kOutOfBounds:
                resolver->RejectWithDOMException(
                    DOMExceptionCode::kInvalidStateError,
                    "The shape does not meet the minimum size requirement "
                    "within the window bounds.");
                break;
            }
          })));

  return promise;
}

void SetShape::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
