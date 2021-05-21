// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/element-inl.h"
#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

// This file contains functions that, on ARM32, cause a significant
// performance degradation of blink_perf benchmarks when compiled with -Oz
// instead of -O2.

namespace blink {

WTF::AtomicStringTable::WeakResult Element::WeakLowercaseIfNecessary(
    const StringView& name) const {
  if (LIKELY(IsHTMLElement() && IsA<HTMLDocument>(GetDocument()))) {
    StringImpl* impl = name.SharedImpl();
    if (impl && impl->IsAtomic() && impl->IsLowerASCII())
      return WTF::AtomicStringTable::WeakResult(impl);
    return WTF::AtomicStringTable::Instance().WeakFindLowercased(name);
  }

  return WTF::AtomicStringTable::Instance().WeakFind(name);
}

// Note, SynchronizeAttributeHinted is safe to call between a WeakFind() and
// a check on the AttributeCollection for the element even though it may
// modify the AttributeCollection to insert a "style" attribute. The reason
// is because html_names::kStyleAttr.LocalName() is an AtomicString
// representing "style". This means the AtomicStringTable will always have
// an entry for "style" and a `hint` that corresponds to
// html_names::kStyleAttr.LocalName() will never refer to a deleted object
// thus it is safe to insert html_names::kStyleAttr.LocalName() into the
// AttributeCollection collection after the WeakFind() when `hint` is
// referring to "style". A subsequent lookup will match itself correctly
// without worry for UaF or false positives.
void Element::SynchronizeAttributeHinted(
    const AtomicString& local_name,
    WTF::AtomicStringTable::WeakResult hint) const {
  // This version of SynchronizeAttribute() is streamlined for the case where
  // you don't have a full QualifiedName, e.g when called from DOM API.
  if (!GetElementData())
    return;
  // TODO(ajwong): Does this unnecessarily synchronize style attributes on
  // SVGElements?
  if (GetElementData()->style_attribute_is_dirty() &&
      hint == html_names::kStyleAttr.LocalName()) {
    DCHECK(IsStyledElement());
    SynchronizeStyleAttributeInternal();
    return;
  }
  if (GetElementData()->svg_attributes_are_dirty()) {
    // We're passing a null namespace argument. svg_names::k*Attr are defined in
    // the null namespace, but for attributes that are not (like 'href' in the
    // XLink NS), this will not do the right thing.

    // TODO(fs): svg_attributes_are_dirty_ stays dirty unless
    // SynchronizeSVGAttribute is called with AnyQName(). This means that even
    // if Element::SynchronizeAttribute() is called on all attributes,
    // svg_attributes_are_dirty_ remains true. This information is available in
    // the attribute->property map in SVGElement.
    To<SVGElement>(this)->SynchronizeSVGAttribute(
        QualifiedName(g_null_atom, local_name, g_null_atom));
  }
}

const AtomicString& Element::GetAttributeHinted(
    const AtomicString& name,
    WTF::AtomicStringTable::WeakResult hint) const {
  if (!GetElementData())
    return g_null_atom;
  SynchronizeAttributeHinted(name, hint);
  if (const Attribute* attribute =
          GetElementData()->Attributes().FindHinted(name, hint))
    return attribute->Value();
  return g_null_atom;
}

std::pair<wtf_size_t, const QualifiedName> Element::LookupAttributeQNameHinted(
    const AtomicString& name,
    WTF::AtomicStringTable::WeakResult hint) const {
  if (!GetElementData()) {
    return std::make_pair(
        kNotFound,
        QualifiedName(g_null_atom, LowercaseIfNecessary(name), g_null_atom));
  }

  AttributeCollection attributes = GetElementData()->Attributes();
  wtf_size_t index = attributes.FindIndexHinted(name, hint);
  return std::make_pair(
      index, index != kNotFound
                 ? attributes[index].GetName()
                 : QualifiedName(g_null_atom, LowercaseIfNecessary(name),
                                 g_null_atom));
}

void Element::setAttribute(const QualifiedName& name,
                           const AtomicString& value) {
  SynchronizeAttribute(name);
  wtf_size_t index = GetElementData()
                         ? GetElementData()->Attributes().FindIndex(name)
                         : kNotFound;
  SetAttributeInternal(index, name, value,
                       kNotInSynchronizationOfLazyAttribute);
}

void Element::setAttribute(const QualifiedName& name,
                           const AtomicString& value,
                           ExceptionState& exception_state) {
  SynchronizeAttribute(name);
  wtf_size_t index = GetElementData()
                         ? GetElementData()->Attributes().FindIndex(name)
                         : kNotFound;

  AtomicString trusted_value(
      TrustedTypesCheckFor(ExpectedTrustedTypeForAttribute(name), value,
                           GetExecutionContext(), exception_state));
  if (exception_state.HadException())
    return;

  SetAttributeInternal(index, name, trusted_value,
                       kNotInSynchronizationOfLazyAttribute);
}

void Element::SetSynchronizedLazyAttribute(const QualifiedName& name,
                                           const AtomicString& value) {
  wtf_size_t index = GetElementData()
                         ? GetElementData()->Attributes().FindIndex(name)
                         : kNotFound;
  SetAttributeInternal(index, name, value, kInSynchronizationOfLazyAttribute);
}

void Element::SetAttributeHinted(const AtomicString& local_name,
                                 WTF::AtomicStringTable::WeakResult hint,
                                 const AtomicString& value,
                                 ExceptionState& exception_state) {
  if (!Document::IsValidName(local_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "'" + local_name + "' is not a valid attribute name.");
    return;
  }

  SynchronizeAttributeHinted(local_name, hint);
  wtf_size_t index;
  QualifiedName q_name = QualifiedName::Null();
  std::tie(index, q_name) = LookupAttributeQNameHinted(local_name, hint);

  AtomicString trusted_value(TrustedTypesCheckFor(
      ExpectedTrustedTypeForAttribute(q_name), std::move(value),
      GetExecutionContext(), exception_state));
  if (exception_state.HadException())
    return;

  SetAttributeInternal(index, q_name, trusted_value,
                       kNotInSynchronizationOfLazyAttribute);
}

void Element::SetAttributeHinted(const AtomicString& local_name,
                                 WTF::AtomicStringTable::WeakResult hint,
                                 const V8TrustedString* trusted_string,
                                 ExceptionState& exception_state) {
  if (!Document::IsValidName(local_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "'" + local_name + "' is not a valid attribute name.");
    return;
  }

  SynchronizeAttributeHinted(local_name, hint);
  wtf_size_t index;
  QualifiedName q_name = QualifiedName::Null();
  std::tie(index, q_name) = LookupAttributeQNameHinted(local_name, hint);
  AtomicString value(TrustedTypesCheckFor(
      ExpectedTrustedTypeForAttribute(q_name), trusted_string,
      GetExecutionContext(), exception_state));
  if (exception_state.HadException())
    return;
  SetAttributeInternal(index, q_name, value,
                       kNotInSynchronizationOfLazyAttribute);
}

ALWAYS_INLINE void Element::SetAttributeInternal(
    wtf_size_t index,
    const QualifiedName& name,
    const AtomicString& new_value,
    SynchronizationOfLazyAttribute in_synchronization_of_lazy_attribute) {
  if (new_value.IsNull()) {
    if (index != kNotFound)
      RemoveAttributeInternal(index, in_synchronization_of_lazy_attribute);
    return;
  }

  if (index == kNotFound) {
    AppendAttributeInternal(name, new_value,
                            in_synchronization_of_lazy_attribute);
    return;
  }

  const Attribute& existing_attribute =
      GetElementData()->Attributes().at(index);
  AtomicString existing_attribute_value = existing_attribute.Value();
  QualifiedName existing_attribute_name = existing_attribute.GetName();

  if (!in_synchronization_of_lazy_attribute) {
    WillModifyAttribute(existing_attribute_name, existing_attribute_value,
                        new_value);
  }
  if (new_value != existing_attribute_value)
    EnsureUniqueElementData().Attributes().at(index).SetValue(new_value);
  if (!in_synchronization_of_lazy_attribute) {
    DidModifyAttribute(existing_attribute_name, existing_attribute_value,
                       new_value);
  }
}

Attr* Element::setAttributeNode(Attr* attr_node,
                                ExceptionState& exception_state) {
  Attr* old_attr_node = AttrIfExists(attr_node->GetQualifiedName());
  if (old_attr_node == attr_node)
    return attr_node;  // This Attr is already attached to the element.

  // InUseAttributeError: Raised if node is an Attr that is already an attribute
  // of another Element object.  The DOM user must explicitly clone Attr nodes
  // to re-use them in other elements.
  if (attr_node->ownerElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInUseAttributeError,
        "The node provided is an attribute node that is already an attribute "
        "of another Element; attribute nodes must be explicitly cloned.");
    return nullptr;
  }

  if (!IsHTMLElement() && IsA<HTMLDocument>(attr_node->GetDocument()) &&
      attr_node->name() != attr_node->name().LowerASCII()) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::
            kNonHTMLElementSetAttributeNodeFromHTMLDocumentNameNotLowercase);
  }

  SynchronizeAllAttributes();
  const UniqueElementData& element_data = EnsureUniqueElementData();

  AtomicString value(TrustedTypesCheckFor(
      ExpectedTrustedTypeForAttribute(attr_node->GetQualifiedName()),
      attr_node->value(), GetExecutionContext(), exception_state));
  if (exception_state.HadException())
    return nullptr;

  AttributeCollection attributes = element_data.Attributes();
  wtf_size_t index = attributes.FindIndex(attr_node->GetQualifiedName());
  AtomicString local_name;
  if (index != kNotFound) {
    const Attribute& attr = attributes[index];

    // If the name of the ElementData attribute doesn't
    // (case-sensitively) match that of the Attr node, record it
    // on the Attr so that it can correctly resolve the value on
    // the Element.
    if (!attr.GetName().Matches(attr_node->GetQualifiedName()))
      local_name = attr.LocalName();

    if (old_attr_node) {
      DetachAttrNodeFromElementWithValue(old_attr_node, attr.Value());
    } else {
      // FIXME: using attrNode's name rather than the
      // Attribute's for the replaced Attr is compatible with
      // all but Gecko (and, arguably, the DOM Level1 spec text.)
      // Consider switching.
      old_attr_node = MakeGarbageCollected<Attr>(
          GetDocument(), attr_node->GetQualifiedName(), attr.Value());
    }
  }

  SetAttributeInternal(index, attr_node->GetQualifiedName(), value,
                       kNotInSynchronizationOfLazyAttribute);

  attr_node->AttachToElement(this, local_name);
  GetTreeScope().AdoptIfNeeded(*attr_node);
  EnsureElementRareData().AddAttr(attr_node);

  return old_attr_node;
}

void Element::RemoveAttributeHinted(const AtomicString& name,
                                    WTF::AtomicStringTable::WeakResult hint) {
  if (!GetElementData())
    return;

  wtf_size_t index = GetElementData()->Attributes().FindIndexHinted(name, hint);
  if (index == kNotFound) {
    if (UNLIKELY(hint == html_names::kStyleAttr.LocalName()) &&
        GetElementData()->style_attribute_is_dirty() && IsStyledElement())
      RemoveAllInlineStyleProperties();
    return;
  }

  RemoveAttributeInternal(index, kNotInSynchronizationOfLazyAttribute);
}

}  // namespace blink
