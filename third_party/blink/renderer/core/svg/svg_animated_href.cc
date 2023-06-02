// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_animated_href.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

void SVGAnimatedHref::Trace(Visitor* visitor) const {
  visitor->Trace(xlink_href_);
  SVGAnimatedString::Trace(visitor);
}

SVGAnimatedHref::SVGAnimatedHref(SVGElement* context_element)
    : SVGAnimatedString(context_element, svg_names::kHrefAttr),
      xlink_href_(
          MakeGarbageCollected<SVGAnimatedString>(context_element,
                                                  xlink_names::kHrefAttr)) {}

SVGAnimatedPropertyBase* SVGAnimatedHref::PropertyFromAttribute(
    const QualifiedName& attribute_name) {
  if (attribute_name == svg_names::kHrefAttr) {
    return this;
  } else if (attribute_name.Matches(xlink_names::kHrefAttr)) {
    return xlink_href_.Get();
  } else {
    return nullptr;
  }
}

bool SVGAnimatedHref::IsKnownAttribute(const QualifiedName& attr_name) {
  return attr_name.Matches(svg_names::kHrefAttr) ||
         attr_name.Matches(xlink_names::kHrefAttr);
}

SVGString* SVGAnimatedHref::CurrentValue() {
  return BackingString()->SVGAnimatedString::CurrentValue();
}

const SVGString* SVGAnimatedHref::CurrentValue() const {
  return BackingString()->SVGAnimatedString::CurrentValue();
}

V8UnionStringOrTrustedScriptURL* SVGAnimatedHref::baseVal() {
  UseCounter::Count(ContextElement()->GetDocument(),
                    WebFeature::kSVGHrefBaseVal);
  return BackingString()->SVGAnimatedString::baseVal();
}

void SVGAnimatedHref::setBaseVal(const V8UnionStringOrTrustedScriptURL* value,
                                 ExceptionState& exception_state) {
  UseCounter::Count(ContextElement()->GetDocument(),
                    WebFeature::kSVGHrefBaseVal);
  BackingString()->SVGAnimatedString::setBaseVal(value, exception_state);
}

String SVGAnimatedHref::animVal() {
  UseCounter::Count(ContextElement()->GetDocument(),
                    WebFeature::kSVGHrefAnimVal);
  return BackingString()->SVGAnimatedString::animVal();
}

SVGAnimatedString* SVGAnimatedHref::BackingString() {
  return UseXLink() ? xlink_href_.Get() : this;
}

const SVGAnimatedString* SVGAnimatedHref::BackingString() const {
  return UseXLink() ? xlink_href_.Get() : this;
}

bool SVGAnimatedHref::UseXLink() const {
  return !SVGAnimatedString::IsSpecified() && xlink_href_->IsSpecified();
}

}  // namespace blink
