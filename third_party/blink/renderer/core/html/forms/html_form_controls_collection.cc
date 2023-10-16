/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/html/forms/html_form_controls_collection.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_radionodelist.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// Since the collections are to be "live", we have to do the
// calculation every time if anything has changed.

HTMLFormControlsCollection::HTMLFormControlsCollection(
    ContainerNode& owner_node)
    : HTMLCollection(owner_node, kFormControls, kOverridesItemAfter),
      cached_element_(nullptr),
      cached_element_offset_in_array_(0) {
  DCHECK(IsA<HTMLFormElement>(owner_node));
}

HTMLFormControlsCollection::HTMLFormControlsCollection(
    ContainerNode& owner_node,
    CollectionType type)
    : HTMLFormControlsCollection(owner_node) {
  DCHECK_EQ(type, kFormControls);
}

HTMLFormControlsCollection::~HTMLFormControlsCollection() = default;

const ListedElement::List& HTMLFormControlsCollection::ListedElements() const {
  return To<HTMLFormElement>(ownerNode()).ListedElements();
}

const HeapVector<Member<HTMLImageElement>>&
HTMLFormControlsCollection::FormImageElements() const {
  return To<HTMLFormElement>(ownerNode()).ImageElements();
}

static unsigned FindListedElement(const ListedElement::List& listed_elements,
                                  Element* element) {
  unsigned i = 0;
  for (; i < listed_elements.size(); ++i) {
    ListedElement* listed_element = listed_elements[i];
    if (listed_element->IsEnumeratable() &&
        &listed_element->ToHTMLElement() == element)
      break;
  }
  return i;
}

HTMLElement* HTMLFormControlsCollection::VirtualItemAfter(
    Element* previous) const {
  const ListedElement::List& listed_elements = ListedElements();
  unsigned offset;
  if (!previous)
    offset = 0;
  else if (cached_element_ == previous)
    offset = cached_element_offset_in_array_ + 1;
  else
    offset = FindListedElement(listed_elements, previous) + 1;

  for (unsigned i = offset; i < listed_elements.size(); ++i) {
    ListedElement* listed_element = listed_elements[i];
    if (listed_element->IsEnumeratable()) {
      cached_element_ = listed_element->ToHTMLElement();
      cached_element_offset_in_array_ = i;
      return cached_element_.Get();
    }
  }
  return nullptr;
}

void HTMLFormControlsCollection::InvalidateCache(Document* old_document) const {
  HTMLCollection::InvalidateCache(old_document);
  cached_element_ = nullptr;
  cached_element_offset_in_array_ = 0;
}

static HTMLElement* FirstNamedItem(const ListedElement::List& elements_array,
                                   const QualifiedName& attr_name,
                                   const String& name) {
  DCHECK(attr_name == html_names::kIdAttr ||
         attr_name == html_names::kNameAttr);

  for (const auto& listed_element : elements_array) {
    HTMLElement& element = listed_element->ToHTMLElement();
    if (listed_element->IsEnumeratable() &&
        element.FastGetAttribute(attr_name) == name)
      return &element;
  }
  return nullptr;
}

HTMLElement* HTMLFormControlsCollection::namedItem(
    const AtomicString& name) const {
  // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
  // This method first searches for an object with a matching id
  // attribute. If a match is not found, the method then searches for an
  // object with a matching name attribute, but only on those elements
  // that are allowed a name attribute.
  if (HTMLElement* item =
          FirstNamedItem(ListedElements(), html_names::kIdAttr, name))
    return item;
  return FirstNamedItem(ListedElements(), html_names::kNameAttr, name);
}

void HTMLFormControlsCollection::UpdateIdNameCache() const {
  if (HasValidIdNameCache())
    return;

  auto* cache = MakeGarbageCollected<NamedItemCache>();
  HashSet<StringImpl*> found_input_elements;

  for (const auto& listed_element : ListedElements()) {
    if (listed_element->IsEnumeratable()) {
      HTMLElement& element = listed_element->ToHTMLElement();
      const AtomicString& id_attr_val = element.GetIdAttribute();
      const AtomicString& name_attr_val = element.GetNameAttribute();
      if (!id_attr_val.empty()) {
        cache->AddElementWithId(id_attr_val, &element);
        found_input_elements.insert(id_attr_val.Impl());
      }
      if (!name_attr_val.empty() && id_attr_val != name_attr_val) {
        cache->AddElementWithName(name_attr_val, &element);
        found_input_elements.insert(name_attr_val.Impl());
      }
    }
  }

  // HTMLFormControlsCollection doesn't support named getter for IMG
  // elements. However we still need to handle IMG elements here because
  // HTMLFormElement named getter relies on this.
  for (const auto& element : FormImageElements()) {
    const AtomicString& id_attr_val = element->GetIdAttribute();
    const AtomicString& name_attr_val = element->GetNameAttribute();
    if (!id_attr_val.empty() &&
        !found_input_elements.Contains(id_attr_val.Impl()))
      cache->AddElementWithId(id_attr_val, element);
    if (!name_attr_val.empty() && id_attr_val != name_attr_val &&
        !found_input_elements.Contains(name_attr_val.Impl()))
      cache->AddElementWithName(name_attr_val, element);
  }

  // Set the named item cache last as traversing the tree may cause cache
  // invalidation.
  SetNamedItemCache(cache);
}

V8UnionElementOrRadioNodeList* HTMLFormControlsCollection::namedGetter(
    const AtomicString& name) {
  HeapVector<Member<Element>> named_items;
  NamedItems(name, named_items);

  if (named_items.empty())
    return nullptr;

  if (named_items.size() == 1) {
    if (!IsA<HTMLImageElement>(*named_items[0])) {
      return MakeGarbageCollected<V8UnionElementOrRadioNodeList>(
          named_items[0]);
    }
    return nullptr;
  }

  // This path never returns a RadioNodeList for <img> because
  // onlyMatchingImgElements flag is false by default.
  return MakeGarbageCollected<V8UnionElementOrRadioNodeList>(
      ownerNode().GetRadioNodeList(name));
}

void HTMLFormControlsCollection::SupportedPropertyNames(Vector<String>& names) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmlformcontrolscollection-0:
  // The supported property names consist of the non-empty values of all the id
  // and name attributes of all the elements represented by the collection, in
  // tree order, ignoring later duplicates, with the id of an element preceding
  // its name if it contributes both, they differ from each other, and neither
  // is the duplicate of an earlier entry.
  HashSet<AtomicString> existing_names;
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    HTMLElement* element = item(i);
    DCHECK(element);
    const AtomicString& id_attribute = element->GetIdAttribute();
    if (!id_attribute.empty()) {
      HashSet<AtomicString>::AddResult add_result =
          existing_names.insert(id_attribute);
      if (add_result.is_new_entry)
        names.push_back(id_attribute);
    }
    const AtomicString& name_attribute = element->GetNameAttribute();
    if (!name_attribute.empty()) {
      HashSet<AtomicString>::AddResult add_result =
          existing_names.insert(name_attribute);
      if (add_result.is_new_entry)
        names.push_back(name_attribute);
    }
  }
}

void HTMLFormControlsCollection::Trace(Visitor* visitor) const {
  visitor->Trace(cached_element_);
  HTMLCollection::Trace(visitor);
}

}  // namespace blink
