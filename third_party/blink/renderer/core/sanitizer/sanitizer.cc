// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_attribute_namespace.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_element_namespace.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_element_namespace_with_attributes.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizerattributenamespace_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizerelementnamespace_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizerelementnamespacewithattributes_string.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

Sanitizer* Sanitizer::Create(ExecutionContext* execution_context,
                             const SanitizerConfig* sanitizer_config,
                             ExceptionState& exception_state) {
  Sanitizer* sanitizer = MakeGarbageCollected<Sanitizer>();
  if (!sanitizer_config) {
    NOTREACHED();  // Default handling not yet implemented.
  }
  if (!sanitizer->setFrom(sanitizer_config)) {
    // As currently implemented, all inputs will lead to successful creation
    // of a Sanitizer instance. But the current spec discussion aims to
    // introduce invalid configurations. Once we implement that, this will be
    // replaced with `exception_state.ThrowTypeError(...); return nullptr;`.
    NOTREACHED();
  }
  return sanitizer;
}

void Sanitizer::allowElement(
    const V8UnionSanitizerElementNamespaceWithAttributesOrString* element) {
  const QualifiedName name = getFrom(element);
  AllowElement(name);

  // The internal AllowElement doesn't handle per-element attrs (yet).
  if (element->IsSanitizerElementNamespaceWithAttributes()) {
    const SanitizerElementNamespaceWithAttributes* element_with_attrs =
        element->GetAsSanitizerElementNamespaceWithAttributes();
    if (element_with_attrs->hasAttributes()) {
      const auto add_result =
          allow_attrs_per_element_.insert(name, HashSet<QualifiedName>());
      for (const auto& attr : element_with_attrs->attributes()) {
        add_result.stored_value->value.insert(getFrom(attr));
      }
    }
    if (element_with_attrs->hasRemoveAttributes()) {
      const auto add_result =
          remove_attrs_per_element_.insert(name, HashSet<QualifiedName>());
      for (const auto& attr : element_with_attrs->removeAttributes()) {
        add_result.stored_value->value.insert(getFrom(attr));
      }
    }
  }
}

void Sanitizer::removeElement(
    const V8UnionSanitizerElementNamespaceOrString* element) {
  RemoveElement(getFrom(element));
}

void Sanitizer::replaceWithChildrenElement(
    const V8UnionSanitizerElementNamespaceOrString* element) {
  ReplaceElement(getFrom(element));
}

void Sanitizer::allowAttribute(
    const V8UnionSanitizerAttributeNamespaceOrString* attribute) {
  AllowAttribute(getFrom(attribute));
}

void Sanitizer::removeAttribute(
    const V8UnionSanitizerAttributeNamespaceOrString* attribute) {
  RemoveAttribute(getFrom(attribute));
}

void Sanitizer::setComments(bool comments) {
  allow_comments_ = comments;
}

void Sanitizer::setDataAttributes(bool data_attributes) {
  allow_data_attrs_ = data_attributes;
}

void Sanitizer::removeUnsafe() {
  NOTREACHED();
}

SanitizerConfig* Sanitizer::get() const {
  HeapVector<Member<V8UnionSanitizerElementNamespaceWithAttributesOrString>>
      allow_elements;
  for (const QualifiedName& name : allow_elements_) {
    Member<SanitizerElementNamespaceWithAttributes> element =
        SanitizerElementNamespaceWithAttributes::Create();
    element->setName(name.LocalName());
    element->setNamespaceURI(name.NamespaceURI());

    const auto& allow_attrs_per_element_iter =
        allow_attrs_per_element_.find(name);
    if (allow_attrs_per_element_iter != allow_attrs_per_element_.end()) {
      HeapVector<Member<V8UnionSanitizerAttributeNamespaceOrString>>
          allow_attrs_per_element;
      for (const QualifiedName& attr_name :
           allow_attrs_per_element_iter->value) {
        Member<SanitizerAttributeNamespace> attr =
            SanitizerAttributeNamespace::Create();
        attr->setName(attr_name.LocalName());
        attr->setNamespaceURI(attr_name.NamespaceURI());
        allow_attrs_per_element.push_back(
            MakeGarbageCollected<V8UnionSanitizerAttributeNamespaceOrString>(
                attr));
      }
      element->setAttributes(allow_attrs_per_element);
    }

    const auto& remove_attrs_per_element_iter =
        remove_attrs_per_element_.find(name);
    if (remove_attrs_per_element_iter != remove_attrs_per_element_.end()) {
      HeapVector<Member<V8UnionSanitizerAttributeNamespaceOrString>>
          remove_attrs_per_element;
      for (const QualifiedName& attr_name :
           remove_attrs_per_element_iter->value) {
        Member<SanitizerAttributeNamespace> attr =
            SanitizerAttributeNamespace::Create();
        attr->setName(attr_name.LocalName());
        attr->setNamespaceURI(attr_name.NamespaceURI());
        remove_attrs_per_element.push_back(
            MakeGarbageCollected<V8UnionSanitizerAttributeNamespaceOrString>(
                attr));
      }
      element->setRemoveAttributes(remove_attrs_per_element);
    }

    allow_elements.push_back(
        MakeGarbageCollected<
            V8UnionSanitizerElementNamespaceWithAttributesOrString>(element));
  }

  HeapVector<Member<V8UnionSanitizerElementNamespaceOrString>> remove_elements;
  for (const QualifiedName& name : remove_elements_) {
    Member<SanitizerElementNamespace> element =
        SanitizerElementNamespace::Create();
    element->setName(name.LocalName());
    element->setNamespaceURI(name.NamespaceURI());
    remove_elements.push_back(
        MakeGarbageCollected<V8UnionSanitizerElementNamespaceOrString>(
            element));
  }

  HeapVector<Member<V8UnionSanitizerElementNamespaceOrString>> replace_elements;
  for (const QualifiedName& name : replace_elements_) {
    Member<SanitizerElementNamespace> element =
        SanitizerElementNamespace::Create();
    element->setName(name.LocalName());
    element->setNamespaceURI(name.NamespaceURI());
    replace_elements.push_back(
        MakeGarbageCollected<V8UnionSanitizerElementNamespaceOrString>(
            element));
  }

  HeapVector<Member<V8UnionSanitizerAttributeNamespaceOrString>> allow_attrs;
  for (const QualifiedName& name : allow_attrs_) {
    Member<SanitizerAttributeNamespace> attr =
        SanitizerAttributeNamespace::Create();
    attr->setName(name.LocalName());
    attr->setNamespaceURI(name.NamespaceURI());
    allow_attrs.push_back(
        MakeGarbageCollected<V8UnionSanitizerAttributeNamespaceOrString>(attr));
  }

  HeapVector<Member<V8UnionSanitizerAttributeNamespaceOrString>> remove_attrs;
  for (const QualifiedName& name : remove_attrs_) {
    Member<SanitizerAttributeNamespace> attr =
        SanitizerAttributeNamespace::Create();
    attr->setName(name.LocalName());
    attr->setNamespaceURI(name.NamespaceURI());
    remove_attrs.push_back(
        MakeGarbageCollected<V8UnionSanitizerAttributeNamespaceOrString>(attr));
  }

  SanitizerConfig* config = SanitizerConfig::Create();
  config->setElements(allow_elements);
  config->setRemoveElements(remove_elements);
  config->setReplaceWithChildrenElements(replace_elements);
  config->setAttributes(allow_attrs);
  config->setRemoveAttributes(remove_attrs);
  config->setDataAttributes(allow_data_attrs_);
  config->setComments(allow_comments_);

  return config;
}

void Sanitizer::AllowElement(const QualifiedName& name) {
  allow_elements_.insert(name);
  remove_elements_.erase(name);
  replace_elements_.erase(name);
  allow_attrs_per_element_.erase(name);
  remove_attrs_per_element_.erase(name);
}

void Sanitizer::RemoveElement(const QualifiedName& name) {
  allow_elements_.erase(name);
  remove_elements_.insert(name);
  replace_elements_.erase(name);
  allow_attrs_per_element_.erase(name);
  remove_attrs_per_element_.erase(name);
}

void Sanitizer::ReplaceElement(const QualifiedName& name) {
  allow_elements_.erase(name);
  remove_elements_.erase(name);
  replace_elements_.insert(name);
  allow_attrs_per_element_.erase(name);
  remove_attrs_per_element_.erase(name);
}

void Sanitizer::AllowAttribute(const QualifiedName& name) {
  allow_attrs_.insert(name);
  remove_attrs_.erase(name);
}

void Sanitizer::RemoveAttribute(const QualifiedName& name) {
  allow_attrs_.erase(name);
  remove_attrs_.insert(name);
}

bool Sanitizer::setFrom(const SanitizerConfig* config) {
  // This method assumes a newly constructed instance.
  CHECK(allow_elements_.empty());
  CHECK(remove_elements_.empty());
  CHECK(replace_elements_.empty());
  CHECK(allow_attrs_.empty());
  CHECK(remove_attrs_.empty());
  CHECK(allow_attrs_per_element_.empty());
  CHECK(remove_attrs_per_element_.empty());

  if (config->hasElements()) {
    for (const auto& element : config->elements()) {
      allowElement(element);
    }
  }
  if (config->hasRemoveElements()) {
    for (const auto& element : config->removeElements()) {
      removeElement(element);
    }
  }
  if (config->hasReplaceWithChildrenElements()) {
    for (const auto& element : config->replaceWithChildrenElements()) {
      replaceWithChildrenElement(element);
    }
  }
  if (config->hasAttributes()) {
    for (const auto& attribute : config->attributes()) {
      allowAttribute(attribute);
    }
  }
  if (config->hasRemoveAttributes()) {
    for (const auto& attribute : config->removeAttributes()) {
      removeAttribute(attribute);
    }
  }
  if (config->hasComments()) {
    setComments(config->comments());
  }
  if (config->hasDataAttributes()) {
    setDataAttributes(config->dataAttributes());
  }
  return true;
}

QualifiedName Sanitizer::getFrom(const String& name,
                                 const String& namespaceURI) const {
  return QualifiedName(g_null_atom, AtomicString(name),
                       AtomicString(namespaceURI));
}

QualifiedName Sanitizer::getFrom(
    const SanitizerElementNamespace* element) const {
  CHECK(element->hasNamespaceURI());  // Declared with default.
  if (!element->hasName()) {
    return g_null_name;
  }
  return getFrom(element->name(), element->namespaceURI());
}

QualifiedName Sanitizer::getFrom(
    const V8UnionSanitizerElementNamespaceWithAttributesOrString* element)
    const {
  if (element->IsString()) {
    return getFrom(element->GetAsString(), "http://www.w3.org/1999/xhtml");
  }
  return getFrom(element->GetAsSanitizerElementNamespaceWithAttributes());
}

QualifiedName Sanitizer::getFrom(
    const V8UnionSanitizerElementNamespaceOrString* element) const {
  if (element->IsString()) {
    return getFrom(element->GetAsString(), "http://www.w3.org/1999/xhtml");
  }
  return getFrom(element->GetAsSanitizerElementNamespace());
}

QualifiedName Sanitizer::getFrom(
    const V8UnionSanitizerAttributeNamespaceOrString* attr) const {
  if (attr->IsString()) {
    return getFrom(attr->GetAsString(), g_empty_atom);
  }
  const SanitizerAttributeNamespace* attr_namespace =
      attr->GetAsSanitizerAttributeNamespace();
  if (!attr_namespace->hasName()) {
    return g_null_name;
  }
  return getFrom(attr_namespace->name(), attr_namespace->namespaceURI());
}

}  // namespace blink
