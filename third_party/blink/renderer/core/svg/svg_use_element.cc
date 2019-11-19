/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Renata Hodovan <reni@webkit.org>
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

#include "third_party/blink/renderer/core/svg/svg_use_element.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/core/svg/svg_title_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

SVGUseElement::SVGUseElement(Document& document)
    : SVGGraphicsElement(svg_names::kUseTag, document),
      SVGURIReference(this),
      x_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kXAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kX)),
      y_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kYAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kY)),
      width_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kWidthAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero)),
      element_url_is_local_(true),
      have_fired_load_event_(false),
      needs_shadow_tree_recreation_(false) {
  DCHECK(HasCustomStyleCallbacks());

  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);

  AttachShadowRootInternal(ShadowRootType::kClosed);
}

SVGUseElement::~SVGUseElement() = default;

void SVGUseElement::Dispose() {
  ClearResource();
}

void SVGUseElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(target_id_observer_);
  SVGGraphicsElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
  ResourceClient::Trace(visitor);
}

#if DCHECK_IS_ON()
static inline bool IsWellFormedDocument(const Document& document) {
  if (document.IsXMLDocument())
    return static_cast<XMLDocumentParser*>(document.Parser())->WellFormed();
  return true;
}
#endif

Node::InsertionNotificationRequest SVGUseElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGGraphicsElement::InsertedInto(root_parent);
  if (root_parent.isConnected()) {
    InvalidateShadowTree();
#if DCHECK_IS_ON()
    DCHECK(!InstanceRoot() || !IsWellFormedDocument(GetDocument()));
#endif
  }
  return kInsertionDone;
}

void SVGUseElement::RemovedFrom(ContainerNode& root_parent) {
  SVGGraphicsElement::RemovedFrom(root_parent);
  if (root_parent.isConnected()) {
    ClearResourceReference();
    CancelShadowTreeRecreation();
  }
}

static void TransferUseWidthAndHeightIfNeeded(
    const SVGUseElement& use,
    SVGElement& shadow_element,
    const SVGElement& original_element) {
  // Use |original_element| for checking the element type, because we will
  // have replaced a <symbol> with an <svg> in the instance tree.
  if (!IsSVGSymbolElement(original_element) &&
      !IsSVGSVGElement(original_element))
    return;

  // "The width and height properties on the 'use' element override the values
  // for the corresponding properties on a referenced 'svg' or 'symbol' element
  // when determining the used value for that property on the instance root
  // element. However, if the computed value for the property on the 'use'
  // element is auto, then the property is computed as normal for the element
  // instance. ... Because auto is the initial value, if dimensions are not
  // explicitly set on the 'use' element, the values set on the 'svg' or
  // 'symbol' will be used as defaults."
  // (https://svgwg.org/svg2-draft/struct.html#UseElement)
  AtomicString width_value(
      use.width()->IsSpecified()
          ? use.width()->CurrentValue()->ValueAsString()
          : original_element.getAttribute(svg_names::kWidthAttr));
  shadow_element.setAttribute(svg_names::kWidthAttr, width_value);
  AtomicString height_value(
      use.height()->IsSpecified()
          ? use.height()->CurrentValue()->ValueAsString()
          : original_element.getAttribute(svg_names::kHeightAttr));
  shadow_element.setAttribute(svg_names::kHeightAttr, height_value);
}

void SVGUseElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == x_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            x_->CssValue());
  } else if (property == y_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            y_->CssValue());
  } else {
    SVGGraphicsElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

bool SVGUseElement::IsStructurallyExternal() const {
  return !element_url_is_local_ &&
         !EqualIgnoringFragmentIdentifier(element_url_, GetDocument().Url());
}

void SVGUseElement::UpdateTargetReference() {
  const String& url_string = HrefString();
  element_url_ = GetDocument().CompleteURL(url_string);
  element_url_is_local_ = url_string.StartsWith('#');
  if (!IsStructurallyExternal()) {
    ClearResource();
    return;
  }
  if (!element_url_.HasFragmentIdentifier() ||
      (GetResource() &&
       EqualIgnoringFragmentIdentifier(element_url_, GetResource()->Url())))
    return;

  ResourceLoaderOptions options;
  options.initiator_info.name = localName();
  FetchParameters params(ResourceRequest(element_url_), options);
  params.MutableResourceRequest().SetMode(
      network::mojom::RequestMode::kSameOrigin);
  ResourceFetcher* fetcher = GetDocument().Fetcher();
  if (base::FeatureList::IsEnabled(
          features::kHtmlImportsRequestInitiatorLock)) {
    // For @imports from HTML imported Documents, we use the
    // context document for getting origin and ResourceFetcher to use the
    // main Document's origin, while using the element document for
    // CompleteURL() to use imported Documents' base URLs.
    if (!GetDocument().ContextDocument()) {
      ClearResource();
      return;
    }
    fetcher = GetDocument().ContextDocument()->Fetcher();
  }
  DocumentResource::FetchSVGDocument(params, fetcher, this);
}

void SVGUseElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr ||
      attr_name == svg_names::kHeightAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr) {
      InvalidateSVGPresentationAttributeStyle();
      SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::FromAttribute(attr_name));
    }

    UpdateRelativeLengthsInformation();
    if (SVGElement* instance_root = InstanceRoot()) {
      DCHECK(instance_root->CorrespondingElement());
      TransferUseWidthAndHeightIfNeeded(*this, *instance_root,
                                        *instance_root->CorrespondingElement());
    }

    if (LayoutObject* object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(*object);
    return;
  }

  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    UpdateTargetReference();
    InvalidateShadowTree();
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

static bool IsDisallowedElement(const Element& element) {
  // Spec: "Any 'svg', 'symbol', 'g', graphics element or other 'use' is
  // potentially a template object that can be re-used (i.e., "instanced") in
  // the SVG document via a 'use' element." "Graphics Element" is defined as
  // 'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline',
  // 'rect', 'text' Excluded are anything that is used by reference or that only
  // make sense to appear once in a document.
  if (!element.IsSVGElement())
    return true;

  DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, allowed_element_tags,
                      ({
                          svg_names::kATag,        svg_names::kCircleTag,
                          svg_names::kDescTag,     svg_names::kEllipseTag,
                          svg_names::kGTag,        svg_names::kImageTag,
                          svg_names::kLineTag,     svg_names::kMetadataTag,
                          svg_names::kPathTag,     svg_names::kPolygonTag,
                          svg_names::kPolylineTag, svg_names::kRectTag,
                          svg_names::kSVGTag,      svg_names::kSwitchTag,
                          svg_names::kSymbolTag,   svg_names::kTextTag,
                          svg_names::kTextPathTag, svg_names::kTitleTag,
                          svg_names::kTSpanTag,    svg_names::kUseTag,
                      }));
  return !allowed_element_tags.Contains<SVGAttributeHashTranslator>(
      element.TagQName());
}

void SVGUseElement::ScheduleShadowTreeRecreation() {
  if (InUseShadowTree())
    return;
  needs_shadow_tree_recreation_ = true;
  GetDocument().ScheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::CancelShadowTreeRecreation() {
  needs_shadow_tree_recreation_ = false;
  GetDocument().UnscheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::ClearResourceReference() {
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

Element* SVGUseElement::ResolveTargetElement(ObserveBehavior observe_behavior) {
  if (!element_url_.HasFragmentIdentifier())
    return nullptr;
  AtomicString element_identifier(DecodeURLEscapeSequences(
      element_url_.FragmentIdentifier(), DecodeURLMode::kUTF8OrIsomorphic));
  if (!IsStructurallyExternal()) {
    if (observe_behavior == kDontAddObserver)
      return GetTreeScope().getElementById(element_identifier);
    return ObserveTarget(
        target_id_observer_, GetTreeScope(), element_identifier,
        WTF::BindRepeating(&SVGUseElement::InvalidateShadowTree,
                           WrapWeakPersistent(this)));
  }
  if (!ResourceIsValid())
    return nullptr;
  return ToDocumentResource(GetResource())
      ->GetDocument()
      ->getElementById(element_identifier);
}

SVGElement* SVGUseElement::InstanceRoot() const {
  if (ShadowTreeRebuildPending())
    return nullptr;
  return To<SVGElement>(UseShadowRoot().firstChild());
}

void SVGUseElement::BuildPendingResource() {
  // This runs just before computed style is updated, so this SVGUseElement
  // should always be connected to the Document. It should also not be an
  // SVGUseElement that is part of a shadow tree, since we should never
  // schedule shadow tree updates for those.
  DCHECK(!InUseShadowTree());
  DCHECK(isConnected());

  DetachShadowTree();
  ClearResourceReference();
  CancelShadowTreeRecreation();
  DCHECK(isConnected());

  auto* target = DynamicTo<SVGElement>(ResolveTargetElement(kAddObserver));
  if (target) {
    DCHECK(target->isConnected());
    AttachShadowTree(*target);
  }
  DCHECK(!needs_shadow_tree_recreation_);
}

String SVGUseElement::title() const {
  // Find the first <title> child in <use> which doesn't cover shadow tree.
  if (Element* title_element = Traversal<SVGTitleElement>::FirstChild(*this))
    return title_element->innerText();

  // If there is no <title> child in <use>, we lookup first <title> child in
  // shadow tree.
  if (SVGElement* instance_root = InstanceRoot()) {
    if (Element* title_element =
            Traversal<SVGTitleElement>::FirstChild(*instance_root))
      return title_element->innerText();
  }
  // Otherwise return a null string.
  return String();
}

static void AssociateCorrespondingElements(SVGElement& target_root,
                                           SVGElement& instance_root) {
  auto target_range =
      Traversal<SVGElement>::InclusiveDescendantsOf(target_root);
  auto target_iterator = target_range.begin();
  for (SVGElement& instance :
       Traversal<SVGElement>::InclusiveDescendantsOf(instance_root)) {
    DCHECK(!instance.CorrespondingElement());
    instance.SetCorrespondingElement(&*target_iterator);
    ++target_iterator;
  }
  DCHECK(!(target_iterator != target_range.end()));
}

// We don't walk the target tree element-by-element, and clone each element,
// but instead use cloneNode(deep=true). This is an optimization for the common
// case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
// Though if there are disallowed elements in the subtree, we have to remove
// them.  For instance: <use> on <g> containing <foreignObject> (indirect
// case).
static inline void RemoveDisallowedElementsFromSubtree(SVGElement& subtree) {
  DCHECK(!subtree.isConnected());
  Element* element = ElementTraversal::FirstWithin(subtree);
  while (element) {
    if (IsDisallowedElement(*element)) {
      Element* next =
          ElementTraversal::NextSkippingChildren(*element, &subtree);
      // The subtree is not in document so this won't generate events that could
      // mutate the tree.
      element->parentNode()->RemoveChild(element);
      element = next;
    } else {
      element = ElementTraversal::Next(*element, &subtree);
    }
  }
}

static void MoveChildrenToReplacementElement(ContainerNode& source_root,
                                             ContainerNode& destination_root) {
  for (Node* child = source_root.firstChild(); child;) {
    Node* next_child = child->nextSibling();
    destination_root.AppendChild(child);
    child = next_child;
  }
}

SVGElement* SVGUseElement::CreateInstanceTree(SVGElement& target_root) const {
  SVGElement* instance_root = &To<SVGElement>(target_root.CloneWithChildren());
  if (IsSVGSymbolElement(target_root)) {
    // Spec: The referenced 'symbol' and its contents are deep-cloned into
    // the generated tree, with the exception that the 'symbol' is replaced
    // by an 'svg'. This generated 'svg' will always have explicit values
    // for attributes width and height. If attributes width and/or height
    // are provided on the 'use' element, then these attributes will be
    // transferred to the generated 'svg'. If attributes width and/or
    // height are not specified, the generated 'svg' element will use
    // values of 100% for these attributes.
    auto* svg_element =
        MakeGarbageCollected<SVGSVGElement>(target_root.GetDocument());
    // Transfer all attributes from the <symbol> to the new <svg>
    // element.
    svg_element->CloneAttributesFrom(*instance_root);
    // Move already cloned elements to the new <svg> element.
    MoveChildrenToReplacementElement(*instance_root, *svg_element);
    instance_root = svg_element;
  }
  TransferUseWidthAndHeightIfNeeded(*this, *instance_root, target_root);
  AssociateCorrespondingElements(target_root, *instance_root);
  RemoveDisallowedElementsFromSubtree(*instance_root);
  return instance_root;
}

void SVGUseElement::AttachShadowTree(SVGElement& target) {
  DCHECK(!InstanceRoot());
  DCHECK(!needs_shadow_tree_recreation_);
  DCHECK(!InUseShadowTree());

  // Do not allow self-referencing.
  if (IsDisallowedElement(target) || HasCycleUseReferencing(*this, target))
    return;

  // Set up root SVG element in shadow tree.
  // Clone the target subtree into the shadow tree, not handling <use> and
  // <symbol> yet.
  UseShadowRoot().AppendChild(CreateInstanceTree(target));

  AddReferencesToFirstDegreeNestedUseElements(target);

  // Assure shadow tree building was successful.
  DCHECK(InstanceRoot());
  DCHECK_EQ(InstanceRoot()->CorrespondingUseElement(), this);
  DCHECK_EQ(InstanceRoot()->CorrespondingElement(), &target);

  // Expand all <use> elements in the shadow tree.
  // Expand means: replace the actual <use> element by what it references.
  ExpandUseElementsInShadowTree();

  for (SVGElement& instance :
       Traversal<SVGElement>::DescendantsOf(UseShadowRoot())) {
    SVGElement* corresponding_element = instance.CorrespondingElement();
    // Transfer non-markup event listeners.
    if (EventTargetData* data = corresponding_element->GetEventTargetData()) {
      data->event_listener_map.CopyEventListenersNotCreatedFromMarkupToTarget(
          &instance);
    }
    // Setup the mapping from the corresponding (original) element back to the
    // instance.
    corresponding_element->MapInstanceToElement(&instance);
  }

  // Update relative length information.
  UpdateRelativeLengthsInformation();
}

void SVGUseElement::DetachShadowTree() {
  ShadowRoot& shadow_root = UseShadowRoot();
  // Tear down the mapping from the corresponding (original) element back to
  // the instance.
  for (SVGElement& instance :
       Traversal<SVGElement>::DescendantsOf(shadow_root)) {
    // When the instances of an element are invalidated the corresponding
    // element is cleared, so it can be null here.
    if (SVGElement* corresponding_element = instance.CorrespondingElement())
      corresponding_element->RemoveInstanceMapping(&instance);
  }
  // FIXME: We should try to optimize this, to at least allow partial reclones.
  shadow_root.RemoveChildren(kOmitSubtreeModifiedEvent);
}

LayoutObject* SVGUseElement::CreateLayoutObject(const ComputedStyle& style,
                                                LegacyLayout) {
  return new LayoutSVGTransformableContainer(this);
}

static bool IsDirectReference(const SVGElement& element) {
  return IsSVGPathElement(element) || IsSVGRectElement(element) ||
         IsSVGCircleElement(element) || IsSVGEllipseElement(element) ||
         IsSVGPolygonElement(element) || IsSVGPolylineElement(element) ||
         IsSVGTextElement(element);
}

Path SVGUseElement::ToClipPath() const {
  const SVGGraphicsElement* element = VisibleTargetGraphicsElementForClipping();
  if (!element || !element->IsSVGGeometryElement())
    return Path();

  DCHECK(GetLayoutObject());
  Path path = ToSVGGeometryElement(*element).ToClipPath();
  AffineTransform transform = GetLayoutObject()->LocalSVGTransform();
  if (!transform.IsIdentity())
    path.Transform(transform);
  return path;
}

SVGGraphicsElement* SVGUseElement::VisibleTargetGraphicsElementForClipping()
    const {
  auto* svg_graphics_element = DynamicTo<SVGGraphicsElement>(InstanceRoot());
  if (!svg_graphics_element)
    return nullptr;

  // Spec: "If a <use> element is a child of a clipPath element, it must
  // directly reference <path>, <text> or basic shapes elements. Indirect
  // references are an error and the clipPath element must be ignored."
  // https://drafts.fxtf.org/css-masking/#the-clip-path
  if (!IsDirectReference(*svg_graphics_element)) {
    // Spec: Indirect references are an error (14.3.5)
    return nullptr;
  }

  return svg_graphics_element;
}

void SVGUseElement::AddReferencesToFirstDegreeNestedUseElements(
    SVGElement& target) {
  // Don't track references to external documents.
  if (IsStructurallyExternal())
    return;
  // We only need to track first degree <use> dependencies. Indirect
  // references are handled as the invalidation bubbles up the dependency
  // chain.
  SVGUseElement* use_element =
      IsSVGUseElement(target) ? ToSVGUseElement(&target)
                              : Traversal<SVGUseElement>::FirstWithin(target);
  for (; use_element;
       use_element = Traversal<SVGUseElement>::NextSkippingChildren(
           *use_element, &target))
    AddReferenceTo(use_element);
}

bool SVGUseElement::HasCycleUseReferencing(const ContainerNode& target_instance,
                                           const SVGElement& target) const {
  // Shortcut for self-references
  if (&target == this)
    return true;

  AtomicString target_id = target.GetIdAttribute();
  auto* element =
      DynamicTo<SVGElement>(target_instance.ParentOrShadowHostElement());
  while (element) {
    if (element->HasID() && element->GetIdAttribute() == target_id &&
        element->GetDocument() == target.GetDocument())
      return true;
    element = DynamicTo<SVGElement>(element->ParentOrShadowHostElement());
  }
  return false;
}

// Spec: In the generated content, the 'use' will be replaced by 'g', where all
// attributes from the 'use' element except for x, y, width, height and
// xlink:href are transferred to the generated 'g' element.
static void RemoveAttributesFromReplacementElement(
    SVGElement& replacement_element) {
  replacement_element.removeAttribute(svg_names::kXAttr);
  replacement_element.removeAttribute(svg_names::kYAttr);
  replacement_element.removeAttribute(svg_names::kWidthAttr);
  replacement_element.removeAttribute(svg_names::kHeightAttr);
  replacement_element.removeAttribute(svg_names::kHrefAttr);
  replacement_element.removeAttribute(xlink_names::kHrefAttr);
}

void SVGUseElement::ExpandUseElementsInShadowTree() {
  // Why expand the <use> elements in the shadow tree here, and not just
  // do this directly in buildShadowTree, if we encounter a <use> element?
  //
  // Short answer: Because we may miss to expand some elements. For example, if
  // a <symbol> contains <use> tags, we'd miss them. So once we're done with
  // setting up the actual shadow tree (after the special case modification for
  // svg/symbol) we have to walk it completely and expand all <use> elements.
  ShadowRoot& shadow_root = UseShadowRoot();
  for (SVGUseElement* use = Traversal<SVGUseElement>::FirstWithin(shadow_root);
       use;) {
    SVGUseElement& original_use = ToSVGUseElement(*use->CorrespondingElement());
    auto* target = DynamicTo<SVGElement>(
        original_use.ResolveTargetElement(kDontAddObserver));
    if (target) {
      if (IsDisallowedElement(*target) || HasCycleUseReferencing(*use, *target))
        return;
    }

    // Don't DCHECK(target) here, it may be "pending", too.
    // Setup sub-shadow tree root node
    auto* clone_parent =
        MakeGarbageCollected<SVGGElement>(original_use.GetDocument());
    // Transfer all data (attributes, etc.) from <use> to the new <g> element.
    clone_parent->CloneAttributesFrom(*use);
    clone_parent->SetCorrespondingElement(&original_use);

    RemoveAttributesFromReplacementElement(*clone_parent);

    // Move already cloned elements to the new <g> element.
    MoveChildrenToReplacementElement(*use, *clone_parent);

    if (target)
      clone_parent->AppendChild(use->CreateInstanceTree(*target));

    // Replace <use> with referenced content.
    use->parentNode()->ReplaceChild(clone_parent, use);

    use = Traversal<SVGUseElement>::Next(*clone_parent, &shadow_root);
  }
}

bool SVGUseElement::ShadowTreeRebuildPending() const {
  // The shadow tree is torn down lazily, so check if there's a pending rebuild
  // or if we're disconnected from the document.
  return !InActiveDocument() || needs_shadow_tree_recreation_;
}

void SVGUseElement::InvalidateShadowTree() {
  if (ShadowTreeRebuildPending())
    return;
  ScheduleShadowTreeRecreation();
  InvalidateDependentShadowTrees();
}

void SVGUseElement::InvalidateDependentShadowTrees() {
  // Recursively invalidate dependent <use> shadow trees
  const HeapHashSet<WeakMember<SVGElement>>& raw_instances =
      InstancesForElement();
  HeapVector<Member<SVGElement>> instances;
  instances.AppendRange(raw_instances.begin(), raw_instances.end());
  for (auto& instance : instances) {
    if (SVGUseElement* element = instance->CorrespondingUseElement()) {
      DCHECK(element->isConnected());
      element->InvalidateShadowTree();
    }
  }
}

bool SVGUseElement::SelfHasRelativeLengths() const {
  if (x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
      width_->CurrentValue()->IsRelative() ||
      height_->CurrentValue()->IsRelative())
    return true;
  SVGElement* instance_root = InstanceRoot();
  return instance_root && instance_root->HasRelativeLengths();
}

FloatRect SVGUseElement::GetBBox() {
  DCHECK(GetLayoutObject());
  LayoutSVGTransformableContainer& transformable_container =
      ToLayoutSVGTransformableContainer(*GetLayoutObject());
  // Don't apply the additional translation if the oBB is invalid.
  if (!transformable_container.IsObjectBoundingBoxValid())
    return FloatRect();

  // TODO(fs): Preferably this would just use objectBoundingBox() (and hence
  // don't need to override SVGGraphicsElement::getBBox at all) and be
  // correct without additional work. That will not work out ATM without
  // additional quirks. The problem stems from including the additional
  // translation directly on the LayoutObject corresponding to the
  // SVGUseElement.
  FloatRect bbox = transformable_container.ObjectBoundingBox();
  bbox.Move(transformable_container.AdditionalTranslation());
  return bbox;
}

void SVGUseElement::DispatchPendingEvent() {
  DCHECK(IsStructurallyExternal());
  DCHECK(have_fired_load_event_);
  DispatchEvent(*Event::Create(event_type_names::kLoad));
}

void SVGUseElement::NotifyFinished(Resource* resource) {
  DCHECK_EQ(GetResource(), resource);
  if (!isConnected())
    return;

  InvalidateShadowTree();
  if (!ResourceIsValid()) {
    DispatchEvent(*Event::Create(event_type_names::kError));
  } else if (!resource->WasCanceled()) {
    if (have_fired_load_event_)
      return;
    if (!IsStructurallyExternal())
      return;
    DCHECK(!have_fired_load_event_);
    have_fired_load_event_ = true;
    GetDocument()
        .GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE, WTF::Bind(&SVGUseElement::DispatchPendingEvent,
                                        WrapPersistent(this)));
  }
}

bool SVGUseElement::ResourceIsValid() const {
  return GetResource() && GetResource()->IsLoaded() &&
         !GetResource()->ErrorOccurred() &&
         ToDocumentResource(GetResource())->GetDocument();
}

}  // namespace blink
