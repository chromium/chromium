// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "v8/include/v8.h"

namespace blink {

class CustomElementDescriptor;
class CustomElementRegistry;
class V8CustomElementConstructor;
class V8Function;

class CORE_EXPORT ScriptCustomElementDefinition final
    : public CustomElementDefinition {
  WTF_MAKE_NONCOPYABLE(ScriptCustomElementDefinition);

 public:
  static ScriptCustomElementDefinition* ForConstructor(
      ScriptState*,
      CustomElementRegistry*,
      v8::Local<v8::Value> constructor);

  static ScriptCustomElementDefinition* Create(
      ScriptState*,
      CustomElementRegistry*,
      const CustomElementDescriptor&,
      CustomElementDefinition::Id,
      V8CustomElementConstructor* constructor,
      V8Function* connected_callback,
      V8Function* disconnected_callback,
      V8Function* adopted_callback,
      V8Function* attribute_changed_callback,
      HashSet<AtomicString>&& observed_attributes);

  ~ScriptCustomElementDefinition() override = default;

  void Trace(Visitor*) override;

  v8::Local<v8::Object> Constructor() const;

  HTMLElement* CreateAutonomousCustomElementSync(Document&,
                                                 const QualifiedName&) override;

  bool HasConnectedCallback() const override;
  bool HasDisconnectedCallback() const override;
  bool HasAdoptedCallback() const override;

  void RunConnectedCallback(Element*) override;
  void RunDisconnectedCallback(Element*) override;
  void RunAdoptedCallback(Element*,
                          Document* old_owner,
                          Document* new_owner) override;
  void RunAttributeChangedCallback(Element*,
                                   const QualifiedName&,
                                   const AtomicString& old_value,
                                   const AtomicString& new_value) override;

 private:
  ScriptCustomElementDefinition(ScriptState*,
                                const CustomElementDescriptor&,
                                V8CustomElementConstructor* constructor,
                                V8Function* connected_callback,
                                V8Function* disconnected_callback,
                                V8Function* adopted_callback,
                                V8Function* attribute_changed_callback,
                                HashSet<AtomicString>&& observed_attributes);

  // Implementations of |CustomElementDefinition|
  ScriptValue GetConstructorForScript() final;
  bool RunConstructor(Element*) override;

  // Calls the constructor. The script scope, etc. must already be set up.
  Element* CallConstructor();

  HTMLElement* HandleCreateElementSyncException(Document&,
                                                const QualifiedName& tag_name,
                                                v8::Isolate*,
                                                ExceptionState&);

  Member<ScriptState> script_state_;
  TraceWrapperMember<V8CustomElementConstructor> constructor_;
  TraceWrapperMember<V8Function> connected_callback_;
  TraceWrapperMember<V8Function> disconnected_callback_;
  TraceWrapperMember<V8Function> adopted_callback_;
  TraceWrapperMember<V8Function> attribute_changed_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_H_
