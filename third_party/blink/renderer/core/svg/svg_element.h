/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009, 2014 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ELEMENT_H_

#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/properties/svg_property_info.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class AffineTransform;
class Document;
class ElementSMILAnimations;
class ExecutionContext;
class SVGAnimatedPropertyBase;
class SVGAnimatedString;
class SVGElement;
class SVGElementRareData;
class SVGElementResourceClient;
class SVGPropertyBase;
class SVGSVGElement;
class SVGUseElement;

typedef HeapHashSet<Member<SVGElement>> SVGElementSet;

// Structure for referencing/tracking a "resource target" (an element in an
// external resource document that is targeted by a <use> element).
struct SVGResourceTarget : public GarbageCollected<SVGResourceTarget> {
  void Trace(Visitor* visitor) const { visitor->Trace(target); }

  Member<SVGElement> target;
};

class CORE_EXPORT SVGElement : public Element {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~SVGElement() override;

  bool IsOutermostSVGSVGElement() const;

  bool HasTagName(const SVGQualifiedName& name) const {
    return HasLocalName(name.LocalName());
  }

  String title() const override;
  bool HasRelativeLengths() const {
    return !elements_with_relative_lengths_.empty();
  }
  static bool IsAnimatableCSSProperty(const QualifiedName&);

  bool HasMotionTransform() const { return HasSVGRareData(); }
  // Apply any "motion transform" contribution (if existing.)
  void ApplyMotionTransform(AffineTransform&) const;

  enum ApplyMotionTransformTag {
    kExcludeMotionTransform,
    kIncludeMotionTransform
  };
  bool HasTransform(ApplyMotionTransformTag) const;
  AffineTransform CalculateTransform(ApplyMotionTransformTag) const;

  enum CTMScope {
    kNearestViewportScope,  // Used by SVGGraphicsElement::getCTM()
    kScreenScope,           // Used by SVGGraphicsElement::getScreenCTM()
    kAncestorScope  // Used by SVGSVGElement::get{Enclosure|Intersection}List()
  };
  virtual AffineTransform LocalCoordinateSpaceTransform(CTMScope) const;

  // Records the SVG element as having a Web Animation on an SVG attribute that
  // needs applying.
  void SetWebAnimationsPending();
  void ApplyActiveWebAnimations();

  void BaseValueChanged(const SVGAnimatedPropertyBase&);
  void EnsureAttributeAnimValUpdated();

  void SetWebAnimatedAttribute(const QualifiedName& attribute,
                               SVGPropertyBase*);
  void ClearWebAnimatedAttributes();

  ElementSMILAnimations* GetSMILAnimations() const;
  ElementSMILAnimations& EnsureSMILAnimations();
  const ComputedStyle* BaseComputedStyleForSMIL();

  void SetAnimatedAttribute(const QualifiedName&, SVGPropertyBase*);
  void ClearAnimatedAttribute(const QualifiedName&);
  void SetAnimatedMotionTransform(const AffineTransform&);
  void ClearAnimatedMotionTransform();

  bool HasNonCSSPropertyAnimations() const;

  SVGSVGElement* ownerSVGElement() const;
  SVGElement* viewportElement() const;

  virtual bool IsSVGGeometryElement() const { return false; }
  virtual bool IsSVGGraphicsElement() const { return false; }
  virtual bool IsFilterEffect() const { return false; }
  virtual bool IsTextContent() const { return false; }
  virtual bool IsTextPositioning() const { return false; }
  virtual bool IsStructurallyExternal() const { return false; }

  // For SVGTests
  virtual bool IsValid() const { return true; }

  struct SvgAttributeChangedParams {
    STACK_ALLOCATED();

   public:
    const SVGAnimatedPropertyBase& property;
    const QualifiedName& name;
    const AttributeModificationReason reason;
  };
  virtual void SvgAttributeChanged(const SvgAttributeChangedParams&);

  virtual SVGAnimatedPropertyBase* PropertyFromAttribute(
      const QualifiedName& attribute_name) const;
  static AnimatedPropertyType AnimatedPropertyTypeForCSSAttribute(
      const QualifiedName& attribute_name);

  void SendSVGLoadEventToSelfAndAncestorChainIfPossible();
  bool SendSVGLoadEventIfPossible();

  virtual AffineTransform* AnimateMotionTransform() { return nullptr; }

  void InvalidateSVGPresentationAttributeStyle() {
    EnsureUniqueElementData().SetPresentationAttributeStyleIsDirty(true);
  }

  const HeapHashSet<WeakMember<SVGElement>>& InstancesForElement() const;
  void AddInstance(SVGElement*);
  void RemoveInstance(SVGElement*);

  SVGElement* CorrespondingElement() const;
  void SetCorrespondingElement(SVGElement*);
  SVGUseElement* GeneratingUseElement() const;

  SVGResourceTarget& EnsureResourceTarget();
  bool IsResourceTarget() const;

  void SynchronizeSVGAttribute(const QualifiedName&) const;
  virtual void SynchronizeAllSVGAttributes() const;

  const ComputedStyle* CustomStyleForLayoutObject(
      const StyleRecalcContext&) final;
  bool LayoutObjectIsNeeded(const DisplayStyle&) const override;

#if DCHECK_IS_ON()
  virtual bool IsAnimatableAttribute(const QualifiedName&) const;
#endif

  MutableCSSPropertyValueSet* AnimatedSMILStyleProperties() const;
  MutableCSSPropertyValueSet* EnsureAnimatedSMILStyleProperties();

  virtual void BuildPendingResource() {}
  virtual bool HaveLoadedRequiredResources();

  void InvalidateRelativeLengthClients();

  SVGAnimatedString* className() { return class_name_.Get(); }

  bool InUseShadowTree() const;

  void AddReferenceTo(SVGElement*);
  template <typename InvalidationFunction>
  void NotifyIncomingReferences(InvalidationFunction&&);
  void RemoveAllIncomingReferences();
  void RemoveAllOutgoingReferences();

  SVGElementResourceClient* GetSVGResourceClient();
  SVGElementResourceClient& EnsureSVGResourceClient();

  void SetNeedsStyleRecalcForInstances(StyleChangeType,
                                       const StyleChangeReasonForTracing&);

  void Trace(Visitor*) const override;

  static const AtomicString& EventParameterName();

  bool IsPresentationAttribute(const QualifiedName&) const override;

  bool HasSVGParent() const;

  // Utility function for implementing SynchronizeAllSVGAttributes() in
  // subclasses (and mixins such as SVGTests).
  static void SynchronizeListOfSVGAttributes(
      const base::span<SVGAnimatedPropertyBase*> attributes);

  bool HasFocusEventListeners() const;

 protected:
  SVGElement(const QualifiedName&,
             Document&,
             ConstructionType = kCreateSVGElement);

  void ParseAttribute(const AttributeModificationParams&) override;
  void AttributeChanged(const AttributeModificationParams&) override;
  void InvalidateInstances();

  void UpdatePresentationAttributeStyle(const SVGAnimatedPropertyBase&);
  void UpdatePresentationAttributeStyle(CSSPropertyID,
                                        const QualifiedName&,
                                        const AtomicString& value);
  MutableCSSPropertyValueSet* GetPresentationAttributeStyleForDirectUpdate();
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  void AddPropertyToPresentationAttributeStyleWithCache(
      MutableCSSPropertyValueSet*,
      CSSPropertyID,
      const AtomicString& value);
  void AddAnimatedPropertyToPresentationAttributeStyle(
      const SVGAnimatedPropertyBase& property,
      MutableCSSPropertyValueSet* style);
  void AddAnimatedPropertiesToPresentationAttributeStyle(
      const base::span<const SVGAnimatedPropertyBase*> properties,
      MutableCSSPropertyValueSet* style);

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void ChildrenChanged(const ChildrenChange&) override;

  void DetachLayoutTree(bool performing_reattach) override;

  static CSSPropertyID CssPropertyIdForSVGAttributeName(const ExecutionContext*,
                                                        const QualifiedName&);
  void UpdateRelativeLengthsInformation() {
    UpdateRelativeLengthsInformation(SelfHasRelativeLengths(), this);
  }
  void UpdateRelativeLengthsInformation(bool has_relative_lengths, SVGElement*);
  static void MarkForLayoutAndParentResourceInvalidation(LayoutObject&);
  void NotifyResourceClients() const;

  virtual bool SelfHasRelativeLengths() const { return false; }

  SVGElementSet* SetOfIncomingReferences() const;

  SVGElementRareData* EnsureSVGRareData();
  inline bool HasSVGRareData() const { return svg_rare_data_ != nullptr; }
  inline SVGElementRareData* SvgRareData() const {
    DCHECK(svg_rare_data_);
    return svg_rare_data_.Get();
  }

  void ReportAttributeParsingError(SVGParsingError,
                                   const QualifiedName&,
                                   const AtomicString&);
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) final;

  void AccessKeyAction(SimulatedClickCreationScope creation_scope) override;

  void AttachLayoutTree(AttachContext&) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SVGElementTest,
                           BaseComputedStyleForSMILWithContainerQueries);

  bool IsSVGElement() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsStyledElement() const =
      delete;  // This will catch anyone doing an unnecessary check.

  FocusableState SupportsFocus(UpdateBehavior) const override {
    return FocusableState::kNotFocusable;
  }

  void WillRecalcStyle(const StyleRecalcChange) override;
  static SVGElementSet& GetDependencyTraversalVisitedSet();
  void UpdateWebAnimatedAttributeOnBaseValChange(
      const SVGAnimatedPropertyBase&);

  SMILTimeContainer* GetTimeContainer() const;

  HeapHashSet<WeakMember<SVGElement>> elements_with_relative_lengths_;

#if DCHECK_IS_ON()
  bool in_relative_length_clients_invalidation_ = false;
#endif

  Member<SVGElementRareData> svg_rare_data_;
  Member<SVGAnimatedString> class_name_;
};

template <typename InvalidationFunction>
void SVGElement::NotifyIncomingReferences(
    InvalidationFunction&& invalidation_function) {
  SVGElementSet* dependencies = SetOfIncomingReferences();
  if (!dependencies)
    return;

  // We allow cycles in the reference graph in order to avoid expensive
  // adjustments on changes, so we need to break possible cycles here.
  SVGElementSet& invalidating_dependencies = GetDependencyTraversalVisitedSet();

  for (auto& member : *dependencies) {
    auto* element = member.Get();
    if (!element->GetLayoutObject())
      continue;
    if (!invalidating_dependencies.insert(element).is_new_entry) [[unlikely]] {
      // Reference cycle: we are in process of invalidating this dependant.
      continue;
    }
    invalidation_function(*element);
    invalidating_dependencies.erase(element);
  }
}

struct SVGAttributeHashTranslator {
  STATIC_ONLY(SVGAttributeHashTranslator);
  static unsigned GetHash(const QualifiedName& key) {
    if (key.HasPrefix()) {
      QualifiedNameComponents components = {g_null_atom.Impl(),
                                            key.LocalName().Impl(),
                                            key.NamespaceURI().Impl()};
      return HashComponents(components);
    }
    return WTF::GetHash(key);
  }
  static bool Equal(const QualifiedName& a, const QualifiedName& b) {
    return a.Matches(b);
  }
};

template <typename T>
bool IsElementOfType(const SVGElement&);
template <>
inline bool IsElementOfType<const SVGElement>(const SVGElement&) {
  return true;
}
template <>
inline bool IsElementOfType<const SVGElement>(const Node& node) {
  return IsA<SVGElement>(node);
}
template <>
struct DowncastTraits<SVGElement> {
  static bool AllowFrom(const Node& node) { return node.IsSVGElement(); }
};
inline bool Node::HasTagName(const SVGQualifiedName& name) const {
  auto* svg_element = DynamicTo<SVGElement>(this);
  return svg_element && svg_element->HasTagName(name);
}

}  // namespace blink

#include "third_party/blink/renderer/core/svg_element_type_helpers.h"

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ELEMENT_H_
