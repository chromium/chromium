/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google, Inc.
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

#include "third_party/blink/renderer/core/svg/svg_svg_element.h"

#include <algorithm>

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_model_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/svg_angle_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_animated_rect.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_length_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_matrix_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_number_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_point_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_rect_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/core/svg/svg_transform.h"
#include "third_party/blink/renderer/core/svg/svg_transform_list.h"
#include "third_party/blink/renderer/core/svg/svg_transform_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/svg/svg_view_element.h"
#include "third_party/blink/renderer/core/svg/svg_view_spec.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

SVGSVGElement::SVGSVGElement(Document& doc)
    : SVGViewportContainerElement(svg_names::kSVGTag, doc),
      time_container_(MakeGarbageCollected<SMILTimeContainer>(*this)),
      translation_(MakeGarbageCollected<SVGPoint>()),
      current_scale_(1) {
  UseCounter::Count(doc, WebFeature::kSVGSVGElement);
}

SVGSVGElement::~SVGSVGElement() = default;

float SVGSVGElement::currentScale() const {
  if (!isConnected() || !IsOutermostSVGSVGElement())
    return 1;

  return current_scale_;
}

void SVGSVGElement::setCurrentScale(float scale) {
  DCHECK(std::isfinite(scale));
  if (!isConnected() || !IsOutermostSVGSVGElement())
    return;

  current_scale_ = scale;
  UpdateUserTransform();
}

class SVGCurrentTranslateTearOff : public SVGPointTearOff {
 public:
  SVGCurrentTranslateTearOff(SVGSVGElement* context_element)
      : SVGPointTearOff(context_element->translation_, context_element) {}

  void CommitChange(SVGPropertyCommitReason) override {
    DCHECK(ContextElement());
    To<SVGSVGElement>(ContextElement())->UpdateUserTransform();
  }
};

SVGPointTearOff* SVGSVGElement::currentTranslateFromJavascript() {
  return MakeGarbageCollected<SVGCurrentTranslateTearOff>(this);
}

void SVGSVGElement::SetCurrentTranslate(const gfx::Vector2dF& point) {
  translation_->SetValue(gfx::PointAtOffsetFromOrigin(point));
  UpdateUserTransform();
}

void SVGSVGElement::UpdateUserTransform() {
  if (LayoutObject* object = GetLayoutObject()) {
    object->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kUnknown);
  }
}

bool SVGSVGElement::ZoomAndPanEnabled() const {
  SVGZoomAndPanType zoom_and_pan = zoomAndPan();
  if (view_spec_ && view_spec_->ZoomAndPan() != kSVGZoomAndPanUnknown)
    zoom_and_pan = view_spec_->ZoomAndPan();
  return zoom_and_pan == kSVGZoomAndPanMagnify;
}

// There are few cases when the width and height attributes on an inner `svg`
// may need to be collected explicitly as styles.
//
// Case 1: The width and height attributes on the `use` element override the
// values for the corresponding attributes on a referenced `svg` or `symbol`
// element when determining the used value for that property on the instance
// root element. [1]
//
// Case 2:If no width or height attributes are specified on the `use` element,
// corresponding reference element's width or height is used. For `svg` element
// since width and height are presentation attributes now, they are collected
// as styles but for `symbol` since width and height currently are not collected
// as styles so for `symbol` element we need to collect these styles
// explicitly. (crbug.com/41413321)
//
//[1] (https://svgwg.org/svg2-draft/struct.html#UseElement)
CSSPropertyValueSet*
SVGSVGElement::CreateWidthAndHeightPresentationAttributeStyleIfNeeded(
    const Element& original_element) {
  if (IsOutermostSVGSVGElement()) {
    return nullptr;
  }

  if (InUseShadowTree()) {
    auto* use_element = DynamicTo<SVGUseElement>(ParentOrShadowHostElement());

    if (use_element && (use_element->width()->IsSpecified() ||
                        use_element->height()->IsSpecified() ||
                        IsA<SVGSymbolElement>(original_element))) {
      HeapVector<CSSPropertyValue, 8> values;
      SVGAnimatedPropertyBase* properties[]{width_.Get(), height_.Get()};

      for (SVGAnimatedPropertyBase* property : properties) {
        if (const CSSValue* css_value = property->CssValue()) {
          AddPropertyToPresentationAttributeStyle(
              values, property->CssPropertyId(), *css_value);
        }
      }

      return ImmutableCSSPropertyValueSet::Create(values, kSVGAttributeMode);
    }
  }

  return nullptr;
}

void SVGSVGElement::ParseAttribute(const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (!nearestViewportElement()) {
    bool set_listener = true;

    // Only handle events if we're the outermost <svg> element
    if (name == html_names::kOnunloadAttr) {
      GetDocument().SetWindowAttributeEventListener(
          event_type_names::kUnload, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
    } else if (name == html_names::kOnresizeAttr) {
      GetDocument().SetWindowAttributeEventListener(
          event_type_names::kResize, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
    } else if (name == html_names::kOnscrollAttr) {
      GetDocument().SetWindowAttributeEventListener(
          event_type_names::kScroll, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
    } else {
      set_listener = false;
    }

    if (set_listener)
      return;
  }

  if (name == html_names::kOnabortAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kAbort, JSEventHandlerForContentAttribute::Create(
                                      GetExecutionContext(), name, value));
  } else if (name == html_names::kOnerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kError,
        JSEventHandlerForContentAttribute::Create(
            GetExecutionContext(), name, value,
            JSEventHandler::HandlerType::kOnErrorEventHandler));
  } else if (SVGZoomAndPan::ParseAttribute(name, value)) {
  } else {
    SVGElement::ParseAttribute(params);
  }
}

bool SVGSVGElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (!RuntimeEnabledFeatures::
          CollectWidthAndHeightAsStylesForNestedSvgEnabled()) {
    if ((name == svg_names::kWidthAttr || name == svg_names::kHeightAttr) &&
        !IsOutermostSVGSVGElement()) {
      return false;
    }
  }
  return SVGViewportContainerElement::IsPresentationAttribute(name);
}

void SVGSVGElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    HeapVector<CSSPropertyValue, 8>& style) {
  if (!RuntimeEnabledFeatures::
          CollectWidthAndHeightAsStylesForNestedSvgEnabled()) {
    // We shouldn't collect style for 'width' and 'height' on inner <svg>, so
    // bail here in that case to avoid having the generic logic in SVGElement
    // picking it up.
    if ((name == svg_names::kWidthAttr || name == svg_names::kHeightAttr) &&
        !IsOutermostSVGSVGElement()) {
      return;
    }
  }

  SVGViewportContainerElement::CollectStyleForPresentationAttribute(name, value,
                                                                    style);
}

void SVGSVGElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  if (SVGZoomAndPan::IsKnownAttribute(params.name)) {
    if (auto* layout_object = GetLayoutObject()) {
      MarkForLayoutAndParentResourceInvalidation(*layout_object);
    }
  }
  SVGViewportContainerElement::SvgAttributeChanged(params);
}

void SVGSVGElement::DidMoveToNewDocument(Document& old_document) {
  SVGViewportContainerElement::DidMoveToNewDocument(old_document);
  if (TimeContainer()->IsStarted()) {
    TimeContainer()->ResetDocumentTime();
  }
}

namespace {

const SVGElement* InnermostCommonSubtreeRoot(
    const SVGSVGElement& svg_root,
    const SVGElement* reference_element) {
  if (reference_element) {
    // The reference element is a descendant of the <svg> element
    // -> reference element is root of the common subtree.
    if (svg_root.contains(reference_element)) {
      return reference_element;
    }
    // The <svg> element is not a descendant of the reference element
    // -> no common subtree.
    if (!svg_root.IsDescendantOf(reference_element)) {
      return nullptr;
    }
  }
  return &svg_root;
}

enum class ElementResultFilter {
  kOnlyDescendants,
  kDescendantsOrReference,
};

HeapVector<Member<Element>> ComputeIntersectionList(
    const SVGSVGElement& root,
    const SVGElement* reference_element,
    const gfx::RectF& rect,
    ElementResultFilter filter) {
  HeapVector<Member<Element>> elements;
  LocalFrameView* frame_view = root.GetDocument().View();
  if (!frame_view || !frame_view->UpdateAllLifecyclePhasesExceptPaint(
                         DocumentUpdateReason::kJavaScript)) {
    return elements;
  }
  const LayoutObject* layout_object = root.GetLayoutObject();
  if (!layout_object) {
    return elements;
  }
  const SVGElement* common_subtree_root =
      InnermostCommonSubtreeRoot(root, reference_element);
  if (!common_subtree_root) {
    return elements;
  }

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                         HitTestRequest::kListBased |
                         HitTestRequest::kPenetratingList);
  HitTestLocation location(rect.CenterPoint(), gfx::QuadF(rect));
  HitTestResult result(request, location);
  // Transform to the local space of `root`.
  // We could transform the location to the space of the reference element (the
  // common subtree), but that quickly gets quite hairy.
  TransformedHitTestLocation local_location(
      location, root.ComputeCTM(SVGElement::kAncestorScope, &root));
  if (local_location) {
    if (const auto* layout_root = DynamicTo<LayoutSVGRoot>(layout_object)) {
      layout_root->IntersectChildren(result, *local_location);
    } else {
      To<LayoutSVGViewportContainer>(layout_object)
          ->IntersectChildren(result, *local_location);
    }
  }
  // Do a first pass transforming text-nodes to their parents.
  elements = root.GetTreeScope().ElementsFromHitTestResult(result);
  // We want all elements that are SVGGraphicsElements and descendants of the
  // common subtree root.
  auto partition_condition = [common_subtree_root,
                              filter](const Member<Element>& item) {
    if (!IsA<SVGGraphicsElement>(*item)) {
      return false;
    }
    return filter == ElementResultFilter::kDescendantsOrReference
               ? common_subtree_root->contains(item)
               : item->IsDescendantOf(common_subtree_root);
  };
  auto to_remove = std::stable_partition(elements.begin(), elements.end(),
                                         partition_condition);
  elements.erase(to_remove, elements.end());
  // Hit-testing traverses the tree from last to first child for each
  // container, so the result needs to be reversed.
  std::ranges::reverse(elements);
  return elements;
}

}  // namespace

StaticNodeTypeList<Element>* SVGSVGElement::getIntersectionList(
    SVGRectTearOff* rect,
    SVGElement* reference_element) const {
  // https://svgwg.org/svg2-draft/struct.html#__svg__SVGSVGElement__getIntersectionList
  HeapVector<Member<Element>> intersecting_elements =
      ComputeIntersectionList(*this, reference_element, rect->Target()->Rect(),
                              ElementResultFilter::kOnlyDescendants);
  return StaticNodeTypeList<Element>::Adopt(intersecting_elements);
}

bool SVGSVGElement::checkIntersection(SVGElement* element,
                                      SVGRectTearOff* rect) const {
  // https://svgwg.org/svg2-draft/struct.html#__svg__SVGSVGElement__checkIntersection
  DCHECK(element);
  auto* graphics_element = DynamicTo<SVGGraphicsElement>(*element);
  // If `element` is not an SVGGraphicsElement it can not intersect.
  if (!graphics_element) {
    return false;
  }

  // Collect intersecting descendants of the SVGSVGElement within `rect`.
  HeapVector<Member<Element>> intersecting_elements =
      ComputeIntersectionList(*this, element, rect->Target()->Rect(),
                              ElementResultFilter::kDescendantsOrReference);
  HeapHashSet<Member<Element>> intersecting_element_set;
  for (const auto& intersected_element : intersecting_elements) {
    intersecting_element_set.insert(intersected_element);
  }

  // This implements the spec section named "find the non-container graphics
  // elements" combined with the step that checks if all such elements are also
  // part of the intersecting descendants.
  size_t elements_matched = 0;
  for (SVGGraphicsElement& descendant :
       Traversal<SVGGraphicsElement>::InclusiveDescendantsOf(
           *graphics_element)) {
    if (IsA<SVGGElement>(descendant) || IsA<SVGSVGElement>(descendant)) {
      continue;
    }
    if (!intersecting_element_set.Contains(&descendant)) {
      return false;
    }
    elements_matched++;
  }
  // If at least one SVGGraphicsElement matched it's an intersection.
  return elements_matched > 0;
}

// One of the element types that can cause graphics to be drawn onto the target
// canvas. Specifically: circle, ellipse, image, line, path, polygon, polyline,
// rect, text and use.
static bool IsEnclosureTarget(const LayoutObject* layout_object) {
  if (!layout_object ||
      layout_object->StyleRef().UsedPointerEvents() == EPointerEvents::kNone) {
    return false;
  }
  return layout_object->IsSVGShape() || layout_object->IsSVGText() ||
         layout_object->IsSVGImage() ||
         IsA<SVGUseElement>(*layout_object->GetNode());
}

bool SVGSVGElement::CheckEnclosure(const SVGElement& element,
                                   const gfx::RectF& rect) const {
  const LayoutObject* layout_object = element.GetLayoutObject();
  if (!IsEnclosureTarget(layout_object)) {
    return false;
  }

  AffineTransform ctm =
      To<SVGGraphicsElement>(element).ComputeCTM(kAncestorScope, this);
  gfx::RectF visual_rect = layout_object->VisualRectInLocalSVGCoordinates();
  SVGLayoutSupport::AdjustWithClipPathAndMask(
      *layout_object, layout_object->ObjectBoundingBox(), visual_rect);
  gfx::RectF mapped_repaint_rect = ctm.MapRect(visual_rect);
  return rect.Contains(mapped_repaint_rect);
}

StaticNodeList* SVGSVGElement::getEnclosureList(
    SVGRectTearOff* query_rect,
    SVGElement* reference_element) const {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  const gfx::RectF& rect = query_rect->Target()->Rect();
  HeapVector<Member<Node>> nodes;
  if (const SVGElement* root =
          InnermostCommonSubtreeRoot(*this, reference_element)) {
    for (SVGGraphicsElement& element :
         Traversal<SVGGraphicsElement>::DescendantsOf(*root)) {
      if (CheckEnclosure(element, rect)) {
        nodes.push_back(&element);
      }
    }
  }
  return StaticNodeList::Adopt(nodes);
}

bool SVGSVGElement::checkEnclosure(SVGElement* element,
                                   SVGRectTearOff* rect) const {
  DCHECK(element);
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  return CheckEnclosure(*element, rect->Target()->Rect());
}

void SVGSVGElement::deselectAll() {
  if (LocalFrame* frame = GetDocument().GetFrame())
    frame->Selection().Clear();
}

SVGNumberTearOff* SVGSVGElement::createSVGNumber() {
  return SVGNumberTearOff::CreateDetached();
}

SVGLengthTearOff* SVGSVGElement::createSVGLength() {
  return SVGLengthTearOff::CreateDetached();
}

SVGAngleTearOff* SVGSVGElement::createSVGAngle() {
  return SVGAngleTearOff::CreateDetached();
}

SVGPointTearOff* SVGSVGElement::createSVGPoint() {
  return SVGPointTearOff::CreateDetached(gfx::PointF(0, 0));
}

SVGMatrixTearOff* SVGSVGElement::createSVGMatrix() {
  return MakeGarbageCollected<SVGMatrixTearOff>(AffineTransform());
}

SVGRectTearOff* SVGSVGElement::createSVGRect() {
  return SVGRectTearOff::CreateDetached(0, 0, 0, 0);
}

SVGTransformTearOff* SVGSVGElement::createSVGTransform() {
  return SVGTransformTearOff::CreateDetached();
}

SVGTransformTearOff* SVGSVGElement::createSVGTransformFromMatrix(
    SVGMatrixTearOff* matrix) {
  return MakeGarbageCollected<SVGTransformTearOff>(matrix);
}

AffineTransform SVGSVGElement::LocalCoordinateSpaceTransform(
    CTMScope mode) const {
  const LayoutObject* layout_object = GetLayoutObject();
  gfx::SizeF viewport_size;
  AffineTransform transform;
  if (!IsOutermostSVGSVGElement()) {
    if (layout_object) {
      transform.PreConcat(
          To<LayoutSVGViewportContainer>(*layout_object).LocalSVGTransform());
    }

    SVGLengthContext length_context(this);
    transform.Translate(x_->CurrentValue()->Value(length_context),
                        y_->CurrentValue()->Value(length_context));
    if (layout_object) {
      viewport_size =
          To<LayoutSVGViewportContainer>(*layout_object).Viewport().size();
    }
  } else if (layout_object) {
    if (mode == kScreenScope) {
      gfx::Transform matrix;
      // Adjust for the zoom level factored into CSS coordinates (WK bug
      // #96361).
      matrix.Scale(1.0 / layout_object->View()->StyleRef().EffectiveZoom());

      // Apply transforms from our ancestor coordinate space, including any
      // non-SVG ancestor transforms.
      matrix.PreConcat(layout_object->LocalToAbsoluteTransform());

      // At the SVG/HTML boundary (aka LayoutSVGRoot), we need to apply the
      // localToBorderBoxTransform to map an element from SVG viewport
      // coordinates to CSS box coordinates.
      matrix.PreConcat(To<LayoutSVGRoot>(layout_object)
                           ->LocalToBorderBoxTransform()
                           .ToTransform());
      // Drop any potential non-affine parts, because we're not able to convey
      // that information further anyway until getScreenCTM returns a DOMMatrix
      // (4x4 matrix.)
      return AffineTransform::FromTransform(matrix);
    }
    viewport_size = To<LayoutSVGRoot>(*layout_object).ViewportSize();
  }
  if (!HasEmptyViewBox()) {
    transform.PreConcat(ViewBoxToViewTransform(viewport_size));
  }
  return transform;
}

bool SVGSVGElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  // FIXME: We should respect display: none on the documentElement svg element
  // but many things in LocalFrameView and SVGImage depend on the LayoutSVGRoot
  // when they should instead depend on the LayoutView.
  // https://bugs.webkit.org/show_bug.cgi?id=103493
  if (IsDocumentElement())
    return true;

  // <svg> elements don't need an SVG parent to render, so we bypass
  // SVGElement::layoutObjectIsNeeded.
  return IsValid() && Element::LayoutObjectIsNeeded(style);
}

void SVGSVGElement::AttachLayoutTree(AttachContext& context) {
  SVGViewportContainerElement::AttachLayoutTree(context);

  if (GetLayoutObject() && GetLayoutObject()->IsSVGRoot()) {
    To<LayoutSVGRoot>(GetLayoutObject())->IntrinsicSizingInfoChanged();
  }
}

LayoutObject* SVGSVGElement::CreateLayoutObject(const ComputedStyle&) {
  if (IsOutermostSVGSVGElement())
    return MakeGarbageCollected<LayoutSVGRoot>(this);

  return MakeGarbageCollected<LayoutSVGViewportContainer>(this);
}

Node::InsertionNotificationRequest SVGSVGElement::InsertedInto(
    ContainerNode& root_parent) {
  if (root_parent.isConnected()) {
    UseCounter::Count(GetDocument(), WebFeature::kSVGSVGElementInDocument);
    if (IsA<XMLDocument>(root_parent.GetDocument()))
      UseCounter::Count(GetDocument(), WebFeature::kSVGSVGElementInXMLDocument);

    GetDocument().AccessSVGExtensions().AddTimeContainer(this);

    // Animations are started at the end of document parsing and after firing
    // the load event, but if we miss that train (deferred programmatic
    // element insertion for example) we need to initialize the time container
    // here.
    if (!GetDocument().Parsing() && GetDocument().LoadEventFinished() &&
        !TimeContainer()->IsStarted())
      TimeContainer()->Start();
  }
  return SVGViewportContainerElement::InsertedInto(root_parent);
}

void SVGSVGElement::RemovedFrom(ContainerNode& root_parent) {
  if (root_parent.isConnected()) {
    SVGDocumentExtensions& svg_extensions = GetDocument().AccessSVGExtensions();
    svg_extensions.RemoveTimeContainer(this);
  }

  SVGViewportContainerElement::RemovedFrom(root_parent);
}

void SVGSVGElement::pauseAnimations() {
  if (!time_container_->IsPaused())
    time_container_->Pause();
}

void SVGSVGElement::unpauseAnimations() {
  if (time_container_->IsPaused())
    time_container_->Unpause();
}

bool SVGSVGElement::animationsPaused() const {
  return time_container_->IsPaused();
}

float SVGSVGElement::getCurrentTime() const {
  return ClampTo<float>(time_container_->Elapsed().InSecondsF());
}

void SVGSVGElement::setCurrentTime(float seconds) {
  DCHECK(std::isfinite(seconds));
  time_container_->SetElapsed(SMILTime::FromSecondsD(std::max(seconds, 0.0f)));
}

bool SVGSVGElement::ShouldSynthesizeViewBox() const {
  if (!IsDocumentElement())
    return false;
  const auto* svg_root = DynamicTo<LayoutSVGRoot>(GetLayoutObject());
  return svg_root && svg_root->IsEmbeddedThroughSVGImage();
}

const SVGRect& SVGSVGElement::CurrentViewBox() const {
  if (view_spec_ && view_spec_->ViewBox()) {
    return *view_spec_->ViewBox();
  }
  return SVGViewportContainerElement::CurrentViewBox();
}

gfx::RectF SVGSVGElement::CurrentViewBoxRect() const {
  gfx::RectF use_view_box = SVGViewportContainerElement::CurrentViewBoxRect();
  if (!use_view_box.IsEmpty() || !ShouldSynthesizeViewBox()) {
    return use_view_box;
  }

  // If no viewBox is specified but non-relative width/height values, then we
  // should always synthesize a viewBox if we're embedded through a SVGImage.
  SVGLengthContext length_context(this);
  gfx::SizeF synthesized_view_box_size(
      width()->CurrentValue()->Value(length_context),
      height()->CurrentValue()->Value(length_context));
  return gfx::RectF(synthesized_view_box_size);
}

const SVGPreserveAspectRatio* SVGSVGElement::CurrentPreserveAspectRatio()
    const {
  if (view_spec_ && view_spec_->PreserveAspectRatio())
    return view_spec_->PreserveAspectRatio();

  if (!HasValidViewBox(CurrentViewBox()) && ShouldSynthesizeViewBox()) {
    // If no (valid) viewBox is specified and we're embedded through SVGImage,
    // then synthesize a pAR with the value 'none'.
    auto* synthesized_par = MakeGarbageCollected<SVGPreserveAspectRatio>();
    synthesized_par->SetAlign(
        SVGPreserveAspectRatio::kSvgPreserveaspectratioNone);
    return synthesized_par;
  }
  return SVGViewportContainerElement::CurrentPreserveAspectRatio();
}

std::optional<float> SVGSVGElement::IntrinsicWidth() const {
  const SVGLength& width_attr = *width()->CurrentValue();
  // TODO(crbug.com/979895): This is the result of a refactoring, which might
  // have revealed an existing bug that we are not handling math functions
  // involving percentages correctly. Fix it if necessary.
  if (width_attr.IsPercentage())
    return std::nullopt;
  SVGLengthContext length_context(this);
  return std::max(0.0f, width_attr.Value(length_context));
}

std::optional<float> SVGSVGElement::IntrinsicHeight() const {
  const SVGLength& height_attr = *height()->CurrentValue();
  // TODO(crbug.com/979895): This is the result of a refactoring, which might
  // have revealed an existing bug that we are not handling math functions
  // involving percentages correctly. Fix it if necessary.
  if (height_attr.IsPercentage())
    return std::nullopt;
  SVGLengthContext length_context(this);
  return std::max(0.0f, height_attr.Value(length_context));
}

AffineTransform SVGSVGElement::ViewBoxToViewTransform(
    const gfx::SizeF& viewport_size) const {
  AffineTransform ctm =
      SVGViewportContainerElement::ViewBoxToViewTransform(viewport_size);
  if (!view_spec_ || !view_spec_->Transform()) {
    return ctm;
  }
  const SVGTransformList* transform_list = view_spec_->Transform();
  if (!transform_list->IsEmpty()) {
    ctm *= transform_list->Concatenate();
  }
  return ctm;
}

void SVGSVGElement::SetViewSpec(const SVGViewSpec* view_spec) {
  // Even if the viewspec object itself doesn't change, it could still
  // have been mutated, so only treat a "no viewspec" -> "no viewspec"
  // transition as a no-op.
  if (!view_spec_ && !view_spec)
    return;
  view_spec_ = view_spec;
  if (LayoutObject* layout_object = GetLayoutObject()) {
    if (auto* svg_root = DynamicTo<LayoutSVGRoot>(*layout_object)) {
      svg_root->IntrinsicSizingInfoChanged();
    }
    MarkForLayoutAndParentResourceInvalidation(*layout_object);
  }
}

const SVGViewSpec* SVGSVGElement::ParseViewSpec(
    const String& fragment_identifier,
    Element* anchor_node) const {
  if (fragment_identifier.StartsWith("svgView(")) {
    const SVGViewSpec* view_spec =
        SVGViewSpec::CreateFromFragment(fragment_identifier);
    if (view_spec) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kSVGSVGElementFragmentSVGView);
      return view_spec;
    }
  }
  if (auto* svg_view_element = DynamicTo<SVGViewElement>(anchor_node)) {
    // Spec: If the SVG fragment identifier addresses a 'view' element within an
    // SVG document (e.g., MyDrawing.svg#MyView) then the root 'svg' element is
    // displayed in the SVG viewport. Any view specification attributes included
    // on the given 'view' element override the corresponding view specification
    // attributes on the root 'svg' element.
    const SVGViewSpec* view_spec =
        SVGViewSpec::CreateForViewElement(*svg_view_element);
    UseCounter::Count(GetDocument(),
                      WebFeature::kSVGSVGElementFragmentSVGViewElement);
    return view_spec;
  }
  return nullptr;
}

void SVGSVGElement::FinishParsingChildren() {
  SVGViewportContainerElement::FinishParsingChildren();

  // The outermost SVGSVGElement SVGLoad event is fired through
  // LocalDOMWindow::dispatchWindowLoadEvent.
  if (IsOutermostSVGSVGElement())
    return;

  // finishParsingChildren() is called when the close tag is reached for an
  // element (e.g. </svg>) we send SVGLoad events here if we can, otherwise
  // they'll be sent when any required loads finish
  SendSVGLoadEventIfPossible();
}

void SVGSVGElement::Trace(Visitor* visitor) const {
  visitor->Trace(translation_);
  visitor->Trace(time_container_);
  visitor->Trace(view_spec_);
  SVGViewportContainerElement::Trace(visitor);
}

}  // namespace blink
