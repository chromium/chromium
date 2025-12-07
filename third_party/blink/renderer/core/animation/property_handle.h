// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PROPERTY_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PROPERTY_HANDLE_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Represents the property of a PropertySpecificKeyframe.
class CORE_EXPORT PropertyHandle {
  DISALLOW_NEW();

 public:
  explicit PropertyHandle(const CSSProperty& property)
      : handle_type_(kHandleCSSProperty), css_property_(&property) {
    DCHECK_NE(CSSPropertyID::kVariable, property.PropertyID());
  }

  // TODO(crbug.com/980160): Eliminate call to GetCSSPropertyVariable().
  explicit PropertyHandle(const AtomicString& property_name)
      : handle_type_(kHandleCSSCustomProperty),
        css_property_(&GetCSSPropertyVariable()),
        property_name_(property_name) {}

  // TODO(crbug.com/980160): Eliminate call to GetCSSPropertyVariable().
  explicit PropertyHandle(const CSSPropertyName& property_name)
      : handle_type_(property_name.IsCustomProperty() ? kHandleCSSCustomProperty
                                                      : kHandleCSSProperty),
        css_property_(property_name.IsCustomProperty()
                          ? &GetCSSPropertyVariable()
                          : &CSSProperty::Get(property_name.Id())),
        property_name_(property_name.IsCustomProperty()
                           ? property_name.ToAtomicString()
                           : g_null_atom) {}

  bool operator==(const PropertyHandle&) const;

  unsigned GetHash() const;

  bool IsCSSProperty() const {
    return handle_type_ == kHandleCSSProperty || IsCSSCustomProperty();
  }
  const CSSProperty& GetCSSProperty() const {
    DCHECK(IsCSSProperty());
    return *css_property_;
  }

  bool IsCSSCustomProperty() const {
    return handle_type_ == kHandleCSSCustomProperty;
  }
  const AtomicString& CustomPropertyName() const {
    DCHECK(IsCSSCustomProperty());
    return property_name_;
  }

  CSSPropertyName GetCSSPropertyName() const {
    if (handle_type_ == kHandleCSSCustomProperty)
      return CSSPropertyName(property_name_);
    DCHECK(IsCSSProperty());
    return CSSPropertyName(css_property_->PropertyID());
  }

 private:
  enum HandleType {
    kHandleEmptyValueForHashTraits,
    kHandleDeletedValueForHashTraits,
    kHandleCSSProperty,
    kHandleCSSCustomProperty,
  };

  explicit PropertyHandle(HandleType handle_type) : handle_type_(handle_type) {}

  static PropertyHandle EmptyValueForHashTraits() {
    return PropertyHandle(kHandleEmptyValueForHashTraits);
  }

  static PropertyHandle DeletedValueForHashTraits() {
    return PropertyHandle(kHandleDeletedValueForHashTraits);
  }

  bool IsDeletedValueForHashTraits() const {
    return handle_type_ == kHandleDeletedValueForHashTraits;
  }

  HandleType handle_type_;
  const CSSProperty* css_property_;
  AtomicString property_name_;

  friend struct HashTraits<PropertyHandle>;
};

template <>
struct HashTraits<PropertyHandle> : SimpleClassHashTraits<PropertyHandle> {
  static unsigned GetHash(const PropertyHandle& handle) {
    return handle.GetHash();
  }

  static void ConstructDeletedValue(PropertyHandle& slot) {
    new (base::NotNullTag::kNotNull, &slot)
        PropertyHandle(PropertyHandle::DeletedValueForHashTraits());
  }
  static bool IsDeletedValue(const PropertyHandle& value) {
    return value.IsDeletedValueForHashTraits();
  }

  static PropertyHandle EmptyValue() {
    return PropertyHandle::EmptyValueForHashTraits();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PROPERTY_HANDLE_H_
