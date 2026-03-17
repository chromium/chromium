// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/isolated_web_app/isolated_web_app.h"

#include <cmath>
#include <utility>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/chromeos/isolated_web_app_api_bridge.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

const char IsolatedWebApp::kSupplementName[] = "IsolatedWebApp";

// static
IsolatedWebApp& IsolatedWebApp::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  IsolatedWebApp* supplement =
      Supplement<ExecutionContext>::From<IsolatedWebApp>(execution_context);
  if (!supplement) {
    supplement = MakeGarbageCollected<IsolatedWebApp>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

IsolatedWebApp::IsolatedWebApp(ExecutionContext& execution_context)
    : Supplement(execution_context), remote_(&execution_context) {}

HeapMojoRemote<mojom::blink::IsolatedWebAppApiBridge>&
IsolatedWebApp::GetRemote() {
  if (!remote_.is_bound()) {
    ExecutionContext* context = GetSupplementable();
    context->GetBrowserInterfaceBroker().GetInterface(
        remote_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kInternalLoading)));
  }
  return remote_;
}

ScriptPromise<IDLUndefined> IsolatedWebApp::setShape(
    ScriptState* script_state,
    const HeapVector<Member<DOMRectReadOnly>>& rects,
    ExceptionState& exception_state) {
  if (rects.size() > mojom::blink::kMaxSetShapeRects) {
    exception_state.ThrowTypeError("Invalid number of rectangles.");
    return EmptyPromise();
  }

  Vector<gfx::Rect> converted_rects;
  converted_rects.reserve(rects.size());
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

    converted_rects.push_back(
        gfx::Rect(base::saturated_cast<int>(rect->x()),
                  base::saturated_cast<int>(rect->y()),
                  base::saturated_cast<int>(rect->width()),
                  base::saturated_cast<int>(rect->height())));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  GetRemote()->SetShape(
      std::move(converted_rects),
      resolver->WrapCallbackInScriptScope(
          BindOnce([](ScriptPromiseResolver<IDLUndefined>* resolver,
                      mojom::blink::SetShapeResult result) {
            switch (result) {
              case mojom::blink::SetShapeResult::kSuccess:
                resolver->Resolve();
                break;
              case mojom::blink::SetShapeResult::kInvalidLength:
                resolver->RejectWithTypeError("Invalid number of rectangles.");
                break;
              case mojom::blink::SetShapeResult::kNoWindow:
                resolver->RejectWithDOMException(
                    DOMExceptionCode::kInvalidStateError,
                    "The window could not be found.");
                break;
            }
          })));

  return promise;
}

void IsolatedWebApp::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  ScriptWrappable::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
