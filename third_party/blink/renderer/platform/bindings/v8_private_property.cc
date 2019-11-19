// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"

#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"

namespace blink {

// As SymbolKey is widely used, numerous instances of SymbolKey are created,
// plus all the instances have static storage duration (defined as static
// variables).  Thus, it's important to make SymbolKey
// trivially-constructible/destructible so that compilers can remove all the
// constructor/destructor calls and reduce the code size.
static_assert(
    std::is_trivially_constructible<V8PrivateProperty::SymbolKey>::value,
    "SymbolKey is not trivially constructible");
static_assert(
    std::is_trivially_destructible<V8PrivateProperty::SymbolKey>::value,
    "SymbolKey is not trivially destructible");

v8::MaybeLocal<v8::Value> V8PrivateProperty::Symbol::GetFromMainWorld(
    ScriptWrappable* script_wrappable) {
  v8::Local<v8::Object> wrapper = script_wrappable->MainWorldWrapper(isolate_);
  return wrapper.IsEmpty() ? v8::MaybeLocal<v8::Value>()
                           : GetOrUndefined(wrapper);
}

V8PrivateProperty::Symbol V8PrivateProperty::GetWindowDocumentCachedAccessor(
    v8::Isolate* isolate) {
  V8PrivateProperty* private_prop =
      V8PerIsolateData::From(isolate)->PrivateProperty();
  if (UNLIKELY(
          private_prop->symbol_window_document_cached_accessor_.IsEmpty())) {
    // This private property is used in Window, and Window and Document are
    // stored in the V8 context snapshot.  So, this private property needs to
    // be restorable from the snapshot, and only v8::Private::ForApi supports
    // it so far.
    //
    // TODO(peria): Explore a better way to connect a Document to a Window.
    v8::Local<v8::Private> private_symbol = v8::Private::ForApi(
        isolate, V8String(isolate, "Window#DocumentCachedAccessor"));
    private_prop->symbol_window_document_cached_accessor_.Set(isolate,
                                                              private_symbol);
  }
  return Symbol(
      isolate,
      private_prop->symbol_window_document_cached_accessor_.NewLocal(isolate));
}

V8PrivateProperty::Symbol V8PrivateProperty::GetSymbol(
    v8::Isolate* isolate,
    const V8PrivateProperty::SymbolKey& key) {
  V8PrivateProperty* private_prop =
      V8PerIsolateData::From(isolate)->PrivateProperty();
  auto& symbol_map = private_prop->symbol_map_;
  auto iter = symbol_map.find(&key);
  v8::Local<v8::Private> v8_private;
  if (UNLIKELY(iter == symbol_map.end())) {
    v8_private = CreateV8Private(isolate, nullptr);
    symbol_map.insert(&key, v8::Eternal<v8::Private>(isolate, v8_private));
  } else {
    v8_private = iter->value.Get(isolate);
  }
  return Symbol(isolate, v8_private);
}

v8::Local<v8::Private> V8PrivateProperty::CreateV8Private(v8::Isolate* isolate,
                                                          const char* symbol) {
  return v8::Private::New(isolate, V8String(isolate, symbol));
}

}  // namespace blink
