// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_EXPANSION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_EXPANSION_H_

#include <limits>
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_origin.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"

namespace blink {

struct MatchedProperties;

inline uint32_t EncodeMatchResultPosition(uint16_t block,
                                          uint16_t declaration) {
  return (static_cast<uint32_t>(block) << 16) | declaration;
}

inline size_t DecodeMatchedPropertiesIndex(uint32_t position) {
  return (position >> 16) & 0xFFFF;
}

inline size_t DecodeDeclarationIndex(uint32_t position) {
  return position & 0xFFFF;
}

// CascadeExpansion takes a declaration block (MatchedProperties) and
// expands the declarations found into the final list of declarations observed
// by StyleCascade. It exists to prevent callers to deal with the complexity
// of the 'all' property, '-internal-visited-' properties, '-internal-ua-'
// properties, and filtering of both regular declarations and "generated"
// declarations.
//
// For example, for the declaration block:
//
//   top:1px;
//   all:unset;
//   top:2px;
//
// CascadeExpansion would emit:
//
//   top:1px;
//   animation-delay:unset;
//   animation-direction:unset;
//   /* ... <all longhands affected by 'all'> ... */
//   -webkit-text-emphasis:unset;
//   -webkit-text-stroke:unset;
//   top:2px;
//
// In other words, 'all' is expanded into the actual longhands it represents.
// A similar expansion happens for properties which have companion
// -internal-visited-* properties (depending on inside-link status).
//
// Usage:
//
//   CascadeExpansion e = ...;
//   for (; !e.AtEnd(); a.Next())
//     DoStuff(e);
//
class CORE_EXPORT CascadeExpansion {
  STACK_ALLOCATED();

  enum class State { kInit, kNormal, kVisited, kAll };

 public:
  // CascadeExpansion objects which exceed these limits will emit nothing.
  static constexpr size_t kMaxDeclarationIndex =
      std::numeric_limits<uint16_t>::max();
  static constexpr size_t kMaxMatchedPropertiesIndex =
      std::numeric_limits<uint16_t>::max();

  CascadeExpansion(const MatchedProperties&,
                   const Document&,
                   CascadeFilter,
                   size_t matched_properties_index);
  // We need an explicit copy constructor, since CascadeExpansion has self-
  // pointers.
  CascadeExpansion(const CascadeExpansion& o);
  void Next();
  inline bool AtEnd() const { return index_ >= size_; }
  inline CSSPropertyID Id() const { return id_; }
  inline CSSPropertyName Name() const {
    if (id_ != CSSPropertyID::kVariable)
      return CSSPropertyName(id_);
    return Property().GetCSSPropertyName();
  }
  inline const CSSProperty& Property() const {
    DCHECK(!AtEnd());
    return *property_;
  }
  inline const CSSValue& Value() const {
    DCHECK(!AtEnd());
    return PropertyAt(index_).Value();
  }
  inline CascadePriority Priority() const { return priority_; }

 private:
  static bool IsAffectedByAll(CSSPropertyID);

  bool ShouldEmitVisited() const;

  void AdvanceNormal();
  bool AdvanceVisited();
  void AdvanceAll();

  CSSPropertyValueSet::PropertyReference PropertyAt(size_t) const;

  const Document& document_;
  State state_ = State::kInit;
  const MatchedProperties& matched_properties_;

  // The priority of the current declaration pointed to by index_. This does
  // not change for generated declarations.
  CascadePriority priority_;

  // Index and size of the regular declarations. In other words, index_ will
  // only move during State::kNormal, and not while expanding 'all', etc. It
  // will always point to a valid index in matched_properties_ (unless we're
  // AtEnd()).
  //
  // Note that this is initialized to ~0 such that the first call to Next()
  // (done by the constructor) will produce ~0+1 = 0.
  size_t index_ = std::numeric_limits<size_t>::max();
  size_t size_;

  CascadeFilter filter_;
  const size_t matched_properties_index_;

  // The id/property of the current "virtual" declaration. In other words,
  // the id/property will be updated when expanding 'all', etc.
  CSSPropertyID id_ = CSSPropertyID::kInvalid;
  const CSSProperty* property_ = nullptr;
  CustomProperty custom_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_EXPANSION_H_
