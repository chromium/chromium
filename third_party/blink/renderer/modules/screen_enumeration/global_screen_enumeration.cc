// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/global_screen_enumeration.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/screen_enumeration/screen_enumeration.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/display/mojom/display.mojom-blink.h"

namespace blink {

namespace {

void DidGetDisplays(ScriptPromiseResolver* resolver,
                    mojo::Remote<mojom::blink::ScreenEnumeration>,
                    mojom::blink::DisplaysPtr result) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  ScriptState::Scope scope(script_state);
  if (result.is_null()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "Screen information is unavailable or access is not allowed."));
    return;
  }

  HeapVector<Member<Screen>> screens;
  screens.ReserveInitialCapacity(result->displays.size());
  for (display::mojom::blink::DisplayPtr& display : result->displays) {
    const bool internal = display->id == result->internal_id;
    const bool primary = display->id == result->primary_id;
    // TODO(http://crbug.com/994889): Implement temporary, generated per-origin
    // unique device IDs that reset when cookies are deleted. See related:
    // web_bluetooth_device_id.h, unguessable_token.h, and uuid.h
    const String id = String::NumberToStringECMAScript(screens.size());
    screens.emplace_back(MakeGarbageCollected<Screen>(std::move(display),
                                                      internal, primary, id));
  }
  resolver->Resolve(std::move(screens));
}

void DidHasMultipleDisplays(ScriptPromiseResolver* resolver,
                            mojo::Remote<mojom::blink::ScreenEnumeration>,
                            mojom::blink::MultipleDisplays result) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  ScriptState::Scope scope(script_state);
  if (result == mojom::blink::MultipleDisplays::kError) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "Screen information is unavailable or access is not allowed."));
    return;
  }

  resolver->Resolve(result == mojom::blink::MultipleDisplays::kTrue);
}

}  // namespace

// static
ScriptPromise GlobalScreenEnumeration::getScreensDeprecated(
    ScriptState* script_state,
    LocalDOMWindow&,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  // TODO(msw): Cache the backend connection.
  mojo::Remote<mojom::blink::ScreenEnumeration> backend;
  ExecutionContext::From(script_state)
      ->GetBrowserInterfaceBroker()
      .GetInterface(backend.BindNewPipeAndPassReceiver());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* raw_backend = backend.get();
  raw_backend->GetDisplays(
      WTF::Bind(&DidGetDisplays, WrapPersistent(resolver), std::move(backend)));
  return resolver->Promise();
}

// static
ScriptPromise GlobalScreenEnumeration::isMultiScreen(
    ScriptState* script_state,
    LocalDOMWindow&,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  // TODO(msw): Cache the backend connection.
  mojo::Remote<mojom::blink::ScreenEnumeration> backend;
  ExecutionContext::From(script_state)
      ->GetBrowserInterfaceBroker()
      .GetInterface(backend.BindNewPipeAndPassReceiver());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto* raw_backend = backend.get();
  raw_backend->HasMultipleDisplays(WTF::Bind(
      &DidHasMultipleDisplays, WrapPersistent(resolver), std::move(backend)));
  return resolver->Promise();
}

}  // namespace blink
