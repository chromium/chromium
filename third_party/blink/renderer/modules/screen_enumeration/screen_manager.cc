// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/screen_manager.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/display/mojom/display.mojom-blink.h"

namespace blink {

namespace {

void DidGetDisplays(
    ScriptPromiseResolver* resolver,
    WTF::Vector<display::mojom::blink::DisplayPtr> backend_displays,
    int64_t primary_id,
    bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  HeapVector<Member<Screen>> screens;
  screens.ReserveInitialCapacity(backend_displays.size());
  for (display::mojom::blink::DisplayPtr& backend_display : backend_displays) {
    const bool primary = backend_display->id == primary_id;
    screens.emplace_back(
        MakeGarbageCollected<Screen>(std::move(backend_display), primary));
  }
  resolver->Resolve(std::move(screens));
}

}  // namespace

ScreenManager::ScreenManager(
    mojo::Remote<mojom::blink::ScreenEnumeration> backend)
    : backend_(std::move(backend)) {
  backend_.set_disconnect_handler(WTF::Bind(
      &ScreenManager::OnBackendDisconnected, WrapWeakPersistent(this)));
}

ScriptPromise ScreenManager::getScreens(ScriptState* script_state,
                                        ExceptionState& exception_state) {
  if (!backend_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ScreenManager backend went away");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->GetDisplays(WTF::Bind(&DidGetDisplays, WrapPersistent(resolver)));

  return resolver->Promise();
}

void ScreenManager::OnBackendDisconnected() {
  backend_.reset();
}

}  // namespace blink
