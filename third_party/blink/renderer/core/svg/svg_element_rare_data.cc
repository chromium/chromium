// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"

#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/svg/animation/element_smil_animations.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ElementSMILAnimations& SVGElementRareData::EnsureSMILAnimations() {
  if (!smil_animations_)
    smil_animations_ = MakeGarbageCollected<ElementSMILAnimations>();
  return *smil_animations_;
}

MutableCSSPropertyValueSet*
SVGElementRareData::EnsureAnimatedSMILStyleProperties() {
  if (!animated_smil_style_properties_) {
    animated_smil_style_properties_ =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kSVGAttributeMode);
  }
  return animated_smil_style_properties_.Get();
}

const ComputedStyle* SVGElementRareData::OverrideComputedStyle(
    Element* element,
    const ComputedStyle* parent_style) {
  DCHECK(element);
  if (!override_computed_style_ || needs_override_computed_style_update_) {
    auto style_recalc_context = StyleRecalcContext::FromAncestors(*element);
    style_recalc_context.old_style =
        PostStyleUpdateScope::GetOldStyle(*element);

    StyleRequest style_request;
    style_request.parent_override = parent_style;
    style_request.layout_parent_override = parent_style;
    style_request.matching_behavior = kMatchAllRulesExcludingSMIL;
    style_request.can_trigger_animations = false;

    // The style computed here contains no CSS Animations/Transitions or SMIL
    // induced rules - this is needed to compute the "base value" for the SMIL
    // animation sandwhich model.
    element->GetDocument().GetStyleEngine().UpdateViewportSize();
    override_computed_style_ =
        element->GetDocument().GetStyleResolver().ResolveStyle(
            element, style_recalc_context, style_request);
    needs_override_computed_style_update_ = false;
  }
  DCHECK(override_computed_style_);
  return override_computed_style_.Get();
}

void SVGElementRareData::ClearOverriddenComputedStyle() {
  override_computed_style_ = nullptr;
}

SVGElementResourceClient& SVGElementRareData::EnsureSVGResourceClient(
    SVGElement* element) {
  if (!resource_client_)
    resource_client_ = MakeGarbageCollected<SVGElementResourceClient>(element);
  return *resource_client_;
}

SVGResourceTarget& SVGElementRareData::EnsureResourceTarget(
    SVGElement& element) {
  if (!resource_target_) {
    resource_target_ = MakeGarbageCollected<SVGResourceTarget>();
    resource_target_->target = element;
  }
  return *resource_target_;
}

bool SVGElementRareData::HasResourceTarget() const {
  return resource_target_;
}

void SVGElementRareData::Trace(Visitor* visitor) const {
  visitor->Trace(outgoing_references_);
  visitor->Trace(incoming_references_);
  visitor->Trace(animated_smil_style_properties_);
  visitor->Trace(override_computed_style_);
  visitor->Trace(element_instances_);
  visitor->Trace(corresponding_element_);
  visitor->Trace(resource_client_);
  visitor->Trace(smil_animations_);
  visitor->Trace(resource_target_);
}

AffineTransform* SVGElementRareData::AnimateMotionTransform() {
  return &animate_motion_transform_;
}

}  // namespace blink
