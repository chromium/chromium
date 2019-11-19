// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

MutableCSSPropertyValueSet*
SVGElementRareData::EnsureAnimatedSMILStyleProperties() {
  if (!animated_smil_style_properties_) {
    animated_smil_style_properties_ =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kSVGAttributeMode);
  }
  return animated_smil_style_properties_.Get();
}

ComputedStyle* SVGElementRareData::OverrideComputedStyle(
    Element* element,
    const ComputedStyle* parent_style) {
  DCHECK(element);
  if (!use_override_computed_style_)
    return nullptr;
  if (!override_computed_style_ || needs_override_computed_style_update_) {
    // The style computed here contains no CSS Animations/Transitions or SMIL
    // induced rules - this is needed to compute the "base value" for the SMIL
    // animation sandwhich model.
    override_computed_style_ =
        element->GetDocument().EnsureStyleResolver().StyleForElement(
            element, parent_style, parent_style, kMatchAllRulesExcludingSMIL);
    needs_override_computed_style_update_ = false;
  }
  DCHECK(override_computed_style_);
  return override_computed_style_.get();
}

void SVGElementRareData::ClearOverriddenComputedStyle() {
  override_computed_style_ = nullptr;
}

SVGResourceClient& SVGElementRareData::EnsureSVGResourceClient(
    SVGElement* element) {
  if (!resource_client_)
    resource_client_ = MakeGarbageCollected<SVGElementResourceClient>(element);
  return *resource_client_;
}

void SVGElementRareData::Trace(blink::Visitor* visitor) {
  visitor->Trace(outgoing_references_);
  visitor->Trace(incoming_references_);
  visitor->Trace(animated_smil_style_properties_);
  visitor->Trace(element_instances_);
  visitor->Trace(corresponding_element_);
  visitor->Trace(resource_client_);
}

AffineTransform* SVGElementRareData::AnimateMotionTransform() {
  return &animate_motion_transform_;
}

}  // namespace blink
