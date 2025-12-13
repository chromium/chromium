// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url/dom_origin.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event.h"
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

namespace {

// Each of the follow V8 classes wraps a DOM class that implements
// `DOMOriginUtils`. We'll try each type in sequence (the order is irrelevant),
// and return the first valid result or `nullptr` if the object isn't one of
// these types.
DOMOriginUtils* GetDOMOriginUtilsFromV8Object(v8::Isolate* i,
                                              v8::Local<v8::Value> o) {
  if (auto* p = V8Event::ToWrappable(i, o)) {
    return p;
  }
  if (auto* p = V8HTMLAnchorElement::ToWrappable(i, o)) {
    return p;
  }
  if (auto* p = V8HTMLAreaElement::ToWrappable(i, o)) {
    return p;
  }
  if (auto* p = V8MessageEvent::ToWrappable(i, o)) {
    return p;
  }
  if (auto* p = V8URL::ToWrappable(i, o)) {
    return p;
  }
  if (auto* p = V8Window::ToWrappable(i, o)) {
    return p;
  }
  if (auto* p = V8WorkerGlobalScope::ToWrappable(i, o)) {
    return p;
  }

  return nullptr;
}

}  // namespace

// Static `::Create()` methods:
DOMOrigin* DOMOrigin::Create() {
  return DOMOrigin::Create(SecurityOrigin::CreateUniqueOpaque());
}

// static
DOMOrigin* DOMOrigin::Create(scoped_refptr<const SecurityOrigin> origin) {
  return MakeGarbageCollected<DOMOrigin>(base::PassKey<DOMOrigin>(),
                                         std::move(origin));
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
    return DOMOrigin::Create(SecurityOrigin::Create(parsed_url));
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

  // Origin
  if (DOMOrigin* origin = V8Origin::ToWrappable(isolate, v8_object)) {
    return DOMOrigin::Create(origin->origin_);
  }

  // DOMOriginUtils objects:
  LocalDOMWindow* accessing_window = LocalDOMWindow::From(script_state);
  if (DOMOriginUtils* dom_origin_utils =
          GetDOMOriginUtilsFromV8Object(isolate, v8_object)) {
    if (DOMOrigin* result = dom_origin_utils->GetDOMOrigin(accessing_window)) {
      return result;
    }
  }

  // If we didn't receive an object we know how to handle, or we weren't able
  // to extract an origin from that object, throw an exception:
  exception_state.ThrowTypeError(
      "An origin cannot be extracted from the given parameter.");
  return nullptr;
}

// Constructor
DOMOrigin::DOMOrigin(base::PassKey<DOMOrigin>,
                     scoped_refptr<const SecurityOrigin> origin)
    : origin_(std::move(origin)) {}

// Destructor
DOMOrigin::~DOMOrigin() = default;


bool DOMOrigin::opaque() const {
  return origin_->IsOpaque();
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
