// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "v8/include/v8.h"

namespace blink {

class CustomElementDescriptor;
class CustomElementRegistry;
class ScriptCustomElementDefinitionData;
class V8CustomElementAdoptedCallback;
class V8CustomElementAttributeChangedCallback;
class V8CustomElementConstructor;
class V8CustomElementFormAssociatedCallback;
class V8CustomElementFormDisabledCallback;
class V8CustomElementFormStateRestoreCallback;
class V8VoidFunction;

class CORE_EXPORT ScriptCustomElementDefinition final
    : public CustomElementDefinition {
 public:
  static ScriptCustomElementDefinition* ForConstructor(
      ScriptState*,
      CustomElementRegistry*,
      v8::Local<v8::Value> constructor);

  ScriptCustomElementDefinition(const ScriptCustomElementDefinitionData& data,
                                const CustomElementDescriptor&,
                                CustomElementDefinition::Id);
  ~ScriptCustomElementDefinition() override = default;

  void Trace(Visitor*) override;

  v8::Local<v8::Object> Constructor() const;

  HTMLElement* CreateAutonomousCustomElementSync(Document&,
                                                 const QualifiedName&) override;

  bool HasConnectedCallback() const override;
  bool HasDisconnectedCallback() const override;
  bool HasAdoptedCallback() const override;
  bool HasFormAssociatedCallback() const override;
  bool HasFormResetCallback() const override;
  bool HasFormDisabledCallback() const override;
  bool HasFormStateRestoreCallback() const override;

  void RunConnectedCallback(Element&) override;
  void RunDisconnectedCallback(Element&) override;
  void RunAdoptedCallback(Element&,
                          Document& old_owner,
                          Document& new_owner) override;
  void RunAttributeChangedCallback(Element&,
                                   const QualifiedName&,
                                   const AtomicString& old_value,
                                   const AtomicString& new_value) override;
  void RunFormAssociatedCallback(Element& element,
                                 HTMLFormElement* nullable_form) override;
  void RunFormResetCallback(Element& element) override;
  void RunFormDisabledCallback(Element& element, bool is_disabled) override;
  void RunFormStateRestoreCallback(Element& element,
                                   const FileOrUSVStringOrFormData& value,
                                   const String& mode) override;

 private:
  // Implementations of |CustomElementDefinition|
  ScriptValue GetConstructorForScript() final;
  bool RunConstructor(Element&) override;

  // Calls the constructor. The script scope, etc. must already be set up.
  Element* CallConstructor();

  HTMLElement* HandleCreateElementSyncException(Document&,
                                                const QualifiedName& tag_name,
                                                v8::Isolate*,
                                                ExceptionState&);

  Member<ScriptState> script_state_;
  Member<V8CustomElementConstructor> constructor_;
  Member<V8VoidFunction> connected_callback_;
  Member<V8VoidFunction> disconnected_callback_;
  Member<V8CustomElementAdoptedCallback> adopted_callback_;
  Member<V8CustomElementAttributeChangedCallback> attribute_changed_callback_;
  Member<V8CustomElementFormAssociatedCallback> form_associated_callback_;
  Member<V8VoidFunction> form_reset_callback_;
  Member<V8CustomElementFormDisabledCallback> form_disabled_callback_;
  Member<V8CustomElementFormStateRestoreCallback> form_state_restore_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScriptCustomElementDefinition);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_CUSTOM_ELEMENT_DEFINITION_H_
