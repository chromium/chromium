/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/serializers/markup_accumulator.h"

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

class MarkupAccumulator::NamespaceContext final {
  USING_FAST_MALLOC(MarkupAccumulator::NamespaceContext);

 public:
  // https://w3c.github.io/DOM-Parsing/#dfn-add
  //
  // This function doesn't accept empty prefix and empty namespace URI.
  //  - The default namespace is managed separately.
  //  - Namespace URI never be empty if the prefix is not empty.
  void Add(const AtomicString& prefix, const AtomicString& namespace_uri) {
    DCHECK(!prefix.IsEmpty())
        << " prefix=" << prefix << " namespace_uri=" << namespace_uri;
    DCHECK(!namespace_uri.IsEmpty())
        << " prefix=" << prefix << " namespace_uri=" << namespace_uri;
    prefix_ns_map_.Set(prefix, namespace_uri);
    auto result =
        ns_prefixes_map_.insert(namespace_uri, Vector<AtomicString>());
    result.stored_value->value.push_back(prefix);
  }

  // https://w3c.github.io/DOM-Parsing/#dfn-recording-the-namespace-information
  AtomicString RecordNamespaceInformation(const Element& element) {
    AtomicString local_default_namespace;
    // 2. For each attribute attr in element's attributes, in the order they are
    // specified in the element's attribute list:
    for (const auto& attr : element.Attributes()) {
      // We don't check xmlns namespace of attr here because xmlns attributes in
      // HTML documents don't have namespace URI. Some web tests serialize
      // HTML documents with XMLSerializer, and Firefox has the same behavior.
      if (attr.Prefix().IsEmpty() && attr.LocalName() == g_xmlns_atom) {
        // 3.1. If attribute prefix is null, then attr is a default namespace
        // declaration. Set the default namespace attr value to attr's value
        // and stop running these steps, returning to Main to visit the next
        // attribute.
        local_default_namespace = attr.Value();
      } else if (attr.Prefix() == g_xmlns_atom) {
        Add(attr.Prefix() ? attr.LocalName() : g_empty_atom, attr.Value());
      }
    }
    // 3. Return the value of default namespace attr value.
    return local_default_namespace;
  }

  AtomicString LookupNamespaceURI(const AtomicString& prefix) const {
    return prefix_ns_map_.at(prefix ? prefix : g_empty_atom);
  }

  const AtomicString& ContextNamespace() const { return context_namespace_; }
  void SetContextNamespace(const AtomicString& context_ns) {
    context_namespace_ = context_ns;
  }

  void InheritLocalDefaultNamespace(
      const AtomicString& local_default_namespace) {
    if (!local_default_namespace)
      return;
    SetContextNamespace(local_default_namespace.IsEmpty()
                            ? g_null_atom
                            : local_default_namespace);
  }

  const Vector<AtomicString> PrefixList(const AtomicString& ns) const {
    return ns_prefixes_map_.at(ns ? ns : g_empty_atom);
  }

 private:
  using PrefixToNamespaceMap = HashMap<AtomicString, AtomicString>;
  PrefixToNamespaceMap prefix_ns_map_;

  // Map a namespace URI to a list of prefixes.
  // https://w3c.github.io/DOM-Parsing/#the-namespace-prefix-map
  using NamespaceToPrefixesMap = HashMap<AtomicString, Vector<AtomicString>>;
  NamespaceToPrefixesMap ns_prefixes_map_;

  // https://w3c.github.io/DOM-Parsing/#dfn-context-namespace
  AtomicString context_namespace_;
};

// This stores values used to serialize an element. The values are not
// inherited to child node serialization.
class MarkupAccumulator::ElementSerializationData final {
  STACK_ALLOCATED();

 public:
  // https://w3c.github.io/DOM-Parsing/#dfn-ignore-namespace-definition-attribute
  bool ignore_namespace_definition_attribute_ = false;

  AtomicString serialized_prefix_;
};

MarkupAccumulator::MarkupAccumulator(AbsoluteURLs resolve_urls_method,
                                     SerializationType serialization_type)
    : formatter_(resolve_urls_method, serialization_type) {}

MarkupAccumulator::~MarkupAccumulator() = default;

void MarkupAccumulator::AppendString(const String& string) {
  markup_.Append(string);
}

void MarkupAccumulator::AppendEndTag(const Element& element,
                                     const AtomicString& prefix) {
  formatter_.AppendEndMarkup(markup_, element, prefix, element.localName());
}

void MarkupAccumulator::AppendStartMarkup(const Node& node) {
  switch (node.getNodeType()) {
    case Node::kTextNode:
      formatter_.AppendText(markup_, To<Text>(node));
      break;
    case Node::kElementNode:
      NOTREACHED();
      break;
    case Node::kAttributeNode:
      // Only XMLSerializer can pass an Attr.  So, |documentIsHTML| flag is
      // false.
      formatter_.AppendAttributeValue(markup_, To<Attr>(node).value(), false);
      break;
    default:
      formatter_.AppendStartMarkup(markup_, node);
      break;
  }
}

void MarkupAccumulator::AppendCustomAttributes(const Element&) {}

bool MarkupAccumulator::ShouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) const {
  return false;
}

bool MarkupAccumulator::ShouldIgnoreElement(const Element& element) const {
  return false;
}

AtomicString MarkupAccumulator::AppendElement(const Element& element) {
  const ElementSerializationData data = AppendStartTagOpen(element);
  if (SerializeAsHTML()) {
    // https://html.spec.whatwg.org/C/#html-fragment-serialisation-algorithm

    AttributeCollection attributes = element.Attributes();
    // 3.2. Element: If current node's is value is not null, and the
    // element does not have an is attribute in its attribute list, ...
    const AtomicString& is_value = element.IsValue();
    if (!is_value.IsNull() && !attributes.Find(html_names::kIsAttr)) {
      AppendAttribute(element, Attribute(html_names::kIsAttr, is_value));
    }
    for (const auto& attribute : attributes) {
      if (!ShouldIgnoreAttribute(element, attribute))
        AppendAttribute(element, attribute);
    }
  } else {
    // https://w3c.github.io/DOM-Parsing/#xml-serializing-an-element-node

    for (const auto& attribute : element.Attributes()) {
      if (data.ignore_namespace_definition_attribute_ &&
          attribute.NamespaceURI() == xmlns_names::kNamespaceURI &&
          attribute.Prefix().IsEmpty()) {
        // Drop xmlns= only if it's inconsistent with element's namespace.
        // https://github.com/w3c/DOM-Parsing/issues/47
        if (!EqualIgnoringNullity(attribute.Value(), element.namespaceURI()))
          continue;
      }
      if (!ShouldIgnoreAttribute(element, attribute))
        AppendAttribute(element, attribute);
    }
  }

  // Give an opportunity to subclasses to add their own attributes.
  AppendCustomAttributes(element);

  AppendStartTagClose(element);
  return data.serialized_prefix_;
}

MarkupAccumulator::ElementSerializationData
MarkupAccumulator::AppendStartTagOpen(const Element& element) {
  ElementSerializationData data;
  data.serialized_prefix_ = element.prefix();
  if (SerializeAsHTML()) {
    formatter_.AppendStartTagOpen(markup_, element);
    return data;
  }

  // https://w3c.github.io/DOM-Parsing/#xml-serializing-an-element-node

  NamespaceContext& namespace_context = namespace_stack_.back();

  // 5. Let ignore namespace definition attribute be a boolean flag with value
  // false.
  data.ignore_namespace_definition_attribute_ = false;
  // 8. Let local default namespace be the result of recording the namespace
  // information for node given map and local prefixes map.
  AtomicString local_default_namespace =
      namespace_context.RecordNamespaceInformation(element);
  // 9. Let inherited ns be a copy of namespace.
  AtomicString inherited_ns = namespace_context.ContextNamespace();
  // 10. Let ns be the value of node's namespaceURI attribute.
  AtomicString ns = element.namespaceURI();

  // 11. If inherited ns is equal to ns, then:
  if (inherited_ns == ns) {
    // 11.1. If local default namespace is not null, then set ignore namespace
    // definition attribute to true.
    data.ignore_namespace_definition_attribute_ =
        !local_default_namespace.IsNull();
    // 11.3. Otherwise, append to qualified name the value of node's
    // localName. The node's prefix if it exists, is dropped.

    // 11.4. Append the value of qualified name to markup.
    formatter_.AppendStartTagOpen(markup_, g_null_atom, element.localName());
    data.serialized_prefix_ = g_null_atom;
    return data;
  }

  // 12. Otherwise, inherited ns is not equal to ns (the node's own namespace is
  // different from the context namespace of its parent). Run these sub-steps:
  // 12.1. Let prefix be the value of node's prefix attribute.
  AtomicString prefix = element.prefix();
  // 12.2. Let candidate prefix be the result of retrieving a preferred prefix
  // string prefix from map given namespace ns.
  AtomicString candidate_prefix;
  if (!ns.IsEmpty() && (!prefix.IsEmpty() || ns != local_default_namespace)) {
    candidate_prefix = RetrievePreferredPrefixString(ns, prefix);
  }
  // 12.4. if candidate prefix is not null (a namespace prefix is defined which
  // maps to ns), then:
  if (!candidate_prefix.IsNull() && LookupNamespaceURI(candidate_prefix)) {
    // 12.4.1. Append to qualified name the concatenation of candidate prefix,
    // ":" (U+003A COLON), and node's localName.
    // 12.4.3. Append the value of qualified name to markup.
    formatter_.AppendStartTagOpen(markup_, candidate_prefix,
                                  element.localName());
    data.serialized_prefix_ = candidate_prefix;
    // 12.4.2. If the local default namespace is not null (there exists a
    // locally-defined default namespace declaration attribute) and its value is
    // not the XML namespace, then let inherited ns get the value of local
    // default namespace unless the local default namespace is the empty string
    // in which case let it get null (the context namespace is changed to the
    // declared default, rather than this node's own namespace).
    if (local_default_namespace != xml_names::kNamespaceURI)
      namespace_context.InheritLocalDefaultNamespace(local_default_namespace);
    return data;
  }

  // 12.5. Otherwise, if prefix is not null, then:
  if (!prefix.IsEmpty()) {
    // 12.5.1. If the local prefixes map contains a key matching prefix, then
    // let prefix be the result of generating a prefix providing as input map,
    // ns, and prefix index
    if (element.hasAttribute(WTF::g_xmlns_with_colon + prefix)) {
      prefix = GeneratePrefix(ns);
    } else {
      // 12.5.2. Add prefix to map given namespace ns.
      AddPrefix(prefix, ns);
    }
    // 12.5.3. Append to qualified name the concatenation of prefix, ":" (U+003A
    // COLON), and node's localName.
    // 12.5.4. Append the value of qualified name to markup.
    formatter_.AppendStartTagOpen(markup_, prefix, element.localName());
    data.serialized_prefix_ = prefix;
    // 12.5.5. Append the following to markup, in the order listed:
    MarkupFormatter::AppendAttribute(markup_, g_xmlns_atom, prefix, ns, false);
    // 12.5.5.7. If local default namespace is not null (there exists a
    // locally-defined default namespace declaration attribute), then let
    // inherited ns get the value of local default namespace unless the local
    // default namespace is the empty string in which case let it get null.
    namespace_context.InheritLocalDefaultNamespace(local_default_namespace);
    return data;
  }

  // 12.6. Otherwise, if local default namespace is null, or local default
  // namespace is not null and its value is not equal to ns, then:
  if (local_default_namespace.IsNull() ||
      !EqualIgnoringNullity(local_default_namespace, ns)) {
    // 12.6.1. Set the ignore namespace definition attribute flag to true.
    data.ignore_namespace_definition_attribute_ = true;
    // 12.6.3. Let the value of inherited ns be ns.
    namespace_context.SetContextNamespace(ns);
    // 12.6.4. Append the value of qualified name to markup.
    formatter_.AppendStartTagOpen(markup_, element);
    // 12.6.5. Append the following to markup, in the order listed:
    MarkupFormatter::AppendAttribute(markup_, g_null_atom, g_xmlns_atom, ns,
                                     false);
    return data;
  }

  // 12.7. Otherwise, the node has a local default namespace that matches
  // ns. Append to qualified name the value of node's localName, let the value
  // of inherited ns be ns, and append the value of qualified name to markup.
  DCHECK(EqualIgnoringNullity(local_default_namespace, ns));
  namespace_context.SetContextNamespace(ns);
  formatter_.AppendStartTagOpen(markup_, element);
  return data;
}

void MarkupAccumulator::AppendStartTagClose(const Element& element) {
  formatter_.AppendStartTagClose(markup_, element);
}

void MarkupAccumulator::AppendAttribute(const Element& element,
                                        const Attribute& attribute) {
  String value = formatter_.ResolveURLIfNeeded(element, attribute);
  if (SerializeAsHTML()) {
    MarkupFormatter::AppendAttributeAsHTML(markup_, attribute, value);
  } else {
    AppendAttributeAsXMLWithNamespace(element, attribute, value);
  }
}

void MarkupAccumulator::AppendAttributeAsXMLWithNamespace(
    const Element& element,
    const Attribute& attribute,
    const String& value) {
  // https://w3c.github.io/DOM-Parsing/#serializing-an-element-s-attributes

  // 3.3. Let attribute namespace be the value of attr's namespaceURI value.
  const AtomicString& attribute_namespace = attribute.NamespaceURI();

  // 3.4. Let candidate prefix be null.
  AtomicString candidate_prefix;

  if (attribute_namespace.IsNull()) {
    MarkupFormatter::AppendAttribute(markup_, candidate_prefix,
                                     attribute.LocalName(), value, false);
    return;
  }
  // 3.5. If attribute namespace is not null, then run these sub-steps:

  // 3.5.1. Let candidate prefix be the result of retrieving a preferred
  // prefix string from map given namespace attribute namespace with preferred
  // prefix being attr's prefix value.
  candidate_prefix =
      RetrievePreferredPrefixString(attribute_namespace, attribute.Prefix());

  // 3.5.2. If the value of attribute namespace is the XMLNS namespace, then
  // run these steps:
  if (attribute_namespace == xmlns_names::kNamespaceURI) {
    if (!attribute.Prefix() && attribute.LocalName() != g_xmlns_atom)
      candidate_prefix = g_xmlns_atom;
  } else {
    // 3.5.3. Otherwise, the attribute namespace in not the XMLNS namespace.
    // Run these steps:
    if (ShouldAddNamespaceAttribute(attribute, candidate_prefix)) {
      if (!candidate_prefix || LookupNamespaceURI(candidate_prefix)) {
        // 3.5.3.1. Let candidate prefix be the result of generating a prefix
        // providing map, attribute namespace, and prefix index as input.
        candidate_prefix = GeneratePrefix(attribute_namespace);
        // 3.5.3.2. Append the following to result, in the order listed:
        MarkupFormatter::AppendAttribute(markup_, g_xmlns_atom,
                                         candidate_prefix, attribute_namespace,
                                         false);
      } else {
        DCHECK(candidate_prefix);
        AppendNamespace(candidate_prefix, attribute_namespace);
      }
    }
  }
  MarkupFormatter::AppendAttribute(markup_, candidate_prefix,
                                   attribute.LocalName(), value, false);
}

bool MarkupAccumulator::ShouldAddNamespaceAttribute(
    const Attribute& attribute,
    const AtomicString& candidate_prefix) {
  // xmlns and xmlns:prefix attributes should be handled by another branch in
  // AppendAttributeAsXMLWithNamespace().
  DCHECK_NE(attribute.NamespaceURI(), xmlns_names::kNamespaceURI);
  // Null namespace is checked earlier in AppendAttributeAsXMLWithNamespace().
  DCHECK(attribute.NamespaceURI());

  // Attributes without a prefix will need one generated for them, and an xmlns
  // attribute for that prefix.
  if (!candidate_prefix)
    return true;

  return !EqualIgnoringNullity(LookupNamespaceURI(candidate_prefix),
                               attribute.NamespaceURI());
}

void MarkupAccumulator::AppendNamespace(const AtomicString& prefix,
                                        const AtomicString& namespace_uri) {
  AtomicString found_uri = LookupNamespaceURI(prefix);
  if (!EqualIgnoringNullity(found_uri, namespace_uri)) {
    AddPrefix(prefix, namespace_uri);
    if (prefix.IsEmpty()) {
      MarkupFormatter::AppendAttribute(markup_, g_null_atom, g_xmlns_atom,
                                       namespace_uri, false);
    } else {
      MarkupFormatter::AppendAttribute(markup_, g_xmlns_atom, prefix,
                                       namespace_uri, false);
    }
  }
}

EntityMask MarkupAccumulator::EntityMaskForText(const Text& text) const {
  return formatter_.EntityMaskForText(text);
}

void MarkupAccumulator::PushNamespaces(const Element& element) {
  if (SerializeAsHTML())
    return;
  DCHECK_GT(namespace_stack_.size(), 0u);
  // TODO(tkent): Avoid to copy the whole map.
  // We can't do |namespace_stack_.emplace_back(namespace_stack_.back())|
  // because back() returns a reference in the vector backing, and
  // emplace_back() can reallocate it.
  namespace_stack_.push_back(NamespaceContext(namespace_stack_.back()));
}

void MarkupAccumulator::PopNamespaces(const Element& element) {
  if (SerializeAsHTML())
    return;
  namespace_stack_.pop_back();
}

// https://w3c.github.io/DOM-Parsing/#dfn-retrieving-a-preferred-prefix-string
AtomicString MarkupAccumulator::RetrievePreferredPrefixString(
    const AtomicString& ns,
    const AtomicString& preferred_prefix) {
  DCHECK(!ns.IsEmpty()) << ns;
  AtomicString ns_for_preferred = LookupNamespaceURI(preferred_prefix);
  // Preserve the prefix if the prefix is used in the scope and the namespace
  // for it is matches to the node's one.
  // This is equivalent to the following step in the specification:
  // 2.1. If prefix matches preferred prefix, then stop running these steps and
  // return prefix.
  if (!preferred_prefix.IsEmpty() && !ns_for_preferred.IsNull() &&
      EqualIgnoringNullity(ns_for_preferred, ns))
    return preferred_prefix;

  const Vector<AtomicString>& candidate_list =
      namespace_stack_.back().PrefixList(ns);
  // Get the last effective prefix.
  //
  // <el1 xmlns:p="U1" xmlns:q="U1">
  //   <el2 xmlns:q="U2">
  //    el2.setAttributeNS(U1, 'n', 'v');
  // We should get 'p'.
  //
  // <el1 xmlns="U1">
  //  el1.setAttributeNS(U1, 'n', 'v');
  // We should not get '' for attributes.
  for (auto it = candidate_list.rbegin(); it != candidate_list.rend(); ++it) {
    AtomicString candidate_prefix = *it;
    DCHECK(!candidate_prefix.IsEmpty());
    AtomicString ns_for_candaite = LookupNamespaceURI(candidate_prefix);
    if (EqualIgnoringNullity(ns_for_candaite, ns))
      return candidate_prefix;
  }

  // No prefixes for |ns|.
  // Preserve the prefix if the prefix is not used in the current scope.
  if (!preferred_prefix.IsEmpty() && ns_for_preferred.IsNull())
    return preferred_prefix;
  // If a prefix is not specified, or the prefix is mapped to a
  // different namespace, we should generate new prefix.
  return g_null_atom;
}

void MarkupAccumulator::AddPrefix(const AtomicString& prefix,
                                  const AtomicString& namespace_uri) {
  namespace_stack_.back().Add(prefix, namespace_uri);
}

AtomicString MarkupAccumulator::LookupNamespaceURI(const AtomicString& prefix) {
  return namespace_stack_.back().LookupNamespaceURI(prefix);
}

// https://w3c.github.io/DOM-Parsing/#dfn-generating-a-prefix
AtomicString MarkupAccumulator::GeneratePrefix(
    const AtomicString& new_namespace) {
  AtomicString generated_prefix;
  do {
    // 1. Let generated prefix be the concatenation of the string "ns" and the
    // current numerical value of prefix index.
    generated_prefix = "ns" + String::Number(prefix_index_);
    // 2. Let the value of prefix index be incremented by one.
    ++prefix_index_;
  } while (LookupNamespaceURI(generated_prefix));
  // 3. Add to map the generated prefix given the new namespace namespace.
  AddPrefix(generated_prefix, new_namespace);
  // 4. Return the value of generated prefix.
  return generated_prefix;
}

bool MarkupAccumulator::SerializeAsHTML() const {
  return formatter_.SerializeAsHTML();
}

std::pair<Node*, Element*> MarkupAccumulator::GetAuxiliaryDOMTree(
    const Element& element) const {
  return std::pair<Node*, Element*>();
}

template <typename Strategy>
void MarkupAccumulator::SerializeNodesWithNamespaces(
    const Node& target_node,
    ChildrenOnly children_only) {
  if (!target_node.IsElementNode()) {
    if (!children_only)
      AppendStartMarkup(target_node);
    for (const Node& child : Strategy::ChildrenOf(target_node))
      SerializeNodesWithNamespaces<Strategy>(child, kIncludeNode);
    return;
  }

  const auto& target_element = To<Element>(target_node);
  if (ShouldIgnoreElement(target_element))
    return;

  PushNamespaces(target_element);

  AtomicString prefix_override;
  if (!children_only)
    prefix_override = AppendElement(target_element);

  bool has_end_tag =
      !(SerializeAsHTML() && ElementCannotHaveEndTag(target_element));
  if (has_end_tag) {
    const Node* parent = &target_element;
    if (auto* template_element = DynamicTo<HTMLTemplateElement>(target_element))
      parent = template_element->content();
    for (const Node& child : Strategy::ChildrenOf(*parent))
      SerializeNodesWithNamespaces<Strategy>(child, kIncludeNode);

    // Traverses other DOM tree, i.e., shadow tree.
    std::pair<Node*, Element*> auxiliary_pair =
        GetAuxiliaryDOMTree(target_element);
    if (Node* auxiliary_tree = auxiliary_pair.first) {
      Element* enclosing_element = auxiliary_pair.second;
      AtomicString enclosing_element_prefix;
      if (enclosing_element)
        enclosing_element_prefix = AppendElement(*enclosing_element);
      for (const Node& child : Strategy::ChildrenOf(*auxiliary_tree))
        SerializeNodesWithNamespaces<Strategy>(child, kIncludeNode);
      if (enclosing_element)
        AppendEndTag(*enclosing_element, enclosing_element_prefix);
    }

    if (!children_only)
      AppendEndTag(target_element, prefix_override);
  }

  PopNamespaces(target_element);
}

template <typename Strategy>
String MarkupAccumulator::SerializeNodes(const Node& target_node,
                                         ChildrenOnly children_only) {
  if (!SerializeAsHTML()) {
    // https://w3c.github.io/DOM-Parsing/#dfn-xml-serialization
    DCHECK_EQ(namespace_stack_.size(), 0u);
    // 2. Let prefix map be a new namespace prefix map.
    namespace_stack_.emplace_back();
    // 3. Add the XML namespace with prefix value "xml" to prefix map.
    AddPrefix(g_xml_atom, xml_names::kNamespaceURI);
    // 4. Let prefix index be a generated namespace prefix index with value 1.
    prefix_index_ = 1;
  }

  SerializeNodesWithNamespaces<Strategy>(target_node, children_only);
  return ToString();
}

template String MarkupAccumulator::SerializeNodes<EditingStrategy>(
    const Node&,
    ChildrenOnly);

}  // namespace blink
