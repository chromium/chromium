// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DEFINITION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/dom/create_element_flags.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class FileOrUSVStringOrFormData;
class HTMLElement;
class HTMLFormElement;
class QualifiedName;

enum class FormAssociationFlag {
  kNo,
  kYes,
};

class CORE_EXPORT CustomElementDefinition
    : public GarbageCollected<CustomElementDefinition>,
      public NameClient {
 public:
  // Each definition has an ID that is unique within the
  // CustomElementRegistry that created it.
  using Id = uint32_t;

  virtual ~CustomElementDefinition();

  virtual void Trace(Visitor*);
  const char* NameInHeapSnapshot() const override {
    return "CustomElementDefinition";
  }

  const CustomElementDescriptor& Descriptor() { return descriptor_; }

  // TODO(yosin): To support Web Modules, introduce an abstract
  // class |CustomElementConstructor| to allow us to have JavaScript
  // and C++ constructors and ask the binding layer to convert
  // |CustomElementConstructor| to |ScriptValue|. Replace
  // |getConstructorForScript()| by |getConstructor() ->
  // CustomElementConstructor|.
  virtual ScriptValue GetConstructorForScript() = 0;

  using ConstructionStack = HeapVector<Member<Element>, 1>;
  ConstructionStack& GetConstructionStack() { return construction_stack_; }

  HTMLElement* CreateElementForConstructor(Document&);
  virtual HTMLElement* CreateAutonomousCustomElementSync(
      Document&,
      const QualifiedName&) = 0;
  HTMLElement* CreateElement(Document&,
                             const QualifiedName&,
                             const CreateElementFlags);

  void Upgrade(Element&);

  virtual bool HasConnectedCallback() const = 0;
  virtual bool HasDisconnectedCallback() const = 0;
  virtual bool HasAdoptedCallback() const = 0;
  bool HasAttributeChangedCallback(const QualifiedName&) const;
  bool HasStyleAttributeChangedCallback() const;
  virtual bool HasFormAssociatedCallback() const = 0;
  virtual bool HasFormResetCallback() const = 0;
  virtual bool HasFormDisabledCallback() const = 0;
  virtual bool HasFormStateRestoreCallback() const = 0;

  virtual void RunConnectedCallback(Element&) = 0;
  virtual void RunDisconnectedCallback(Element&) = 0;
  virtual void RunAdoptedCallback(Element&,
                                  Document& old_owner,
                                  Document& new_owner) = 0;
  virtual void RunAttributeChangedCallback(Element&,
                                           const QualifiedName&,
                                           const AtomicString& old_value,
                                           const AtomicString& new_value) = 0;
  virtual void RunFormAssociatedCallback(Element& element,
                                         HTMLFormElement* nullable_form) = 0;
  virtual void RunFormResetCallback(Element& element) = 0;
  virtual void RunFormDisabledCallback(Element& element, bool is_disabled) = 0;
  virtual void RunFormStateRestoreCallback(
      Element& element,
      const FileOrUSVStringOrFormData& value,
      const String& mode) = 0;

  void EnqueueUpgradeReaction(Element&,
                              bool upgrade_invisible_elements = false);
  void EnqueueConnectedCallback(Element&);
  void EnqueueDisconnectedCallback(Element&);
  void EnqueueAdoptedCallback(Element&,
                              Document& old_owner,
                              Document& new_owner);
  void EnqueueAttributeChangedCallback(Element&,
                                       const QualifiedName&,
                                       const AtomicString& old_value,
                                       const AtomicString& new_value);

  void SetDefaultStyleSheets(
      const HeapVector<Member<CSSStyleSheet>>& default_style_sheets) {
    default_style_sheets_ = default_style_sheets;
  }

  const HeapVector<Member<CSSStyleSheet>>& DefaultStyleSheets() const {
    return default_style_sheets_;
  }

  bool HasDefaultStyleSheets() const {
    return !default_style_sheets_.IsEmpty();
  }
  bool DisableShadow() const { return disable_shadow_; }
  bool DisableInternals() const { return disable_internals_; }
  bool IsFormAssociated() const { return is_form_associated_; }

  class CORE_EXPORT ConstructionStackScope final {
    STACK_ALLOCATED();
    DISALLOW_COPY_AND_ASSIGN(ConstructionStackScope);

   public:
    ConstructionStackScope(CustomElementDefinition&, Element&);
    ~ConstructionStackScope();

   private:
    ConstructionStack& construction_stack_;
    Member<Element> element_;
    size_t depth_;
  };

 protected:
  CustomElementDefinition(const CustomElementDescriptor&);

  CustomElementDefinition(const CustomElementDescriptor&,
                          const HashSet<AtomicString>& observed_attributes,
                          const Vector<String>& disabled_features,
                          FormAssociationFlag form_association_flag);

  void AddDefaultStylesTo(Element&);

  virtual bool RunConstructor(Element&) = 0;

  static void CheckConstructorResult(Element*,
                                     Document&,
                                     const QualifiedName&,
                                     ExceptionState&);

 private:
  const CustomElementDescriptor descriptor_;
  ConstructionStack construction_stack_;
  HashSet<AtomicString> observed_attributes_;
  bool has_style_attribute_changed_callback_;
  bool added_default_style_sheet_ = false;
  bool disable_shadow_ = false;
  bool disable_internals_ = false;
  bool is_form_associated_ = false;

  HeapVector<Member<CSSStyleSheet>> default_style_sheets_;

  void EnqueueAttributeChangedCallbackForAllAttributes(Element&);

  DISALLOW_COPY_AND_ASSIGN(CustomElementDefinition);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DEFINITION_H_
