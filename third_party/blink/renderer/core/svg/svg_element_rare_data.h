/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ELEMENT_RARE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ELEMENT_RARE_DATA_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ElementSMILAnimations;
class SVGElementResourceClient;

class SVGElementRareData final : public GarbageCollected<SVGElementRareData> {
 public:
  SVGElementRareData()
      : corresponding_element_(nullptr),
        instances_updates_blocked_(false),
        needs_override_computed_style_update_(false),
        web_animated_attributes_dirty_(false) {}

  SVGElementSet& OutgoingReferences() { return outgoing_references_; }
  const SVGElementSet& OutgoingReferences() const {
    return outgoing_references_;
  }
  SVGElementSet& IncomingReferences() { return incoming_references_; }
  const SVGElementSet& IncomingReferences() const {
    return incoming_references_;
  }

  HeapHashSet<WeakMember<SVGElement>>& ElementInstances() {
    return element_instances_;
  }
  const HeapHashSet<WeakMember<SVGElement>>& ElementInstances() const {
    return element_instances_;
  }

  bool InstanceUpdatesBlocked() const { return instances_updates_blocked_; }
  void SetInstanceUpdatesBlocked(bool value) {
    instances_updates_blocked_ = value;
  }

  SVGElement* CorrespondingElement() const {
    return corresponding_element_.Get();
  }
  void SetCorrespondingElement(SVGElement* corresponding_element) {
    corresponding_element_ = corresponding_element;
  }

  void SetWebAnimatedAttributesDirty(bool dirty) {
    web_animated_attributes_dirty_ = dirty;
  }
  bool WebAnimatedAttributesDirty() const {
    return web_animated_attributes_dirty_;
  }

  HashSet<QualifiedName>& WebAnimatedAttributes() {
    return web_animated_attributes_;
  }

  ElementSMILAnimations* GetSMILAnimations() { return smil_animations_; }
  ElementSMILAnimations& EnsureSMILAnimations();

  MutableCSSPropertyValueSet* AnimatedSMILStyleProperties() const {
    return animated_smil_style_properties_.Get();
  }
  MutableCSSPropertyValueSet* EnsureAnimatedSMILStyleProperties();

  const ComputedStyle* OverrideComputedStyle(Element*, const ComputedStyle*);
  void ClearOverriddenComputedStyle();

  void SetNeedsOverrideComputedStyleUpdate() {
    needs_override_computed_style_update_ = true;
  }

  SVGElementResourceClient* GetSVGResourceClient() { return resource_client_; }
  SVGElementResourceClient& EnsureSVGResourceClient(SVGElement*);

  AffineTransform* AnimateMotionTransform();

  void Trace(Visitor*) const;

 private:
  SVGElementSet outgoing_references_;
  SVGElementSet incoming_references_;
  HeapHashSet<WeakMember<SVGElement>> element_instances_;
  Member<SVGElement> corresponding_element_;
  Member<SVGElementResourceClient> resource_client_;
  Member<ElementSMILAnimations> smil_animations_;
  bool instances_updates_blocked_ : 1;
  bool needs_override_computed_style_update_ : 1;
  bool web_animated_attributes_dirty_ : 1;
  HashSet<QualifiedName> web_animated_attributes_;
  Member<MutableCSSPropertyValueSet> animated_smil_style_properties_;
  scoped_refptr<ComputedStyle> override_computed_style_;
  // Used by <animateMotion>
  AffineTransform animate_motion_transform_;
  DISALLOW_COPY_AND_ASSIGN(SVGElementRareData);
};

}  // namespace blink

#endif
