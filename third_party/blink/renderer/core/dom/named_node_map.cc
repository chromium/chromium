/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights
 * reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/named_node_map.h"

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Attr* NamedNodeMap::getNamedItem(const AtomicString& name) const {
  return element_->getAttributeNode(name);
}

Attr* NamedNodeMap::getNamedItemNS(const AtomicString& namespace_uri,
                                   const AtomicString& local_name) const {
  return element_->getAttributeNodeNS(namespace_uri, local_name);
}

Attr* NamedNodeMap::removeNamedItem(const AtomicString& name,
                                    ExceptionState& exception_state) {
  WTF::AtomicStringTable::WeakResult hint =
      element_->WeakLowercaseIfNecessary(name);
  wtf_size_t index = element_->Attributes().FindIndexHinted(name, hint);
  if (index == kNotFound) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "No item with name '" + name + "' was found.");
    return nullptr;
  }
  return element_->DetachAttribute(index);
}

Attr* NamedNodeMap::removeNamedItemNS(const AtomicString& namespace_uri,
                                      const AtomicString& local_name,
                                      ExceptionState& exception_state) {
  wtf_size_t index = element_->Attributes().FindIndex(
      QualifiedName(g_null_atom, local_name, namespace_uri));
  if (index == kNotFound) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "No item with name '" + namespace_uri +
                                          "::" + local_name + "' was found.");
    return nullptr;
  }
  return element_->DetachAttribute(index);
}

Attr* NamedNodeMap::setNamedItem(Attr* attr, ExceptionState& exception_state) {
  DCHECK(attr);
  return element_->setAttributeNode(attr, exception_state);
}

Attr* NamedNodeMap::setNamedItemNS(Attr* attr,
                                   ExceptionState& exception_state) {
  DCHECK(attr);
  return element_->setAttributeNodeNS(attr, exception_state);
}

Attr* NamedNodeMap::item(uint32_t index) const {
  AttributeCollection attributes = element_->Attributes();
  if (index >= attributes.size())
    return nullptr;
  return element_->EnsureAttr(attributes[index].GetName());
}

uint32_t NamedNodeMap::length() const {
  return element_->Attributes().size();
}

void NamedNodeMap::NamedPropertyEnumerator(Vector<String>& names,
                                           ExceptionState&) const {
  // https://dom.spec.whatwg.org/#interface-namednodemap
  // A NamedNodeMap object’s supported property names are the return value of
  // running these steps:
  // 1. Let names be the qualified names of the attributes in this NamedNodeMap
  //    object’s attribute list, with duplicates omitted, in order.
  // 2. If this NamedNodeMap object’s element is in the HTML namespace and its
  //    node document is an HTML document, then for each name in names:
  //    2.1. Let lowercaseName be name, in ASCII lowercase.
  //    2.2. If lowercaseName is not equal to name, remove name from names.
  // 3. Return names.
  const AttributeCollection attributes = element_->Attributes();
  names.ReserveInitialCapacity(attributes.size());
  if (element_->IsHTMLElement() && IsA<HTMLDocument>(element_->GetDocument())) {
    for (const Attribute& attribute : attributes) {
      if ((attribute.Prefix() == attribute.Prefix().LowerASCII()) &&
          (attribute.LocalName() == attribute.LocalName().LowerASCII())) {
        names.UncheckedAppend(attribute.GetName().ToString());
      }
    }
  } else {
    for (const Attribute& attribute : attributes) {
      names.UncheckedAppend(attribute.GetName().ToString());
    }
  }
}

bool NamedNodeMap::NamedPropertyQuery(const AtomicString& name,
                                      ExceptionState& exception_state) const {
  Vector<String> properties;
  NamedPropertyEnumerator(properties, exception_state);
  return properties.Contains(name);
}

void NamedNodeMap::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  ScriptWrappable::Trace(visitor);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
