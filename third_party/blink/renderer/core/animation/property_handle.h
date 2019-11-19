// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PROPERTY_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PROPERTY_HANDLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Represents the property of a PropertySpecificKeyframe.
class CORE_EXPORT PropertyHandle {
  DISALLOW_NEW();

 public:
  explicit PropertyHandle(const CSSProperty& property,
                          bool is_presentation_attribute = false)
      : handle_type_(is_presentation_attribute ? kHandlePresentationAttribute
                                               : kHandleCSSProperty),
        css_property_(&property) {
    DCHECK_NE(CSSPropertyID::kVariable, property.PropertyID());
  }

  explicit PropertyHandle(const AtomicString& property_name)
      : handle_type_(kHandleCSSCustomProperty),
        css_property_(&GetCSSPropertyVariable()),
        property_name_(property_name) {}

  explicit PropertyHandle(const QualifiedName& attribute_name)
      : handle_type_(kHandleSVGAttribute), svg_attribute_(&attribute_name) {}

  bool operator==(const PropertyHandle&) const;
  bool operator!=(const PropertyHandle& other) const {
    return !(*this == other);
  }

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

  bool IsPresentationAttribute() const {
    return handle_type_ == kHandlePresentationAttribute;
  }
  const CSSProperty& PresentationAttribute() const {
    DCHECK(IsPresentationAttribute());
    return *css_property_;
  }

  bool IsSVGAttribute() const { return handle_type_ == kHandleSVGAttribute; }
  const QualifiedName& SvgAttribute() const {
    DCHECK(IsSVGAttribute());
    return *svg_attribute_;
  }

  CSSPropertyName GetCSSPropertyName() const {
    if (handle_type_ == kHandleCSSCustomProperty)
      return CSSPropertyName(property_name_);
    DCHECK(IsCSSProperty() || IsPresentationAttribute());
    return CSSPropertyName(css_property_->PropertyID());
  }

 private:
  enum HandleType {
    kHandleEmptyValueForHashTraits,
    kHandleDeletedValueForHashTraits,
    kHandleCSSProperty,
    kHandleCSSCustomProperty,
    kHandlePresentationAttribute,
    kHandleSVGAttribute,
  };

  explicit PropertyHandle(HandleType handle_type)
      : handle_type_(handle_type), svg_attribute_(nullptr) {}

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
  union {
    const CSSProperty* css_property_;
    const QualifiedName* svg_attribute_;
  };
  AtomicString property_name_;

  friend struct ::WTF::HashTraits<blink::PropertyHandle>;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::PropertyHandle> {
  struct Hash {
    STATIC_ONLY(Hash);
    static unsigned GetHash(const blink::PropertyHandle& handle) {
      return handle.GetHash();
    }

    static bool Equal(const blink::PropertyHandle& a,
                      const blink::PropertyHandle& b) {
      return a == b;
    }

    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

template <>
struct HashTraits<blink::PropertyHandle>
    : SimpleClassHashTraits<blink::PropertyHandle> {
  static const bool kNeedsDestruction = true;
  static void ConstructDeletedValue(blink::PropertyHandle& slot, bool) {
    new (NotNull, &slot) blink::PropertyHandle(
        blink::PropertyHandle::DeletedValueForHashTraits());
  }
  static bool IsDeletedValue(const blink::PropertyHandle& value) {
    return value.IsDeletedValueForHashTraits();
  }

  static blink::PropertyHandle EmptyValue() {
    return blink::PropertyHandle::EmptyValueForHashTraits();
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_PROPERTY_HANDLE_H_
