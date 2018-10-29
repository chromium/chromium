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
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptWrappable;

// TODO(peria): Remove properties just to keep V8 objects alive.
// e.g. IDBCursor.Request.
// Apply |X| for each pair of (InterfaceName, PrivateKeyName).
#define V8_PRIVATE_PROPERTY_FOR_EACH(X)               \
  X(CustomElement, Document)                          \
  X(CustomElement, IsInterfacePrototypeObject)        \
  X(CustomElement, NamespaceURI)                      \
  X(CustomElement, TagName)                           \
  X(CustomElement, Type)                              \
  X(CustomElementLifecycle, AttachedCallback)         \
  X(CustomElementLifecycle, AttributeChangedCallback) \
  X(CustomElementLifecycle, CreatedCallback)          \
  X(CustomElementLifecycle, DetachedCallback)         \
  X(DOMException, Error)                              \
  X(Global, Event)                                    \
  X(IDBCursor, Request)                               \
  X(IntersectionObserver, Callback)                   \
  X(MessageChannel, Port1)                            \
  X(MessageChannel, Port2)                            \
  X(MessageEvent, CachedData)                         \
  X(MutationObserver, Callback)                       \
  X(NamedConstructor, Initialized)                    \
  X(PopStateEvent, State)                             \
  X(SameObject, DetectedBarcodeCornerPoints)          \
  X(SameObject, DetectedFaceLandmarks)                \
  X(SameObject, NotificationActions)                  \
  X(SameObject, NotificationData)                     \
  X(SameObject, NotificationVibrate)                  \
  X(SameObject, PerformanceLongTaskTimingAttribution) \
  X(SameObject, PushManagerSupportedContentEncodings) \
  X(CustomWrappable, EventHandler)                    \
  X(CustomWrappable, EventListener)                   \
  SCRIPT_PROMISE_PROPERTIES(X, Promise)               \
  SCRIPT_PROMISE_PROPERTIES(X, Resolver)

// The getter's name for a private property.
#define V8_PRIVATE_PROPERTY_GETTER_NAME(InterfaceName, PrivateKeyName) \
  Get##InterfaceName##PrivateKeyName

// The member variable's name for a private property.
#define V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, PrivateKeyName) \
  m_symbol##InterfaceName##PrivateKeyName

// The string used to create a private symbol.  Must be unique per V8 instance.
#define V8_PRIVATE_PROPERTY_SYMBOL_STRING(InterfaceName, PrivateKeyName) \
  #InterfaceName "#" #PrivateKeyName  // NOLINT(whitespace/indent)

// Provides access to V8's private properties.
//
// Usage 1) Fast path to use a pre-registered symbol.
//   auto private = V8PrivateProperty::getMessageEventCachedData(isolate);
//   v8::Local<v8::Object> object = ...;
//   v8::Local<v8::Value> value;
//   if (!private.GetOrUndefined(object).ToLocal(&value)) return;
//   value = ...;
//   private.set(object, value);
//
// Usage 2) Slow path to create a global private symbol.
//   const char symbolName[] = "Interface#PrivateKeyName";
//   auto private = V8PrivateProperty::createSymbol(isolate, symbolName);
//   ...
class PLATFORM_EXPORT V8PrivateProperty {
  USING_FAST_MALLOC(V8PrivateProperty);
  WTF_MAKE_NONCOPYABLE(V8PrivateProperty);

 public:
  enum CachedAccessorSymbol : unsigned {
    kNoCachedAccessor = 0,
    kWindowDocumentCachedAccessor,
  };

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
    friend class V8CustomEvent;
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

  static std::unique_ptr<V8PrivateProperty> Create() {
    return base::WrapUnique(new V8PrivateProperty());
  }

#define V8_PRIVATE_PROPERTY_DEFINE_GETTER(InterfaceName, KeyName)              \
  static Symbol V8_PRIVATE_PROPERTY_GETTER_NAME(/* // NOLINT */                \
                                                InterfaceName, KeyName)(       \
      v8::Isolate * isolate) {                                                 \
    V8PrivateProperty* private_prop =                                          \
        V8PerIsolateData::From(isolate)->PrivateProperty();                    \
    v8::Eternal<v8::Private>& property_handle =                                \
        private_prop->V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, KeyName); \
    if (UNLIKELY(property_handle.IsEmpty())) {                                 \
      property_handle.Set(                                                     \
          isolate, CreateV8Private(isolate, V8_PRIVATE_PROPERTY_SYMBOL_STRING( \
                                                InterfaceName, KeyName)));     \
    }                                                                          \
    return Symbol(isolate, property_handle.Get(isolate));                      \
  }

  V8_PRIVATE_PROPERTY_FOR_EACH(V8_PRIVATE_PROPERTY_DEFINE_GETTER)
#undef V8_PRIVATE_PROPERTY_DEFINE_GETTER

  // TODO(peria): Do not use this specialized hack. See a TODO comment
  // on m_symbolWindowDocumentCachedAccessor.
  static Symbol GetWindowDocumentCachedAccessor(v8::Isolate* isolate) {
    V8PrivateProperty* private_prop =
        V8PerIsolateData::From(isolate)->PrivateProperty();
    if (UNLIKELY(
            private_prop->symbol_window_document_cached_accessor_.IsEmpty())) {
      private_prop->symbol_window_document_cached_accessor_.Set(
          isolate, CreateCachedV8Private(
                       isolate, V8_PRIVATE_PROPERTY_SYMBOL_STRING(
                                    "Window", "DocumentCachedAccessor")));
    }
    return Symbol(
        isolate, private_prop->symbol_window_document_cached_accessor_.NewLocal(
                     isolate));
  }

  static Symbol GetCachedAccessor(v8::Isolate* isolate,
                                  CachedAccessorSymbol symbol_id) {
    switch (symbol_id) {
      case kWindowDocumentCachedAccessor:
        return GetWindowDocumentCachedAccessor(isolate);
      case kNoCachedAccessor:
        break;
    };
    NOTREACHED();
    return GetSymbol(isolate, "unexpected cached accessor");
  }

  static Symbol GetSymbol(v8::Isolate* isolate, const char* symbol) {
    return Symbol(isolate, CreateCachedV8Private(isolate, symbol));
  }

 private:
  V8PrivateProperty() = default;

  static v8::Local<v8::Private> CreateV8Private(v8::Isolate*,
                                                const char* symbol);
  // TODO(peria): Remove this method. We should not use v8::Private::ForApi().
  static v8::Local<v8::Private> CreateCachedV8Private(v8::Isolate*,
                                                      const char* symbol);

#define V8_PRIVATE_PROPERTY_DECLARE_MEMBER(InterfaceName, KeyName) \
  v8::Eternal<v8::Private> V8_PRIVATE_PROPERTY_MEMBER_NAME(        \
      InterfaceName, KeyName);  // NOLINT(readability/naming/underscores)
  V8_PRIVATE_PROPERTY_FOR_EACH(V8_PRIVATE_PROPERTY_DECLARE_MEMBER)
#undef V8_PRIVATE_PROPERTY_DECLARE_MEMBER

  // TODO(peria): Do not use this specialized hack for
  // Window#DocumentCachedAccessor. This is required to put v8::Private key in
  // a snapshot, and it cannot be a v8::Eternal<> due to V8 serializer's
  // requirement.
  ScopedPersistent<v8::Private> symbol_window_document_cached_accessor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_PRIVATE_PROPERTY_H_
