// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_NAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_NAME_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

// This class may be used to represent the name of any valid CSS property,
// including custom properties.
class CORE_EXPORT CSSPropertyName {
  DISALLOW_NEW();
 public:
  explicit CSSPropertyName(CSSPropertyID property_id)
      : value_(static_cast<int>(property_id)) {
    DCHECK_NE(Id(), CSSPropertyID::kInvalid);
    DCHECK_NE(Id(), CSSPropertyID::kVariable);
  }

  explicit CSSPropertyName(const AtomicString& custom_property_name)
      : value_(static_cast<int>(CSSPropertyID::kVariable)),
        custom_property_name_(custom_property_name) {
    DCHECK(!custom_property_name.IsNull());
  }

  static base::Optional<CSSPropertyName> From(const String& value) {
    const CSSPropertyID property_id = cssPropertyID(value);
    if (property_id == CSSPropertyID::kInvalid)
      return base::nullopt;
    if (property_id == CSSPropertyID::kVariable)
      return base::make_optional(CSSPropertyName(AtomicString(value)));
    return base::make_optional(CSSPropertyName(property_id));
  }

  bool operator==(const CSSPropertyName&) const;
  bool operator!=(const CSSPropertyName& other) const {
    return !(*this == other);
  }

  CSSPropertyID Id() const {
    DCHECK(!IsEmptyValue() && !IsDeletedValue());
    return static_cast<CSSPropertyID>(value_);
  }

  bool IsCustomProperty() const { return Id() == CSSPropertyID::kVariable; }

  AtomicString ToAtomicString() const;

 private:
  // For HashTraits::EmptyValue().
  static constexpr int kEmptyValue = -1;
  // For HashTraits::ConstructDeletedValue(...).
  static constexpr int kDeletedValue = -2;

  explicit CSSPropertyName(int value) : value_(value) {
    DCHECK(value == kEmptyValue || value == kDeletedValue);
  }

  unsigned GetHash() const;
  bool IsEmptyValue() const { return value_ == kEmptyValue; }
  bool IsDeletedValue() const { return value_ == kDeletedValue; }

  // The value_ field is either a CSSPropertyID, kEmptyValue, or
  // kDeletedValue.
  int value_;
  AtomicString custom_property_name_;

  friend class CSSPropertyNameTest;
  friend struct ::WTF::DefaultHash<blink::CSSPropertyName>;
  friend struct ::WTF::HashTraits<blink::CSSPropertyName>;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::CSSPropertyName> {
  struct Hash {
    STATIC_ONLY(Hash);
    static unsigned GetHash(const blink::CSSPropertyName& name) {
      return name.GetHash();
    }

    static bool Equal(const blink::CSSPropertyName& a,
                      const blink::CSSPropertyName& b) {
      return a == b;
    }

    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

template <>
struct HashTraits<blink::CSSPropertyName>
    : SimpleClassHashTraits<blink::CSSPropertyName> {
  using CSSPropertyName = blink::CSSPropertyName;
  static const bool kEmptyValueIsZero = false;
  static const bool kNeedsDestruction = true;
  static void ConstructDeletedValue(CSSPropertyName& slot, bool) {
    new (NotNull, &slot) CSSPropertyName(CSSPropertyName::kDeletedValue);
  }
  static bool IsDeletedValue(CSSPropertyName value) {
    return value.IsDeletedValue();
  }
  static blink::CSSPropertyName EmptyValue() {
    return blink::CSSPropertyName(CSSPropertyName::kEmptyValue);
  }
};

}  // namespace WTF

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::CSSPropertyName)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_NAME_H_
