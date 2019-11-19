// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATED_HREF_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATED_HREF_H_

#include "third_party/blink/renderer/core/svg/svg_animated_string.h"

namespace blink {

// This is an "access wrapper" for the 'href' attribute. The object
// itself holds the value for 'href' in the null/default NS and wraps
// one for 'href' in the XLink NS. Both objects are added to an
// SVGElement's property map and hence any updates/synchronization/etc
// via the "attribute DOM" (setAttribute and friends) will operate on
// the independent objects, while users of an 'href' value will be
// using this interface (which essentially just selects either itself
// or the wrapped object and forwards the operation to it.)
class SVGAnimatedHref final : public SVGAnimatedString {
 public:
  explicit SVGAnimatedHref(SVGElement* context_element);

  SVGString* CurrentValue();
  const SVGString* CurrentValue() const;

  String baseVal() override;
  void setBaseVal(const String&, ExceptionState&) override;
  String animVal() override;

  bool IsSpecified() const {
    return SVGAnimatedString::IsSpecified() || xlink_href_->IsSpecified();
  }

  static bool IsKnownAttribute(const QualifiedName&);
  void AddToPropertyMap(SVGElement*);

  void Trace(blink::Visitor*) override;

 private:
  SVGAnimatedString* BackingString();
  const SVGAnimatedString* BackingString() const;
  bool UseXLink() const;

  Member<SVGAnimatedString> xlink_href_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATED_HREF_H_
