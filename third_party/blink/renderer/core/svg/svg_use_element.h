/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_USE_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_USE_ELEMENT_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/document_resource.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_geometry_element.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class SVGUseElement final : public SVGGraphicsElement,
                            public SVGURIReference,
                            public ResourceClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGUseElement);
  USING_PRE_FINALIZER(SVGUseElement, Dispose);

 public:
  explicit SVGUseElement(Document&);
  ~SVGUseElement() override;

  void InvalidateShadowTree();

  // Return the element that should be used for clipping,
  // or null if a valid clip element is not directly referenced.
  SVGGraphicsElement* VisibleTargetGraphicsElementForClipping() const;

  SVGAnimatedLength* x() const { return x_.Get(); }
  SVGAnimatedLength* y() const { return y_.Get(); }
  SVGAnimatedLength* width() const { return width_.Get(); }
  SVGAnimatedLength* height() const { return height_.Get(); }

  void BuildPendingResource() override;
  String title() const override;

  void DispatchPendingEvent();
  Path ToClipPath() const;

  void Trace(blink::Visitor*) override;

 private:
  void Dispose();

  FloatRect GetBBox() override;

  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;

  bool IsStructurallyExternal() const override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  void SvgAttributeChanged(const QualifiedName&) override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;

  void ScheduleShadowTreeRecreation();
  void CancelShadowTreeRecreation();
  bool HaveLoadedRequiredResources() override {
    return !IsStructurallyExternal() || have_fired_load_event_;
  }
  bool ShadowTreeRebuildPending() const;

  bool SelfHasRelativeLengths() const override;

  ShadowRoot& UseShadowRoot() const {
    CHECK(ClosedShadowRoot());
    return *ClosedShadowRoot();
  }

  // Instance tree handling
  enum ObserveBehavior {
    kAddObserver,
    kDontAddObserver,
  };
  Element* ResolveTargetElement(ObserveBehavior);
  void AttachShadowTree(SVGElement& target);
  void DetachShadowTree();
  CORE_EXPORT SVGElement* InstanceRoot() const;
  SVGElement* CreateInstanceTree(SVGElement& target_root) const;
  void ClearResourceReference();
  bool HasCycleUseReferencing(const ContainerNode& target_instance,
                              const SVGElement& new_target) const;
  void ExpandUseElementsInShadowTree();
  void AddReferencesToFirstDegreeNestedUseElements(SVGElement& target);

  void InvalidateDependentShadowTrees();

  bool ResourceIsValid() const;
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "SVGUseElement"; }
  void UpdateTargetReference();

  Member<SVGAnimatedLength> x_;
  Member<SVGAnimatedLength> y_;
  Member<SVGAnimatedLength> width_;
  Member<SVGAnimatedLength> height_;

  KURL element_url_;
  bool element_url_is_local_;
  bool have_fired_load_event_;
  bool needs_shadow_tree_recreation_;
  Member<IdTargetObserver> target_id_observer_;

  FRIEND_TEST_ALL_PREFIXES(SVGUseElementTest,
                           NullInstanceRootWhenNotConnectedToDocument);
  FRIEND_TEST_ALL_PREFIXES(SVGUseElementTest,
                           NullInstanceRootWhenConnectedToInactiveDocument);
  FRIEND_TEST_ALL_PREFIXES(SVGUseElementTest,
                           NullInstanceRootWhenShadowTreePendingRebuild);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_USE_ELEMENT_H_
