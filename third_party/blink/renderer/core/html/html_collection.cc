/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2008, 2011, 2012, 2014 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_collection.h"

#include "third_party/blink/renderer/core/dom/class_collection.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/html/collection_type.h"
#include "third_party/blink/renderer/core/html/document_all_name_collection.h"
#include "third_party/blink/renderer/core/html/document_name_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_options_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_tag_collection.h"
#include "third_party/blink/renderer/core/html/window_name_collection.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

static bool ShouldTypeOnlyIncludeDirectChildren(CollectionType type) {
  switch (type) {
    case kClassCollectionType:
    case kTagCollectionType:
    case kTagCollectionNSType:
    case kHTMLTagCollectionType:
    case kDocAll:
    case kDocAnchors:
    case kDocApplets:
    case kDocEmbeds:
    case kDocForms:
    case kDocImages:
    case kDocLinks:
    case kDocScripts:
    case kDocumentNamedItems:
    case kDocumentAllNamedItems:
    case kMapAreas:
    case kTableRows:
    case kSelectOptions:
    case kSelectedOptions:
    case kDataListOptions:
    case kWindowNamedItems:
    case kFormControls:
    case kPopoverInvokers:
      return false;
    case kNodeChildren:
    case kTRCells:
    case kTSectionRows:
    case kTableTBodies:
      return true;
    case kNameNodeListType:
    case kRadioNodeListType:
    case kRadioImgNodeListType:
    case kLabelsNodeListType:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

static NodeListSearchRoot SearchRootFromCollectionType(
    const ContainerNode& owner,
    CollectionType type) {
  switch (type) {
    case kDocImages:
    case kDocApplets:
    case kDocEmbeds:
    case kDocForms:
    case kDocLinks:
    case kDocAnchors:
    case kDocScripts:
    case kDocAll:
    case kWindowNamedItems:
    case kDocumentNamedItems:
    case kDocumentAllNamedItems:
    case kClassCollectionType:
    case kTagCollectionType:
    case kTagCollectionNSType:
    case kHTMLTagCollectionType:
    case kNodeChildren:
    case kTableTBodies:
    case kTSectionRows:
    case kTableRows:
    case kTRCells:
    case kSelectOptions:
    case kSelectedOptions:
    case kDataListOptions:
    case kMapAreas:
      return NodeListSearchRoot::kOwnerNode;
    case kFormControls:
      if (IsA<HTMLFieldSetElement>(owner))
        return NodeListSearchRoot::kOwnerNode;
      DCHECK(IsA<HTMLFormElement>(owner));
      return NodeListSearchRoot::kTreeScope;
    case kPopoverInvokers:
      return NodeListSearchRoot::kTreeScope;
    case kNameNodeListType:
    case kRadioNodeListType:
    case kRadioImgNodeListType:
    case kLabelsNodeListType:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return NodeListSearchRoot::kOwnerNode;
}

static NodeListInvalidationType InvalidationTypeExcludingIdAndNameAttributes(
    CollectionType type) {
  switch (type) {
    case kTagCollectionType:
    case kTagCollectionNSType:
    case kHTMLTagCollectionType:
    case kDocImages:
    case kDocEmbeds:
    case kDocForms:
    case kDocScripts:
    case kDocAll:
    case kNodeChildren:
    case kTableTBodies:
    case kTSectionRows:
    case kTableRows:
    case kTRCells:
    case kSelectOptions:
    case kMapAreas:
      return kDoNotInvalidateOnAttributeChanges;
    case kDocApplets:
    case kSelectedOptions:
    case kDataListOptions:
      // FIXME: We can do better some day.
      return kInvalidateOnAnyAttrChange;
    case kDocAnchors:
      return kInvalidateOnNameAttrChange;
    case kDocLinks:
      return kInvalidateOnHRefAttrChange;
    case kWindowNamedItems:
      return kInvalidateOnIdNameAttrChange;
    case kDocumentNamedItems:
      return kInvalidateOnIdNameAttrChange;
    case kDocumentAllNamedItems:
      return kInvalidateOnIdNameAttrChange;
    case kFormControls:
      return kInvalidateForFormControls;
    case kClassCollectionType:
      return kInvalidateOnClassAttrChange;
    case kPopoverInvokers:
      return kInvalidateOnPopoverInvokerAttrChange;
    case kNameNodeListType:
    case kRadioNodeListType:
    case kRadioImgNodeListType:
    case kLabelsNodeListType:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return kDoNotInvalidateOnAttributeChanges;
}

HTMLCollection::HTMLCollection(ContainerNode& owner_node,
                               CollectionType type,
                               ItemAfterOverrideType item_after_override_type)
    : LiveNodeListBase(owner_node,
                       SearchRootFromCollectionType(owner_node, type),
                       InvalidationTypeExcludingIdAndNameAttributes(type),
                       type),
      overrides_item_after_(item_after_override_type == kOverridesItemAfter),
      should_only_include_direct_children_(
          ShouldTypeOnlyIncludeDirectChildren(type)) {
  // Keep this in the child class because |registerNodeList| requires wrapper
  // tracing and potentially calls virtual methods which is not allowed in a
  // base class constructor.
  GetDocument().RegisterNodeList(this);
}

HTMLCollection::~HTMLCollection() = default;

void HTMLCollection::InvalidateCache(Document* old_document) const {
  collection_items_cache_.Invalidate();
  InvalidateIdNameCacheMaps(old_document);
}

unsigned HTMLCollection::length() const {
  return collection_items_cache_.NodeCount(*this);
}

Element* HTMLCollection::item(unsigned offset) const {
  return collection_items_cache_.NodeAt(*this, offset);
}

static inline bool IsMatchingHTMLElement(const HTMLCollection& html_collection,
                                         const HTMLElement& element) {
  switch (html_collection.GetType()) {
    case kDocImages:
      return element.HasTagName(html_names::kImgTag);
    case kDocScripts:
      return element.HasTagName(html_names::kScriptTag);
    case kDocForms:
      return element.HasTagName(html_names::kFormTag);
    case kDocumentNamedItems:
      return To<DocumentNameCollection>(html_collection)
          .ElementMatches(element);
    case kDocumentAllNamedItems:
      return To<DocumentAllNameCollection>(html_collection)
          .ElementMatches(element);
    case kTableTBodies:
      return element.HasTagName(html_names::kTbodyTag);
    case kTRCells:
      return element.HasTagName(html_names::kTdTag) ||
             element.HasTagName(html_names::kThTag);
    case kTSectionRows:
      return element.HasTagName(html_names::kTrTag);
    case kSelectOptions:
      return To<HTMLOptionsCollection>(html_collection).ElementMatches(element);
    case kSelectedOptions: {
      auto* option_element = DynamicTo<HTMLOptionElement>(element);
      return option_element && option_element->Selected();
    }
    case kDataListOptions:
      return To<HTMLDataListOptionsCollection>(html_collection)
          .ElementMatches(element);
    case kMapAreas:
      return element.HasTagName(html_names::kAreaTag);
    case kDocApplets: {
      auto* html_image_element = DynamicTo<HTMLObjectElement>(element);
      return html_image_element && html_image_element->ContainsJavaApplet();
    }
    case kDocEmbeds:
      return element.HasTagName(html_names::kEmbedTag);
    case kDocLinks:
      return (element.HasTagName(html_names::kATag) ||
              element.HasTagName(html_names::kAreaTag)) &&
             element.FastHasAttribute(html_names::kHrefAttr);
    case kDocAnchors:
      return element.HasTagName(html_names::kATag) &&
             element.FastHasAttribute(html_names::kNameAttr);
    case kFormControls:
      DCHECK(IsA<HTMLFieldSetElement>(html_collection.ownerNode()));
      return IsA<HTMLObjectElement>(element) ||
             IsA<HTMLFormControlElement>(element) ||
             element.IsFormAssociatedCustomElement();
    case kPopoverInvokers:
      if (auto* invoker = DynamicTo<HTMLFormControlElement>(
              const_cast<HTMLElement&>(element))) {
        return invoker->popoverTargetElement().popover != nullptr;
      }
      return false;
    case kClassCollectionType:
    case kTagCollectionType:
    case kTagCollectionNSType:
    case kHTMLTagCollectionType:
    case kDocAll:
    case kNodeChildren:
    case kTableRows:
    case kWindowNamedItems:
    case kNameNodeListType:
    case kRadioNodeListType:
    case kRadioImgNodeListType:
    case kLabelsNodeListType:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
}

inline bool HTMLCollection::ElementMatches(const Element& element) const {
  // These collections apply to any kind of Elements, not just HTMLElements.
  switch (GetType()) {
    case kDocAll:
    case kNodeChildren:
      return true;
    case kClassCollectionType:
      return To<ClassCollection>(*this).ElementMatches(element);
    case kTagCollectionType:
      return To<TagCollection>(*this).ElementMatches(element);
    case kHTMLTagCollectionType:
      return To<HTMLTagCollection>(*this).ElementMatches(element);
    case kTagCollectionNSType:
      return To<TagCollectionNS>(*this).ElementMatches(element);
    case kWindowNamedItems:
      return To<WindowNameCollection>(*this).ElementMatches(element);
    case kDocumentAllNamedItems:
      return To<DocumentAllNameCollection>(*this).ElementMatches(element);
    default:
      break;
  }

  // The following only applies to HTMLElements.
  auto* html_element = DynamicTo<HTMLElement>(element);
  return html_element && IsMatchingHTMLElement(*this, *html_element);
}

namespace {

template <class HTMLCollectionType>
class IsMatch {
  STACK_ALLOCATED();

 public:
  IsMatch(const HTMLCollectionType& list) : list_(&list) {}

  bool operator()(const Element& element) const {
    return list_->ElementMatches(element);
  }

 private:
  const HTMLCollectionType* list_;
};

}  // namespace

template <class HTMLCollectionType>
static inline IsMatch<HTMLCollectionType> MakeIsMatch(
    const HTMLCollectionType& list) {
  return IsMatch<HTMLCollectionType>(list);
}

Element* HTMLCollection::VirtualItemAfter(Element*) const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// https://html.spec.whatwg.org/C/#all-named-elements
// The document.all collection returns only certain types of elements by name,
// although it returns any type of element by id.
static inline bool NameShouldBeVisibleInDocumentAll(
    const HTMLElement& element) {
  return element.HasTagName(html_names::kATag) ||
         element.HasTagName(html_names::kButtonTag) ||
         element.HasTagName(html_names::kEmbedTag) ||
         element.HasTagName(html_names::kFormTag) ||
         element.HasTagName(html_names::kFrameTag) ||
         element.HasTagName(html_names::kFramesetTag) ||
         element.HasTagName(html_names::kIFrameTag) ||
         element.HasTagName(html_names::kImgTag) ||
         element.HasTagName(html_names::kInputTag) ||
         element.HasTagName(html_names::kMapTag) ||
         element.HasTagName(html_names::kMetaTag) ||
         element.HasTagName(html_names::kObjectTag) ||
         element.HasTagName(html_names::kSelectTag) ||
         element.HasTagName(html_names::kTextareaTag);
}

Element* HTMLCollection::TraverseToFirst() const {
  switch (GetType()) {
    case kHTMLTagCollectionType:
      return ElementTraversal::FirstWithin(
          RootNode(), MakeIsMatch(To<HTMLTagCollection>(*this)));
    case kClassCollectionType:
      return ElementTraversal::FirstWithin(
          RootNode(), MakeIsMatch(To<ClassCollection>(*this)));
    default:
      if (OverridesItemAfter())
        return VirtualItemAfter(nullptr);
      if (ShouldOnlyIncludeDirectChildren())
        return ElementTraversal::FirstChild(RootNode(), MakeIsMatch(*this));
      return ElementTraversal::FirstWithin(RootNode(), MakeIsMatch(*this));
  }
}

Element* HTMLCollection::TraverseToLast() const {
  DCHECK(CanTraverseBackward());
  if (ShouldOnlyIncludeDirectChildren())
    return ElementTraversal::LastChild(RootNode(), MakeIsMatch(*this));
  return ElementTraversal::LastWithin(RootNode(), MakeIsMatch(*this));
}

Element* HTMLCollection::TraverseForwardToOffset(
    unsigned offset,
    Element& current_element,
    unsigned& current_offset) const {
  DCHECK_LT(current_offset, offset);
  switch (GetType()) {
    case kHTMLTagCollectionType:
      return TraverseMatchingElementsForwardToOffset(
          current_element, &RootNode(), offset, current_offset,
          MakeIsMatch(To<HTMLTagCollection>(*this)));
    case kClassCollectionType:
      return TraverseMatchingElementsForwardToOffset(
          current_element, &RootNode(), offset, current_offset,
          MakeIsMatch(To<ClassCollection>(*this)));
    default:
      if (OverridesItemAfter()) {
        for (Element* next = VirtualItemAfter(&current_element); next;
             next = VirtualItemAfter(next)) {
          if (++current_offset == offset)
            return next;
        }
        return nullptr;
      }
      if (ShouldOnlyIncludeDirectChildren()) {
        IsMatch<HTMLCollection> is_match(*this);
        for (Element* next =
                 ElementTraversal::NextSibling(current_element, is_match);
             next; next = ElementTraversal::NextSibling(*next, is_match)) {
          if (++current_offset == offset)
            return next;
        }
        return nullptr;
      }
      return TraverseMatchingElementsForwardToOffset(
          current_element, &RootNode(), offset, current_offset,
          MakeIsMatch(*this));
  }
}

Element* HTMLCollection::TraverseBackwardToOffset(
    unsigned offset,
    Element& current_element,
    unsigned& current_offset) const {
  DCHECK_GT(current_offset, offset);
  DCHECK(CanTraverseBackward());
  if (ShouldOnlyIncludeDirectChildren()) {
    IsMatch<HTMLCollection> is_match(*this);
    for (Element* previous =
             ElementTraversal::PreviousSibling(current_element, is_match);
         previous;
         previous = ElementTraversal::PreviousSibling(*previous, is_match)) {
      if (--current_offset == offset)
        return previous;
    }
    return nullptr;
  }
  return TraverseMatchingElementsBackwardToOffset(
      current_element, &RootNode(), offset, current_offset, MakeIsMatch(*this));
}

Element* HTMLCollection::namedItem(const AtomicString& name) const {
  // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
  // This method first searches for an object with a matching id
  // attribute. If a match is not found, the method then searches for an
  // object with a matching name attribute, but only on those elements
  // that are allowed a name attribute.
  UpdateIdNameCache();

  const NamedItemCache& cache = GetNamedItemCache();
  const auto* id_results = cache.GetElementsById(name);
  if (id_results && !id_results->empty())
    return id_results->front().Get();

  const auto* name_results = cache.GetElementsByName(name);
  if (name_results && !name_results->empty())
    return name_results->front().Get();

  return nullptr;
}

bool HTMLCollection::NamedPropertyQuery(const AtomicString& name,
                                        ExceptionState&) {
  return namedItem(name);
}

void HTMLCollection::SupportedPropertyNames(Vector<String>& names) {
  // As per the specification (https://dom.spec.whatwg.org/#htmlcollection):
  // The supported property names are the values from the list returned by these
  // steps:
  // 1. Let result be an empty list.
  // 2. For each element represented by the collection, in tree order, run these
  //    substeps:
  //   1. If element has an ID which is neither the empty string nor is in
  //      result, append element's ID to result.
  //   2. If element is in the HTML namespace and has a name attribute whose
  //      value is neither the empty string nor is in result, append element's
  //      name attribute value to result.
  // 3. Return result.
  HashSet<AtomicString> existing_names;
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    Element* element = item(i);
    const AtomicString& id_attribute = element->GetIdAttribute();
    if (!id_attribute.empty()) {
      HashSet<AtomicString>::AddResult add_result =
          existing_names.insert(id_attribute);
      if (add_result.is_new_entry)
        names.push_back(id_attribute);
    }
    auto* html_element = DynamicTo<HTMLElement>(element);
    if (!html_element)
      continue;
    const AtomicString& name_attribute = element->GetNameAttribute();
    if (!name_attribute.empty() &&
        (GetType() != kDocAll ||
         NameShouldBeVisibleInDocumentAll(*html_element))) {
      HashSet<AtomicString>::AddResult add_result =
          existing_names.insert(name_attribute);
      if (add_result.is_new_entry)
        names.push_back(name_attribute);
    }
  }
}

void HTMLCollection::NamedPropertyEnumerator(Vector<String>& names,
                                             ExceptionState&) {
  SupportedPropertyNames(names);
}

void HTMLCollection::UpdateIdNameCache() const {
  if (HasValidIdNameCache())
    return;

  auto* cache = MakeGarbageCollected<NamedItemCache>();
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    Element* element = item(i);
    const AtomicString& id_attr_val = element->GetIdAttribute();
    if (!id_attr_val.empty())
      cache->AddElementWithId(id_attr_val, element);
    auto* html_element = DynamicTo<HTMLElement>(element);
    if (!html_element)
      continue;
    const AtomicString& name_attr_val = element->GetNameAttribute();
    if (!name_attr_val.empty() && id_attr_val != name_attr_val &&
        (GetType() != kDocAll ||
         NameShouldBeVisibleInDocumentAll(*html_element)))
      cache->AddElementWithName(name_attr_val, element);
  }
  // Set the named item cache last as traversing the tree may cause cache
  // invalidation.
  SetNamedItemCache(cache);
}

void HTMLCollection::NamedItems(const AtomicString& name,
                                HeapVector<Member<Element>>& result) const {
  DCHECK(result.empty());
  if (name.empty())
    return;

  UpdateIdNameCache();

  const NamedItemCache& cache = GetNamedItemCache();
  if (const auto* id_results = cache.GetElementsById(name))
    result.AppendVector(*id_results);
  if (const auto* name_results = cache.GetElementsByName(name))
    result.AppendVector(*name_results);
}

bool HTMLCollection::HasNamedItems(const AtomicString& name) const {
  if (name.empty()) {
    return false;
  }

  UpdateIdNameCache();

  const NamedItemCache& cache = GetNamedItemCache();
  return cache.GetElementsById(name) || cache.GetElementsByName(name);
}

HTMLCollection::NamedItemCache::NamedItemCache() = default;

void HTMLCollection::Trace(Visitor* visitor) const {
  visitor->Trace(named_item_cache_);
  visitor->Trace(collection_items_cache_);
  ScriptWrappable::Trace(visitor);
  LiveNodeListBase::Trace(visitor);
}

}  // namespace blink
