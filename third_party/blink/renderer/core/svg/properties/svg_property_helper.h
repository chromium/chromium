// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROPERTIES_SVG_PROPERTY_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROPERTIES_SVG_PROPERTY_HELPER_H_

#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace WTF {
class String;
}  // namespace WTF

namespace blink {

template <typename Derived>
class SVGPropertyHelper : public SVGPropertyBase {
 public:
  SVGPropertyBase* CloneForAnimation(const WTF::String& value) const override {
    auto* property = MakeGarbageCollected<Derived>();
    property->SetValueAsString(value);
    return property;
  }

  AnimatedPropertyType GetType() const override { return Derived::ClassType(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROPERTIES_SVG_PROPERTY_HELPER_H_
