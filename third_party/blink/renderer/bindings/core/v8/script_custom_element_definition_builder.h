// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_BUILDER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition_builder.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "v8/include/v8.h"

namespace blink {

class CustomElementRegistry;
class ExceptionState;
class ScriptState;
class V8CustomElementConstructor;
class V8Function;

class CORE_EXPORT ScriptCustomElementDefinitionBuilder
    : public CustomElementDefinitionBuilder {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScriptCustomElementDefinitionBuilder);

 public:
  ScriptCustomElementDefinitionBuilder(ScriptState*,
                                       CustomElementRegistry*,
                                       V8CustomElementConstructor* constructor,
                                       ExceptionState&);
  ~ScriptCustomElementDefinitionBuilder() = default;

  bool CheckConstructorIntrinsics() override;
  bool CheckConstructorNotRegistered() override;
  bool RememberOriginalProperties() override;
  CustomElementDefinition* Build(const CustomElementDescriptor&,
                                 CustomElementDefinition::Id) override;

 private:
  Member<ScriptState> script_state_;
  ExceptionState& exception_state_;
  Member<CustomElementRegistry> registry_;
  const Member<V8CustomElementConstructor> constructor_;
  // These v8::Local handles on stack make the function objects alive until we
  // finish building the CustomElementDefinition and wrapper-tracing on it gets
  // available.
  v8::Local<v8::Value> v8_connected_callback_;
  v8::Local<v8::Value> v8_disconnected_callback_;
  v8::Local<v8::Value> v8_adopted_callback_;
  v8::Local<v8::Value> v8_attribute_changed_callback_;
  Member<V8Function> connected_callback_;
  Member<V8Function> disconnected_callback_;
  Member<V8Function> adopted_callback_;
  Member<V8Function> attribute_changed_callback_;
  HashSet<AtomicString> observed_attributes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_BUILDER_H_
