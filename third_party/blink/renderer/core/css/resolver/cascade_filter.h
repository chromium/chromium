// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_FILTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

// Pass only properties with the given flags set.
//
// For example, the following applies only inherited properties:
//
//  CascadeFilter filter;
//  filter = filter.Add(CSSProperty::kInherited);
//  filter.Accepts(GetCSSPropertyColor());            // -> true
//  filter.Accepts(GetCSSPropertyScrollbarGutter());  // -> false
//
class CORE_EXPORT CascadeFilter {
 public:
  // Empty filter. Rejects nothing.
  CascadeFilter() = default;

  // Creates a filter with a single rule.
  //
  // This is equivalent to:
  //
  //  CascadeFilter filter;
  //  filter = filter.Add(flag, v);
  //
  explicit CascadeFilter(CSSProperty::Flag flag) : required_bits_(flag) {}

  bool operator==(const CascadeFilter& o) const {
    return required_bits_ == o.required_bits_;
  }

  // Add a given rule to the filter. For instance:
  //
  //  CascadeFilter f1(CSSProperty::kInherited); // Rejects non-inherited
  //
  // Note that it is not possible to reject on a negative. However, some flags
  // have deliberately inverted flag (e.g. every property has exactly one of
  // kAnimated and kNotAnimated). If you wish to reject all properties, you
  // can do so by testing on both of the flags at the same time.
  //
  // Add() will have no effect if there already is a rule for the given flag:
  //
  //  CascadeFilter filter;
  //  CascadeFilter f1 = filter.Add(CSSProperty::kInherited);
  //  CascadeFilter f2 = f1.Add(CSSProperty::kInherited);
  //  bool equal = f1 == f2; // true. Second call to Add had to effect.
  CascadeFilter Add(CSSProperty::Flag flag) const {
    const CSSProperty::Flags required_bits = required_bits_ | flag;
    return CascadeFilter(required_bits);
  }

  bool Accepts(const CSSProperty& property) const {
    return (property.GetFlags() & required_bits_) == required_bits_;
  }

  bool Requires(CSSProperty::Flag flag) const {
    return (required_bits_ & flag) != 0;
  }

  bool IsEmpty() const { return required_bits_ == 0; }

 private:
  explicit CascadeFilter(CSSProperty::Flags required_bits)
      : required_bits_(required_bits) {}
  // Contains the flags to require.
  CSSProperty::Flags required_bits_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_FILTER_H_
