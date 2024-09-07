/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_VALUE_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_VALUE_SET_H_

#include "base/bits.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/property_set_css_style_declaration.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSStyleDeclaration;
class ExecutionContext;
class ImmutableCSSPropertyValueSet;
class MutableCSSPropertyValueSet;
class StyleSheetContents;
enum class CSSValueID;
enum class SecureContextMode;

class CORE_EXPORT CSSPropertyValueSet
    : public GarbageCollected<CSSPropertyValueSet> {
  friend class PropertyReference;

 public:
  CSSPropertyValueSet(const CSSPropertyValueSet&) = delete;
  CSSPropertyValueSet& operator=(const CSSPropertyValueSet&) = delete;

  void FinalizeGarbageCollectedObject();

  class PropertyReference {
    STACK_ALLOCATED();

   public:
    PropertyReference(const CSSPropertyValueSet& property_set, unsigned index)
        : property_set_(&property_set), index_(index) {}

    CSSPropertyID Id() const {
      return static_cast<CSSPropertyID>(PropertyMetadata().PropertyID());
    }
    CSSPropertyID ShorthandID() const {
      return PropertyMetadata().ShorthandID();
    }

    CSSPropertyName Name() const { return PropertyMetadata().Name(); }

    bool IsImportant() const { return PropertyMetadata().important_; }
    bool IsImplicit() const { return PropertyMetadata().implicit_; }
    bool IsAffectedByAll() const {
      return Id() != CSSPropertyID::kVariable &&
             CSSProperty::Get(Id()).IsAffectedByAll();
    }

    const CSSValue& Value() const { return PropertyValue(); }

    const CSSPropertyValueMetadata& PropertyMetadata() const;

   private:
    const CSSValue& PropertyValue() const;

    const CSSPropertyValueSet* property_set_;
    unsigned index_;
  };

  unsigned PropertyCount() const;
  bool IsEmpty() const;
  PropertyReference PropertyAt(unsigned index) const {
    return PropertyReference(*this, index);
  }

  template <typename T>  // CSSPropertyID or AtomicString
  int FindPropertyIndex(const T& property) const;

  bool HasProperty(CSSPropertyID property) const {
    return FindPropertyIndex(property) != -1;
  }

  template <typename T>  // CSSPropertyID or AtomicString
  const CSSValue* GetPropertyCSSValue(const T& property) const;

  template <typename T>  // CSSPropertyID or AtomicString
  String GetPropertyValue(const T& property) const;

  template <typename T>  // CSSPropertyID or AtomicString
  bool PropertyIsImportant(const T& property) const;

  const CSSValue* GetPropertyCSSValueWithHint(const AtomicString& property_name,
                                              unsigned index) const;
  String GetPropertyValueWithHint(const AtomicString& property_name,
                                  unsigned index) const;
  bool PropertyIsImportantWithHint(const AtomicString& property_name,
                                   unsigned index) const;

  bool ShorthandIsImportant(CSSPropertyID) const;
  bool ShorthandIsImportant(const AtomicString& custom_property_name) const {
    // Custom properties are never shorthands.
    return false;
  }

  CSSPropertyID GetPropertyShorthand(CSSPropertyID) const;
  bool IsPropertyImplicit(CSSPropertyID) const;

  CSSParserMode CssParserMode() const {
    return static_cast<CSSParserMode>(css_parser_mode_);
  }

  MutableCSSPropertyValueSet* MutableCopy() const;
  ImmutableCSSPropertyValueSet* ImmutableCopyIfNeeded() const;

  MutableCSSPropertyValueSet* CopyPropertiesInSet(
      const Vector<const CSSProperty*>&) const;

  String AsText() const;

  bool IsMutable() const { return is_mutable_; }
  bool ContainsCursorHand() const { return contains_cursor_hand_; }

  bool HasFailedOrCanceledSubresources() const;

  static unsigned AverageSizeInBytes();

#ifndef NDEBUG
  void ShowStyle();
#endif

  bool PropertyMatches(CSSPropertyID, const CSSValue&) const;

  void Trace(Visitor*) const;
  void TraceAfterDispatch(blink::Visitor* visitor) const {}

 protected:
  enum { kMaxArraySize = (1 << 27) - 1 };

  explicit CSSPropertyValueSet(CSSParserMode css_parser_mode)
      : array_size_(0),
        css_parser_mode_(css_parser_mode),
        is_mutable_(true),
        contains_cursor_hand_(false) {}

  CSSPropertyValueSet(CSSParserMode css_parser_mode,
                      unsigned immutable_array_size,
                      bool contains_cursor_hand)
      // Avoid min()/max() from std here in the header, because that would
      // require inclusion of <algorithm>, which is slow to compile.
      : array_size_((immutable_array_size < unsigned(kMaxArraySize))
                        ? immutable_array_size
                        : unsigned(kMaxArraySize)),
        css_parser_mode_(css_parser_mode),
        is_mutable_(false),
        contains_cursor_hand_(contains_cursor_hand) {}

  const uint32_t array_size_ : 26;
  const uint32_t css_parser_mode_ : 4;
  const uint32_t is_mutable_ : 1;
  const uint32_t contains_cursor_hand_ : 1;

  friend class PropertySetCSSStyleDeclaration;
};

// Used for lazily parsing properties.
class CSSLazyPropertyParser : public GarbageCollected<CSSLazyPropertyParser> {
 public:
  CSSLazyPropertyParser() = default;
  CSSLazyPropertyParser(const CSSLazyPropertyParser&) = delete;
  CSSLazyPropertyParser& operator=(const CSSLazyPropertyParser&) = delete;
  virtual ~CSSLazyPropertyParser() = default;
  virtual CSSPropertyValueSet* ParseProperties() = 0;
  virtual void Trace(Visitor*) const;
};

class CORE_EXPORT alignas(std::max(alignof(Member<const CSSValue>),
                                   alignof(CSSPropertyValueMetadata)))
    ImmutableCSSPropertyValueSet : public CSSPropertyValueSet {
 public:
  ImmutableCSSPropertyValueSet(const CSSPropertyValue*,
                               unsigned count,
                               CSSParserMode,
                               bool contains_cursor_hand = false);

  static ImmutableCSSPropertyValueSet* Create(
      const CSSPropertyValue* properties,
      unsigned count,
      CSSParserMode,
      bool contains_cursor_hand = false);

  unsigned PropertyCount() const { return array_size_; }

  const Member<const CSSValue>* ValueArray() const;
  const CSSPropertyValueMetadata* MetadataArray() const;

  template <typename T>  // CSSPropertyID or AtomicString
  int FindPropertyIndex(const T& property) const;

  void TraceAfterDispatch(blink::Visitor*) const;
};

inline const Member<const CSSValue>* ImmutableCSSPropertyValueSet::ValueArray()
    const {
  static_assert(
      sizeof(ImmutableCSSPropertyValueSet) % alignof(Member<const CSSValue>) ==
          0,
      "ValueArray may be improperly aligned");
  return reinterpret_cast<const Member<const CSSValue>*>(this + 1);
}

inline const CSSPropertyValueMetadata*
ImmutableCSSPropertyValueSet::MetadataArray() const {
  static_assert(sizeof(ImmutableCSSPropertyValueSet) %
                        alignof(CSSPropertyValueMetadata) ==
                    0,
                "MetadataArray may be improperly aligned");
  // Size of Member<> can be smaller than that of CSSPropertyValueMetadata.
  // Align it up.
  return reinterpret_cast<const CSSPropertyValueMetadata*>(base::bits::AlignUp(
      reinterpret_cast<const uint8_t*>(ValueArray() + array_size_),
      alignof(CSSPropertyValueMetadata)));
}

template <>
struct DowncastTraits<ImmutableCSSPropertyValueSet> {
  static bool AllowFrom(const CSSPropertyValueSet& set) {
    return !set.IsMutable();
  }
};

class CORE_EXPORT MutableCSSPropertyValueSet : public CSSPropertyValueSet {
 public:
  explicit MutableCSSPropertyValueSet(CSSParserMode);
  explicit MutableCSSPropertyValueSet(const CSSPropertyValueSet&);
  MutableCSSPropertyValueSet(const CSSPropertyValue* properties,
                             unsigned count);
  ~MutableCSSPropertyValueSet() = default;

  unsigned PropertyCount() const { return property_vector_.size(); }

  enum SetResult {
    // The value failed to parse correctly, and thus, there was no change.
    kParseError = 0,

    // The value parsed correctly, but there was no change,
    // as it matched the value already in place.
    kUnchanged = 1,

    // The value parsed correctly, and there was a change to a property that
    // already existed.
    kModifiedExisting = 2,

    // The value parsed correctly, and caused a property to be added or
    // modified. (If you do not care whether it did, you can compare the
    // enum using result >= kModifiedExisting.)
    kChangedPropertySet = 3,
  };

  // Wrapper around SetLonghandProperty() for setting multiple properties
  // at a time.
  SetResult AddParsedProperties(const HeapVector<CSSPropertyValue, 64>&);

  // Wrapper around SetLonghandProperty() that does nothing if the same property
  // already exists with an !important declaration.
  //
  // Returns whether this style set was changed.
  bool AddRespectingCascade(const CSSPropertyValue&);

  // Expands shorthand properties into multiple properties.
  void SetProperty(CSSPropertyID, const CSSValue&, bool important = false);

  // Convenience wrapper around the above that also supports custom properties.
  void SetProperty(const CSSPropertyName&,
                   const CSSValue&,
                   bool important = false);

  // Also a convenience wrapper around SetProperty(), parsing the value from a
  // string before setting it. If the value is empty, the property is removed.
  // Only for non-custom properties.
  SetResult ParseAndSetProperty(
      CSSPropertyID unresolved_property,
      StringView value,
      bool important,
      SecureContextMode,
      StyleSheetContents* context_style_sheet = nullptr);

  // Similar to ParseAndSetProperty(), but for custom properties instead.
  // (By implementation quirk, it attempts shorthand expansion, even though
  // custom properties can never be shorthands.) If the value is empty,
  // the property is removed.
  SetResult ParseAndSetCustomProperty(const AtomicString& custom_property_name,
                                      StringView value,
                                      bool important,
                                      SecureContextMode,
                                      StyleSheetContents* context_style_sheet,
                                      bool is_animation_tainted);

  // This one does not expand longhands, but is the second-most efficient form
  // save for the CSSPropertyID variant below.
  // All the other property setters eventually call down into this.
  SetResult SetLonghandProperty(CSSPropertyValue);

  // A streamlined version of the above, which can be used if you don't need
  // custom properties and don't need the return value (which requires an extra
  // comparison with the old property). This is the fastest form.
  void SetLonghandProperty(CSSPropertyID, const CSSValue&);

  // Convenience form of the CSSPropertyValue overload above.
  SetResult SetLonghandProperty(CSSPropertyID,
                                CSSValueID identifier,
                                bool important = false);

  template <typename T>  // CSSPropertyID or AtomicString
  bool RemoveProperty(const T& property, String* return_text = nullptr);
  bool RemovePropertiesInSet(base::span<const CSSProperty* const> set);
  void RemoveEquivalentProperties(const CSSPropertyValueSet*);
  void RemoveEquivalentProperties(const CSSStyleDeclaration*);

  void MergeAndOverrideOnConflict(const CSSPropertyValueSet*);

  void Clear();
  void ParseDeclarationList(const String& style_declaration,
                            SecureContextMode,
                            StyleSheetContents* context_style_sheet);

  CSSStyleDeclaration* EnsureCSSStyleDeclaration(
      ExecutionContext* execution_context);

  template <typename T>  // CSSPropertyID or AtomicString
  int FindPropertyIndex(const T& property) const;

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  template <typename T>  // CSSPropertyID or AtomicString
  const CSSPropertyValue* FindPropertyPointer(const T& property) const;

  // Returns nullptr if there is no property to be overwritten.
  //
  // If property_id is a logical property we've already seen a different
  // property matching, this will remove the existing property (and return
  // nullptr).
  ALWAYS_INLINE CSSPropertyValue* FindInsertionPointForID(
      CSSPropertyID property_id);

  bool RemovePropertyAtIndex(int, String* return_text);

  bool RemoveShorthandProperty(CSSPropertyID);
  bool RemoveShorthandProperty(const AtomicString& custom_property_name) {
    return false;
  }
  CSSPropertyValue* FindCSSPropertyWithName(const CSSPropertyName&);
  Member<PropertySetCSSStyleDeclaration> cssom_wrapper_;

  friend class CSSPropertyValueSet;

  HeapVector<CSSPropertyValue, 4> property_vector_;
  bool may_have_logical_properties_{false};
};

template <>
struct DowncastTraits<MutableCSSPropertyValueSet> {
  static bool AllowFrom(const CSSPropertyValueSet& set) {
    return set.IsMutable();
  }
};

inline const CSSPropertyValueMetadata&
CSSPropertyValueSet::PropertyReference::PropertyMetadata() const {
  if (auto* mutable_property_set =
          DynamicTo<MutableCSSPropertyValueSet>(property_set_)) {
    return mutable_property_set->property_vector_.at(index_).Metadata();
  }
  return To<ImmutableCSSPropertyValueSet>(*property_set_)
      .MetadataArray()[index_];
}

inline const CSSValue& CSSPropertyValueSet::PropertyReference::PropertyValue()
    const {
  if (auto* mutable_property_set =
          DynamicTo<MutableCSSPropertyValueSet>(property_set_)) {
    return *mutable_property_set->property_vector_.at(index_).Value();
  }
  return *To<ImmutableCSSPropertyValueSet>(*property_set_).ValueArray()[index_];
}

inline unsigned CSSPropertyValueSet::PropertyCount() const {
  if (auto* mutable_property_set =
          DynamicTo<MutableCSSPropertyValueSet>(this)) {
    return mutable_property_set->property_vector_.size();
  }
  return array_size_;
}

inline bool CSSPropertyValueSet::IsEmpty() const {
  return !PropertyCount();
}

template <typename T>
inline int CSSPropertyValueSet::FindPropertyIndex(const T& property) const {
  if (auto* mutable_property_set =
          DynamicTo<MutableCSSPropertyValueSet>(this)) {
    return mutable_property_set->FindPropertyIndex(property);
  }
  return To<ImmutableCSSPropertyValueSet>(this)->FindPropertyIndex(property);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_VALUE_SET_H_
