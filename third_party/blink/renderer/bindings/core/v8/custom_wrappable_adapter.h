// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CUSTOM_WRAPPABLE_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CUSTOM_WRAPPABLE_ADAPTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/custom_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;

// CustomWrappableAdapter establishes a link from a given JavaScript object to
// the Blink object inheriting from CustomWrappableAdapter. The link is known to
// garbage collectors and the lifetime of the V8 and Blink objects will be tied
// together. The adapter can be used to model liveness across V8 and Blink
// component boundaries. In contrast to ScriptWrappable, no IDL definitions are
// required.
//
// The intended use case is binding the lifetime of a Blink object to a
// user-provided JavaScript object.
class CORE_EXPORT CustomWrappableAdapter : public CustomWrappable {
 public:
  // Lookup the CustomWrappableAdapter implementation on a given |object|'s
  // |property|. Returns nullptr if no adapter has been attached. See Attach.
  template <typename T>
  static T* Lookup(v8::Local<v8::Object> object,
                   const V8PrivateProperty::Symbol& property) {
    return static_cast<T*>(LookupInternal(object, property));
  }

  // Attaches |this| adapter to |object|'s |property|.
  void Attach(ScriptState*,
              v8::Local<v8::Object> object,
              const V8PrivateProperty::Symbol& property);

  // Creates and sets up the JS wrapper object. May only be called once. Returns
  // the wrapper object.
  //
  // This method can be used when the wrapper is needed to actually create the
  // object that it should be attached to. Prefer |Attach| when possible.
  v8::Local<v8::Object> CreateAndInitializeWrapper(ScriptState*);

  ~CustomWrappableAdapter() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(wrapper_);
    CustomWrappable::Trace(visitor);
  }

 private:
  static CustomWrappableAdapter* LookupInternal(
      v8::Local<v8::Object>,
      const V8PrivateProperty::Symbol&);

  // Internal wrapper reference is needed as Oilpan looks up its roots from V8
  // by following all configured wrapper references.
  TraceWrapperV8Reference<v8::Object> wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CUSTOM_WRAPPABLE_ADAPTER_H_
