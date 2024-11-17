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

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_circle_element.h"
#include "third_party/blink/renderer/core/svg/svg_ellipse_element.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_path_element.h"
#include "third_party/blink/renderer/core/svg/svg_polygon_element.h"
#include "third_party/blink/renderer/core/svg/svg_polyline_element.h"
#include "third_party/blink/renderer/core/svg/svg_rect_element.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/core/svg/svg_title_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
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
      needs_shadow_tree_recreation_(false) {
  DCHECK(HasCustomStyleCallbacks());

  CreateUserAgentShadowRoot();
}

SVGUseElement::~SVGUseElement() = default;

void SVGUseElement::Trace(Visitor* visitor) const {
  visitor->Trace(document_content_);
  visitor->Trace(external_resource_target_);
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(target_id_observer_);
  SVGGraphicsElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

#if DCHECK_IS_ON()
static inline bool IsWellFormedDocument(const Document& document) {
  if (IsA<XMLDocument>(document))
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

void SVGUseElement::DidMoveToNewDocument(Document& old_document) {
  SVGGraphicsElement::DidMoveToNewDocument(old_document);
  if (load_event_delayer_) {
    load_event_delayer_->DocumentChanged(GetDocument());
  }
  UpdateTargetReference();
}

static void TransferUseWidthAndHeightIfNeeded(
    const SVGUseElement& use,
    SVGElement& shadow_element,
    const SVGElement& original_element) {
  // Use |original_element| for checking the element type, because we will
  // have replaced a <symbol> with an <svg> in the instance tree.
  if (!IsA<SVGSymbolElement>(original_element) &&
      !IsA<SVGSVGElement>(original_element))
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

bool SVGUseElement::IsStructurallyExternal() const {
  return !element_url_is_local_ &&
         !EqualIgnoringFragmentIdentifier(element_url_, GetDocument().Url());
}

bool SVGUseElement::HaveLoadedRequiredResources() {
  return !document_content_ || !document_content_->IsLoading();
}

void SVGUseElement::UpdateDocumentContent(
    SVGResourceDocumentContent* document_content) {
  if (document_content_ == document_content) {
    return;
  }
  auto old_load_event_delayer = std::move(load_event_delayer_);
  if (document_content_) {
    document_content_->RemoveObserver(this);
  }
  document_content_ = document_content;
  if (document_content_) {
    load_event_delayer_ =
        std::make_unique<IncrementLoadEventDelayCount>(GetDocument());
    document_content_->AddObserver(this);
  }
}

void SVGUseElement::UpdateTargetReference() {
  const String& url_string = HrefString();
  element_url_ = GetDocument().CompleteURL(url_string);
  element_url_is_local_ = url_string.StartsWith('#');
  if (!IsStructurallyExternal() || !GetDocument().IsActive()) {
    UpdateDocumentContent(nullptr);
    pending_event_.Cancel();
    return;
  }
  if (!element_url_.HasFragmentIdentifier() ||
      (document_content_ && EqualIgnoringFragmentIdentifier(
                                element_url_, document_content_->Url()))) {
    return;
  }

  pending_event_.Cancel();

  if (element_url_.ProtocolIsData()) {
    Deprecation::CountDeprecation(GetDocument().domWindow(),
                                  WebFeature::kDataUrlInSvgUse);
  }

  auto* context_document = &GetDocument();
  ExecutionContext* execution_context = context_document->GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kUse;
  FetchParameters params(ResourceRequest(element_url_), options);
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  auto* document_content =
      SVGResourceDocumentContent::Fetch(params, *context_document);
  UpdateDocumentContent(document_content);
}

void SVGUseElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr ||
      attr_name == svg_names::kHeightAttr) {
    if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr) {
      UpdatePresentationAttributeStyle(params.property);
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
    UpdateTargetReference();
    InvalidateShadowTree();
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(params);
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
  needs_shadow_tree_recreation_ = true;
  GetDocument().ScheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::CancelShadowTreeRecreation() {
  needs_shadow_tree_recreation_ = false;
  GetDocument().UnscheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::ClearResourceReference() {
  external_resource_target_.Clear();
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

Element* SVGUseElement::ResolveTargetElement() {
  if (!element_url_.HasFragmentIdentifier())
    return nullptr;
  AtomicString element_identifier(DecodeURLEscapeSequences(
      element_url_.FragmentIdentifier(), DecodeURLMode::kUTF8OrIsomorphic));

  if (!IsStructurallyExternal()) {
    // Only create observers for non-instance use elements.
    // Instances will be updated by their corresponding elements.
    if (InUseShadowTree()) {
      return OriginatingTreeScope().getElementById(element_identifier);
    } else {
      return ObserveTarget(
          target_id_observer_, OriginatingTreeScope(), element_identifier,
          WTF::BindRepeating(&SVGUseElement::InvalidateTargetReference,
                             WrapWeakPersistent(this)));
    }
  }
  if (!document_content_) {
    return nullptr;
  }
  external_resource_target_ =
      document_content_->GetResourceTarget(element_identifier);
  if (!external_resource_target_) {
    return nullptr;
  }
  return external_resource_target_->target;
}

SVGElement* SVGUseElement::InstanceRoot() const {
  if (ShadowTreeRebuildPending())
    return nullptr;
  return To<SVGElement>(UseShadowRoot().firstChild());
}

void SVGUseElement::BuildPendingResource() {
  if (!isConnected()) {
    DCHECK(!needs_shadow_tree_recreation_);
    return;  // Already replaced by rebuilding ancestor.
  }
  CancelShadowTreeRecreation();

  // Check if this element is scheduled (by an ancestor) to be replaced.
  SVGUseElement* ancestor = GeneratingUseElement();
  while (ancestor) {
    if (ancestor->needs_shadow_tree_recreation_)
      return;
    ancestor = ancestor->GeneratingUseElement();
  }

  DetachShadowTree();
  ClearResourceReference();

  if (auto* target = DynamicTo<SVGElement>(ResolveTargetElement())) {
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

static void PostProcessInstanceTree(SVGElement& target_root,
                                    SVGElement& instance_root) {
  DCHECK(!instance_root.isConnected());
  // We checked this before creating the cloned subtree.
  DCHECK(!IsDisallowedElement(instance_root));
  // Associate the roots.
  instance_root.SetCorrespondingElement(&target_root);

  // The subtrees defined by |target_root| and |instance_root| should be
  // isomorphic at this point, so we can walk both trees simultaneously to be
  // able to create the corresponding element mapping.
  //
  // We don't walk the target tree element-by-element, and clone each element,
  // but instead use cloneNode(deep=true). This is an optimization for the
  // common case where <use> doesn't contain disallowed elements
  // (ie. <foreignObject>).  Though if there are disallowed elements in the
  // subtree, we have to remove them. For instance: <use> on <g> containing
  // <foreignObject> (indirect case).
  // We do that at the same time as the association back to the corresponding
  // element is performed to avoid having instance elements in a half-way
  // inconsistent state.
  Element* target_element = ElementTraversal::FirstWithin(target_root);
  Element* instance_element = ElementTraversal::FirstWithin(instance_root);
  while (target_element) {
    DCHECK(instance_element);
    DCHECK(!IsA<SVGElement>(*instance_element) ||
           !To<SVGElement>(*instance_element).CorrespondingElement());
    if (IsDisallowedElement(*target_element)) {
      Element* instance_next = ElementTraversal::NextSkippingChildren(
          *instance_element, &instance_root);
      // The subtree is not in the document so this won't generate events that
      // could mutate the tree.
      instance_element->parentNode()->RemoveChild(instance_element);

      // Since the target subtree isn't mutated, it can just be traversed in
      // the normal way (without saving next traversal target).
      target_element =
          ElementTraversal::NextSkippingChildren(*target_element, &target_root);
      instance_element = instance_next;
    } else {
      // Set up the corresponding element association.
      if (auto* svg_instance_element =
              DynamicTo<SVGElement>(instance_element)) {
        svg_instance_element->SetCorrespondingElement(
            To<SVGElement>(target_element));
      }
      target_element = ElementTraversal::Next(*target_element, &target_root);
      instance_element =
          ElementTraversal::Next(*instance_element, &instance_root);
    }
  }
  DCHECK(!instance_element);
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
  NodeCloningData data{CloneOption::kIncludeDescendants};
  SVGElement* instance_root = &To<SVGElement>(target_root.CloneWithChildren(
      data, /*document*/ nullptr, /*append_to*/ nullptr));
  if (IsA<SVGSymbolElement>(target_root)) {
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
  PostProcessInstanceTree(target_root, *instance_root);
  return instance_root;
}

void SVGUseElement::AttachShadowTree(SVGElement& target) {
  DCHECK(!InstanceRoot());
  DCHECK(!needs_shadow_tree_recreation_);

  // Do not allow self-referencing.
  if (IsDisallowedElement(target) || HasCycleUseReferencing(*this, target))
    return;

  // Set up root SVG element in shadow tree.
  // Clone the target subtree into the shadow tree, not handling <use> and
  // <symbol> yet.
  UseShadowRoot().AppendChild(CreateInstanceTree(target));

  // Assure shadow tree building was successful.
  DCHECK(InstanceRoot());
  DCHECK_EQ(InstanceRoot()->GeneratingUseElement(), this);
  DCHECK_EQ(InstanceRoot()->CorrespondingElement(), &target);

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
    corresponding_element->AddInstance(&instance);
  }
}

void SVGUseElement::DetachShadowTree() {
  ShadowRoot& shadow_root = UseShadowRoot();
  // FIXME: We should try to optimize this, to at least allow partial reclones.
  shadow_root.RemoveChildren(kOmitSubtreeModifiedEvent);
}

LayoutObject* SVGUseElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGTransformableContainer>(this);
}

static bool IsDirectReference(const SVGElement& element) {
  return IsA<SVGPathElement>(element) || IsA<SVGRectElement>(element) ||
         IsA<SVGCircleElement>(element) || IsA<SVGEllipseElement>(element) ||
         IsA<SVGPolygonElement>(element) || IsA<SVGPolylineElement>(element) ||
         IsA<SVGTextElement>(element);
}

Path SVGUseElement::ToClipPath() const {
  const SVGGraphicsElement* element = VisibleTargetGraphicsElementForClipping();
  auto* geometry_element = DynamicTo<SVGGeometryElement>(element);
  if (!geometry_element)
    return Path();

  DCHECK(GetLayoutObject());
  Path path = geometry_element->ToClipPath();
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

bool SVGUseElement::ShadowTreeRebuildPending() const {
  // The shadow tree is torn down lazily, so check if there's a pending rebuild
  // or if we're disconnected from the document.
  return !InActiveDocument() || needs_shadow_tree_recreation_;
}

void SVGUseElement::InvalidateShadowTree() {
  if (ShadowTreeRebuildPending())
    return;
  ScheduleShadowTreeRecreation();
}

void SVGUseElement::InvalidateTargetReference() {
  InvalidateShadowTree();
  for (SVGElement* instance : InstancesForElement())
    To<SVGUseElement>(instance)->InvalidateShadowTree();
}

bool SVGUseElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

gfx::RectF SVGUseElement::GetBBox() {
  DCHECK(GetLayoutObject());
  auto& transformable_container =
      To<LayoutSVGTransformableContainer>(*GetLayoutObject());
  // Don't apply the additional translation if the oBB is invalid.
  if (!transformable_container.IsObjectBoundingBoxValid())
    return gfx::RectF();

  // TODO(fs): Preferably this would just use objectBoundingBox() (and hence
  // don't need to override SVGGraphicsElement::getBBox at all) and be
  // correct without additional work. That will not work out ATM without
  // additional quirks. The problem stems from including the additional
  // translation directly on the LayoutObject corresponding to the
  // SVGUseElement.
  gfx::RectF bbox = transformable_container.ObjectBoundingBox();
  bbox.Offset(transformable_container.AdditionalTranslation());
  return bbox;
}

void SVGUseElement::QueueOrDispatchPendingEvent(
    const AtomicString& event_name) {
  if (GetDocument().GetExecutionContext() &&
      GetDocument().GetExecutionContext()->is_in_back_forward_cache()) {
    // Queue the event if the page is in back/forward cache.
    EnqueueEvent(*Event::Create(event_name), TaskType::kDOMManipulation);
  } else {
    DispatchEvent(*Event::Create(event_name));
  }
}

void SVGUseElement::ResourceNotifyFinished(
    SVGResourceDocumentContent* document_content) {
  DCHECK_EQ(document_content_, document_content);
  load_event_delayer_.reset();
  if (!isConnected())
    return;
  InvalidateShadowTree();

  const bool is_error = document_content->ErrorOccurred();
  const AtomicString& event_name =
      is_error ? event_type_names::kError : event_type_names::kLoad;
  DCHECK(!pending_event_.IsActive());
  pending_event_ = PostCancellableTask(
      *GetDocument().GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      WTF::BindOnce(&SVGUseElement::QueueOrDispatchPendingEvent,
                    WrapPersistent(this), event_name));
}

SVGAnimatedPropertyBase* SVGUseElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kWidthAttr) {
    return width_.Get();
  } else if (attribute_name == svg_names::kHeightAttr) {
    return height_.Get();
  } else {
    SVGAnimatedPropertyBase* ret =
        SVGURIReference::PropertyFromAttribute(attribute_name);
    if (ret) {
      return ret;
    } else {
      return SVGGraphicsElement::PropertyFromAttribute(attribute_name);
    }
  }
}

void SVGUseElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x_.Get(), y_.Get(), width_.Get(),
                                   height_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGGraphicsElement::SynchronizeAllSVGAttributes();
}

void SVGUseElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  auto pres_attrs =
      std::to_array<const SVGAnimatedPropertyBase*>({x_.Get(), y_.Get()});
  AddAnimatedPropertiesToPresentationAttributeStyle(pres_attrs, style);
  SVGGraphicsElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink
