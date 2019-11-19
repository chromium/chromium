/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"

#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/layout/subtree_layout_scope.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

struct SearchCandidate {
  SearchCandidate()
      : layout_object(nullptr), distance(std::numeric_limits<float>::max()) {}
  SearchCandidate(LayoutObject* layout_object, float distance)
      : layout_object(layout_object), distance(distance) {}
  LayoutObject* layout_object;
  float distance;
};

FloatRect SVGLayoutSupport::LocalVisualRect(const LayoutObject& object) {
  // For LayoutSVGRoot, use LayoutSVGRoot::localVisualRect() instead.
  DCHECK(!object.IsSVGRoot());

  // Return early for any cases where we don't actually paint
  if (object.StyleRef().Visibility() != EVisibility::kVisible &&
      !object.EnclosingLayer()->HasVisibleContent())
    return FloatRect();

  FloatRect visual_rect = object.VisualRectInLocalSVGCoordinates();
  if (int outline_outset = object.StyleRef().OutlineOutsetExtent())
    visual_rect.Inflate(outline_outset);
  return visual_rect;
}

PhysicalRect SVGLayoutSupport::VisualRectInAncestorSpace(
    const LayoutObject& object,
    const LayoutBoxModelObject& ancestor,
    VisualRectFlags flags) {
  PhysicalRect rect;
  MapToVisualRectInAncestorSpace(object, &ancestor, LocalVisualRect(object),
                                 rect, flags);
  return rect;
}

PhysicalRect SVGLayoutSupport::TransformVisualRect(
    const LayoutObject& object,
    const AffineTransform& root_transform,
    const FloatRect& local_rect) {
  FloatRect adjusted_rect = root_transform.MapRect(local_rect);

  if (adjusted_rect.IsEmpty())
    return PhysicalRect();

  // Use EnclosingIntRect because we cannot properly apply subpixel offset of
  // the SVGRoot since we don't know the desired subpixel accumulation at this
  // point.
  return PhysicalRect(EnclosingIntRect(adjusted_rect));
}

static const LayoutSVGRoot& ComputeTransformToSVGRoot(
    const LayoutObject& object,
    AffineTransform& root_border_box_transform) {
  DCHECK(object.IsSVGChild());

  const LayoutObject* parent;
  for (parent = &object; !parent->IsSVGRoot(); parent = parent->Parent())
    root_border_box_transform.PreMultiply(parent->LocalToSVGParentTransform());

  const LayoutSVGRoot& svg_root = ToLayoutSVGRoot(*parent);
  root_border_box_transform.PreMultiply(svg_root.LocalToBorderBoxTransform());
  return svg_root;
}

bool SVGLayoutSupport::MapToVisualRectInAncestorSpace(
    const LayoutObject& object,
    const LayoutBoxModelObject* ancestor,
    const FloatRect& local_visual_rect,
    PhysicalRect& result_rect,
    VisualRectFlags visual_rect_flags) {
  AffineTransform root_border_box_transform;
  const LayoutSVGRoot& svg_root =
      ComputeTransformToSVGRoot(object, root_border_box_transform);
  result_rect =
      TransformVisualRect(object, root_border_box_transform, local_visual_rect);

  // Apply initial viewport clip.
  if (svg_root.ShouldApplyViewportClip()) {
    PhysicalRect clip_rect(svg_root.OverflowClipRect(PhysicalOffset()));
    if (visual_rect_flags & kEdgeInclusive) {
      if (!result_rect.InclusiveIntersect(clip_rect))
        return false;
    } else {
      result_rect.Intersect(clip_rect);
    }
  }
  return svg_root.MapToVisualRectInAncestorSpace(ancestor, result_rect,
                                                 visual_rect_flags);
}

void SVGLayoutSupport::MapLocalToAncestor(const LayoutObject* object,
                                          const LayoutBoxModelObject* ancestor,
                                          TransformState& transform_state,
                                          MapCoordinatesFlags flags) {
  transform_state.ApplyTransform(object->LocalToSVGParentTransform());

  LayoutObject* parent = object->Parent();

  // At the SVG/HTML boundary (aka LayoutSVGRoot), we apply the
  // localToBorderBoxTransform to map an element from SVG viewport coordinates
  // to CSS box coordinates.
  // LayoutSVGRoot's mapLocalToAncestor method expects CSS box coordinates.
  if (parent->IsSVGRoot())
    transform_state.ApplyTransform(
        ToLayoutSVGRoot(parent)->LocalToBorderBoxTransform());

  parent->MapLocalToAncestor(ancestor, transform_state, flags);
}

void SVGLayoutSupport::MapAncestorToLocal(const LayoutObject& object,
                                          const LayoutBoxModelObject* ancestor,
                                          TransformState& transform_state,
                                          MapCoordinatesFlags flags) {
  // |object| is either a LayoutSVGModelObject or a LayoutSVGBlock here. In
  // the former case, |object| can never be an ancestor while in the latter
  // the caller is responsible for doing the ancestor check. Because of this,
  // computing the transform to the SVG root is always what we want to do here.
  DCHECK_NE(ancestor, &object);
  DCHECK(object.IsSVGContainer() || object.IsSVGShape() ||
         object.IsSVGImage() || object.IsSVGText() ||
         object.IsSVGForeignObject());
  AffineTransform local_to_svg_root;
  const LayoutSVGRoot& svg_root =
      ComputeTransformToSVGRoot(object, local_to_svg_root);

  svg_root.MapAncestorToLocal(ancestor, transform_state, flags);

  transform_state.ApplyTransform(local_to_svg_root);
}

const LayoutObject* SVGLayoutSupport::PushMappingToContainer(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) {
  DCHECK_NE(ancestor_to_stop_at, object);

  LayoutObject* parent = object->Parent();

  // At the SVG/HTML boundary (aka LayoutSVGRoot), we apply the
  // localToBorderBoxTransform to map an element from SVG viewport coordinates
  // to CSS box coordinates.
  // LayoutSVGRoot's mapLocalToAncestor method expects CSS box coordinates.
  if (parent->IsSVGRoot()) {
    TransformationMatrix matrix(
        ToLayoutSVGRoot(parent)->LocalToBorderBoxTransform());
    matrix.Multiply(object->LocalToSVGParentTransform());
    geometry_map.Push(object, matrix);
  } else {
    geometry_map.Push(object, object->LocalToSVGParentTransform());
  }

  return parent;
}

// Update a bounding box taking into account the validity of the other bounding
// box.
inline void SVGLayoutSupport::UpdateObjectBoundingBox(
    FloatRect& object_bounding_box,
    bool& object_bounding_box_valid,
    LayoutObject* other,
    FloatRect other_bounding_box) {
  bool other_valid =
      other->IsSVGContainer()
          ? ToLayoutSVGContainer(other)->IsObjectBoundingBoxValid()
          : true;
  if (!other_valid)
    return;

  if (!object_bounding_box_valid) {
    object_bounding_box = other_bounding_box;
    object_bounding_box_valid = true;
    return;
  }

  object_bounding_box.UniteEvenIfEmpty(other_bounding_box);
}

static bool HasValidBoundingBoxForContainer(const LayoutObject* object) {
  if (object->IsSVGShape())
    return !ToLayoutSVGShape(object)->IsShapeEmpty();

  if (object->IsSVGText())
    return ToLayoutSVGText(object)->IsObjectBoundingBoxValid();

  if (object->IsSVGHiddenContainer())
    return false;

  if (object->IsSVGForeignObject())
    return ToLayoutSVGForeignObject(object)->IsObjectBoundingBoxValid();

  if (object->IsSVGImage())
    return ToLayoutSVGImage(object)->IsObjectBoundingBoxValid();

  // TODO(fs): Can we refactor this code to include the container case
  // in a more natural way?
  return true;
}

void SVGLayoutSupport::ComputeContainerBoundingBoxes(
    const LayoutObject* container,
    FloatRect& object_bounding_box,
    bool& object_bounding_box_valid,
    FloatRect& stroke_bounding_box,
    FloatRect& local_visual_rect) {
  object_bounding_box = FloatRect();
  object_bounding_box_valid = false;
  stroke_bounding_box = FloatRect();

  // When computing the strokeBoundingBox, we use the visualRects of
  // the container's children so that the container's stroke includes the
  // resources applied to the children (such as clips and filters). This allows
  // filters applied to containers to correctly bound the children, and also
  // improves inlining of SVG content, as the stroke bound is used in that
  // situation also.
  for (LayoutObject* current = container->SlowFirstChild(); current;
       current = current->NextSibling()) {
    // Don't include elements that are not rendered in the union.
    if (!HasValidBoundingBoxForContainer(current))
      continue;

    const AffineTransform& transform = current->LocalToSVGParentTransform();
    UpdateObjectBoundingBox(object_bounding_box, object_bounding_box_valid,
                            current,
                            transform.MapRect(current->ObjectBoundingBox()));
    stroke_bounding_box.Unite(
        transform.MapRect(current->VisualRectInLocalSVGCoordinates()));
  }

  local_visual_rect = stroke_bounding_box;
  AdjustVisualRectWithResources(*container, object_bounding_box,
                                local_visual_rect);
}

bool SVGLayoutSupport::LayoutSizeOfNearestViewportChanged(
    const LayoutObject* start) {
  for (; start; start = start->Parent()) {
    if (start->IsSVGRoot())
      return ToLayoutSVGRoot(start)->IsLayoutSizeChanged();
    if (start->IsSVGViewportContainer())
      return ToLayoutSVGViewportContainer(start)->IsLayoutSizeChanged();
  }
  NOTREACHED();
  return false;
}

bool SVGLayoutSupport::ScreenScaleFactorChanged(const LayoutObject* ancestor) {
  for (; ancestor; ancestor = ancestor->Parent()) {
    if (ancestor->IsSVGRoot())
      return ToLayoutSVGRoot(ancestor)->DidScreenScaleFactorChange();
    if (ancestor->IsSVGTransformableContainer())
      return ToLayoutSVGTransformableContainer(ancestor)
          ->DidScreenScaleFactorChange();
    if (ancestor->IsSVGViewportContainer())
      return ToLayoutSVGViewportContainer(ancestor)
          ->DidScreenScaleFactorChange();
  }
  NOTREACHED();
  return false;
}

void SVGLayoutSupport::LayoutChildren(LayoutObject* first_child,
                                      bool force_layout,
                                      bool screen_scaling_factor_changed,
                                      bool layout_size_changed) {
  for (LayoutObject* child = first_child; child; child = child->NextSibling()) {
    bool force_child_layout = force_layout;

    if (screen_scaling_factor_changed) {
      // If the screen scaling factor changed we need to update the text
      // metrics (note: this also happens for layoutSizeChanged=true).
      if (child->IsSVGText())
        ToLayoutSVGText(child)->SetNeedsTextMetricsUpdate();
      force_child_layout = true;
    }

    if (layout_size_changed) {
      // When selfNeedsLayout is false and the layout size changed, we have to
      // check whether this child uses relative lengths
      if (auto* element = DynamicTo<SVGElement>(child->GetNode())) {
        if (element->HasRelativeLengths()) {
          // FIXME: this should be done on invalidation, not during layout.
          // When the layout size changed and when using relative values tell
          // the LayoutSVGShape to update its shape object
          if (child->IsSVGShape()) {
            ToLayoutSVGShape(child)->SetNeedsShapeUpdate();
          } else if (child->IsSVGText()) {
            ToLayoutSVGText(child)->SetNeedsTextMetricsUpdate();
            ToLayoutSVGText(child)->SetNeedsPositioningValuesUpdate();
          }

          force_child_layout = true;
        }
      }
    }

    // Resource containers are nasty: they can invalidate clients outside the
    // current SubtreeLayoutScope.
    // Since they only care about viewport size changes (to resolve their
    // relative lengths), we trigger their invalidation directly from
    // SVGSVGElement::svgAttributeChange() or at a higher SubtreeLayoutScope (in
    // LayoutView::layout()). We do not create a SubtreeLayoutScope for
    // resources because their ability to reference each other leads to circular
    // layout. We protect against that within the layout code for resources, but
    // it causes assertions if we use a SubTreeLayoutScope for them.
    if (child->IsSVGResourceContainer()) {
      // Lay out any referenced resources before the child.
      LayoutResourcesIfNeeded(*child);
      child->LayoutIfNeeded();
    } else {
      SubtreeLayoutScope layout_scope(*child);
      if (force_child_layout) {
        layout_scope.SetNeedsLayout(child,
                                    layout_invalidation_reason::kSvgChanged);
      }

      // Lay out any referenced resources before the child.
      LayoutResourcesIfNeeded(*child);
      child->LayoutIfNeeded();
    }
  }
}

void SVGLayoutSupport::LayoutResourcesIfNeeded(const LayoutObject& object) {
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object);
  if (resources)
    resources->LayoutIfNeeded();
}

bool SVGLayoutSupport::IsOverflowHidden(const LayoutObject& object) {
  // LayoutSVGRoot should never query for overflow state - it should always clip
  // itself to the initial viewport size.
  DCHECK(!object.IsDocumentElement());
  return IsOverflowHidden(object.StyleRef());
}

bool SVGLayoutSupport::IsOverflowHidden(const ComputedStyle& style) {
  return style.OverflowX() == EOverflow::kHidden ||
         style.OverflowX() == EOverflow::kScroll;
}

void SVGLayoutSupport::AdjustVisualRectWithResources(
    const LayoutObject& layout_object,
    const FloatRect& object_bounding_box,
    FloatRect& visual_rect) {
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(layout_object);
  if (!resources)
    return;

  if (LayoutSVGResourceFilter* filter = resources->Filter())
    visual_rect = filter->ResourceBoundingBox(object_bounding_box);

  if (LayoutSVGResourceClipper* clipper = resources->Clipper())
    visual_rect.Intersect(clipper->ResourceBoundingBox(object_bounding_box));

  if (LayoutSVGResourceMasker* masker = resources->Masker())
    visual_rect.Intersect(masker->ResourceBoundingBox(object_bounding_box));
}

bool SVGLayoutSupport::HasFilterResource(const LayoutObject& object) {
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object);
  return resources && resources->Filter();
}

bool SVGLayoutSupport::IntersectsClipPath(const LayoutObject& object,
                                          const FloatRect& reference_box,
                                          const HitTestLocation& location) {
  ClipPathOperation* clip_path_operation = object.StyleRef().ClipPath();
  if (!clip_path_operation)
    return true;
  if (clip_path_operation->GetType() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation& clip_path =
        To<ShapeClipPathOperation>(*clip_path_operation);
    return clip_path.GetPath(reference_box)
        .Contains(location.TransformedPoint());
  }
  DCHECK_EQ(clip_path_operation->GetType(), ClipPathOperation::REFERENCE);
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object);
  if (!resources || !resources->Clipper())
    return true;
  return resources->Clipper()->HitTestClipContent(reference_box, location);
}

bool SVGLayoutSupport::HitTestChildren(LayoutObject* last_child,
                                       HitTestResult& result,
                                       const HitTestLocation& location,
                                       const PhysicalOffset& accumulated_offset,
                                       HitTestAction hit_test_action) {
  for (LayoutObject* child = last_child; child;
       child = child->PreviousSibling()) {
    if (child->IsSVGForeignObject()) {
      if (ToLayoutSVGForeignObject(child)->NodeAtPointFromSVG(
              result, location, accumulated_offset, hit_test_action))
        return true;
    } else {
      if (child->NodeAtPoint(result, location, accumulated_offset,
                             hit_test_action))
        return true;
    }
  }
  return false;
}

DashArray SVGLayoutSupport::ResolveSVGDashArray(
    const SVGDashArray& svg_dash_array,
    const ComputedStyle& style,
    const SVGLengthContext& length_context) {
  DashArray dash_array;
  for (const Length& dash_length : svg_dash_array.data)
    dash_array.push_back(length_context.ValueForLength(dash_length, style));
  return dash_array;
}

void SVGLayoutSupport::ApplyStrokeStyleToStrokeData(StrokeData& stroke_data,
                                                    const ComputedStyle& style,
                                                    const LayoutObject& object,
                                                    float dash_scale_factor) {
  DCHECK(object.GetNode());
  DCHECK(object.GetNode()->IsSVGElement());

  const SVGComputedStyle& svg_style = style.SvgStyle();

  SVGLengthContext length_context(To<SVGElement>(object.GetNode()));
  stroke_data.SetThickness(
      length_context.ValueForLength(svg_style.StrokeWidth()));
  stroke_data.SetLineCap(svg_style.CapStyle());
  stroke_data.SetLineJoin(svg_style.JoinStyle());
  stroke_data.SetMiterLimit(svg_style.StrokeMiterLimit());

  DashArray dash_array =
      ResolveSVGDashArray(*svg_style.StrokeDashArray(), style, length_context);
  float dash_offset =
      length_context.ValueForLength(svg_style.StrokeDashOffset(), style);
  // Apply scaling from 'pathLength'.
  if (dash_scale_factor != 1) {
    DCHECK_GE(dash_scale_factor, 0);
    dash_offset *= dash_scale_factor;
    for (auto& dash_item : dash_array)
      dash_item *= dash_scale_factor;
  }
  stroke_data.SetLineDash(dash_array, dash_offset);
}

bool SVGLayoutSupport::IsLayoutableTextNode(const LayoutObject* object) {
  DCHECK(object->IsText());
  // <br> is marked as text, but is not handled by the SVG layout code-path.
  return object->IsSVGInlineText() &&
         !ToLayoutSVGInlineText(object)->HasEmptyText();
}

bool SVGLayoutSupport::WillIsolateBlendingDescendantsForStyle(
    const ComputedStyle& style) {
  const SVGComputedStyle& svg_style = style.SvgStyle();

  return style.HasIsolation() || style.HasOpacity() || style.HasBlendMode() ||
         style.HasFilter() || svg_style.HasMasker() || style.ClipPath();
}

bool SVGLayoutSupport::WillIsolateBlendingDescendantsForObject(
    const LayoutObject* object) {
  if (object->IsSVGHiddenContainer())
    return false;
  if (!object->IsSVGRoot() && !object->IsSVGContainer())
    return false;
  return WillIsolateBlendingDescendantsForStyle(object->StyleRef());
}

bool SVGLayoutSupport::IsIsolationRequired(const LayoutObject* object) {
  return WillIsolateBlendingDescendantsForObject(object) &&
         object->HasNonIsolatedBlendingDescendants();
}

AffineTransform::Transform
    SubtreeContentTransformScope::current_content_transformation_ =
        IDENTITY_TRANSFORM;

SubtreeContentTransformScope::SubtreeContentTransformScope(
    const AffineTransform& subtree_content_transformation)
    : saved_content_transformation_(current_content_transformation_) {
  AffineTransform content_transformation =
      subtree_content_transformation *
      AffineTransform(current_content_transformation_);
  content_transformation.CopyTransformTo(current_content_transformation_);
}

SubtreeContentTransformScope::~SubtreeContentTransformScope() {
  saved_content_transformation_.CopyTransformTo(
      current_content_transformation_);
}

AffineTransform SVGLayoutSupport::DeprecatedCalculateTransformToLayer(
    const LayoutObject* layout_object) {
  AffineTransform transform;
  while (layout_object) {
    transform = layout_object->LocalToSVGParentTransform() * transform;
    if (layout_object->IsSVGRoot())
      break;
    layout_object = layout_object->Parent();
  }

  // Continue walking up the layer tree, accumulating CSS transforms.
  // FIXME: this queries layer compositing state - which is not
  // supported during layout. Hence, the result may not include all CSS
  // transforms.
  PaintLayer* layer = layout_object ? layout_object->EnclosingLayer() : nullptr;
  while (layer && layer->IsAllowedToQueryCompositingState()) {
    // We can stop at compositing layers, to match the backing resolution.
    // FIXME: should we be computing the transform to the nearest composited
    // layer, or the nearest composited layer that does not paint into its
    // ancestor? I think this is the nearest composited ancestor since we will
    // inherit its transforms in the composited layer tree.
    if (layer->GetCompositingState() != kNotComposited)
      break;

    if (TransformationMatrix* layer_transform = layer->Transform())
      transform = layer_transform->ToAffineTransform() * transform;

    layer = layer->Parent();
  }

  return transform;
}

float SVGLayoutSupport::CalculateScreenFontSizeScalingFactor(
    const LayoutObject* layout_object) {
  DCHECK(layout_object);

  // FIXME: trying to compute a device space transform at record time is wrong.
  // All clients should be updated to avoid relying on this information, and the
  // method should be removed.
  AffineTransform ctm =
      DeprecatedCalculateTransformToLayer(layout_object) *
      SubtreeContentTransformScope::CurrentContentTransformation();
  ctm.Scale(
      layout_object->GetDocument().GetPage()->DeviceScaleFactorDeprecated());

  return clampTo<float>(sqrt((ctm.XScaleSquared() + ctm.YScaleSquared()) / 2));
}

static inline bool CompareCandidateDistance(const SearchCandidate& r1,
                                            const SearchCandidate& r2) {
  return r1.distance < r2.distance;
}

static inline float DistanceToChildLayoutObject(LayoutObject* child,
                                                const FloatPoint& point) {
  const AffineTransform& local_to_parent_transform =
      child->LocalToSVGParentTransform();
  if (!local_to_parent_transform.IsInvertible())
    return std::numeric_limits<float>::max();
  FloatPoint child_local_point =
      local_to_parent_transform.Inverse().MapPoint(point);
  return child->ObjectBoundingBox().SquaredDistanceTo(child_local_point);
}

static SearchCandidate SearchTreeForFindClosestLayoutSVGText(
    const LayoutObject* layout_object,
    const FloatPoint& point) {
  // Try to find the closest LayoutSVGText.
  SearchCandidate closest_text;
  Vector<SearchCandidate> candidates;

  // Find the closest LayoutSVGText on this tree level, and also collect any
  // containers that could contain LayoutSVGTexts that are closer.
  for (LayoutObject* child = layout_object->SlowLastChild(); child;
       child = child->PreviousSibling()) {
    if (child->IsSVGText()) {
      float distance = DistanceToChildLayoutObject(child, point);
      if (distance >= closest_text.distance)
        continue;
      candidates.clear();
      closest_text.layout_object = child;
      closest_text.distance = distance;
      continue;
    }

    if (child->IsSVGContainer() && !layout_object->IsSVGHiddenContainer()) {
      float distance = DistanceToChildLayoutObject(child, point);
      if (distance > closest_text.distance)
        continue;
      candidates.push_back(SearchCandidate(child, distance));
    }
  }

  // If a LayoutSVGText was found and there are no potentially closer sub-trees,
  // just return |closestText|.
  if (closest_text.layout_object && candidates.IsEmpty())
    return closest_text;

  std::stable_sort(candidates.begin(), candidates.end(),
                   CompareCandidateDistance);

  // Find the closest LayoutSVGText in the sub-trees in |candidates|.
  // If a LayoutSVGText is found that is strictly closer than any previous
  // candidate, then end the search.
  for (const SearchCandidate& search_candidate : candidates) {
    if (closest_text.distance < search_candidate.distance)
      break;
    LayoutObject* candidate_layout_object = search_candidate.layout_object;
    FloatPoint candidate_local_point =
        candidate_layout_object->LocalToSVGParentTransform().Inverse().MapPoint(
            point);

    SearchCandidate candidate_text = SearchTreeForFindClosestLayoutSVGText(
        candidate_layout_object, candidate_local_point);

    if (candidate_text.distance < closest_text.distance)
      closest_text = candidate_text;
  }

  return closest_text;
}

LayoutObject* SVGLayoutSupport::FindClosestLayoutSVGText(
    const LayoutObject* layout_object,
    const FloatPoint& point) {
  return SearchTreeForFindClosestLayoutSVGText(layout_object, point)
      .layout_object;
}

}  // namespace blink
