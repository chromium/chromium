// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_PRIVATE_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_PRIVATE_PROPERTY_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/bindings/script_promise_properties.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptWrappable;

// Provides access to V8's private properties with a symbol key.
//
//   static const V8PrivateProperty::SymbolKey kPrivateProperty;
//   auto private_property = V8PrivateProperty::GetSymbol(
//       isolate, kPrivateProperty);
//   v8::Local<v8::Object> object = ...;
//   v8::Local<v8::Value> value;
//   if (!private_property.GetOrUndefined(object).ToLocal(&value)) return;
//   value = ...;
//   private_property.Set(object, value);
//   ...
class PLATFORM_EXPORT V8PrivateProperty {
  USING_FAST_MALLOC(V8PrivateProperty);

 public:
  enum CachedAccessorSymbol : unsigned {
    kNoCachedAccessor = 0,
    kWindowDocumentCachedAccessor,
  };

  V8PrivateProperty() = default;

  // Provides fast access to V8's private properties.
  //
  // Retrieving/creating a global private symbol from a string is very
  // expensive compared to get or set a private property.  This class
  // provides a way to cache a private symbol and re-use it.
  class PLATFORM_EXPORT Symbol {
    STACK_ALLOCATED();

   public:
    bool HasValue(v8::Local<v8::Object> object) const {
      return object->HasPrivate(GetContext(), private_symbol_).ToChecked();
    }

    // Returns the value of the private property if set, or undefined.
    WARN_UNUSED_RESULT v8::MaybeLocal<v8::Value> GetOrUndefined(
        v8::Local<v8::Object> object) const {
      return object->GetPrivate(GetContext(), private_symbol_);
    }

    bool Set(v8::Local<v8::Object> object, v8::Local<v8::Value> value) const {
      return object->SetPrivate(GetContext(), private_symbol_, value)
          .ToChecked();
    }

    bool DeleteProperty(v8::Local<v8::Object> object) const {
      return object->DeletePrivate(GetContext(), private_symbol_).ToChecked();
    }

    v8::Local<v8::Private> GetPrivate() const { return private_symbol_; }

   private:
    friend class V8PrivateProperty;
    // The following classes are exceptionally allowed to call to
    // getFromMainWorld.
    friend class V8ExtendableMessageEvent;

    Symbol(v8::Isolate* isolate, v8::Local<v8::Private> private_symbol)
        : private_symbol_(private_symbol), isolate_(isolate) {}

    // To get/set private property, we should use the current context.
    v8::Local<v8::Context> GetContext() const {
      return isolate_->GetCurrentContext();
    }

    // Only friend classes are allowed to use this API.
    WARN_UNUSED_RESULT v8::MaybeLocal<v8::Value> GetFromMainWorld(
        ScriptWrappable*);

    v8::Local<v8::Private> private_symbol_;
    v8::Isolate* isolate_;
  };

  // This class is used for a key to get Symbol.
  //
  // We can improve ability of tracking private properties by using an instance
  // of this class.
  class PLATFORM_EXPORT SymbolKey final {
   public:
    SymbolKey() = default;

   private:
    SymbolKey(const SymbolKey&) = delete;
    SymbolKey& operator=(const SymbolKey&) = delete;
  };

  // TODO(peria): Do not use this specialized hack. See a TODO comment
  // on m_symbolWindowDocumentCachedAccessor.
  static Symbol GetWindowDocumentCachedAccessor(v8::Isolate* isolate);

  static Symbol GetCachedAccessor(v8::Isolate* isolate,
                                  CachedAccessorSymbol symbol_id) {
    switch (symbol_id) {
      case kWindowDocumentCachedAccessor:
        return GetWindowDocumentCachedAccessor(isolate);
      case kNoCachedAccessor:
        break;
    }
    NOTREACHED();
    return GetEmptySymbol();
  }

  // This is a hack for PopStateEvent to get the same private property of
  // History, named State.
  static Symbol GetHistoryStateSymbol(v8::Isolate* isolate) {
    static const SymbolKey kPrivatePropertyKey;
    return GetSymbol(isolate, kPrivatePropertyKey);
  }

  // Returns a Symbol to access a private property. Symbol instances from same
  // |key| are guaranteed to access the same property.
  static Symbol GetSymbol(v8::Isolate* isolate, const SymbolKey& key);

  // This function is always called after NOTREACHED(). The Symbol returned from
  // this function must not be used.
  static Symbol GetEmptySymbol() {
    return Symbol(nullptr, v8::Local<v8::Private>());
  }

 private:
  static v8::Local<v8::Private> CreateV8Private(v8::Isolate*,
                                                const char* symbol);

  // TODO(peria): Do not use this specialized hack for
  // Window#DocumentCachedAccessor. This is required to put v8::Private key in
  // a snapshot, and it cannot be a v8::Eternal<> due to V8 serializer's
  // requirement.
  ScopedPersistent<v8::Private> symbol_window_document_cached_accessor_;

  WTF::HashMap<const void*, v8::Eternal<v8::Private>> symbol_map_;

  DISALLOW_COPY_AND_ASSIGN(V8PrivateProperty);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_PRIVATE_PROPERTY_H_
