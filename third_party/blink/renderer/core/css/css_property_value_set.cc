/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_property_value_set.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_property_serializer.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

static AdditionalBytes
AdditionalBytesForImmutableCSSPropertyValueSetWithPropertyCount(
    unsigned count) {
  return AdditionalBytes(sizeof(Member<CSSValue>) * count +
                         sizeof(CSSPropertyValueMetadata) * count);
}

ImmutableCSSPropertyValueSet* ImmutableCSSPropertyValueSet::Create(
    const CSSPropertyValue* properties,
    unsigned count,
    CSSParserMode css_parser_mode) {
  DCHECK_LE(count, static_cast<unsigned>(kMaxArraySize));
  return MakeGarbageCollected<ImmutableCSSPropertyValueSet>(
      AdditionalBytesForImmutableCSSPropertyValueSetWithPropertyCount(count),
      properties, count, css_parser_mode);
}

CSSPropertyName CSSPropertyValueSet::PropertyReference::Name() const {
  if (Id() != CSSPropertyID::kVariable)
    return CSSPropertyName(Id());
  return CSSPropertyName(To<CSSCustomPropertyDeclaration>(Value()).GetName());
}

ImmutableCSSPropertyValueSet* CSSPropertyValueSet::ImmutableCopyIfNeeded()
    const {
  auto* immutable_property_set = DynamicTo<ImmutableCSSPropertyValueSet>(
      const_cast<CSSPropertyValueSet*>(this));
  if (immutable_property_set)
    return immutable_property_set;

  const auto* mutable_this = To<MutableCSSPropertyValueSet>(this);
  return ImmutableCSSPropertyValueSet::Create(
      mutable_this->property_vector_.data(),
      mutable_this->property_vector_.size(), CssParserMode());
}

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    CSSParserMode css_parser_mode)
    : CSSPropertyValueSet(css_parser_mode) {}

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    const CSSPropertyValue* properties,
    unsigned length)
    : CSSPropertyValueSet(kHTMLStandardMode) {
  property_vector_.ReserveInitialCapacity(length);
  for (unsigned i = 0; i < length; ++i)
    property_vector_.UncheckedAppend(properties[i]);
}

ImmutableCSSPropertyValueSet::ImmutableCSSPropertyValueSet(
    const CSSPropertyValue* properties,
    unsigned length,
    CSSParserMode css_parser_mode)
    : CSSPropertyValueSet(css_parser_mode, length) {
  CSSPropertyValueMetadata* metadata_array =
      const_cast<CSSPropertyValueMetadata*>(this->MetadataArray());
  Member<const CSSValue>* value_array =
      const_cast<Member<const CSSValue>*>(this->ValueArray());
  for (unsigned i = 0; i < array_size_; ++i) {
    metadata_array[i] = properties[i].Metadata();
    value_array[i] = properties[i].Value();
  }
}

ImmutableCSSPropertyValueSet::~ImmutableCSSPropertyValueSet() = default;

// Convert property into an uint16_t for comparison with metadata's property id
// to avoid the compiler converting it to an int multiple times in a loop.
static uint16_t GetConvertedCSSPropertyID(CSSPropertyID property_id) {
  return static_cast<uint16_t>(property_id);
}

static uint16_t GetConvertedCSSPropertyID(const AtomicString&) {
  return static_cast<uint16_t>(CSSPropertyID::kVariable);
}

static uint16_t GetConvertedCSSPropertyID(AtRuleDescriptorID descriptor_id) {
  return static_cast<uint16_t>(
      AtRuleDescriptorIDAsCSSPropertyID(descriptor_id));
}

static bool IsPropertyMatch(const CSSPropertyValueMetadata& metadata,
                            const CSSValue&,
                            uint16_t id,
                            CSSPropertyID property_id) {
  DCHECK_EQ(id, static_cast<uint16_t>(property_id));
  bool result = static_cast<uint16_t>(metadata.Property().PropertyID()) == id;
// Only enabled properties should be part of the style.
#if DCHECK_IS_ON()
  DCHECK(!result ||
         CSSProperty::Get(resolveCSSPropertyID(property_id)).IsWebExposed());
#endif
  return result;
}

static bool IsPropertyMatch(const CSSPropertyValueMetadata& metadata,
                            const CSSValue& value,
                            uint16_t id,
                            const AtomicString& custom_property_name) {
  DCHECK_EQ(id, static_cast<uint16_t>(CSSPropertyID::kVariable));
  return static_cast<uint16_t>(metadata.Property().PropertyID()) == id &&
         To<CSSCustomPropertyDeclaration>(value).GetName() ==
             custom_property_name;
}

static bool IsPropertyMatch(const CSSPropertyValueMetadata& metadata,
                            const CSSValue& css_value,
                            uint16_t id,
                            AtRuleDescriptorID descriptor_id) {
  return IsPropertyMatch(metadata, css_value, id,
                         AtRuleDescriptorIDAsCSSPropertyID(descriptor_id));
}

template <typename T>
int ImmutableCSSPropertyValueSet::FindPropertyIndex(T property) const {
  uint16_t id = GetConvertedCSSPropertyID(property);
  for (int n = array_size_ - 1; n >= 0; --n) {
    if (IsPropertyMatch(MetadataArray()[n], *ValueArray()[n], id, property))
      return n;
  }

  return -1;
}
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    CSSPropertyID) const;
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    AtomicString) const;
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    AtRuleDescriptorID) const;

void ImmutableCSSPropertyValueSet::TraceAfterDispatch(blink::Visitor* visitor) {
  const Member<const CSSValue>* values = ValueArray();
  for (unsigned i = 0; i < array_size_; i++)
    visitor->Trace(values[i]);
  CSSPropertyValueSet::TraceAfterDispatch(visitor);
}

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    const CSSPropertyValueSet& other)
    : CSSPropertyValueSet(other.CssParserMode()) {
  if (auto* other_mutable_property_set =
          DynamicTo<MutableCSSPropertyValueSet>(other)) {
    property_vector_ = other_mutable_property_set->property_vector_;
  } else {
    property_vector_.ReserveInitialCapacity(other.PropertyCount());
    for (unsigned i = 0; i < other.PropertyCount(); ++i) {
      PropertyReference property = other.PropertyAt(i);
      property_vector_.UncheckedAppend(
          CSSPropertyValue(property.PropertyMetadata(), property.Value()));
    }
  }
}

static String SerializeShorthand(const CSSPropertyValueSet& property_set,
                                 CSSPropertyID property_id) {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length())
    return String();

  return StylePropertySerializer(property_set).SerializeShorthand(property_id);
}

static String SerializeShorthand(const CSSPropertyValueSet&,
                                 const AtomicString& custom_property_name) {
  // Custom properties are never shorthands.
  return String();
}

static String SerializeShorthand(const CSSPropertyValueSet& property_set,
                                 AtRuleDescriptorID atrule_id) {
  // Descriptor shorthands aren't handled yet.
  return String();
}

template <typename T>
String CSSPropertyValueSet::GetPropertyValue(T property) const {
  String shorthand_serialization = SerializeShorthand(*this, property);
  if (!shorthand_serialization.IsNull())
    return shorthand_serialization;
  const CSSValue* value = GetPropertyCSSValue(property);
  if (value)
    return value->CssText();
  return g_empty_string;
}
template CORE_EXPORT String
    CSSPropertyValueSet::GetPropertyValue<CSSPropertyID>(CSSPropertyID) const;
template CORE_EXPORT String
    CSSPropertyValueSet::GetPropertyValue<AtRuleDescriptorID>(
        AtRuleDescriptorID) const;
template CORE_EXPORT String
    CSSPropertyValueSet::GetPropertyValue<AtomicString>(AtomicString) const;

template <typename T>
const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue(T property) const {
  int found_property_index = FindPropertyIndex(property);
  if (found_property_index == -1)
    return nullptr;
  return &PropertyAt(found_property_index).Value();
}
template CORE_EXPORT const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue<
    CSSPropertyID>(CSSPropertyID) const;
template CORE_EXPORT const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue<
    AtRuleDescriptorID>(AtRuleDescriptorID) const;
template CORE_EXPORT const CSSValue*
    CSSPropertyValueSet::GetPropertyCSSValue<AtomicString>(AtomicString) const;

void CSSPropertyValueSet::Trace(blink::Visitor* visitor) {
  if (is_mutable_)
    To<MutableCSSPropertyValueSet>(this)->TraceAfterDispatch(visitor);
  else
    To<ImmutableCSSPropertyValueSet>(this)->TraceAfterDispatch(visitor);
}

void CSSPropertyValueSet::FinalizeGarbageCollectedObject() {
  if (is_mutable_)
    To<MutableCSSPropertyValueSet>(this)->~MutableCSSPropertyValueSet();
  else
    To<ImmutableCSSPropertyValueSet>(this)->~ImmutableCSSPropertyValueSet();
}

bool MutableCSSPropertyValueSet::RemoveShorthandProperty(
    CSSPropertyID property_id) {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length())
    return false;

  return RemovePropertiesInSet(shorthand.properties(), shorthand.length());
}

bool MutableCSSPropertyValueSet::RemovePropertyAtIndex(int property_index,
                                                       String* return_text) {
  if (property_index == -1) {
    if (return_text)
      *return_text = "";
    return false;
  }

  if (return_text)
    *return_text = PropertyAt(property_index).Value().CssText();

  // A more efficient removal strategy would involve marking entries as empty
  // and sweeping them when the vector grows too big.
  property_vector_.EraseAt(property_index);

  return true;
}

template <typename T>
bool MutableCSSPropertyValueSet::RemoveProperty(T property,
                                                String* return_text) {
  if (RemoveShorthandProperty(property)) {
    // FIXME: Return an equivalent shorthand when possible.
    if (return_text)
      *return_text = "";
    return true;
  }

  int found_property_index = FindPropertyIndex(property);
  return RemovePropertyAtIndex(found_property_index, return_text);
}
template CORE_EXPORT bool MutableCSSPropertyValueSet::RemoveProperty(
    CSSPropertyID,
    String*);
template CORE_EXPORT bool MutableCSSPropertyValueSet::RemoveProperty(
    AtomicString,
    String*);

template <typename T>
bool CSSPropertyValueSet::PropertyIsImportant(T property) const {
  int found_property_index = FindPropertyIndex(property);
  if (found_property_index != -1)
    return PropertyAt(found_property_index).IsImportant();
  return ShorthandIsImportant(property);
}
template bool CSSPropertyValueSet::PropertyIsImportant<CSSPropertyID>(
    CSSPropertyID) const;
template bool CSSPropertyValueSet::PropertyIsImportant<AtomicString>(
    AtomicString) const;

bool CSSPropertyValueSet::ShorthandIsImportant(
    CSSPropertyID property_id) const {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length())
    return false;

  for (unsigned i = 0; i < shorthand.length(); ++i) {
    if (!PropertyIsImportant(shorthand.properties()[i]->PropertyID()))
      return false;
  }
  return true;
}

bool CSSPropertyValueSet::ShorthandIsImportant(
    AtomicString custom_property_name) const {
  // Custom properties are never shorthands.
  return false;
}

CSSPropertyID CSSPropertyValueSet::GetPropertyShorthand(
    CSSPropertyID property_id) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return CSSPropertyID::kInvalid;
  return PropertyAt(found_property_index).ShorthandID();
}

bool CSSPropertyValueSet::IsPropertyImplicit(CSSPropertyID property_id) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return false;
  return PropertyAt(found_property_index).IsImplicit();
}

MutableCSSPropertyValueSet::SetResult MutableCSSPropertyValueSet::SetProperty(
    CSSPropertyID unresolved_property,
    const String& value,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* context_style_sheet) {
  DCHECK_GE(unresolved_property, firstCSSProperty);

  // Setting the value to an empty string just removes the property in both IE
  // and Gecko. Setting it to null seems to produce less consistent results, but
  // we treat it just the same.
  if (value.IsEmpty()) {
    bool did_parse = true;
    bool did_change = RemoveProperty(resolveCSSPropertyID(unresolved_property));
    return SetResult{did_parse, did_change};
  }

  // When replacing an existing property value, this moves the property to the
  // end of the list. Firefox preserves the position, and MSIE moves the
  // property to the beginning.
  return CSSParser::ParseValue(this, unresolved_property, value, important,
                               secure_context_mode, context_style_sheet);
}

MutableCSSPropertyValueSet::SetResult MutableCSSPropertyValueSet::SetProperty(
    const AtomicString& custom_property_name,
    const String& value,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* context_style_sheet,
    bool is_animation_tainted) {
  if (value.IsEmpty()) {
    bool did_parse = true;
    bool did_change = RemoveProperty(custom_property_name);
    return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
  }
  return CSSParser::ParseValueForCustomProperty(
      this, custom_property_name, value, important, secure_context_mode,
      context_style_sheet, is_animation_tainted);
}

void MutableCSSPropertyValueSet::SetProperty(CSSPropertyID property_id,
                                             const CSSValue& value,
                                             bool important) {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length()) {
    SetProperty(
        CSSPropertyValue(CSSProperty::Get(property_id), value, important));
    return;
  }

  RemovePropertiesInSet(shorthand.properties(), shorthand.length());

  for (unsigned i = 0; i < shorthand.length(); ++i) {
    property_vector_.push_back(
        CSSPropertyValue(*shorthand.properties()[i], value, important));
  }
}

bool MutableCSSPropertyValueSet::SetProperty(const CSSPropertyValue& property,
                                             CSSPropertyValue* slot) {
  CSSPropertyValue* to_replace =
      slot ? slot : FindCSSPropertyWithName(property.Name());
  if (to_replace && *to_replace == property)
    return false;
  if (to_replace) {
    *to_replace = property;
    return true;
  }
  property_vector_.push_back(property);
  return true;
}

bool MutableCSSPropertyValueSet::SetProperty(CSSPropertyID property_id,
                                             CSSValueID identifier,
                                             bool important) {
  SetProperty(CSSPropertyValue(CSSProperty::Get(property_id),
                               *CSSIdentifierValue::Create(identifier),
                               important));
  return true;
}

void MutableCSSPropertyValueSet::ParseDeclarationList(
    const String& style_declaration,
    SecureContextMode secure_context_mode,
    StyleSheetContents* context_style_sheet) {
  property_vector_.clear();

  CSSParserContext* context;
  if (context_style_sheet) {
    context = MakeGarbageCollected<CSSParserContext>(
        context_style_sheet->ParserContext(), context_style_sheet);
    context->SetMode(CssParserMode());
  } else {
    context = MakeGarbageCollected<CSSParserContext>(CssParserMode(),
                                                     secure_context_mode);
  }

  CSSParser::ParseDeclarationList(context, this, style_declaration);
}

bool MutableCSSPropertyValueSet::AddParsedProperties(
    const HeapVector<CSSPropertyValue, 256>& properties) {
  bool changed = false;
  property_vector_.ReserveCapacity(property_vector_.size() + properties.size());
  for (unsigned i = 0; i < properties.size(); ++i)
    changed |= SetProperty(properties[i]);
  return changed;
}

bool MutableCSSPropertyValueSet::AddRespectingCascade(
    const CSSPropertyValue& property) {
  // Only add properties that have no !important counterpart present
  if (!PropertyIsImportant(property.Id()) || property.IsImportant())
    return SetProperty(property);
  return false;
}

String CSSPropertyValueSet::AsText() const {
  return StylePropertySerializer(*this).AsText();
}

void MutableCSSPropertyValueSet::MergeAndOverrideOnConflict(
    const CSSPropertyValueSet* other) {
  unsigned size = other->PropertyCount();
  for (unsigned n = 0; n < size; ++n) {
    PropertyReference to_merge = other->PropertyAt(n);
    CSSPropertyValue* old = FindCSSPropertyWithName(to_merge.Name());
    if (old) {
      SetProperty(
          CSSPropertyValue(to_merge.PropertyMetadata(), to_merge.Value()), old);
    } else {
      property_vector_.push_back(
          CSSPropertyValue(to_merge.PropertyMetadata(), to_merge.Value()));
    }
  }
}

bool CSSPropertyValueSet::HasFailedOrCanceledSubresources() const {
  unsigned size = PropertyCount();
  for (unsigned i = 0; i < size; ++i) {
    if (PropertyAt(i).Value().HasFailedOrCanceledSubresources())
      return true;
  }
  return false;
}

void MutableCSSPropertyValueSet::Clear() {
  property_vector_.clear();
}

inline bool ContainsId(const CSSProperty* const set[],
                       unsigned length,
                       CSSPropertyID id) {
  for (unsigned i = 0; i < length; ++i) {
    if (set[i]->IDEquals(id))
      return true;
  }
  return false;
}

bool MutableCSSPropertyValueSet::RemovePropertiesInSet(
    const CSSProperty* const set[],
    unsigned length) {
  if (property_vector_.IsEmpty())
    return false;

  CSSPropertyValue* properties = property_vector_.data();
  unsigned old_size = property_vector_.size();
  unsigned new_index = 0;
  for (unsigned old_index = 0; old_index < old_size; ++old_index) {
    const CSSPropertyValue& property = properties[old_index];
    if (ContainsId(set, length, property.Id()))
      continue;
    // Modify property_vector_ in-place since this method is
    // performance-sensitive.
    properties[new_index++] = properties[old_index];
  }
  if (new_index != old_size) {
    property_vector_.Shrink(new_index);
    return true;
  }
  return false;
}

CSSPropertyValue* MutableCSSPropertyValueSet::FindCSSPropertyWithName(
    const CSSPropertyName& name) {
  int found_property_index = name.IsCustomProperty()
                                 ? FindPropertyIndex(name.ToAtomicString())
                                 : FindPropertyIndex(name.Id());
  if (found_property_index == -1)
    return nullptr;
  return &property_vector_.at(found_property_index);
}

bool CSSPropertyValueSet::PropertyMatches(
    CSSPropertyID property_id,
    const CSSValue& property_value) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return false;
  return PropertyAt(found_property_index).Value() == property_value;
}

void MutableCSSPropertyValueSet::RemoveEquivalentProperties(
    const CSSPropertyValueSet* style) {
  Vector<CSSPropertyID> properties_to_remove;
  unsigned size = property_vector_.size();
  for (unsigned i = 0; i < size; ++i) {
    PropertyReference property = PropertyAt(i);
    if (style->PropertyMatches(property.Id(), property.Value()))
      properties_to_remove.push_back(property.Id());
  }
  // FIXME: This should use mass removal.
  for (unsigned i = 0; i < properties_to_remove.size(); ++i)
    RemoveProperty(properties_to_remove[i]);
}

void MutableCSSPropertyValueSet::RemoveEquivalentProperties(
    const CSSStyleDeclaration* style) {
  Vector<CSSPropertyID> properties_to_remove;
  unsigned size = property_vector_.size();
  for (unsigned i = 0; i < size; ++i) {
    PropertyReference property = PropertyAt(i);
    if (style->CssPropertyMatches(property.Id(), property.Value()))
      properties_to_remove.push_back(property.Id());
  }
  // FIXME: This should use mass removal.
  for (unsigned i = 0; i < properties_to_remove.size(); ++i)
    RemoveProperty(properties_to_remove[i]);
}

MutableCSSPropertyValueSet* CSSPropertyValueSet::MutableCopy() const {
  return MakeGarbageCollected<MutableCSSPropertyValueSet>(*this);
}

MutableCSSPropertyValueSet* CSSPropertyValueSet::CopyPropertiesInSet(
    const Vector<const CSSProperty*>& properties) const {
  HeapVector<CSSPropertyValue, 256> list;
  list.ReserveInitialCapacity(properties.size());
  for (unsigned i = 0; i < properties.size(); ++i) {
    const CSSValue* value = GetPropertyCSSValue(properties[i]->PropertyID());
    if (value) {
      list.push_back(CSSPropertyValue(*properties[i], *value, false));
    }
  }
  return MakeGarbageCollected<MutableCSSPropertyValueSet>(list.data(),
                                                          list.size());
}

CSSStyleDeclaration* MutableCSSPropertyValueSet::EnsureCSSStyleDeclaration() {
  // FIXME: get rid of this weirdness of a CSSStyleDeclaration inside of a
  // style property set.
  if (cssom_wrapper_) {
    DCHECK(
        !static_cast<CSSStyleDeclaration*>(cssom_wrapper_.Get())->parentRule());
    DCHECK(!cssom_wrapper_->ParentElement());
    return cssom_wrapper_.Get();
  }
  cssom_wrapper_ = MakeGarbageCollected<PropertySetCSSStyleDeclaration>(*this);
  return cssom_wrapper_.Get();
}

template <typename T>
int MutableCSSPropertyValueSet::FindPropertyIndex(T property) const {
  const CSSPropertyValue* begin = property_vector_.data();
  const CSSPropertyValue* end = begin + property_vector_.size();

  uint16_t id = GetConvertedCSSPropertyID(property);

  const CSSPropertyValue* it = std::find_if(
      begin, end, [property, id](const CSSPropertyValue& css_property) -> bool {
        return IsPropertyMatch(css_property.Metadata(), *css_property.Value(),
                               id, property);
      });

  return (it == end) ? -1 : static_cast<int>(it - begin);
}
template CORE_EXPORT int MutableCSSPropertyValueSet::FindPropertyIndex(
    CSSPropertyID) const;
template CORE_EXPORT int MutableCSSPropertyValueSet::FindPropertyIndex(
    AtomicString) const;

void MutableCSSPropertyValueSet::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(cssom_wrapper_);
  visitor->Trace(property_vector_);
  CSSPropertyValueSet::TraceAfterDispatch(visitor);
}

unsigned CSSPropertyValueSet::AverageSizeInBytes() {
  // Please update this if the storage scheme changes so that this longer
  // reflects the actual size.
  return sizeof(ImmutableCSSPropertyValueSet) +
         AdditionalBytesForImmutableCSSPropertyValueSetWithPropertyCount(4)
             .value;
}

// See the function above if you need to update this.
struct SameSizeAsCSSPropertyValueSet final
    : public GarbageCollected<SameSizeAsCSSPropertyValueSet> {
  unsigned bitfield;
};
static_assert(sizeof(CSSPropertyValueSet) ==
                  sizeof(SameSizeAsCSSPropertyValueSet),
              "CSSPropertyValueSet should stay small");

#ifndef NDEBUG
void CSSPropertyValueSet::ShowStyle() {
  fprintf(stderr, "%s\n", AsText().Ascii().c_str());
}
#endif

void CSSLazyPropertyParser::Trace(blink::Visitor* visitor) {}

}  // namespace blink
