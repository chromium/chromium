// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url/dom_origin.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_anchor_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_area_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worker_global_scope.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worker_location.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/url/dom_url.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Static `::Create()` methods:
DOMOrigin* DOMOrigin::Create() {
  return MakeGarbageCollected<DOMOrigin>(SecurityOrigin::CreateUniqueOpaque());
}

// static
DOMOrigin* DOMOrigin::parse(const String& value) {
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::CreateFromString(value);

  // SecurityOrigin::CreateFromString will accept a wide variety of inputs, as
  // it routes things through URL parsing before minting an origin out of the
  // result. We'd like to ensure that the web-facing API requires a properly
  // serialized origin, so we check here to verify that the value we provided
  // matches the serialization of the SecurityOrigin we received.
  if (security_origin->ToString() != value) {
    return nullptr;
  }
  return MakeGarbageCollected<DOMOrigin>(std::move(security_origin));
}

// static
DOMOrigin* DOMOrigin::fromURL(const String& serialized_url) {
  KURL url(serialized_url);
  if (!url.IsValid()) {
    return nullptr;
  }
  return MakeGarbageCollected<DOMOrigin>(SecurityOrigin::Create(url));
}

// static
DOMOrigin* DOMOrigin::from(ScriptState* script_state,
                           ScriptValue script_value,
                           ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> v8_value = script_value.V8Value();

  // If we're given a string, try to parse it as a URL, and extract an origin
  // from it if possible.
  if (v8_value->IsString()) {
    String serialized_url = ToCoreString(isolate, v8_value.As<v8::String>());

    KURL parsed_url(serialized_url);
    if (!parsed_url.IsValid()) {
      exception_state.ThrowTypeError(
          "The string provided cannot be parsed as a serialized URL.");
      return nullptr;
    }
    return MakeGarbageCollected<DOMOrigin>(SecurityOrigin::Create(parsed_url));
  }

  // If it's not a string, check whether it's an object. If it's not, throw an
  // exception to exit early.
  if (!v8_value->IsObject()) {
    exception_state.ThrowTypeError(
        "An origin cannot be extracted from the given parameter.");
    return nullptr;
  }

  // Given that we're dealing with an object, walk through all the types from
  // which we know how to construct an Origin. These include:
  //
  // * ExtendableMessageEvent
  // * HTMLHyperlinkElementUtils
  // * Location
  // * MessageEvent
  // * Origin
  // * URL
  // * WindowOrWorkerGlobalScope
  // * WorkerLocation
  //
  // For each of these platform object interfaces, we'll check to see whether
  // the value we've been given is an instance of the underlying type we're
  // going to poke at, and extract an origin if so. If nothing matches, we'll
  // fall through to throw a `TypeError`.
  v8::Local<v8::Object> v8_object = v8_value.As<v8::Object>();

  // ExtendableMessageEvent
  //
  // TODO(mkwst): This is implemented as a module, which might mean we need to
  // move `Origin` object itself elsewhere to enable inclusion. Leaving this
  // unimplemented for the moment.

  // HTMLHyperlinkElementUtils (as `HTMLAnchorElement` or `HTMLAreaElement`)
  if (HTMLAnchorElement* el =
          V8HTMLAnchorElement::ToWrappable(isolate, v8_object)) {
    return MakeGarbageCollected<DOMOrigin>(SecurityOrigin::Create(el->Url()));
  }
  if (HTMLAreaElement* el =
          V8HTMLAreaElement::ToWrappable(isolate, v8_object)) {
    return MakeGarbageCollected<DOMOrigin>(SecurityOrigin::Create(el->Url()));
  }

  // Location is accessible cross-origin, so we need to be careful before
  // handing out its Origin.
  if (Location* location = V8Location::ToWrappable(isolate, v8_object)) {
    if (BindingSecurity::ShouldAllowAccessTo(LocalDOMWindow::From(script_state),
                                             location)) {
      return MakeGarbageCollected<DOMOrigin>(
          SecurityOrigin::CreateFromString(location->origin()));
    }
  }

  // MessageEvent
  if (MessageEvent* event = V8MessageEvent::ToWrappable(isolate, v8_object)) {
    // TODO(434131026): We only have a serialized origin here, which means
    // our handling of `null` is broken. We'll need to teach `MessageEvent`
    // to hold a `SecurityOrigin` instead.
    return MakeGarbageCollected<DOMOrigin>(event->GetSecurityOrigin());
  }

  // Origin
  if (DOMOrigin* origin = V8Origin::ToWrappable(isolate, v8_object)) {
    return MakeGarbageCollected<DOMOrigin>(origin->origin_);
  }

  // URL (as `DOMURL`)
  if (DOMURL* dom_url = V8URL::ToWrappable(isolate, v8_object)) {
    return MakeGarbageCollected<DOMOrigin>(
        SecurityOrigin::Create(dom_url->Url()));
  }

  // WindowOrWorkerGlobalScope (as `LocalDOMWindow` or `WorkerGlobalScope`)
  //
  // Note that `Window` is accessible cross-origin, so we'll need security
  // checks before generating an `Origin`.
  if (DOMWindow* window = V8Window::ToWrappable(isolate, v8_object)) {
    if (BindingSecurity::ShouldAllowAccessTo(LocalDOMWindow::From(script_state),
                                             window) &&
        window->IsLocalDOMWindow()) {
      return MakeGarbageCollected<DOMOrigin>(
          To<LocalDOMWindow>(window)->GetSecurityOrigin());
    }
  }
  if (WorkerGlobalScope* worker =
          V8WorkerGlobalScope::ToWrappable(isolate, v8_object)) {
    return MakeGarbageCollected<DOMOrigin>(worker->GetSecurityOrigin());
  }

  // WorkerLocation
  if (WorkerLocation* location =
          V8WorkerLocation::ToWrappable(isolate, v8_object)) {
    return MakeGarbageCollected<DOMOrigin>(
        SecurityOrigin::Create(location->Url()));
  }

  // Finally, if it wasn't an object we know how to handle, throw an exception:
  exception_state.ThrowTypeError(
      "An origin cannot be extracted from the given parameter.");
  return nullptr;
}

DOMOrigin* DOMOrigin::Create(const String& value,
                             ExceptionState& exception_state) {
  if (DOMOrigin* dom_origin = DOMOrigin::parse(value)) {
    return dom_origin;
  }

  exception_state.ThrowTypeError("Invalid serialized origin");
  return nullptr;
}

// Constructor
DOMOrigin::DOMOrigin(scoped_refptr<const SecurityOrigin> origin)
    : origin_(std::move(origin)) {}

// Destructor
DOMOrigin::~DOMOrigin() = default;


bool DOMOrigin::opaque() const {
  return origin_->IsOpaque();
}

String DOMOrigin::toJSON() const {
  return origin_->ToString();
}

bool DOMOrigin::isSameOrigin(const DOMOrigin* other) const {
  return origin_->IsSameOriginWith(other->origin_.get());
}

bool DOMOrigin::isSameSite(const DOMOrigin* other) const {
  return origin_->IsSameSiteWith(other->origin_.get());
}

void DOMOrigin::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
