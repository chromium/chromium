// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_factory.h"

#include "third_party/blink/renderer/bindings/core/v8/file_or_usv_string_or_form_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_file_formdata_usvstring.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"

namespace blink {

class CustomElementUpgradeReaction final : public CustomElementReaction {
 public:
  explicit CustomElementUpgradeReaction(CustomElementDefinition& definition)
      : CustomElementReaction(definition) {}

 private:
  void Invoke(Element& element) override {
    // Don't call Upgrade() if it's already upgraded. Multiple upgrade reactions
    // could be enqueued because the state changes in step 10 of upgrades.
    // https://html.spec.whatwg.org/C/#upgrades
    if (element.GetCustomElementState() == CustomElementState::kUndefined)
      definition_->Upgrade(element);
  }

  DISALLOW_COPY_AND_ASSIGN(CustomElementUpgradeReaction);
};

// ----------------------------------------------------------------

class CustomElementConnectedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementConnectedCallbackReaction(CustomElementDefinition& definition)
      : CustomElementReaction(definition) {
    DCHECK(definition.HasConnectedCallback());
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunConnectedCallback(element);
  }

  DISALLOW_COPY_AND_ASSIGN(CustomElementConnectedCallbackReaction);
};

// ----------------------------------------------------------------

class CustomElementDisconnectedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementDisconnectedCallbackReaction(CustomElementDefinition& definition)
      : CustomElementReaction(definition) {
    DCHECK(definition.HasDisconnectedCallback());
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunDisconnectedCallback(element);
  }

  DISALLOW_COPY_AND_ASSIGN(CustomElementDisconnectedCallbackReaction);
};

// ----------------------------------------------------------------

class CustomElementAdoptedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementAdoptedCallbackReaction(CustomElementDefinition& definition,
                                       Document& old_owner,
                                       Document& new_owner)
      : CustomElementReaction(definition),
        old_owner_(old_owner),
        new_owner_(new_owner) {
    DCHECK(definition.HasAdoptedCallback());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(old_owner_);
    visitor->Trace(new_owner_);
    CustomElementReaction::Trace(visitor);
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunAdoptedCallback(element, *old_owner_, *new_owner_);
  }

  Member<Document> old_owner_;
  Member<Document> new_owner_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementAdoptedCallbackReaction);
};

// ----------------------------------------------------------------

class CustomElementAttributeChangedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementAttributeChangedCallbackReaction(
      CustomElementDefinition& definition,
      const QualifiedName& name,
      const AtomicString& old_value,
      const AtomicString& new_value)
      : CustomElementReaction(definition),
        name_(name),
        old_value_(old_value),
        new_value_(new_value) {
    DCHECK(definition.HasAttributeChangedCallback(name));
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunAttributeChangedCallback(element, name_, old_value_,
                                             new_value_);
  }

  QualifiedName name_;
  AtomicString old_value_;
  AtomicString new_value_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementAttributeChangedCallbackReaction);
};

// ----------------------------------------------------------------

class CustomElementFormAssociatedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementFormAssociatedCallbackReaction(
      CustomElementDefinition& definition,
      HTMLFormElement* nullable_form)
      : CustomElementReaction(definition), form_(nullable_form) {
    DCHECK(definition.HasFormAssociatedCallback());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(form_);
    CustomElementReaction::Trace(visitor);
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunFormAssociatedCallback(element, form_.Get());
  }

  Member<HTMLFormElement> form_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementFormAssociatedCallbackReaction);
};

// ----------------------------------------------------------------

class CustomElementFormResetCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementFormResetCallbackReaction(CustomElementDefinition& definition)
      : CustomElementReaction(definition) {
    DCHECK(definition.HasFormResetCallback());
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunFormResetCallback(element);
  }

  DISALLOW_COPY_AND_ASSIGN(CustomElementFormResetCallbackReaction);
};

// ----------------------------------------------------------------

class CustomElementFormDisabledCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementFormDisabledCallbackReaction(CustomElementDefinition& definition,
                                            bool is_disabled)
      : CustomElementReaction(definition), is_disabled_(is_disabled) {
    DCHECK(definition.HasFormDisabledCallback());
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunFormDisabledCallback(element, is_disabled_);
  }

  bool is_disabled_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementFormDisabledCallbackReaction);
};

// ----------------------------------------------------------------

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
class CustomElementFormStateRestoreCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementFormStateRestoreCallbackReaction(
      CustomElementDefinition& definition,
      const V8ControlValue* value,
      const String& mode)
      : CustomElementReaction(definition), value_(value), mode_(mode) {
    DCHECK(definition.HasFormStateRestoreCallback());
    DCHECK(mode == "restore" || mode == "autocomplete");
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(value_);
    CustomElementReaction::Trace(visitor);
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunFormStateRestoreCallback(element, value_, mode_);
  }

  Member<const V8ControlValue> value_;
  String mode_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementFormStateRestoreCallbackReaction);
};
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
class CustomElementFormStateRestoreCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementFormStateRestoreCallbackReaction(
      CustomElementDefinition& definition,
      const FileOrUSVStringOrFormData& value,
      const String& mode)
      : CustomElementReaction(definition), value_(value), mode_(mode) {
    DCHECK(definition.HasFormStateRestoreCallback());
    DCHECK(mode == "restore" || mode == "autocomplete");
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(value_);
    CustomElementReaction::Trace(visitor);
  }

 private:
  void Invoke(Element& element) override {
    definition_->RunFormStateRestoreCallback(element, value_, mode_);
  }

  FileOrUSVStringOrFormData value_;
  String mode_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementFormStateRestoreCallbackReaction);
};
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

// ----------------------------------------------------------------

CustomElementReaction& CustomElementReactionFactory::CreateUpgrade(
    CustomElementDefinition& definition) {
  return *MakeGarbageCollected<CustomElementUpgradeReaction>(definition);
}

CustomElementReaction& CustomElementReactionFactory::CreateConnected(
    CustomElementDefinition& definition) {
  return *MakeGarbageCollected<CustomElementConnectedCallbackReaction>(
      definition);
}

CustomElementReaction& CustomElementReactionFactory::CreateDisconnected(
    CustomElementDefinition& definition) {
  return *MakeGarbageCollected<CustomElementDisconnectedCallbackReaction>(
      definition);
}

CustomElementReaction& CustomElementReactionFactory::CreateAdopted(
    CustomElementDefinition& definition,
    Document& old_owner,
    Document& new_owner) {
  return *MakeGarbageCollected<CustomElementAdoptedCallbackReaction>(
      definition, old_owner, new_owner);
}

CustomElementReaction& CustomElementReactionFactory::CreateAttributeChanged(
    CustomElementDefinition& definition,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  return *MakeGarbageCollected<CustomElementAttributeChangedCallbackReaction>(
      definition, name, old_value, new_value);
}

CustomElementReaction& CustomElementReactionFactory::CreateFormAssociated(
    CustomElementDefinition& definition,
    HTMLFormElement* nullable_form) {
  return *MakeGarbageCollected<CustomElementFormAssociatedCallbackReaction>(
      definition, nullable_form);
}

CustomElementReaction& CustomElementReactionFactory::CreateFormReset(
    CustomElementDefinition& definition) {
  return *MakeGarbageCollected<CustomElementFormResetCallbackReaction>(
      definition);
}

CustomElementReaction& CustomElementReactionFactory::CreateFormDisabled(
    CustomElementDefinition& definition,
    bool is_disabled) {
  return *MakeGarbageCollected<CustomElementFormDisabledCallbackReaction>(
      definition, is_disabled);
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CustomElementReaction& CustomElementReactionFactory::CreateFormStateRestore(
    CustomElementDefinition& definition,
    const V8ControlValue* value,
    const String& mode) {
  return *MakeGarbageCollected<CustomElementFormStateRestoreCallbackReaction>(
      definition, value, mode);
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CustomElementReaction& CustomElementReactionFactory::CreateFormStateRestore(
    CustomElementDefinition& definition,
    const FileOrUSVStringOrFormData& value,
    const String& mode) {
  return *MakeGarbageCollected<CustomElementFormStateRestoreCallbackReaction>(
      definition, value, mode);
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

}  // namespace blink
