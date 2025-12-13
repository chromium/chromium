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

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_bitsets.h"
#include "third_party/blink/renderer/core/css/style_property_serializer.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

static AdditionalBytes
AdditionalBytesForImmutableCSSPropertyValueSetWithPropertyCount(
    unsigned count) {
  return AdditionalBytes(sizeof(CSSPropertyValue) * count);
}

ImmutableCSSPropertyValueSet* ImmutableCSSPropertyValueSet::Create(
    base::span<const CSSPropertyValue> properties,
    CSSParserMode css_parser_mode,
    bool contains_cursor_hand) {
  DCHECK_LE(properties.size(), static_cast<unsigned>(kMaxArraySize));
  return MakeGarbageCollected<ImmutableCSSPropertyValueSet>(
      AdditionalBytesForImmutableCSSPropertyValueSetWithPropertyCount(
          properties.size()),
      PassKey(), properties, css_parser_mode, contains_cursor_hand);
}

ImmutableCSSPropertyValueSet* CSSPropertyValueSet::ImmutableCopyIfNeeded()
    const {
  auto* immutable_property_set = DynamicTo<ImmutableCSSPropertyValueSet>(
      const_cast<CSSPropertyValueSet*>(this));
  if (immutable_property_set) {
    return immutable_property_set;
  }

  const auto* mutable_this = To<MutableCSSPropertyValueSet>(this);
  return ImmutableCSSPropertyValueSet::Create(
      base::span(mutable_this->property_vector_), CssParserMode());
}

unsigned CSSPropertyValueSet::ComputeHash() const {
  unsigned hash = 3141592653;

  for (const CSSPropertyValue& property : Properties()) {
    if (property.PropertyID() == CSSPropertyID::kVariable) {
      AddIntToHash(hash, property.Name().ToAtomicString().Hash());
    } else {
      AddIntToHash(hash, static_cast<unsigned>(property.PropertyID()));
    }
    AddIntToHash(hash, property.IsImportant());
    AddIntToHash(hash, property.Value().Hash());
  }

  static_assert((HashTraits<unsigned>::EmptyValue() ^ 0x80000000) !=
                    HashTraits<unsigned>::DeletedValue(),
                "We assume below that flipping the top bit will not turn "
                "EmptyValue into DeletedValue or vice versa");
  if (hash == HashTraits<unsigned>::EmptyValue() ||
      hash == HashTraits<unsigned>::DeletedValue()) {
    hash ^= 0x80000000;
  }

  return hash;
}

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    CSSParserMode css_parser_mode)
    : CSSPropertyValueSet(css_parser_mode) {}

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    base::span<const CSSPropertyValue> properties)
    : CSSPropertyValueSet(kHTMLStandardMode) {
  property_vector_.ReserveInitialCapacity(properties.size());
  for (const CSSPropertyValue& property : properties) {
    property_vector_.UncheckedAppend(property);
    bits_.set<MayHaveLogicalPropertiesField>(
        bits_.get<MayHaveLogicalPropertiesField>() ||
        kLogicalGroupProperties.Has(property.PropertyID()));
  }
}

ImmutableCSSPropertyValueSet::ImmutableCSSPropertyValueSet(
    PassKey,
    base::span<const CSSPropertyValue> properties,
    CSSParserMode css_parser_mode,
    bool contains_query_hand)
    : CSSPropertyValueSet(css_parser_mode,
                          properties.size(),
                          contains_query_hand) {
  const unsigned array_size = bits_.get<ArraySizeField>();
  if (array_size > 0) {
    // SAFETY: By funneling all allocation of ImmutableCSSPropertyValueSet
    // through Create(), we guarantee that the arrays will have storage where we
    // expect.
    UNSAFE_BUFFERS(base::span<CSSPropertyValue> array(
        const_cast<CSSPropertyValue*>(ArrayBase()), array_size));
    for (unsigned i = 0; i < array_size; ++i) {
      new (&array[i]) CSSPropertyValue(properties[i]);
    }
  }
}

// Convert property into an uint16_t for comparison with property's ID
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

static bool IsPropertyMatch(const CSSPropertyValue& property,
                            uint16_t id,
                            CSSPropertyID property_id) {
  DCHECK_EQ(id, static_cast<uint16_t>(property_id));
  bool result = static_cast<uint16_t>(property.PropertyID()) == id;
// Only enabled properties except kInternalFontSizeDelta should be part of the
// style.
// TODO(hjkim3323@gmail.com): Remove kInternalFontSizeDelta bypassing hack
#if DCHECK_IS_ON()
  DCHECK(!result || property_id == CSSPropertyID::kInternalFontSizeDelta ||
         CSSProperty::Get(ResolveCSSPropertyID(property_id)).IsWebExposed());
#endif
  return result;
}

static bool IsPropertyMatch(const CSSPropertyValue& property,
                            uint16_t id,
                            const AtomicString& custom_property_name) {
  DCHECK_EQ(id, static_cast<uint16_t>(CSSPropertyID::kVariable));
  if (property.PropertyID() == CSSPropertyID::kVariable) {
    return property.CustomPropertyName() == custom_property_name;
  } else {
    return false;
  }
}

static bool IsPropertyMatch(const CSSPropertyValue& property,
                            uint16_t id,
                            AtRuleDescriptorID descriptor_id) {
  return IsPropertyMatch(property, id,
                         AtRuleDescriptorIDAsCSSPropertyID(descriptor_id));
}

template <typename T>
int ImmutableCSSPropertyValueSet::FindPropertyIndex(const T& property) const {
  uint16_t id = GetConvertedCSSPropertyID(property);
  const base::span<const CSSPropertyValue> properties = Properties();
  for (size_t n = bits_.get<ArraySizeField>(); n; --n) {
    if (IsPropertyMatch(properties[n - 1], id, property)) {
      return static_cast<int>(n - 1);
    }
  }

  return -1;
}
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    const CSSPropertyID&) const;
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    const AtomicString&) const;
template CORE_EXPORT int ImmutableCSSPropertyValueSet::FindPropertyIndex(
    const AtRuleDescriptorID&) const;

void ImmutableCSSPropertyValueSet::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  for (const CSSPropertyValue& value : Properties()) {
    visitor->Trace(value);
  }
  CSSPropertyValueSet::TraceAfterDispatch(visitor);
}

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    const CSSPropertyValueSet& other)
    : CSSPropertyValueSet(other.CssParserMode()) {
  if (auto* other_mutable_property_set =
          DynamicTo<MutableCSSPropertyValueSet>(other)) {
    property_vector_ = other_mutable_property_set->property_vector_;
    bits_.set<MayHaveLogicalPropertiesField>(
        other_mutable_property_set->bits_.get<MayHaveLogicalPropertiesField>());
  } else {
    property_vector_.ReserveInitialCapacity(other.PropertyCount());
    bool may_have_logical_properties =
        bits_.get<MayHaveLogicalPropertiesField>();
    for (const CSSPropertyValue& property : other.Properties()) {
      property_vector_.UncheckedAppend(CSSPropertyValue(property));
      may_have_logical_properties |=
          kLogicalGroupProperties.Has(property.PropertyID());
    }
    bits_.set<MayHaveLogicalPropertiesField>(may_have_logical_properties);
  }
}

static String SerializeShorthand(const CSSPropertyValueSet& property_set,
                                 CSSPropertyID property_id) {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length()) {
    return String();
  }

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
String CSSPropertyValueSet::GetPropertyValue(const T& property) const {
  String shorthand_serialization = SerializeShorthand(*this, property);
  if (!shorthand_serialization.IsNull()) {
    return shorthand_serialization;
  }
  const CSSValue* value = GetPropertyCSSValue(property);
  if (value) {
    return value->CssText();
  }
  return g_empty_string;
}
template CORE_EXPORT String
CSSPropertyValueSet::GetPropertyValue<CSSPropertyID>(
    const CSSPropertyID&) const;
template CORE_EXPORT String
CSSPropertyValueSet::GetPropertyValue<AtRuleDescriptorID>(
    const AtRuleDescriptorID&) const;
template CORE_EXPORT String
CSSPropertyValueSet::GetPropertyValue<AtomicString>(const AtomicString&) const;

String CSSPropertyValueSet::GetPropertyValueWithHint(
    const AtomicString& property_name,
    unsigned index) const {
  const CSSValue* value = GetPropertyCSSValueWithHint(property_name, index);
  if (value) {
    return value->CssText();
  }
  return g_empty_string;
}

template <typename T>
const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue(
    const T& property) const {
  int found_property_index = FindPropertyIndex(property);
  if (found_property_index == -1) {
    return nullptr;
  }
  return &PropertyAt(found_property_index).Value();
}
template CORE_EXPORT const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue<
    CSSPropertyID>(const CSSPropertyID&) const;
template CORE_EXPORT const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue<
    AtRuleDescriptorID>(const AtRuleDescriptorID&) const;
template CORE_EXPORT const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue<
    AtomicString>(const AtomicString&) const;

const CSSValue* CSSPropertyValueSet::GetPropertyCSSValueWithHint(
    const AtomicString& property_name,
    unsigned index) const {
  DCHECK_EQ(property_name, PropertyAt(index).Name().ToAtomicString());
  return &PropertyAt(index).Value();
}

void CSSPropertyValueSet::Trace(Visitor* visitor) const {
  if (bits_.get_concurrently<IsMutableField>()) {
    To<MutableCSSPropertyValueSet>(this)->TraceAfterDispatch(visitor);
  } else {
    To<ImmutableCSSPropertyValueSet>(this)->TraceAfterDispatch(visitor);
  }
}

void CSSPropertyValueSet::FinalizeGarbageCollectedObject() {
  if (bits_.get_concurrently<IsMutableField>()) {
    To<MutableCSSPropertyValueSet>(this)->~MutableCSSPropertyValueSet();
  } else {
    To<ImmutableCSSPropertyValueSet>(this)->~ImmutableCSSPropertyValueSet();
  }
}

bool MutableCSSPropertyValueSet::RemoveShorthandProperty(
    CSSPropertyID property_id) {
  if (property_id == CSSPropertyID::kAll) {
    return RemovePropertiesAffectedByAll();
  }
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length()) {
    return false;
  }

  return RemovePropertiesInSet(shorthand.properties());
}

bool MutableCSSPropertyValueSet::RemovePropertyAtIndex(int property_index,
                                                       String* return_text) {
  if (property_index == -1) {
    if (return_text) {
      *return_text = "";
    }
    return false;
  }

  if (return_text) {
    *return_text = PropertyAt(property_index).Value().CssText();
  }

  // A more efficient removal strategy would involve marking entries as empty
  // and sweeping them when the vector grows too big.
  property_vector_.EraseAt(property_index);

  InvalidateHashIfComputed();

  return true;
}

template <typename T>
bool MutableCSSPropertyValueSet::RemoveProperty(const T& property,
                                                String* return_text) {
  if (RemoveShorthandProperty(property)) {
    // FIXME: Return an equivalent shorthand when possible.
    if (return_text) {
      *return_text = "";
    }
    return true;
  }

  int found_property_index = FindPropertyIndex(property);
  return RemovePropertyAtIndex(found_property_index, return_text);
}
template CORE_EXPORT bool MutableCSSPropertyValueSet::RemoveProperty(
    const CSSPropertyID&,
    String*);
template CORE_EXPORT bool MutableCSSPropertyValueSet::RemoveProperty(
    const AtomicString&,
    String*);

template <typename T>
bool CSSPropertyValueSet::PropertyIsImportant(const T& property) const {
  int found_property_index = FindPropertyIndex(property);
  if (found_property_index != -1) {
    return PropertyAt(found_property_index).IsImportant();
  }
  return ShorthandIsImportant(property);
}
template CORE_EXPORT bool CSSPropertyValueSet::PropertyIsImportant<
    CSSPropertyID>(const CSSPropertyID&) const;
template bool CSSPropertyValueSet::PropertyIsImportant<AtomicString>(
    const AtomicString&) const;

bool CSSPropertyValueSet::PropertyIsImportantWithHint(
    const AtomicString& property_name,
    unsigned index) const {
  DCHECK_EQ(property_name, PropertyAt(index).Name().ToAtomicString());
  return PropertyAt(index).IsImportant();
}

bool CSSPropertyValueSet::ShorthandIsImportant(
    CSSPropertyID property_id) const {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  const StylePropertyShorthand::Properties longhands = shorthand.properties();
  if (longhands.empty()) {
    return false;
  }

  for (const CSSProperty* const longhand : longhands) {
    if (!PropertyIsImportant(longhand->PropertyID())) {
      return false;
    }
  }
  return true;
}

CSSPropertyID CSSPropertyValueSet::GetPropertyShorthand(
    CSSPropertyID property_id) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1) {
    return CSSPropertyID::kInvalid;
  }
  return PropertyAt(found_property_index).ShorthandID();
}

bool CSSPropertyValueSet::IsPropertyImplicit(CSSPropertyID property_id) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1) {
    return false;
  }
  return PropertyAt(found_property_index).IsImplicit();
}

MutableCSSPropertyValueSet::SetResult
MutableCSSPropertyValueSet::ParseAndSetProperty(
    CSSPropertyID unresolved_property,
    StringView value,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* context_style_sheet) {
  DCHECK_GE(unresolved_property, kFirstCSSProperty);

  // Setting the value to an empty string just removes the property in both IE
  // and Gecko. Setting it to null seems to produce less consistent results, but
  // we treat it just the same.
  if (value.empty()) {
    return RemoveProperty(ResolveCSSPropertyID(unresolved_property))
               ? kChangedPropertySet
               : kUnchanged;
  }

  // When replacing an existing property value, this moves the property to the
  // end of the list. Firefox preserves the position, and MSIE moves the
  // property to the beginning.
  return CSSParser::ParseValue(this, unresolved_property, value, important,
                               secure_context_mode, context_style_sheet);
}

MutableCSSPropertyValueSet::SetResult
MutableCSSPropertyValueSet::ParseAndSetCustomProperty(
    const AtomicString& custom_property_name,
    StringView value,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* context_style_sheet,
    bool is_animation_tainted) {
  if (value.empty()) {
    return RemoveProperty(custom_property_name) ? kChangedPropertySet
                                                : kUnchanged;
  }
  return CSSParser::ParseValueForCustomProperty(
      this, custom_property_name, value, important, secure_context_mode,
      context_style_sheet, is_animation_tainted);
}

void MutableCSSPropertyValueSet::SetProperty(const CSSPropertyName& name,
                                             const CSSValue& value,
                                             bool important) {
  if (name.Id() == CSSPropertyID::kVariable) {
    SetLonghandProperty(CSSPropertyValue(name, value, important));
  } else {
    SetProperty(name.Id(), value, important);
  }
}

void MutableCSSPropertyValueSet::SetProperty(CSSPropertyID property_id,
                                             const CSSValue& value,
                                             bool important) {
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  DCHECK_NE(property_id, CSSPropertyID::kWhiteSpace);
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length()) {
    SetLonghandProperty(
        CSSPropertyValue(CSSPropertyName(property_id), value, important));
    return;
  }

  RemovePropertiesInSet(shorthand.properties());

  // The simple shorthand expansion below doesn't work for `white-space`.
  DCHECK_NE(property_id, CSSPropertyID::kWhiteSpace);
  for (const CSSProperty* const longhand : shorthand.properties()) {
    CSSPropertyName longhand_name(longhand->PropertyID());
    property_vector_.push_back(
        CSSPropertyValue(longhand_name, value, important));
  }
  InvalidateHashIfComputed();
}

ALWAYS_INLINE CSSPropertyValue*
MutableCSSPropertyValueSet::FindInsertionPointForID(CSSPropertyID property_id) {
  CSSPropertyValue* to_replace =
      const_cast<CSSPropertyValue*>(FindPropertyPointer(property_id));
  if (to_replace == nullptr) {
    return nullptr;
  }
  if (bits_.get<MayHaveLogicalPropertiesField>()) {
    const CSSProperty& prop = CSSProperty::Get(property_id);
    if (prop.IsInLogicalPropertyGroup()) {
      DCHECK(property_vector_.Contains(*to_replace));
      int to_replace_index =
          static_cast<int>(to_replace - property_vector_.data());
      for (int n = property_vector_.size() - 1; n > to_replace_index; --n) {
        if (prop.IsInSameLogicalPropertyGroupWithDifferentMappingLogic(
                PropertyAt(n).PropertyID())) {
          RemovePropertyAtIndex(to_replace_index, nullptr);
          return nullptr;
        }
      }
    }
  }
  return to_replace;
}

MutableCSSPropertyValueSet::SetResult
MutableCSSPropertyValueSet::SetLonghandProperty(CSSPropertyValue property) {
  const CSSPropertyID id = property.PropertyID();
  DCHECK_EQ(shorthandForProperty(id).length(), 0u)
      << CSSProperty::Get(id).GetPropertyNameString() << " is a shorthand";
  CSSPropertyValue* to_replace;
  if (id == CSSPropertyID::kVariable) {
    to_replace = const_cast<CSSPropertyValue*>(
        FindPropertyPointer(property.CustomPropertyName()));
  } else {
    to_replace = FindInsertionPointForID(id);
  }
  if (to_replace) {
    if (*to_replace == property) {
      return kUnchanged;
    }
    *to_replace = std::move(property);
    InvalidateHashIfComputed();
    return kModifiedExisting;
  } else {
    bits_.set<MayHaveLogicalPropertiesField>(
        bits_.get<MayHaveLogicalPropertiesField>() ||
        kLogicalGroupProperties.Has(id));
  }
  property_vector_.push_back(std::move(property));
  InvalidateHashIfComputed();
  return kChangedPropertySet;
}

void MutableCSSPropertyValueSet::SetLonghandProperty(CSSPropertyID property_id,
                                                     const CSSValue& value) {
  DCHECK_EQ(shorthandForProperty(property_id).length(), 0u)
      << CSSProperty::Get(property_id).GetPropertyNameString()
      << " is a shorthand";
  CSSPropertyValue* to_replace = FindInsertionPointForID(property_id);
  if (to_replace) {
    *to_replace = CSSPropertyValue(CSSPropertyName(property_id), value);
  } else {
    bits_.set<MayHaveLogicalPropertiesField>(
        bits_.get<MayHaveLogicalPropertiesField>() ||
        kLogicalGroupProperties.Has(property_id));
    property_vector_.emplace_back(CSSPropertyName(property_id), value);
  }
  InvalidateHashIfComputed();
}

MutableCSSPropertyValueSet::SetResult
MutableCSSPropertyValueSet::SetLonghandProperty(CSSPropertyID property_id,
                                                CSSValueID identifier,
                                                bool important) {
  CSSPropertyName name(property_id);
  return SetLonghandProperty(CSSPropertyValue(
      name, *CSSIdentifierValue::Create(identifier), important));
}

void MutableCSSPropertyValueSet::ParseDeclarationList(
    const String& style_declaration,
    SecureContextMode secure_context_mode,
    StyleSheetContents* context_style_sheet) {
  property_vector_.clear();
  InvalidateHashIfComputed();

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

MutableCSSPropertyValueSet::SetResult
MutableCSSPropertyValueSet::AddParsedProperties(
    base::span<CSSPropertyValue> properties) {
  SetResult changed = kUnchanged;
  property_vector_.reserve(property_vector_.size() + properties.size());
  for (const CSSPropertyValue& property : properties) {
    changed = std::max(changed, SetLonghandProperty(property));
  }
  return changed;
}

bool MutableCSSPropertyValueSet::AddRespectingCascade(
    const CSSPropertyValue& property) {
  // Only add properties that have no !important counterpart present
  if (!PropertyIsImportant(property.PropertyID()) || property.IsImportant()) {
    return SetLonghandProperty(property);
  }
  return false;
}

String CSSPropertyValueSet::AsText() const {
  return StylePropertySerializer(*this).AsText();
}

void MutableCSSPropertyValueSet::MergeAndOverrideOnConflict(
    const CSSPropertyValueSet* other) {
  for (const CSSPropertyValue& property : other->Properties()) {
    SetLonghandProperty(property);
  }
}

bool CSSPropertyValueSet::HasFailedOrCanceledSubresources() const {
  for (const CSSPropertyValue& property : Properties()) {
    if (property.Value().HasFailedOrCanceledSubresources()) {
      return true;
    }
  }
  return false;
}

void MutableCSSPropertyValueSet::Clear() {
  property_vector_.clear();
  InvalidateHashIfComputed();
  bits_.set<MayHaveLogicalPropertiesField>(false);
}

inline bool ContainsId(const base::span<const CSSProperty* const>& set,
                       CSSPropertyID id) {
  for (const CSSProperty* const property : set) {
    if (property->IDEquals(id)) {
      return true;
    }
  }
  return false;
}

bool MutableCSSPropertyValueSet::RemovePropertiesInSet(
    base::span<const CSSProperty* const> set) {
  if (property_vector_.empty()) {
    return false;
  }

  base::span<CSSPropertyValue> properties(property_vector_);
  unsigned old_size = property_vector_.size();
  unsigned new_index = 0;
  for (unsigned old_index = 0; old_index < old_size; ++old_index) {
    const CSSPropertyValue& property = properties[old_index];
    if (ContainsId(set, property.PropertyID())) {
      continue;
    }
    // Modify property_vector_ in-place since this method is
    // performance-sensitive.
    properties[new_index++] = properties[old_index];
  }
  if (new_index != old_size) {
    property_vector_.Shrink(new_index);
    InvalidateHashIfComputed();
    return true;
  }
  return false;
}

bool MutableCSSPropertyValueSet::RemovePropertiesAffectedByAll() {
  if (property_vector_.empty()) {
    return false;
  }

  base::span<CSSPropertyValue> properties(property_vector_);
  unsigned old_size = property_vector_.size();
  unsigned new_index = 0;
  for (unsigned old_index = 0; old_index < old_size; ++old_index) {
    const CSSPropertyValue& property = properties[old_index];
    if (CSSProperty::Get(property.PropertyID()).IsAffectedByAll()) {
      continue;
    }
    properties[new_index++] = properties[old_index];
  }
  if (new_index != old_size) {
    property_vector_.Shrink(new_index);
    InvalidateHashIfComputed();
    return true;
  }
  return false;
}

CSSPropertyValue* MutableCSSPropertyValueSet::FindCSSPropertyWithName(
    const CSSPropertyName& name) {
  return const_cast<CSSPropertyValue*>(
      name.IsCustomProperty() ? FindPropertyPointer(name.ToAtomicString())
                              : FindPropertyPointer(name.Id()));
}

bool CSSPropertyValueSet::PropertyMatches(
    CSSPropertyID property_id,
    const CSSValue& property_value) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1) {
    return false;
  }
  return PropertyAt(found_property_index).Value() == property_value;
}

void MutableCSSPropertyValueSet::RemoveEquivalentProperties(
    const CSSPropertyValueSet* style) {
  Vector<CSSPropertyID> properties_to_remove;
  unsigned size = property_vector_.size();
  for (unsigned i = 0; i < size; ++i) {
    const CSSPropertyValue& property = PropertyAt(i);
    if (style->PropertyMatches(property.PropertyID(), property.Value())) {
      properties_to_remove.push_back(property.PropertyID());
    }
  }
  // FIXME: This should use mass removal.
  for (CSSPropertyID id : properties_to_remove) {
    RemoveProperty(id);
  }
}

void MutableCSSPropertyValueSet::RemoveEquivalentProperties(
    const CSSStyleDeclaration* style) {
  Vector<CSSPropertyID> properties_to_remove;
  for (const CSSPropertyValue& property : property_vector_) {
    if (style->CssPropertyMatches(property.PropertyID(), property.Value())) {
      properties_to_remove.push_back(property.PropertyID());
    }
  }
  // FIXME: This should use mass removal.
  for (CSSPropertyID id : properties_to_remove) {
    RemoveProperty(id);
  }
}

MutableCSSPropertyValueSet* CSSPropertyValueSet::MutableCopy() const {
  return MakeGarbageCollected<MutableCSSPropertyValueSet>(*this);
}

MutableCSSPropertyValueSet* CSSPropertyValueSet::CopyPropertiesInSet(
    const Vector<const CSSProperty*>& properties) const {
  HeapVector<CSSPropertyValue, 64> list;
  list.ReserveInitialCapacity(properties.size());
  for (const CSSProperty* property : properties) {
    CSSPropertyName name(property->PropertyID());
    const CSSValue* value = GetPropertyCSSValue(name.Id());
    if (value) {
      list.push_back(CSSPropertyValue(name, *value, false));
    }
  }
  return MakeGarbageCollected<MutableCSSPropertyValueSet>(list);
}

CSSStyleDeclaration* MutableCSSPropertyValueSet::EnsureCSSStyleDeclaration(
    ExecutionContext* execution_context) {
  // FIXME: get rid of this weirdness of a CSSStyleDeclaration inside of a
  // style property set.
  if (cssom_wrapper_) {
    DCHECK(
        !static_cast<CSSStyleDeclaration*>(cssom_wrapper_.Get())->parentRule());
    DCHECK(!cssom_wrapper_->ParentElement());
    return cssom_wrapper_.Get();
  }
  cssom_wrapper_ = MakeGarbageCollected<PropertySetCSSStyleDeclaration>(
      execution_context, *this);
  return cssom_wrapper_.Get();
}

template <typename T>
int MutableCSSPropertyValueSet::FindPropertyIndex(const T& property) const {
  const CSSPropertyValue* begin = property_vector_.data();
  const CSSPropertyValue* it = FindPropertyPointer(property);
  return (it == nullptr) ? -1 : static_cast<int>(it - begin);
}
template CORE_EXPORT int MutableCSSPropertyValueSet::FindPropertyIndex(
    const CSSPropertyID&) const;
template CORE_EXPORT int MutableCSSPropertyValueSet::FindPropertyIndex(
    const AtomicString&) const;

template <typename T>
const CSSPropertyValue* MutableCSSPropertyValueSet::FindPropertyPointer(
    const T& property) const {
  uint16_t id = GetConvertedCSSPropertyID(property);

  auto it = std::find_if(
      property_vector_.begin(), property_vector_.end(),
      [property, id](const CSSPropertyValue& css_property) -> bool {
        return IsPropertyMatch(css_property, id, property);
      });
  return (it == property_vector_.end()) ? nullptr : &*it;
}

void MutableCSSPropertyValueSet::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(cssom_wrapper_);
  visitor->Trace(property_vector_);
  CSSPropertyValueSet::TraceAfterDispatch(visitor);
}

unsigned CSSPropertyValueSet::AverageSizeInBytes() {
  // Please update this if the storage scheme changes so that this longer
  // reflects the actual size.
  return sizeof(ImmutableCSSPropertyValueSet) +
         static_cast<wtf_size_t>(
             AdditionalBytesForImmutableCSSPropertyValueSetWithPropertyCount(4)
                 .value);
}

// See the function above if you need to update this.
struct SameSizeAsCSSPropertyValueSet final
    : public GarbageCollected<SameSizeAsCSSPropertyValueSet> {
  uint32_t bitfield;
  unsigned hash;
};
ASSERT_SIZE(CSSPropertyValueSet, SameSizeAsCSSPropertyValueSet);

#ifndef NDEBUG
void CSSPropertyValueSet::ShowStyle() {
  fprintf(stderr, "%s\n", AsText().Ascii().c_str());
}
#endif

}  // namespace blink
