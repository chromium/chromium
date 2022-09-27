// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_OBSERVABLE_ARRAY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_OBSERVABLE_ARRAY_BASE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8-forward.h"

// Overview of Blink implementation of Web IDL observable arrays
//
// Observable arrays are implemented with two objects:
// 1) backing list object
// https://webidl.spec.whatwg.org/#observable-array-attribute-backing-list
// and 2) exotic object.
// https://webidl.spec.whatwg.org/#backing-observable-array-exotic-object
// As DOM objects are implemented with a ScriptWrappable and its V8 wrappers,
// there are a ScriptWrappable of a backing list object and its V8 wrappers,
// also a ScriptWrappable of a exotic object and its (pseudo) V8 wrappers.
//
// By definition, observable array exotic object is a JS Proxy in ECMAScript
// binding.
//
//   let exotic_object = new Proxy(backing_list_object, handler_object);
//
// For web developers, the JS Proxy object is the only visible object.  Web
// developers cannot access the backing list object directly.
//
// For Blink developers, the backing list object looks the primary object.
// However, when exposing an observable array to web developers, the exotic
// object must be exposed instead of the backing list object.  This is done in
// auto-generated bindings (generated bindings automatically call
// GetExoticObject()).
//
//   class MyIdlInterface : public ScriptWrappable {
//    public:
//     V8ObservableArrayNode* myAttr() const {
//       return my_observable_array_;
//     }
//    private:
//     // my_observable_array_ is a backing list object.
//     Member<V8ObservableArrayNode> my_observable_array_;
//   };
//
// Class hierarchy and relationship:
//   bindings::ObservableArrayBase -- the base class of backing list objects
//   +-- bindings::ObservableArrayImplHelper<T> -- just a helper
//       +-- V8ObservableArrayNode -- generated implementation of IDL
//               ObservableArray<Node>.  Bindings code generator produces this
//               class from *.idl files.
//   ObservableArrayExoticObject -- the base class of exotic objects
//   +-- bindings::ObservableArrayExoticObjectImpl -- the implementation class
//
//   v8_exotic_object (= JS Proxy)
//       --(proxy target)--> v8_array (= JS Array)
//       --(private property)--> v8_backing_list_object
//       --(internal field)--> blink_backing_list_object
//       --(data member)--> blink_exotic_object
//       --(ToV8Traits)--> v8_exotic_object

namespace blink {

class ObservableArrayExoticObject;

namespace bindings {

// ObservableArrayBase is the common base class of all the observable array
// classes, and represents the backing list for an IDL attribute of an
// observable array type (but the actual implementation lives in
// bindings/core/v8/).
// https://webidl.spec.whatwg.org/#observable-array-attribute-backing-list
class PLATFORM_EXPORT ObservableArrayBase : public ScriptWrappable {
 public:
  ObservableArrayBase(
      ScriptWrappable* platform_object,
      ObservableArrayExoticObject* observable_array_exotic_object);
  ~ObservableArrayBase() override = default;

  // Returns the observable array exotic object, which is the value to be
  // returned as the IDL attribute value.  Do not use `this` object (= the
  // observable array backing list object) as the IDL attribute value.
  ObservableArrayExoticObject* GetExoticObject() const {
    return observable_array_exotic_object_.Get();
  }

  v8::MaybeLocal<v8::Object> GetProxyHandlerObject(ScriptState* script_state);

  void Trace(Visitor* visitor) const override;

 protected:
  ScriptWrappable* GetPlatformObject() { return platform_object_.Get(); }

  virtual v8::Local<v8::FunctionTemplate> GetProxyHandlerFunctionTemplate(
      ScriptState* script_state) = 0;

 private:
  Member<ScriptWrappable> platform_object_;  // IDL attribute owner
  Member<ObservableArrayExoticObject> observable_array_exotic_object_;
};

}  // namespace bindings

// Represents a backing observable array exotic object.
// https://webidl.spec.whatwg.org/#backing-observable-array-exotic-object
class PLATFORM_EXPORT ObservableArrayExoticObject : public ScriptWrappable {
 public:
  explicit ObservableArrayExoticObject(
      bindings::ObservableArrayBase* observable_array_backing_list_object);
  ~ObservableArrayExoticObject() override = default;

  bindings::ObservableArrayBase* GetBackingListObject() const {
    return observable_array_backing_list_object_.Get();
  }

  void Trace(Visitor* visitor) const override;

 private:
  Member<bindings::ObservableArrayBase> observable_array_backing_list_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_OBSERVABLE_ARRAY_BASE_H_
