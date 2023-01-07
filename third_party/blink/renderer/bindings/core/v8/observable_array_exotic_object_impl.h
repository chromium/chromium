// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_IMPL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/observable_array_base.h"

namespace blink {

namespace bindings {

class ObservableArrayBase;

// The implementation class of ObservableArrayExoticObject.
class CORE_EXPORT ObservableArrayExoticObjectImpl final
    : public ObservableArrayExoticObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Returns the backing list object extracted from the proxy target object
  // of type JS Array.
  static bindings::ObservableArrayBase* ProxyTargetToObservableArrayBaseOrDie(
      v8::Isolate* isolate,
      v8::Local<v8::Array> v8_proxy_target);

  explicit ObservableArrayExoticObjectImpl(
      bindings::ObservableArrayBase* observable_array_backing_list_object);
  ~ObservableArrayExoticObjectImpl() override = default;

  // ScriptWrappable overrides
  v8::MaybeLocal<v8::Value> Wrap(ScriptState* script_state) override;
  [[nodiscard]] v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate* isolate,
      const WrapperTypeInfo* wrapper_type_info,
      v8::Local<v8::Object> wrapper) override;
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_IMPL_H_
