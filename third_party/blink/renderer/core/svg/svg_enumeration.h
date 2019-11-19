/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ENUMERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ENUMERATION_H_

#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class SVGEnumerationMap;

class SVGEnumerationBase : public SVGPropertyBase {
 public:
  // SVGEnumeration does not have a tear-off type.
  typedef void TearOffType;
  typedef uint16_t PrimitiveType;

  ~SVGEnumerationBase() override;

  uint16_t Value() const {
    return value_ <= MaxExposedEnumValue() ? value_ : 0;
  }
  void SetValue(uint16_t);

  // SVGPropertyBase:
  virtual SVGEnumerationBase* Clone() const = 0;
  SVGPropertyBase* CloneForAnimation(const String&) const override;

  String ValueAsString() const override;
  SVGParsingError SetValueAsString(const String&);

  void Add(SVGPropertyBase*, SVGElement*) override;
  void CalculateAnimatedValue(const SVGAnimateElement&,
                              float percentage,
                              unsigned repeat_count,
                              SVGPropertyBase* from,
                              SVGPropertyBase* to,
                              SVGPropertyBase* to_at_end_of_duration_value,
                              SVGElement*) override;
  float CalculateDistance(SVGPropertyBase* to, SVGElement*) override;

  static AnimatedPropertyType ClassType() { return kAnimatedEnumeration; }
  AnimatedPropertyType GetType() const override { return ClassType(); }

  // This is the maximum value that is exposed as an IDL constant on the
  // relevant interface.
  uint16_t MaxExposedEnumValue() const;

  void SetInitial(unsigned value) { SetValue(static_cast<uint16_t>(value)); }
  static constexpr int kInitialValueBits = 3;

 protected:
  SVGEnumerationBase(uint16_t value, const SVGEnumerationMap& map)
      : value_(value), map_(map) {}

  // This is the maximum value of all the internal enumeration values.
  // This assumes that the map is sorted on the enumeration value.
  uint16_t MaxInternalEnumValue() const;

  // Used by SVGMarkerOrientEnumeration.
  virtual void NotifyChange() {}

  uint16_t value_;
  const SVGEnumerationMap& map_;
};

template <typename Enum>
const SVGEnumerationMap& GetEnumerationMap();

#define DECLARE_SVG_ENUM_MAP(cpp_enum_type) \
  template <>                               \
  const SVGEnumerationMap& GetEnumerationMap<cpp_enum_type>()

template <typename Enum>
class SVGEnumeration : public SVGEnumerationBase {
 public:
  explicit SVGEnumeration(Enum new_value)
      : SVGEnumerationBase(new_value, GetEnumerationMap<Enum>()) {}
  ~SVGEnumeration() override = default;

  SVGEnumerationBase* Clone() const override {
    return MakeGarbageCollected<SVGEnumeration>(EnumValue());
  }

  Enum EnumValue() const {
    DCHECK_LE(value_, MaxInternalEnumValue());
    return static_cast<Enum>(value_);
  }

  void SetEnumValue(Enum value) {
    value_ = value;
    NotifyChange();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ENUMERATION_H_
