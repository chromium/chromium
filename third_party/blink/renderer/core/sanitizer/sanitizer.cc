// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_attribute_namespace.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_element_namespace.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_element_namespace_with_attributes.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_presets.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizerattributenamespace_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizerconfig_sanitizerpresets.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizerelementnamespace_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sanitizerelementnamespacewithattributes_string.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

Sanitizer* Sanitizer::Create(
    const V8UnionSanitizerConfigOrSanitizerPresets* config_or_preset,
    ExceptionState& exception_state) {
  if (!config_or_preset) {
    return Create(nullptr, /*safe*/ false, exception_state);
  } else if (config_or_preset->IsSanitizerConfig()) {
    return Create(config_or_preset->GetAsSanitizerConfig(), /*safe*/ false,
                  exception_state);
  } else if (config_or_preset->IsSanitizerPresets()) {
    return Create(config_or_preset->GetAsSanitizerPresets().AsEnum(),
                  exception_state);
  } else {
    NOTREACHED();
  }
}

Sanitizer* Sanitizer::Create(const SanitizerConfig* sanitizer_config,
                             bool safe,
                             ExceptionState& exception_state) {
  Sanitizer* sanitizer = MakeGarbageCollected<Sanitizer>();
  if (!sanitizer_config) {
    // Default case: Set from builtin Sanitizer.
    sanitizer->setFrom(*(safe ? SanitizerBuiltins::GetDefaultSafe()
                              : SanitizerBuiltins::GetDefaultUnsafe()));
    return sanitizer;
  }

  bool success = sanitizer->setFrom(sanitizer_config, !safe);
  if (!success) {
    exception_state.ThrowTypeError("Invalid Sanitizer configuration.");
    return nullptr;
  }
  return sanitizer;
}

Sanitizer* Sanitizer::Create(const V8SanitizerPresets::Enum preset,
                             ExceptionState&) {
  CHECK_EQ(preset, V8SanitizerPresets::Enum::kDefault);
  Sanitizer* sanitizer = MakeGarbageCollected<Sanitizer>();
  sanitizer->setFrom(*SanitizerBuiltins::GetDefaultSafe());
  return sanitizer;
}

Sanitizer* Sanitizer::CreateEmpty() {
  Sanitizer* sanitizer = MakeGarbageCollected<Sanitizer>();
  sanitizer->remove_elements_ = std::make_unique<SanitizerNameSet>();
  sanitizer->remove_attrs_ = std::make_unique<SanitizerNameSet>();
  sanitizer->data_attrs_ = SanitizerBoolWithAbsence::kAbsent;
  sanitizer->comments_ = SanitizerBoolWithAbsence::kAbsent;
  return sanitizer;
}

Sanitizer::Sanitizer(std::unique_ptr<SanitizerNameSet> allow_elements,
                     std::unique_ptr<SanitizerNameSet> remove_elements,
                     std::unique_ptr<SanitizerNameSet> replace_elements,
                     std::unique_ptr<SanitizerNameSet> allow_attrs,
                     std::unique_ptr<SanitizerNameSet> remove_attrs,
                     bool allow_data_attrs,
                     bool allow_comments)
    : allow_elements_(allow_elements.release()),
      remove_elements_(remove_elements.release()),
      replace_elements_(replace_elements.release()),
      allow_attrs_(allow_attrs.release()),
      remove_attrs_(remove_attrs.release()),
      data_attrs_(allow_data_attrs ? SanitizerBoolWithAbsence::kTrue
                                   : SanitizerBoolWithAbsence::kFalse),
      comments_(allow_comments ? SanitizerBoolWithAbsence::kTrue
                               : SanitizerBoolWithAbsence::kFalse) {}

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
          allow_attrs_per_element_.insert(name, SanitizerNameSet());
      for (const auto& attr : element_with_attrs->attributes()) {
        add_result.stored_value->value.insert(getFrom(attr));
      }
    }
    if (element_with_attrs->hasRemoveAttributes()) {
      const auto add_result =
          remove_attrs_per_element_.insert(name, SanitizerNameSet());
      for (const auto& attr : element_with_attrs->removeAttributes()) {
        add_result.stored_value->value.insert(getFrom(attr));
      }
    }
  }
  // Remove dupes, if both allow and remove attribute lists were given.
  if (allow_attrs_per_element_.Contains(name) &&
      remove_attrs_per_element_.Contains(name)) {
    allow_attrs_per_element_.find(name)->value.RemoveAll(
        remove_attrs_per_element_.at(name));
  }
}

void Sanitizer::removeElement(
    const V8UnionSanitizerElementNamespaceOrString* element) {
  RemoveElement(getFrom(element));
}

void Sanitizer::replaceElementWithChildren(
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
  comments_ = comments ? SanitizerBoolWithAbsence::kTrue
                       : SanitizerBoolWithAbsence::kFalse;
}

void Sanitizer::setDataAttributes(bool data_attributes) {
  data_attrs_ = data_attributes ? SanitizerBoolWithAbsence::kTrue
                                : SanitizerBoolWithAbsence::kFalse;
}

void Sanitizer::removeUnsafe() {
  const Sanitizer* baseline = SanitizerBuiltins::GetBaseline();

  // Below, we rely on the baseline being expressed as allow-lists. Ensure that
  // this is so, given how important `removeUnsafe` is for the Sanitizer.
  CHECK(baseline->remove_elements_);
  CHECK(baseline->remove_attrs_);
  CHECK(!baseline->allow_elements_);
  CHECK(!baseline->replace_elements_);
  CHECK(!baseline->allow_attrs_);
  CHECK(!baseline->replace_elements_);
  CHECK(baseline->allow_attrs_per_element_.empty());
  CHECK(baseline->remove_attrs_per_element_.empty());

  for (const QualifiedName& name : *(baseline->remove_elements_)) {
    RemoveElement(name);
  }
  for (const QualifiedName& name : *(baseline->remove_attrs_)) {
    RemoveAttribute(name);
  }
}

bool SanitizerAtomicStringLessThan(const AtomicString& a,
                                   const AtomicString& b) {
  // https://wicg.github.io/sanitizer-api/#sanitizerconfig-less-than-item
  // The spec wants a comparison operator where null is the bottom element.
  if (b.IsNull()) {
    return false;
  } else if (a.IsNull()) {
    return true;
  } else {
    return CodeUnitCompare(a.Impl(), b.Impl()) < 0;
  }
}

bool SanitizerQNameLessThan(const QualifiedName& a, const QualifiedName& b) {
  // https://wicg.github.io/sanitizer-api/#sanitizerconfig-less-than-item
  return (a.NamespaceURI() != b.NamespaceURI())
             ? SanitizerAtomicStringLessThan(a.NamespaceURI(), b.NamespaceURI())
             : SanitizerAtomicStringLessThan(a.LocalName(), b.LocalName());
}

Vector<QualifiedName> Sorted(const SanitizerNameSet& unsorted) {
  Vector<QualifiedName> result;
  std::ranges::copy(unsorted, std::back_inserter(result));
  std::ranges::sort(result, &SanitizerQNameLessThan);
  return result;
}

SanitizerConfig* Sanitizer::get() const {
  // https://wicg.github.io/sanitizer-api/#dom-sanitizer-get
  //
  // This methods converts from the internal representation (QName sets) to
  // JS objects. This method looks extremely repetitive, but because IDL maps
  // the JS members to different C++ types
  // (V8UnionSanitizerElementNamespaceWithAttributesOrString,
  // V8UnionSanitizerElementNamespaceOrString, or
  // V8UnionSanitizerAttributeNamespaceOrString) these can't easily be
  // refactored into common code.
  SanitizerConfig* config = SanitizerConfig::Create();

  if (allow_elements_) {
    HeapVector<Member<V8UnionSanitizerElementNamespaceWithAttributesOrString>>
        allow_elements;
    for (const QualifiedName& name : Sorted(*allow_elements_)) {
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
             Sorted(allow_attrs_per_element_iter->value)) {
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
             Sorted(remove_attrs_per_element_iter->value)) {
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

      if (!element->hasAttributes() && !element->hasRemoveAttributes()) {
        HeapVector<Member<V8UnionSanitizerAttributeNamespaceOrString>>
            remove_attrs_per_element;
        element->setRemoveAttributes(remove_attrs_per_element);
      }

      allow_elements.push_back(
          MakeGarbageCollected<
              V8UnionSanitizerElementNamespaceWithAttributesOrString>(element));
    }
    config->setElements(allow_elements);
  }

  if (remove_elements_) {
    HeapVector<Member<V8UnionSanitizerElementNamespaceOrString>>
        remove_elements;
    for (const QualifiedName& name : Sorted(*remove_elements_)) {
      Member<SanitizerElementNamespace> element =
          SanitizerElementNamespace::Create();
      element->setName(name.LocalName());
      element->setNamespaceURI(name.NamespaceURI());
      remove_elements.push_back(
          MakeGarbageCollected<V8UnionSanitizerElementNamespaceOrString>(
              element));
    }
    config->setRemoveElements(remove_elements);
  }

  if (replace_elements_) {
    HeapVector<Member<V8UnionSanitizerElementNamespaceOrString>>
        replace_elements;
    for (const QualifiedName& name : Sorted(*replace_elements_)) {
      Member<SanitizerElementNamespace> element =
          SanitizerElementNamespace::Create();
      element->setName(name.LocalName());
      element->setNamespaceURI(name.NamespaceURI());
      replace_elements.push_back(
          MakeGarbageCollected<V8UnionSanitizerElementNamespaceOrString>(
              element));
    }
    config->setReplaceWithChildrenElements(replace_elements);
  }

  if (allow_attrs_) {
    HeapVector<Member<V8UnionSanitizerAttributeNamespaceOrString>> allow_attrs;
    for (const QualifiedName& name : Sorted(*allow_attrs_)) {
      Member<SanitizerAttributeNamespace> attr =
          SanitizerAttributeNamespace::Create();
      attr->setName(name.LocalName());
      attr->setNamespaceURI(name.NamespaceURI());
      allow_attrs.push_back(
          MakeGarbageCollected<V8UnionSanitizerAttributeNamespaceOrString>(
              attr));
    }
    config->setAttributes(allow_attrs);
  }

  if (remove_attrs_) {
    HeapVector<Member<V8UnionSanitizerAttributeNamespaceOrString>> remove_attrs;
    for (const QualifiedName& name : Sorted(*remove_attrs_)) {
      Member<SanitizerAttributeNamespace> attr =
          SanitizerAttributeNamespace::Create();
      attr->setName(name.LocalName());
      attr->setNamespaceURI(name.NamespaceURI());
      remove_attrs.push_back(
          MakeGarbageCollected<V8UnionSanitizerAttributeNamespaceOrString>(
              attr));
    }
    config->setRemoveAttributes(remove_attrs);
  }

  if (data_attrs_ != SanitizerBoolWithAbsence::kAbsent) {
    config->setDataAttributes(data_attrs_ == SanitizerBoolWithAbsence::kTrue);
  }
  if (comments_ != SanitizerBoolWithAbsence::kAbsent) {
    config->setComments(comments_ == SanitizerBoolWithAbsence::kTrue);
  }

  return config;
}

void Sanitizer::AllowElement(const QualifiedName& name) {
  if (!allow_elements_) {
    allow_elements_ = std::make_unique<SanitizerNameSet>();
  }
  allow_elements_->insert(name);
  if (remove_elements_) {
    remove_elements_->erase(name);
  }
  if (replace_elements_) {
    replace_elements_->erase(name);
  }
  allow_attrs_per_element_.erase(name);
  remove_attrs_per_element_.erase(name);
}

void Sanitizer::RemoveElement(const QualifiedName& name) {
  if (allow_elements_) {
    allow_elements_->erase(name);
  }
  if (!remove_elements_) {
    remove_elements_ = std::make_unique<SanitizerNameSet>();
  }
  remove_elements_->insert(name);
  if (replace_elements_) {
    replace_elements_->erase(name);
  }
  allow_attrs_per_element_.erase(name);
  remove_attrs_per_element_.erase(name);
}

void Sanitizer::ReplaceElement(const QualifiedName& name) {
  if (allow_elements_) {
    allow_elements_->erase(name);
  }
  if (remove_elements_) {
    remove_elements_->erase(name);
  }
  if (!replace_elements_) {
    replace_elements_ = std::make_unique<SanitizerNameSet>();
  }
  replace_elements_->insert(name);
  allow_attrs_per_element_.erase(name);
  remove_attrs_per_element_.erase(name);
}

void Sanitizer::AllowAttribute(const QualifiedName& name) {
  if (!allow_attrs_) {
    allow_attrs_ = std::make_unique<SanitizerNameSet>();
  }
  allow_attrs_->insert(name);
  if (remove_attrs_) {
    remove_attrs_->erase(name);
  }
}

void Sanitizer::RemoveAttribute(const QualifiedName& name) {
  if (allow_attrs_) {
    allow_attrs_->erase(name);
  }
  if (!remove_attrs_) {
    remove_attrs_ = std::make_unique<SanitizerNameSet>();
  }
  remove_attrs_->insert(name);
}

void Sanitizer::SanitizeElement(Element* element) const {
  const auto allow_per_element_iter =
      allow_attrs_per_element_.find(element->TagQName());
  const SanitizerNameSet* allow_per_element =
      (allow_per_element_iter == allow_attrs_per_element_.end())
          ? nullptr
          : &allow_per_element_iter->value;
  const auto remove_per_element_iter =
      remove_attrs_per_element_.find(element->TagQName());
  const SanitizerNameSet* remove_per_element =
      (remove_per_element_iter == remove_attrs_per_element_.end())
          ? nullptr
          : &remove_per_element_iter->value;
  for (const QualifiedName& name : element->getAttributeQualifiedNames()) {
    bool keep = false;
    if (allow_attrs_ && allow_attrs_->Contains(name)) {
      keep = true;
    } else if (remove_attrs_ && remove_attrs_->Contains(name)) {
      keep = false;
    } else if (allow_per_element && allow_per_element->Contains(name)) {
      keep = true;
    } else if (remove_per_element && remove_per_element->Contains(name)) {
      keep = false;
    } else if (allow_attrs_ && name.NamespaceURI().IsNull() &&
               name.LocalName().StartsWith("data-")) {
      keep = data_attrs_ == SanitizerBoolWithAbsence::kTrue;
    } else {
      keep =
          !allow_attrs_ && (!allow_per_element || allow_per_element->empty());
    }
    if (!keep) {
      element->removeAttribute(name);
    }
  }
}

void RemoveAttributeIfProtocolIsJavaScript(Element* element,
                                           const QualifiedName& attribute) {
  const AtomicString& value = element->getAttribute(attribute);
  if (value && KURL(value.GetString()).ProtocolIsJavaScript()) {
    element->removeAttribute(attribute);
  }
}

void RemoveAttributeIfValueIsHref(Element* element,
                                  const QualifiedName& attribute) {
  const AtomicString& value = element->getAttribute(attribute);
  if (value == "href" or value == "xlink:href") {
    element->removeAttribute(attribute);
  }
}

void Sanitizer::SanitizeJavascriptNavigationAttributes(Element* element,
                                                       bool safe) const {
  // Special treatment of javascript: URLs when used for navigation.
  // https://wicg.github.io/sanitizer-api/#sanitize-core, Steps 2.4.6.*

  if (!safe) {
    return;
  }

  // Attributes that trigger navigation:
  const QualifiedName& qname = element->TagQName();
  if (qname == html_names::kATag || qname == html_names::kAreaTag ||
      qname == html_names::kBaseTag) {
    RemoveAttributeIfProtocolIsJavaScript(element, html_names::kHrefAttr);
  } else if (qname == svg_names::kATag ||
             element->namespaceURI() == mathml_names::kNamespaceURI) {
    RemoveAttributeIfProtocolIsJavaScript(element, html_names::kHrefAttr);
    RemoveAttributeIfProtocolIsJavaScript(element, xlink_names::kHrefAttr);
  } else if (qname == html_names::kButtonTag ||
             qname == html_names::kInputTag) {
    RemoveAttributeIfProtocolIsJavaScript(element, html_names::kFormactionAttr);
  } else if (qname == html_names::kFormTag) {
    RemoveAttributeIfProtocolIsJavaScript(element, html_names::kActionAttr);
  } else if (qname == html_names::kIFrameTag) {
    RemoveAttributeIfProtocolIsJavaScript(element, html_names::kSrcAttr);

    // SVG animations of navigating attributes:
  } else if (qname == svg_names::kAnimateTag ||
             qname == svg_names::kAnimateMotionTag ||
             qname == svg_names::kAnimateTransformTag ||
             qname == svg_names::kSetTag) {
    RemoveAttributeIfValueIsHref(element, svg_names::kAttributeNameAttr);
  }
}

void Sanitizer::SanitizeTemplate(Node* node, bool safe) const {
  // Recurse into template and (later) shadow root content.
  // TODO(vogelheim): Also implement shadow root support, once that's settled
  // down.
  if (IsA<HTMLTemplateElement>(node)) {
    Node* content = To<HTMLTemplateElement>(node)->content();
    if (content) {
      Sanitize(content, safe);
    }
  }
}

void Sanitizer::SanitizeSafe(Node* root) const {
  // TODO(vogelheim): This is hideously inefficient, but very easy to implement.
  // We'll use this for now, so we can fully build out tests & other
  // infrastructure, and worry about efficiency later.
  Sanitizer* safe = MakeGarbageCollected<Sanitizer>();
  safe->setFrom(*this);
  safe->removeUnsafe();
  safe->Sanitize(root, /*safe*/ true);
}

void Sanitizer::SanitizeUnsafe(Node* root) const {
  Sanitize(root, /*safe*/ false);
}

void Sanitizer::Sanitize(Node* root, bool safe) const {
  SanitizeTemplate(root, safe);

  enum { kKeep, kKeepElement, kDrop, kReplaceWithChildren } action = kKeep;
  Node* node = NodeTraversal::Next(*root);
  while (node) {
    switch (node->getNodeType()) {
      case Node::NodeType::kElementNode: {
        Element* element = To<Element>(node);
        if (allow_elements_ && allow_elements_->Contains(element->TagQName())) {
          action = kKeepElement;
        } else if (replace_elements_ &&
                   replace_elements_->Contains(element->TagQName())) {
          action = kReplaceWithChildren;
        } else if (!allow_elements_ && remove_elements_ &&
                   !remove_elements_->Contains(element->TagQName())) {
          action = kKeepElement;
        } else {
          action = kDrop;
        }
        break;
      }
      case Node::NodeType::kTextNode:
        action = kKeep;
        break;
      case Node::NodeType::kCommentNode:
        action = (comments_ == SanitizerBoolWithAbsence::kTrue) ? kKeep : kDrop;
        break;
      case Node::NodeType::kDocumentTypeNode:
        // Should only happen when parsing full documents w/ parseHTML.
        DCHECK(root->IsDocumentNode());
        action = kKeep;
        break;

      default:
        NOTREACHED();
    }

    switch (action) {
      case kKeepElement: {
        CHECK_EQ(node->getNodeType(), Node::NodeType::kElementNode);
        SanitizeElement(To<Element>(node));
        SanitizeJavascriptNavigationAttributes(To<Element>(node), safe);
        SanitizeTemplate(node, safe);
        node = NodeTraversal::Next(*node);
        break;
      }
      case kKeep: {
        CHECK_NE(node->getNodeType(), Node::NodeType::kElementNode);
        node = NodeTraversal::Next(*node);
        break;
      }
      case kReplaceWithChildren: {
        CHECK_EQ(node->getNodeType(), Node::NodeType::kElementNode);
        Node* next_node = node->firstChild();
        if (!next_node) {
          next_node = NodeTraversal::Next(*node);
        }
        ContainerNode* parent = node->parentNode();
        while (Node* child = node->firstChild()) {
          parent->InsertBefore(child, node);
        }
        node->remove();
        node = next_node;
        break;
      }
      case kDrop: {
        Node* next_node = NodeTraversal::NextSkippingChildren(*node);
        node->parentNode()->removeChild(node);
        node = next_node;
        break;
      }
    }
  }
}

bool Sanitizer::setFrom(const SanitizerConfig* config,
                        bool allowCommentsAndDataAttributes) {
  // https://wicg.github.io/sanitizer-api/#configuration-set
  //
  // Since out internal representation is quite different from the external one,
  // the structure here is quite different from the spec text.

  // This method assumes a newly constructed instance.
  CHECK(!allow_elements_);
  CHECK(!remove_elements_);
  CHECK(!replace_elements_);
  CHECK(!allow_attrs_);
  CHECK(!remove_attrs_);
  CHECK(allow_attrs_per_element_.empty());
  CHECK(remove_attrs_per_element_.empty());

  // The spec checks that no duplicate entries exist. We solve that here by
  // keeping track of whether all set insertion were new entries.
  bool all_new_entries = true;

  if (config->hasElements()) {
    allow_elements_ = std::make_unique<SanitizerNameSet>();
    for (const auto& element : config->elements()) {
      QualifiedName element_name = getFrom(element);
      all_new_entries &= allow_elements_->insert(element_name).is_new_entry;

      if (element->IsSanitizerElementNamespaceWithAttributes()) {
        if (element->GetAsSanitizerElementNamespaceWithAttributes()
                ->hasAttributes()) {
          SanitizerNameSet attrs_per_element;
          for (const auto& attribute :
               element->GetAsSanitizerElementNamespaceWithAttributes()
                   ->attributes()) {
            all_new_entries &=
                attrs_per_element.insert(getFrom(attribute)).is_new_entry;
          }
          allow_attrs_per_element_.insert(element_name, attrs_per_element);
        }
        if (element->GetAsSanitizerElementNamespaceWithAttributes()
                ->hasRemoveAttributes()) {
          SanitizerNameSet attrs_per_element;
          for (const auto& attribute :
               element->GetAsSanitizerElementNamespaceWithAttributes()
                   ->removeAttributes()) {
            all_new_entries &=
                attrs_per_element.insert(getFrom(attribute)).is_new_entry;
          }
          remove_attrs_per_element_.insert(element_name, attrs_per_element);
        }
      }
    }
  }
  if (config->hasRemoveElements()) {
    remove_elements_ = std::make_unique<SanitizerNameSet>();
    for (const auto& element : config->removeElements()) {
      all_new_entries &=
          remove_elements_->insert(getFrom(element)).is_new_entry;
    }
  }
  if (config->hasReplaceWithChildrenElements()) {
    replace_elements_ = std::make_unique<SanitizerNameSet>();
    for (const auto& element : config->replaceWithChildrenElements()) {
      all_new_entries &=
          replace_elements_->insert(getFrom(element)).is_new_entry;
    }
  }
  if (config->hasAttributes()) {
    allow_attrs_ = std::make_unique<SanitizerNameSet>();
    for (const auto& attribute : config->attributes()) {
      all_new_entries &= allow_attrs_->insert(getFrom(attribute)).is_new_entry;
    }
  }
  if (config->hasRemoveAttributes()) {
    remove_attrs_ = std::make_unique<SanitizerNameSet>();
    for (const auto& attribute : config->removeAttributes()) {
      all_new_entries &= remove_attrs_->insert(getFrom(attribute)).is_new_entry;
    }
  }
  setComments(config->getCommentsOr(allowCommentsAndDataAttributes));
  if (allow_attrs_ || config->hasDataAttributes()) {
    setDataAttributes(
        config->getDataAttributesOr(allowCommentsAndDataAttributes));
  }

  // https://wicg.github.io/sanitizer-api/#sanitizer-canonicalize-the-configuration,
  // steps 1 + 2.
  if (!config->hasElements() && !config->hasRemoveElements()) {
    remove_elements_ = std::make_unique<SanitizerNameSet>();
  }
  if (!config->hasAttributes() && !config->hasRemoveAttributes()) {
    remove_attrs_ = std::make_unique<SanitizerNameSet>();
  }

  return all_new_entries && isValid();
}

void Sanitizer::setFrom(const Sanitizer& other) {
  allow_elements_ =
      other.allow_elements_
          ? std::make_unique<SanitizerNameSet>(*other.allow_elements_.get())
          : nullptr;
  remove_elements_ =
      other.remove_elements_
          ? std::make_unique<SanitizerNameSet>(*other.remove_elements_.get())
          : nullptr;
  replace_elements_ =
      other.replace_elements_
          ? std::make_unique<SanitizerNameSet>(*other.replace_elements_.get())
          : nullptr;
  allow_attrs_ =
      other.allow_attrs_
          ? std::make_unique<SanitizerNameSet>(*other.allow_attrs_.get())
          : nullptr;
  remove_attrs_ =
      other.remove_attrs_
          ? std::make_unique<SanitizerNameSet>(*other.remove_attrs_.get())
          : nullptr;
  allow_attrs_per_element_ = other.allow_attrs_per_element_;
  remove_attrs_per_element_ = other.remove_attrs_per_element_;
  data_attrs_ = other.data_attrs_;
  comments_ = other.comments_;
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

bool Intersect(const SanitizerNameSet& a, const SanitizerNameSet& b) {
  for (const QualifiedName& name : a) {
    if (b.Contains(name)) {
      return true;
    }
  }
  return false;
}

bool Intersect(const std::unique_ptr<SanitizerNameSet>& a,
               const SanitizerNameSet& b) {
  if (!a) {
    return false;
  }
  return Intersect(*a.get(), b);
}

bool Intersect(const std::unique_ptr<SanitizerNameSet>& a,
               const std::unique_ptr<SanitizerNameSet>& b) {
  if (!a || !b) {
    return false;
  }
  return Intersect(*a.get(), *b.get());
}

bool Subset(const SanitizerNameSet& a, const SanitizerNameSet& b) {
  for (const QualifiedName& name : a) {
    if (!b.Contains(name)) {
      return false;
    }
  }
  return true;
}

bool Sanitizer::isValid() const {
  // https://wicg.github.io/sanitizer-api/#sanitizerconfig-valid
  // Step 1: [..] either an elements or a removeElements key, but not both.
  if (allow_elements_ && remove_elements_) {
    return false;
  }
  // Step 2: [..] either an attributes or a removeAttributes key, but not both.
  if (allow_attrs_ && remove_attrs_) {
    return false;
  }
  // Step 3: Assert. (Not meaningful here, since we use QNames.)
  // Step 4: None of [...], if they exist, has duplicates.
  //   (Not meaningful here, since we use sets.)
  // Step 5: If both config[elements] and config[replaceWithChildrenElements]
  //   exist, then the intersection of config[elements] and
  //   config[replaceWithChildrenElements] is empty.
  if (Intersect(allow_elements_, replace_elements_)) {
    return false;
  }
  // Step 6: If both config[removeElements] and
  //   config[replaceWithChildrenElements] exist, then the intersection of
  //   config[removeElements] and config[replaceWithChildrenElements] is empty.
  if (Intersect(remove_elements_, replace_elements_)) {
    return false;
  }
  // Step 7: If config[attributes] exists:
  if (allow_attrs_) {
    // Step 7.1: If config[elements] exists:
    if (allow_elements_) {
      // Step 7.1.1: For each element of config[elements]:
      for (const auto& element : *allow_elements_) {
        // Step 7.1.1.1: [No dupes:] element[attributes] +
        //   element[removeAttributes] (Not meaningful here, since we use sets.)
        // Step 7.1.1.2: The intersection of config[attributes] and
        //   element[attributes] [..] is empty.
        if (allow_attrs_per_element_.Contains(element) &&
            Intersect(allow_attrs_, allow_attrs_per_element_.at(element))) {
          return false;
        }
        // Step 7.1.1.3: element[removeAttributes] [..] is a subset of
        // config[attributes]
        if (remove_attrs_per_element_.Contains(element) && allow_attrs_ &&
            !Subset(remove_attrs_per_element_.at(element),
                    *allow_attrs_.get())) {
          return false;
        }
        // Step 7.1.1.4: If dataAttributes exists and dataAttributes is true:
        if (data_attrs_ == SanitizerBoolWithAbsence::kTrue) {
          // Step 7.1.1.5: element[attributes] does not contain a custom data
          // attribute.
          if (allow_attrs_per_element_.Contains(element)) {
            for (const auto& attr : allow_attrs_per_element_.at(element)) {
              if (attr.LocalName().StartsWith("data-")) {
                return false;
              }
            }
          }
        }
      }
    }
    // Step 7.2: If dataAttributes is true:
    if (data_attrs_ == SanitizerBoolWithAbsence::kTrue) {
      // Step 7.2.1: config[attributes] does not contain a custom data
      // attribute.
      for (const auto& attr : *allow_attrs_) {
        if (attr.LocalName().StartsWith("data-")) {
          return false;
        }
      }
    }
  }
  // Step 8: If config[removeAttributes] exists:
  if (remove_attrs_) {
    // Step 8.1: If config[elements] exists, then for each element of
    // config[elements]:
    if (allow_elements_) {
      for (const auto& element : *allow_elements_) {
        // Step 8.1.1: Not both element[attributes] and
        // element[removeAttributes] exist.
        if (allow_attrs_per_element_.Contains(element) &&
            remove_attrs_per_element_.Contains(element)) {
          return false;
        }
        // Step 8.1.2: [No dupes.] (Not meaningful, since we're using sets.)
        // Step 8.1.3: The intersection of config[removeAttributes] and
        //   element[attributes] [..] is empty.
        if (allow_attrs_per_element_.Contains(element) &&
            Intersect(remove_attrs_, allow_attrs_per_element_.at(element))) {
          return false;
        }
        // Step 8.1.4: The intersection of config[removeAttributes] and
        //   element[removeAttributes] [..] is empty.
        if (remove_attrs_per_element_.Contains(element) &&
            Intersect(remove_attrs_, remove_attrs_per_element_.at(element))) {
          return false;
        }
      }
    }
    // Step 8.2: config[dataAttributes] does not exist.
    if (data_attrs_ != SanitizerBoolWithAbsence::kAbsent) {
      return false;
    }
  }

  return true;
}

}  // namespace blink
