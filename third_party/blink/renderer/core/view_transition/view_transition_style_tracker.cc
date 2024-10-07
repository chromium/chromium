// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"

#include <limits>

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/layout_view_transition_root.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_size.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_view_transition_group.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_content_element.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_style_builder.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_transition_element.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
namespace {

const char* kDuplicateTagBaseError =
    "Unexpected duplicate view-transition-name: ";

CSSPropertyID kPropertiesToCapture[] = {
    CSSPropertyID::kBackdropFilter, CSSPropertyID::kColorScheme,
    CSSPropertyID::kMixBlendMode,   CSSPropertyID::kTextOrientation,
    CSSPropertyID::kWritingMode,
};

CSSPropertyID kLayeredCaptureProperties[] = {
    CSSPropertyID::kOpacity,
    CSSPropertyID::kClipPath,
    CSSPropertyID::kFilter,
    // Deliberately capturing the shorthand, to include all the mask-related
    // properties.
    CSSPropertyID::kMask,
};

CSSPropertyID kPropertiesToAnimate[] = {
    CSSPropertyID::kBackdropFilter, CSSPropertyID::kOpacity,
    CSSPropertyID::kClipPath,       CSSPropertyID::kFilter,
    CSSPropertyID::kMask,
};

template <typename K, typename V>
class FlatMapBuilder {
 public:
  explicit FlatMapBuilder(size_t reserve = 0) { data_.reserve(reserve); }

  template <typename... Args>
  void Insert(Args&&... args) {
    data_.emplace_back(std::forward<Args>(args)...);
  }

  base::flat_map<K, V> Finish() && {
    return base::flat_map<K, V>(std::move(data_));
  }

 private:
  std::vector<std::pair<K, V>> data_
      ALLOW_DISCOURAGED_TYPE("flat_map underlying type");
};

mojom::blink::ViewTransitionPropertyId ToTranstionPropertyId(CSSPropertyID id) {
  switch (id) {
    case CSSPropertyID::kBackdropFilter:
      return mojom::blink::ViewTransitionPropertyId::kBackdropFilter;
    case CSSPropertyID::kColorScheme:
      return mojom::blink::ViewTransitionPropertyId::kColorScheme;
    case CSSPropertyID::kMixBlendMode:
      return mojom::blink::ViewTransitionPropertyId::kMixBlendMode;
    case CSSPropertyID::kTextOrientation:
      return mojom::blink::ViewTransitionPropertyId::kTextOrientation;
    case CSSPropertyID::kWritingMode:
      return mojom::blink::ViewTransitionPropertyId::kWritingMode;
    case CSSPropertyID::kOpacity:
      return mojom::blink::ViewTransitionPropertyId::kOpacity;
    case CSSPropertyID::kClipPath:
      return mojom::blink::ViewTransitionPropertyId::kClipPath;
    case CSSPropertyID::kFilter:
      return mojom::blink::ViewTransitionPropertyId::kFilter;
    case CSSPropertyID::kMask:
      return mojom::blink::ViewTransitionPropertyId::kMask;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown id " << static_cast<uint32_t>(id);
  }
  return mojom::blink::ViewTransitionPropertyId::kMinValue;
}

CSSPropertyID FromTransitionPropertyId(
    mojom::blink::ViewTransitionPropertyId id) {
  switch (id) {
    case mojom::blink::ViewTransitionPropertyId::kBackdropFilter:
      return CSSPropertyID::kBackdropFilter;
    case mojom::blink::ViewTransitionPropertyId::kColorScheme:
      return CSSPropertyID::kColorScheme;
    case mojom::blink::ViewTransitionPropertyId::kMixBlendMode:
      return CSSPropertyID::kMixBlendMode;
    case mojom::blink::ViewTransitionPropertyId::kTextOrientation:
      return CSSPropertyID::kTextOrientation;
    case mojom::blink::ViewTransitionPropertyId::kWritingMode:
      return CSSPropertyID::kWritingMode;
    case mojom::blink::ViewTransitionPropertyId::kOpacity:
      return CSSPropertyID::kOpacity;
    case mojom::blink::ViewTransitionPropertyId::kClipPath:
      return CSSPropertyID::kClipPath;
    case mojom::blink::ViewTransitionPropertyId::kFilter:
      return CSSPropertyID::kFilter;
    case mojom::blink::ViewTransitionPropertyId::kMask:
      return CSSPropertyID::kMask;
  }
  return CSSPropertyID::kInvalid;
}

const String& StaticUAStyles() {
  DEFINE_STATIC_LOCAL(
      String, kStaticUAStyles,
      (UncompressResourceAsASCIIString(IDR_UASTYLE_TRANSITION_CSS)));
  return kStaticUAStyles;
}

const String& AnimationUAStyles() {
  DEFINE_STATIC_LOCAL(
      String, kAnimationUAStyles,
      (UncompressResourceAsASCIIString(IDR_UASTYLE_TRANSITION_ANIMATIONS_CSS)));
  return kAnimationUAStyles;
}

// Computes and returns the start offset for element's painting in horizontal or
// vertical direction.
// `start` and `end` denote the offset where the element's ink overflow
// rectangle start and end for a particular direction, relative to the element's
// border box.
// `snapshot_root_dimension` is the length of the snapshot root in the same
// direction.
// `max_capture_size` denotes the maximum bounds we can capture for an element.
float ComputeStartForSide(float start,
                          float end,
                          int snapshot_root_dimension,
                          int max_capture_size) {
  DCHECK_GT((end - start), max_capture_size)
      << "Side must be larger than max texture size";
  DCHECK_GE(max_capture_size, snapshot_root_dimension)
      << "Snapshot root bounds must be a subset of max texture size";
  // In all comments below, | and _ denote the edges for the snapshot root and
  // * denote the edges of the element being captured.

  // This is for the following cases:
  //  ____________
  // |            |
  // |  ******    |
  // |__*____*____|
  //    *    *
  //    ******
  //
  // The element starts after the left edge horizontally or after the top edge
  // vertically and is partially onscreen.
  //  ____________
  // |            |
  // |            |
  // |____________|
  //
  //    ******
  //    *    *
  //    ******
  //
  // The element starts after the left edge horizontally or after the top edge
  // vertically and is completely offscreen.
  //
  // For both these cases, start painting from the left or top edge.
  if (start > 0) {
    return start;
  }

  // This is for the following cases:
  //    ******
  //  __*____*____
  // |  *    *    |
  // |  ******    |
  // |____________|
  //
  // The element ends before the right edge horizontally or before the bottom
  // edge vertically and is partially onscreen.
  //
  //    ******
  //    *    *
  //    ******
  //  ____________
  // |            |
  // |            |
  // |____________|
  //
  // The element ends before the right edge horizontally or before the bottom
  // edge vertically and is completely offscreen.
  //
  // For both these cases, start painting from the right or bottom edge.
  if (end < snapshot_root_dimension) {
    return end - max_capture_size;
  }

  // This is for the following case:
  //    ******
  //  __*____*____
  // |  *    *    |
  // |  *    *    |
  // |__*____*____|
  //    *    *
  //    ******
  //
  // The element covers the complete snapshot root horizontally or vertically
  // and is partially offscreen on both sides.
  //
  // Capture the element's intersection with the snapshot root, inflating it by
  // the remaining margin on both sides. If a side doesn't consume the margin
  // completely, give the remaining capacity to the other side.
  const float delta_to_distribute_per_side =
      (max_capture_size - snapshot_root_dimension) / 2;
  const float delta_on_end_side = end - snapshot_root_dimension;
  const float delta_for_start_side =
      delta_to_distribute_per_side +
      std::max(0.f, (delta_to_distribute_per_side - delta_on_end_side));
  return std::max(start, -delta_for_start_side);
}

// Computes the subset of an element's `ink_overflow_rect_in_border_box_space`
// that should be painted. The return value is relative to the element's border
// box.
// Returns null if the complete ink overflow rect should be painted.
std::optional<gfx::RectF> ComputeCaptureRect(
    const int max_capture_size,
    const PhysicalRect& ink_overflow_rect_in_border_box_space,
    const gfx::Transform& element_to_snapshot_root,
    const gfx::Size& snapshot_root_size) {
  if (ink_overflow_rect_in_border_box_space.Width() <= max_capture_size &&
      ink_overflow_rect_in_border_box_space.Height() <= max_capture_size) {
    return std::nullopt;
  }

  // Compute the matrix to map the element's ink overflow rectangle to snapshot
  // root's coordinate space. This is required to figure out which subset of the
  // element to paint based on its position in the viewport.
  // If the transform is not invertible, fallback to painting from the element's
  // ink overflow rectangle's origin.
  gfx::Transform snapshot_root_to_element;
  if (!element_to_snapshot_root.GetInverse(&snapshot_root_to_element)) {
    gfx::SizeF size(ink_overflow_rect_in_border_box_space.size);
    size.SetToMin(gfx::SizeF(max_capture_size, max_capture_size));
    return gfx::RectF(gfx::PointF(ink_overflow_rect_in_border_box_space.offset),
                      size);
  }

  const gfx::RectF ink_overflow_rect_in_snapshot_root_space =
      element_to_snapshot_root.MapRect(
          gfx::RectF(ink_overflow_rect_in_border_box_space));
  gfx::RectF captured_ink_overflow_subrect_in_snapshot_root_space =
      ink_overflow_rect_in_snapshot_root_space;

  if (ink_overflow_rect_in_snapshot_root_space.width() > max_capture_size) {
    captured_ink_overflow_subrect_in_snapshot_root_space.set_x(
        ComputeStartForSide(ink_overflow_rect_in_snapshot_root_space.x(),
                            ink_overflow_rect_in_snapshot_root_space.right(),
                            snapshot_root_size.width(), max_capture_size));
    captured_ink_overflow_subrect_in_snapshot_root_space.set_width(
        max_capture_size);
  }

  if (ink_overflow_rect_in_snapshot_root_space.height() > max_capture_size) {
    captured_ink_overflow_subrect_in_snapshot_root_space.set_y(
        ComputeStartForSide(ink_overflow_rect_in_snapshot_root_space.y(),
                            ink_overflow_rect_in_snapshot_root_space.bottom(),
                            snapshot_root_size.height(), max_capture_size));
    captured_ink_overflow_subrect_in_snapshot_root_space.set_height(
        max_capture_size);
  }

  return snapshot_root_to_element.MapRect(
      captured_ink_overflow_subrect_in_snapshot_root_space);
}

int ComputeMaxCaptureSize(Document& document,
                          std::optional<int> max_texture_size,
                          const gfx::Size& snapshot_root_size) {
  // If the max texture size is not known yet, use the size of the snapshot
  // root.
  if (!max_texture_size) {
    return std::max(snapshot_root_size.width(), snapshot_root_size.height());
  }

  // The snapshot root corresponds to the maximum screen bounds so we should be
  // able to allocate a buffer of that size. However, Chrome Android's scaling
  // behavior of the position-fixed viewport means the snapshot root may
  // actually be larger than the screen bounds, though it gets scaled down by
  // the page-scale-factor in the compositor. Since this maximum is applied to
  // layout-generated bounds, project it into layout-space by using the minimum
  // possible scale (which is how the position-fixed viewport size is
  // computed).
  const float min_page_scale_factor = document.GetPage()
                                          ->GetPageScaleConstraintsSet()
                                          .FinalConstraints()
                                          .minimum_scale;
  const int max_texture_size_in_layout =
      static_cast<int>(std::ceil(*max_texture_size / min_page_scale_factor));

  LOG_IF(WARNING, snapshot_root_size.width() > max_texture_size_in_layout ||
                      snapshot_root_size.height() > max_texture_size_in_layout)
      << "root snapshot does not fit within max texture size";

  // While we can render up to the max texture size, that would significantly
  // add to the memory overhead. So limit to up to a viewport worth of
  // additional content.
  const int max_bounds_based_on_viewport =
      2 * std::max(snapshot_root_size.width(), snapshot_root_size.height());

  return std::min(max_bounds_based_on_viewport, max_texture_size_in_layout);
}

gfx::Transform ComputeViewportTransform(const LayoutObject& object) {
  DCHECK(object.HasLayer());
  DCHECK(!object.IsLayoutView());

  auto& first_fragment = object.FirstFragment();
  DCHECK(ToRoundedPoint(first_fragment.PaintOffset()).IsOrigin())
      << first_fragment.PaintOffset();
  auto paint_properties = first_fragment.LocalBorderBoxProperties();

  auto& root_fragment = object.GetDocument().GetLayoutView()->FirstFragment();
  const auto& root_properties = root_fragment.LocalBorderBoxProperties();

  auto transform = GeometryMapper::SourceToDestinationProjection(
      paint_properties.Transform(), root_properties.Transform());
  if (auto* layout_inline = DynamicTo<LayoutInline>(object)) {
    // The paint_properties we get from
    // `first_fragment.LocalBorderBoxProperties()` correspond to the origin of
    // the inline's container's border-box. So the transform from GeometryMapper
    // maps a point from the viewport to the container's border-box origin. We
    // need the extra translation to map from container's border box origin to
    // inline's border box origin.
    transform.Translate(
        gfx::Vector2dF(layout_inline->PhysicalLinesBoundingBox().offset));
  }

  if (!transform.HasPerspective()) {
    transform.Round2dTranslationComponents();
  }

  return transform;
}

gfx::Transform ConvertFromTopLeftToCenter(
    const gfx::Transform& transform_from_top_left,
    const PhysicalSize& box_size) {
  gfx::Transform transform_from_center;
  transform_from_center.Translate(-box_size.width / 2, -box_size.height / 2);
  transform_from_center.PreConcat(transform_from_top_left);
  transform_from_center.Translate(box_size.width / 2, box_size.height / 2);

  return transform_from_center;
}

float DevicePixelRatioFromDocument(Document& document) {
  // Prefer to use the effective zoom. This should be the case in most
  // situations, unless the transition is being started before first layout
  // where documentElement gets a layout object.
  if (document.documentElement() &&
      document.documentElement()->GetLayoutObject()) {
    return document.documentElement()
        ->GetLayoutObject()
        ->StyleRef()
        .EffectiveZoom();
  }

  if (!document.GetPage() || !document.GetFrame()) {
    return 0.f;
  }
  return document.GetPage()
      ->GetChromeClient()
      .GetScreenInfo(*document.GetFrame())
      .device_scale_factor;
}

Vector<AtomicString> GetDocumentScopedClassList(Element* element) {
  auto class_list = element->ComputedStyleRef().ViewTransitionClass();
  if (!class_list || class_list->GetNames().empty() ||
      class_list->GetNames().front()->GetTreeScope() !=
          element->GetDocument().GetTreeScope()) {
    return Vector<AtomicString>();
  }
  Vector<AtomicString> result;
  result.ReserveInitialCapacity(class_list->GetNames().size());
  for (const auto& scoped_name : class_list->GetNames()) {
    CHECK(scoped_name->GetTreeScope() == element->GetDocument().GetTreeScope());
    result.emplace_back(scoped_name->GetName());
  }

  return result;
}

}  // namespace

class ViewTransitionStyleTracker::ImageWrapperPseudoElement
    : public ViewTransitionPseudoElementBase {
 public:
  ImageWrapperPseudoElement(Element* parent,
                            PseudoId pseudo_id,
                            const AtomicString& view_transition_name,
                            const ViewTransitionStyleTracker* style_tracker)
      : ViewTransitionPseudoElementBase(parent,
                                        pseudo_id,
                                        view_transition_name,
                                        style_tracker) {}

  ~ImageWrapperPseudoElement() override = default;

 private:
  bool CanGeneratePseudoElement(PseudoId pseudo_id) const override {
    if (!ViewTransitionPseudoElementBase::CanGeneratePseudoElement(pseudo_id)) {
      return false;
    }

    // If we're being called with a name, we must have a tracking for this name.
    auto it = style_tracker_->element_data_map_.find(view_transition_name());
    CHECK(it != style_tracker_->element_data_map_.end());
    const auto& element_data = it->value;

    if (pseudo_id == kPseudoIdViewTransitionOld) {
      return element_data->old_snapshot_id.IsValid();
    } else if (pseudo_id == kPseudoIdViewTransitionNew) {
      return element_data->new_snapshot_id.IsValid();
    }

    // Image wrapper pseudo-elements can only generate old/new image
    // pseudo-elements.
    return false;
  }
};

ViewTransitionStyleTracker::ViewTransitionStyleTracker(
    Document& document,
    const blink::ViewTransitionToken& transition_token)
    : document_(document),
      transition_token_(transition_token),
      device_pixel_ratio_(DevicePixelRatioFromDocument(document)) {}

ViewTransitionStyleTracker::ViewTransitionStyleTracker(
    Document& document,
    ViewTransitionState transition_state)
    : document_(document),
      state_(State::kCaptured),
      transition_token_(transition_state.transition_token),
      deserialized_(true) {
  auto* supplement = ViewTransitionSupplement::FromIfExists(document);
  CHECK(supplement);
  supplement->InitializeResourceIdSequence(
      transition_state.next_element_resource_id);

  device_pixel_ratio_ = transition_state.device_pixel_ratio;
  captured_name_count_ = static_cast<int>(transition_state.elements.size());
  snapshot_root_layout_size_at_capture_ =
      transition_state.snapshot_root_size_at_capture;

  VectorOf<AtomicString> transition_names;
  transition_names.ReserveInitialCapacity(captured_name_count_);
  for (const auto& transition_state_element : transition_state.elements) {
    auto name =
        AtomicString::FromUTF8(transition_state_element.tag_name.c_str());
    transition_names.push_back(name);

    DCHECK(!element_data_map_.Contains(name));
    auto* element_data = MakeGarbageCollected<ElementData>();

    element_data->container_properties.emplace_back(
        PhysicalSize::FromSizeFFloor(
            transition_state_element.border_box_size_in_css_space),
        transition_state_element.viewport_matrix);
    element_data->old_snapshot_id = transition_state_element.snapshot_id;

    element_data->element_index = transition_state_element.paint_order;
    set_element_sequence_id_ = std::max(set_element_sequence_id_,
                                        transition_state_element.paint_order);

    element_data->visual_overflow_rect_in_layout_space =
        PhysicalRect::EnclosingRect(
            transition_state_element.overflow_rect_in_layout_space);
    element_data->captured_rect_in_layout_space =
        transition_state_element.captured_rect_in_layout_space;

    CHECK_LE(
        transition_state_element.captured_css_properties.size(),
        std::size(kPropertiesToCapture) + std::size(kLayeredCaptureProperties));

    FlatMapBuilder<CSSPropertyID, String> css_property_builder(
        transition_state_element.captured_css_properties.size());
    for (const auto& [id, value] :
         transition_state_element.captured_css_properties) {
      css_property_builder.Insert(FromTransitionPropertyId(id),
                                  String::FromUTF8(value));
    }
    element_data->captured_css_properties =
        std::move(css_property_builder).Finish();

    for (const auto& class_name : transition_state_element.class_list) {
      element_data->class_list.push_back(
          AtomicString::FromUTF8(class_name.c_str()));
    }

    element_data->containing_group_name =
        transition_state_element.containing_group_name.empty()
            ? AtomicString()
            : AtomicString::FromUTF8(
                  transition_state_element.containing_group_name.c_str());
    element_data->CacheStateForOldSnapshot();

    element_data_map_.insert(name, std::move(element_data));
  }

  // Re-create the layer to display the old Document's cached content until the
  // new Document is render-blocked. This is conceptually the same layer as on
  // the ViewTransition on the old Document since it uses the same resource ID.
  if (transition_state.subframe_snapshot_id.IsValid()) {
    subframe_snapshot_layer_ = cc::ViewTransitionContentLayer::Create(
        transition_state.subframe_snapshot_id, /*is_live_content_layer=*/false);
  }

  // The aim of this flag is to serialize/deserialize SPA state using MPA
  // machinery. The intent is to use SPA tests to test MPA implementation as
  // well. To that end, if the flag is enabled we should invalidate styles and
  // clear the view transition names, because the "true" MPA implementation
  // would not have any style or names set.
  if (RuntimeEnabledFeatures::SerializeViewTransitionStateInSPAEnabled()) {
    InvalidateHitTestingCache();
    InvalidateStyle();
    document_->GetStyleEngine().SetViewTransitionNames({});
  }
}

ViewTransitionStyleTracker::~ViewTransitionStyleTracker() {
  if (!RuntimeEnabledFeatures::SerializeViewTransitionStateInSPAEnabled()) {
    CHECK_EQ(state_, State::kFinished);
  }
}

void ViewTransitionStyleTracker::AddConsoleError(
    String message,
    Vector<DOMNodeId> related_nodes) {
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kRendering,
      mojom::blink::ConsoleMessageLevel::kError, std::move(message));
  console_message->SetNodes(document_->GetFrame(), std::move(related_nodes));
  document_->AddConsoleMessage(console_message);
}

void ViewTransitionStyleTracker::AddTransitionElement(
    Element* element,
    const AtomicString& name,
    const AtomicString& nearest_containing_group,
    const AtomicString& nearest_group_with_contain) {
  DCHECK(element);

  // Insert an empty hash set for the element if it doesn't exist, or get it if
  // it does.
  auto& value = pending_transition_element_names_
                    .insert(element, HashSet<std::pair<AtomicString, int>>())
                    .stored_value->value;

  if (nearest_containing_group) {
    group_state_map_.Set(name, AncestorGroupNames{
                                   nearest_containing_group,
                                   nearest_group_with_contain,
                               });
  }
  // Find the existing name if one is there. If it is there, do nothing.
  if (base::Contains(value, name, &std::pair<AtomicString, int>::first))
    return;
  // Otherwise, insert a new sequence id with this name. We'll use the sequence
  // to sort later.
  value.insert(std::make_pair(name, set_element_sequence_id_));
  ++set_element_sequence_id_;
}

bool ViewTransitionStyleTracker::MatchForOnlyChild(
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) const {
  switch (pseudo_id) {
    case kPseudoIdViewTransition:
      DCHECK(!view_transition_name);
      return false;

    case kPseudoIdViewTransitionGroup: {
      DCHECK(view_transition_name);
      DCHECK(element_data_map_.Contains(view_transition_name));

      return element_data_map_.size() == 1;
    }

    case kPseudoIdViewTransitionImagePair:
      DCHECK(view_transition_name);
      return true;

    case kPseudoIdViewTransitionOld: {
      DCHECK(view_transition_name);

      auto it = element_data_map_.find(view_transition_name);
      CHECK(it != element_data_map_.end(), base::NotFatalUntil::M130);
      const auto& element_data = it->value;
      return !element_data->new_snapshot_id.IsValid();
    }

    case kPseudoIdViewTransitionNew: {
      DCHECK(view_transition_name);

      auto it = element_data_map_.find(view_transition_name);
      CHECK(it != element_data_map_.end(), base::NotFatalUntil::M130);
      const auto& element_data = it->value;
      return !element_data->old_snapshot_id.IsValid();
    }

    default:
      NOTREACHED_IN_MIGRATION();
  }

  return false;
}

void ViewTransitionStyleTracker::AddTransitionElementsFromCSS() {
  DCHECK(document_ && document_->View());

  // We need our paint layers, and z-order lists which is done during
  // compositing inputs update.
  DCHECK_GE(document_->Lifecycle().GetState(),
            DocumentLifecycle::kCompositingInputsClean);

  Vector<AtomicString> containing_group_stack;

  AddTransitionElementsFromCSSRecursive(
      document_->GetLayoutView()->PaintingLayer(), document_.Get(),
      containing_group_stack, /*nearest_group_with_contain=*/g_null_atom);
}

AtomicString ViewTransitionStyleTracker::GenerateAutoName(
    Element& element,
    const TreeScope* scope) {
  // The flag should be checked much earlier than this, in the CSS parser.
  CHECK(RuntimeEnabledFeatures::CSSViewTransitionAutoNameEnabled());
  if (element.HasID() && scope && *scope == element.GetTreeScope()) {
    return element.GetIdAttribute();
  }
  StringBuilder builder;
  builder.Append("-ua-auto-");
  if (token_.is_zero()) {
    token_ = base::Token::CreateRandom();
  }
  builder.Append(token_.ToString().c_str());
  builder.Append("-");
  builder.AppendNumber(element.GetDomNodeId());
  return builder.ToAtomicString();
}

void ViewTransitionStyleTracker::AddTransitionElementsFromCSSRecursive(
    PaintLayer* root,
    const TreeScope* tree_scope,
    Vector<AtomicString>& containing_group_stack,
    const AtomicString& nearest_group_with_contain) {
  // We want to call AddTransitionElements in the order in which
  // PaintLayerPaintOrderIterator would cause us to paint the elements.
  // Specifically, parents are added before their children, and lower z-index
  // children are added before higher z-index children. Given that, what we
  // need to do is to first add `root`'s element, and then recurse using the
  // PaintLayerPaintOrderIterator which will return values in the correct
  // z-index order.
  //
  // Note that the order of calls to AddTransitionElement determines the DOM
  // order of pseudo-elements constructed to represent the transition elements,
  // which by default will also represent the paint order of the pseudo-elements
  // (unless changed by something like z-index on the pseudo-elements).
  auto& root_object = root->GetLayoutObject();
  auto& root_style = root_object.StyleRef();

  const auto& view_transition_name = root_style.ViewTransitionName();
  AtomicString current_name;
  if (view_transition_name && !root_object.IsFragmented()) {
    auto* node = root_object.GetNode();
    DCHECK(node);
    DCHECK(node->IsElementNode());

    // ATM this will be null if the scope of the view-transition-name comes from
    // e.g. devtools.
    auto* relevant_tree_scope =
        RuntimeEnabledFeatures::ViewTransitionTreeScopedNamesEnabled()
            ? view_transition_name->GetTreeScope()
            : &node->GetTreeScope();

    if (relevant_tree_scope == tree_scope || !relevant_tree_scope) {
      current_name = view_transition_name->IsAuto()
                         ? GenerateAutoName(*To<Element>(node), tree_scope)
                         : view_transition_name->CustomName();
      AddTransitionElement(DynamicTo<Element>(node), current_name,
                           containing_group_stack.empty()
                               ? g_null_atom
                               : containing_group_stack.back(),
                           nearest_group_with_contain);
    }
  }

  if (root_object.ChildPaintBlockedByDisplayLock())
    return;

  if (current_name) {
    containing_group_stack.push_back(current_name);
  }

  // Even if tree scopes don't match, we process children since light slotted
  // children can have outer tree scope.
  PaintLayerPaintOrderIterator child_iterator(root, kAllChildren);
  while (auto* child = child_iterator.Next()) {
    AddTransitionElementsFromCSSRecursive(
        child, tree_scope, containing_group_stack,
        root_style.ViewTransitionGroup().IsNormal() ? nearest_group_with_contain
                                                    : current_name);
  }

  if (current_name) {
    containing_group_stack.pop_back();
  }
}

bool ViewTransitionStyleTracker::FlattenAndVerifyElements(
    VectorOf<Element>& elements,
    VectorOf<AtomicString>& transition_names) {
  // Fail if the document element does not exist, since that's the place where
  // we attach pseudo elements, and if it's not there, we can't do a transition.
  if (!document_->documentElement()) {
    return false;
  }

  // If the root element exists but doesn't generate a layout object then there
  // can't be any elements participating in the transition since no element can
  // generate a box. This is a valid state for things like entry or exit
  // animations.
  if (!document_->documentElement()->GetLayoutObject()) {
    return true;
  }

  // We need to flatten the data first, and sort it by ordering which reflects
  // the setElement ordering.
  struct FlatData : public GarbageCollected<FlatData> {
    FlatData(Element* element, const AtomicString& name, int ordering)
        : element(element), name(name), ordering(ordering) {}
    Member<Element> element;
    AtomicString name;
    int ordering;

    void Trace(Visitor* visitor) const { visitor->Trace(element); }
  };
  VectorOf<FlatData> flat_list;

  // Flatten it.
  for (auto& [element, names] : pending_transition_element_names_) {
    DCHECK(element->GetLayoutObject());

    // TODO(khushalsagar): Simplify this, we don't support multiple
    // view-transition-names per element.
    for (auto& name_pair : names) {
      flat_list.push_back(MakeGarbageCollected<FlatData>(
          element, name_pair.first, name_pair.second));
    }
  }

  // Sort it.
  std::sort(flat_list.begin(), flat_list.end(),
            [](const FlatData* a, const FlatData* b) {
              return a->ordering < b->ordering;
            });

  // Verify it.
  for (auto& flat_data : flat_list) {
    auto& name = flat_data->name;
    auto& element = flat_data->element;

    if (transition_names.Contains(name)) [[unlikely]] {
      StringBuilder message;
      message.Append(kDuplicateTagBaseError);
      message.Append(name);

      Vector<DOMNodeId> nodes;
      // Find all the elements with this name.
      for (auto& name_finder : flat_list) {
        if (name_finder->name == name) {
          nodes.push_back(name_finder->element->GetDomNodeId());
        }
      }

      AddConsoleError(message.ReleaseString(), std::move(nodes));
      return false;
    }

    transition_names.push_back(name);
    elements.push_back(element);
  }
  return true;
}

AtomicString ViewTransitionStyleTracker::ComputeContainingGroupName(
    const AtomicString& name,
    const StyleViewTransitionGroup& group) const {
  if (!group_state_map_.Contains(name)) {
    return g_null_atom;
  }

  const auto& parent_state = group_state_map_.at(name);
  if (group.IsNormal() || group.IsContain()) {
    return parent_state.contain;
  }

  if (group.IsNearest() || group.CustomName() == parent_state.nearest) {
    return parent_state.nearest;
  }

  return ComputeContainingGroupName(parent_state.nearest, group);
}

bool ViewTransitionStyleTracker::Capture(bool snap_browser_controls) {
  DCHECK_EQ(state_, State::kIdle);

  // Flatten `pending_transition_element_names_` into a vector of names and
  // elements. This process also verifies that the name-element combinations are
  // valid.
  VectorOf<AtomicString> transition_names;
  VectorOf<Element> elements;
  bool success = FlattenAndVerifyElements(elements, transition_names);
  if (!success)
    return false;

  // In a cross-document transition, top controls are animated to shown when
  // the navigation starts. When capturing the outgoing snapshots, the
  // animation may still be in progress. Ensure controls are snapped to fully
  // showing before capturing. This ensures the root clip is at the correct
  // size and that fixed elements are positioned by layout in the same way they
  // will be on the incoming view.
  if (snap_browser_controls) {
    SnapBrowserControlsToFullyShown();
  }

  // Now we know that we can start a transition. Update the state and populate
  // `element_data_map_`.
  state_ = State::kCapturing;
  InvalidateHitTestingCache();

  captured_name_count_ = transition_names.size();
  element_data_map_.ReserveCapacityForSize(captured_name_count_);
  HeapHashMap<Member<Element>, viz::ViewTransitionElementResourceId>
      element_snapshot_ids;
  int next_index = 0;
  for (int i = 0; i < captured_name_count_; ++i) {
    const auto& name = transition_names[i];
    const auto& element = elements[i];

    // Reuse any previously generated snapshot_id for this element. If there was
    // none yet, then generate the resource id.
    auto& snapshot_id =
        element_snapshot_ids
            .insert(element, viz::ViewTransitionElementResourceId())
            .stored_value->value;
    if (!snapshot_id.IsValid()) {
      snapshot_id = GenerateResourceId();
      capture_resource_ids_.push_back(snapshot_id);
    }

    auto* element_data = MakeGarbageCollected<ElementData>();
    element_data->target_element = element;
    element_data->element_index = next_index++;
    element_data->old_snapshot_id = snapshot_id;
    element_data->class_list = GetDocumentScopedClassList(element);

    // This is guaranteed to be in order if valid, as transition_names is
    // already sorted.
    element_data->containing_group_name = ComputeContainingGroupName(
        name, element->ComputedStyleRef().ViewTransitionGroup());
    element_data_map_.insert(name, std::move(element_data));

    if (element->IsDocumentElement()) {
      is_root_transitioning_ = true;
    }
  }

#if DCHECK_IS_ON()
  for (wtf_size_t i = 0; i < transition_names.size(); ++i) {
    DCHECK_EQ(transition_names.Find(transition_names[i]), i)
        << " Duplicate transition name: " << transition_names[i];
  }
#endif

  // This informs the style engine the set of names we have, which will be used
  // to create the pseudo element tree.
  document_->GetStyleEngine().SetViewTransitionNames(transition_names);

  // We need a style invalidation to generate the pseudo element tree.
  InvalidateStyle();

  set_element_sequence_id_ = 0;
  pending_transition_element_names_.clear();

  DCHECK(!snapshot_root_layout_size_at_capture_.has_value());
  snapshot_root_layout_size_at_capture_ = GetSnapshotRootSize();

  if (RuntimeEnabledFeatures::PaintHoldingForLocalIframesEnabled() &&
      !document_->GetFrame()->IsLocalRoot()) {
    subframe_snapshot_layer_ = cc::ViewTransitionContentLayer::Create(
        GenerateResourceId(), /*is_live_content_layer=*/true);
    capture_resource_ids_.push_back(
        subframe_snapshot_layer_->ViewTransitionResourceId());
  }

  return true;
}

void ViewTransitionStyleTracker::CaptureResolved() {
  DCHECK_EQ(state_, State::kCapturing);

  state_ = State::kCaptured;
  // TODO(crbug.com/1347473): We should also suppress hit testing at this point,
  // since we're about to start painting the element as a captured snapshot, but
  // we still haven't given script chance to modify the DOM to the new state.
  InvalidateHitTestingCache();

  // Since the elements will be unset, we need to invalidate their style first.
  // TODO(vmpstr): We don't have to invalidate the pseudo styles at this point,
  // just the transition elements. We can split InvalidateStyle() into two
  // functions as an optimization.
  InvalidateStyle();

  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;

    element_data->target_element = nullptr;
  }
  is_root_transitioning_ = false;
}

VectorOf<Element> ViewTransitionStyleTracker::GetTransitioningElements() const {
  // In stable states, we don't have transitioning elements.
  if (state_ == State::kIdle || state_ == State::kCaptured)
    return {};

  VectorOf<Element> result;
  for (auto& entry : element_data_map_) {
    if (entry.value->target_element &&
        !entry.value->target_element->IsDocumentElement()) {
      result.push_back(entry.value->target_element);
    }
  }
  return result;
}

const Vector<AtomicString>&
ViewTransitionStyleTracker::GetViewTransitionClassList(
    const AtomicString& name) const {
  CHECK(element_data_map_.Contains(name));
  return element_data_map_.at(name)->class_list;
}

AtomicString ViewTransitionStyleTracker::GetContainingGroupName(
    const AtomicString& name) const {
  if (!RuntimeEnabledFeatures::NestedViewTransitionEnabled() ||
      state_ != State::kStarted) {
    return g_null_atom;
  }

  // GetContainingGroup can be called on an invalid name, e.g. when searching
  // for the parent of a non-existent name.
  if (!element_data_map_.Contains(name)) {
    return g_null_atom;
  }
  return element_data_map_.at(name)->containing_group_name;
}

bool ViewTransitionStyleTracker::Start() {
  DCHECK_EQ(state_, State::kCaptured);

  subframe_snapshot_layer_.reset();

  // Flatten `pending_transition_element_names_` into a vector of names and
  // elements. This process also verifies that the name-element combinations are
  // valid.
  VectorOf<AtomicString> transition_names;
  VectorOf<Element> elements;
  bool success = FlattenAndVerifyElements(elements, transition_names);
  if (!success)
    return false;

  state_ = State::kStarted;
  InvalidateHitTestingCache();

  HeapHashMap<Member<Element>, viz::ViewTransitionElementResourceId>
      element_snapshot_ids;

  bool found_new_names = false;
  // If this tracker was created from serialized state, transition tags are
  // initialized with the style system in the start phase.
  if (deserialized_) {
    DCHECK(document_->GetStyleEngine().ViewTransitionTags().empty());
    found_new_names = true;
  }

  // We would have an new element index for each of the element_data_map_
  // entries.
  int next_index = element_data_map_.size();
  for (wtf_size_t i = 0; i < elements.size(); ++i) {
    const auto& name = transition_names[i];
    const auto& element = elements[i];

    // Insert a new name data if there is no data for this name yet.
    if (!element_data_map_.Contains(name)) {
      found_new_names = true;
      auto* data = MakeGarbageCollected<ElementData>();
      data->element_index = next_index++;
      element_data_map_.insert(name, data);
    }

    // Reuse any previously generated snapshot_id for this element. If there was
    // none yet, then generate the resource id.
    auto& snapshot_id =
        element_snapshot_ids
            .insert(element, viz::ViewTransitionElementResourceId())
            .stored_value->value;
    if (!snapshot_id.IsValid()) {
      snapshot_id = GenerateResourceId();
    }

    auto& element_data = element_data_map_.find(name)->value;
    DCHECK(!element_data->target_element);
    element_data->target_element = element;
    element_data->new_snapshot_id = snapshot_id;
    element_data->class_list = GetDocumentScopedClassList(element);

    // The parent is guaranteed to be in the list already, as transition_names
    // is sorted by paint order.
    element_data->containing_group_name = ComputeContainingGroupName(
        name, element->ComputedStyleRef().ViewTransitionGroup());

    // Verify that the element_index assigned in Capture is less than next_index
    // here, just as a sanity check.
    DCHECK_LT(element_data->element_index, next_index);

    if (element->IsDocumentElement()) {
      is_root_transitioning_ = true;
    }
  }

  if (found_new_names) {
    VectorOf<std::pair<AtomicString, int>> new_name_pairs;
    for (auto& [name, data] : element_data_map_) {
      new_name_pairs.push_back(std::make_pair(name, data->element_index));
    }

    std::sort(new_name_pairs.begin(), new_name_pairs.end(),
              [](const std::pair<AtomicString, int>& left,
                 const std::pair<AtomicString, int>& right) {
                return left.second < right.second;
              });

#if DCHECK_IS_ON()
    int last_index = -1;
#endif
    VectorOf<AtomicString> new_names;
    for (auto& [name, index] : new_name_pairs) {
      new_names.push_back(name);
#if DCHECK_IS_ON()
      DCHECK_NE(last_index, index);
      last_index = index;
#endif
    }

#if DCHECK_IS_ON()
    for (wtf_size_t i = 0; i < new_names.size(); ++i) {
      DCHECK_EQ(new_names.Find(new_names[i]), i)
          << " Duplicate transition name: " << new_names[i];
    }
#endif

    document_->GetStyleEngine().SetViewTransitionNames(new_names);
  }

  DCHECK_GE(document_->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  // We need a style invalidation to generate new content pseudo elements for
  // new elements in the DOM.
  InvalidateStyle();

  if (auto* page = document_->GetPage())
    page->Animator().SetHasViewTransition(true);
  return true;
}

void ViewTransitionStyleTracker::StartFinished() {
  DCHECK_EQ(state_, State::kStarted);
  EndTransition();
}

void ViewTransitionStyleTracker::Abort() {
  EndTransition();
}

void ViewTransitionStyleTracker::DidThrottleLocalSubframeRendering() {
  DCHECK_EQ(state_, State::kCapturing);

  if (subframe_snapshot_layer_) {
    auto resource_id = subframe_snapshot_layer_->ViewTransitionResourceId();
    subframe_snapshot_layer_ = cc::ViewTransitionContentLayer::Create(
        resource_id, /*is_live_content_layer=*/false);
  }
}

void ViewTransitionStyleTracker::EndTransition() {
  CHECK_NE(state_, State::kFinished);

  state_ = State::kFinished;
  InvalidateHitTestingCache();

  // We need a style invalidation to remove the pseudo element tree. This needs
  // to be done before we clear the data, since we need to invalidate the
  // transition elements stored in `element_data_map_`.
  InvalidateStyle();

  element_data_map_.clear();
  pending_transition_element_names_.clear();
  set_element_sequence_id_ = 0;
  document_->GetStyleEngine().SetViewTransitionNames({});
  is_root_transitioning_ = false;
  if (auto* page = document_->GetPage())
    page->Animator().SetHasViewTransition(false);
}

viz::ViewTransitionElementResourceId ViewTransitionStyleTracker::GetSnapshotId(
    const Element& element) const {
  viz::ViewTransitionElementResourceId resource_id;

  for (const auto& entry : element_data_map_) {
    // This loop is based on the assumption that an element can have multiple
    // names. But this concept is not supported by the web API.
    if (entry.value->target_element == element) {
      const auto& snapshot_id = HasLiveNewContent()
                                    ? entry.value->new_snapshot_id
                                    : entry.value->old_snapshot_id;
      DCHECK(!resource_id.IsValid() || resource_id == snapshot_id);
      if (!resource_id.IsValid())
        resource_id = snapshot_id;
    }
  }

  return resource_id;
}

const scoped_refptr<cc::ViewTransitionContentLayer>&
ViewTransitionStyleTracker::GetSubframeSnapshotLayer() const {
  return subframe_snapshot_layer_;
}

PseudoElement* ViewTransitionStyleTracker::CreatePseudoElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) const {
  DCHECK(IsTransitionPseudoElement(pseudo_id));
  DCHECK(pseudo_id == kPseudoIdViewTransition || view_transition_name);

  switch (pseudo_id) {
    case kPseudoIdViewTransition:
      return MakeGarbageCollected<ViewTransitionTransitionElement>(parent,
                                                                   this);

    case kPseudoIdViewTransitionGroup: {
      return MakeGarbageCollected<ViewTransitionPseudoElementBase>(
          parent, pseudo_id, view_transition_name, this);
    }
    case kPseudoIdViewTransitionImagePair:
      return MakeGarbageCollected<ImageWrapperPseudoElement>(
          parent, pseudo_id, view_transition_name, this);

    case kPseudoIdViewTransitionOld: {
      DCHECK(view_transition_name);
      const auto& element_data =
          element_data_map_.find(view_transition_name)->value;

      // If live data is tracking new elements then use the cached data for
      // the pseudo element displaying snapshot of old element.
      bool use_cached_data = HasLiveNewContent();
      auto captured_rect = element_data->GetCapturedSubrect(use_cached_data);
      auto border_box_rect =
          element_data->GetBorderBoxRect(use_cached_data, device_pixel_ratio_);
      auto snapshot_id = element_data->old_snapshot_id;

      // Note that we say that this layer is not a live content
      // layer, even though it may currently be displaying live contents. The
      // reason is that we want to avoid updating this value later, which
      // involves propagating the update all the way to cc. However, this means
      // that we have to have the save directive come in the same frame as the
      // first frame that displays this content. Otherwise, we risk DCHECK. This
      // is currently the behavior as specced, but this is subtle.
      // TODO(vmpstr): Maybe we should just use HasLiveNewContent() here, and
      // update it when the value changes.
      auto* pseudo_element = MakeGarbageCollected<ViewTransitionContentElement>(
          parent, pseudo_id, view_transition_name, snapshot_id,
          /*is_live_content_element=*/false, this);
      pseudo_element->SetIntrinsicSize(captured_rect, border_box_rect);
      return pseudo_element;
    }

    case kPseudoIdViewTransitionNew: {
      DCHECK(view_transition_name);
      const auto& element_data =
          element_data_map_.find(view_transition_name)->value;
      bool use_cached_data = false;
      auto captured_rect = element_data->GetCapturedSubrect(use_cached_data);
      auto border_box_rect =
          element_data->GetBorderBoxRect(use_cached_data, device_pixel_ratio_);
      auto snapshot_id = element_data->new_snapshot_id;

      auto* pseudo_element = MakeGarbageCollected<ViewTransitionContentElement>(
          parent, pseudo_id, view_transition_name, snapshot_id,
          /*is_live_content_element=*/true, this);
      pseudo_element->SetIntrinsicSize(captured_rect, border_box_rect);
      return pseudo_element;
    }

    default:
      NOTREACHED_IN_MIGRATION();
  }

  return nullptr;
}

bool ViewTransitionStyleTracker::RunPostPrePaintSteps() {
  DCHECK_GE(document_->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  // Abort if the document element is not there.
  if (!document_->documentElement()) {
    return false;
  }

  if (!document_->documentElement()->GetLayoutObject()) {
    // If we have any view transition elements, while having no
    // documentElement->GetLayoutObject(), we should abort. Target elements are
    // only set on the current phase of the animation, so it means that the
    // documentElement's layout object disappeared in this phase.
    for (auto& entry : element_data_map_) {
      auto& element_data = entry.value;
      if (element_data->target_element) {
        return false;
      }
    }
    return true;
  }

  DCHECK(document_->documentElement() &&
         document_->documentElement()->GetLayoutObject());
  // We don't support changing device pixel ratio, because it's uncommon and
  // textures may have already been captured at a different size.
  if (device_pixel_ratio_ != DevicePixelRatioFromDocument(*document_)) {
    return false;
  }

  if (SnapshotRootDidChangeSize()) {
    return false;
  }

  const int max_capture_size_in_layout = ComputeMaxCaptureSize(
      *document_,
      document_->GetPage()->GetChromeClient().GetMaxRenderBufferBounds(
          *document_->GetFrame()),
      *snapshot_root_layout_size_at_capture_);

  if (snapshot_root_layout_size_at_capture_->width() >
          max_capture_size_in_layout ||
      snapshot_root_layout_size_at_capture_->height() >
          max_capture_size_in_layout) {
    // TODO(crbug.com/1516874): This skips the transition if the root is too
    // large to fit into a texture but non-root elements clip in this case
    // instead. It would be better to clip the root like we do child elements,
    // rather than skipping (and that would comply better with the spec).

    // For main frames the capture size should never be bigger than the
    // window so we only expect to end up here due to large subframes.
    CHECK(!document_->GetFrame()->IsOutermostMainFrame());
    return false;
  }

  bool needs_style_invalidation = false;

  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (!element_data->target_element)
      continue;

    DCHECK(document_->documentElement());
    auto* layout_object = element_data->target_element->GetLayoutObject();
    if (!layout_object) {
      return false;
    }

    // End the transition if any of the objects have become fragmented.
    if (layout_object->IsFragmented()) {
      return false;
    }

    ContainerProperties container_properties;
    PhysicalRect visual_overflow_rect_in_layout_space;
    std::optional<gfx::RectF> captured_rect_in_layout_space;

    if (element_data->target_element->IsDocumentElement()) {
      auto layout_view_size = PhysicalSize(GetSnapshotRootSize());
      auto layout_view_size_in_css_space = layout_view_size;
      layout_view_size_in_css_space.Scale(1 / device_pixel_ratio_);
      container_properties =
          ContainerProperties(layout_view_size_in_css_space, gfx::Transform());
      visual_overflow_rect_in_layout_space.size = layout_view_size;
    } else {
      ComputeLiveElementGeometry(
          max_capture_size_in_layout, *layout_object, container_properties,
          visual_overflow_rect_in_layout_space, captured_rect_in_layout_space);
    }

    FlatMapBuilder<CSSPropertyID, String> css_property_builder(
        std::size(kPropertiesToCapture));

    auto capture_property = [&](CSSPropertyID id) {
      if (const CSSValue* css_value =
              CSSProperty::Get(id).CSSValueFromComputedStyle(
                  layout_object->StyleRef(),
                  /*layout_object=*/nullptr,
                  /*allow_visited_style=*/false,
                  CSSValuePhase::kComputedValue)) {
        css_property_builder.Insert(id, css_value->CssText());
      }
    };

    for (CSSPropertyID id : kPropertiesToCapture) {
      capture_property(id);
    }

    if (RuntimeEnabledFeatures::ViewTransitionLayeredCaptureEnabled()) {
      for (CSSPropertyID id : kLayeredCaptureProperties) {
        capture_property(id);
      }
    }

    auto css_properties = std::move(css_property_builder).Finish();

    if (!element_data->container_properties.empty() &&
        element_data->container_properties.back() == container_properties &&
        visual_overflow_rect_in_layout_space ==
            element_data->visual_overflow_rect_in_layout_space &&
        captured_rect_in_layout_space ==
            element_data->captured_rect_in_layout_space &&
        css_properties == element_data->captured_css_properties) {
      continue;
    }

    // Only add a new container properties entry if it differs from the last
    // one.
    if (element_data->container_properties.empty()) {
      element_data->container_properties.push_back(container_properties);
    } else if (element_data->container_properties.back() !=
               container_properties) {
      if (state_ == State::kStarted) {
        element_data->container_properties.push_back(container_properties);
      } else {
        element_data->container_properties.back() = container_properties;
      }
    }

    element_data->visual_overflow_rect_in_layout_space =
        visual_overflow_rect_in_layout_space;
    element_data->captured_css_properties = css_properties;
    element_data->captured_rect_in_layout_space = captured_rect_in_layout_space;

    PseudoId live_content_element = HasLiveNewContent()
                                        ? kPseudoIdViewTransitionNew
                                        : kPseudoIdViewTransitionOld;
    DCHECK(document_->documentElement());
    if (auto* pseudo_element =
            document_->documentElement()->GetStyledPseudoElement(
                live_content_element, entry.key)) {
      // A pseudo element of type |tansition*content| must be created using
      // ViewTransitionContentElement.
      bool use_cached_data = false;
      auto captured_rect = element_data->GetCapturedSubrect(use_cached_data);
      auto border_box_rect =
          element_data->GetBorderBoxRect(use_cached_data, device_pixel_ratio_);
      static_cast<ViewTransitionContentElement*>(pseudo_element)
          ->SetIntrinsicSize(captured_rect, border_box_rect);
    }

    // Ensure that the cached state stays in sync with the current state while
    // we're capturing.
    if (state_ == State::kCapturing) {
      element_data->CacheStateForOldSnapshot();
    }

    needs_style_invalidation = true;
  }

  if (LayoutViewTransitionRoot* snapshot_containing_block =
          document_->GetLayoutView()->GetViewTransitionRoot()) {
    snapshot_containing_block->UpdateSnapshotStyle(*this);
  }

  if (needs_style_invalidation) {
    InvalidateStyle();
  }

  return true;
}

void ViewTransitionStyleTracker::ComputeLiveElementGeometry(
    int max_capture_size,
    LayoutObject& layout_object,
    ContainerProperties& container_properties,
    PhysicalRect& visual_overflow_rect_in_layout_space,
    std::optional<gfx::RectF>& captured_rect_in_layout_space) const {
  DCHECK(!layout_object.IsLayoutView());

  // TODO(bokan): This doesn't account for the local offset of an inline
  // element within its container. The object-view-box inset will ensure the
  // snapshot is rendered in the correct place but the pseudo is positioned
  // w.r.t. to the container. This can look awkward since the opposing
  // snapshot may have a different object-view-box. Inline positioning and
  // scaling more generally might use some improvements.
  // https://crbug.com/1416951.
  auto snapshot_matrix_in_layout_space =
      ComputeViewportTransform(layout_object);

  // The FixedToSnapshot offset below takes points from the fixed
  // viewport into the snapshot viewport. However, the transform is
  // currently into frame coordinates; when a scrollbar (or gutter) appears on
  // the left, the fixed viewport origin is actually at (15, 0) in frame
  // coordinates (assuming 15px scrollbars). Therefore we must first shift
  // by the scrollbar width so we're in fixed viewport coordinates.
  gfx::Vector2d fixed_to_frame =
      -document_->GetLayoutView()->OriginAdjustmentForScrollbars();
  snapshot_matrix_in_layout_space.PostTranslate(fixed_to_frame);

  gfx::Vector2d snapshot_to_fixed_offset = -GetFixedToSnapshotRootOffset();
  snapshot_matrix_in_layout_space.PostTranslate(snapshot_to_fixed_offset);

  auto snapshot_matrix_in_css_space = snapshot_matrix_in_layout_space;
  snapshot_matrix_in_css_space.Zoom(1.0 / device_pixel_ratio_);

  PhysicalSize border_box_size_in_css_space;
  if (layout_object.IsSVGChild() || IsA<LayoutBox>(layout_object)) {
    // ResizeObserverEntry is created to reuse the logic for parsing object
    // size for different types of LayoutObjects. However, this works only
    // for SVGChild and LayoutBox.
    auto* resize_observer_entry = MakeGarbageCollected<ResizeObserverEntry>(
        To<Element>(layout_object.GetNode()));
    auto entry_size = resize_observer_entry->borderBoxSize()[0];
    // ResizeObserver gives us CSS space pixels.
    border_box_size_in_css_space =
        layout_object.IsHorizontalWritingMode()
            ? PhysicalSize(LayoutUnit(entry_size->inlineSize()),
                           LayoutUnit(entry_size->blockSize()))
            : PhysicalSize(LayoutUnit(entry_size->blockSize()),
                           LayoutUnit(entry_size->inlineSize()));
  } else if (auto* layout_inline = DynamicTo<LayoutInline>(layout_object)) {
    border_box_size_in_css_space =
        layout_inline->PhysicalLinesBoundingBox().size;
    // Convert to CSS pixels instead of layout pixels.
    border_box_size_in_css_space.Scale(1.f / device_pixel_ratio_);
  }

  // If the object's effective zoom differs from device_pixel_ratio, adjust
  // the border box size by that difference to get the css space size.
  if (float effective_zoom = layout_object.StyleRef().EffectiveZoom();
      std::abs(effective_zoom - device_pixel_ratio_) >=
      std::numeric_limits<float>::epsilon()) {
    border_box_size_in_css_space.Scale(effective_zoom / device_pixel_ratio_);
  }

  snapshot_matrix_in_css_space = ConvertFromTopLeftToCenter(
      snapshot_matrix_in_css_space, border_box_size_in_css_space);

  if (auto* box = DynamicTo<LayoutBoxModelObject>(layout_object)) {
    visual_overflow_rect_in_layout_space = ComputeVisualOverflowRect(*box);
  }

  // This is intentionally computed in layout space to include scaling from
  // device scale factor. The element's texture will be in physical pixel
  // bounds which includes this scale.
  captured_rect_in_layout_space = ComputeCaptureRect(
      max_capture_size, visual_overflow_rect_in_layout_space,
      snapshot_matrix_in_layout_space, *snapshot_root_layout_size_at_capture_);

  container_properties = ContainerProperties(border_box_size_in_css_space,
                                             snapshot_matrix_in_css_space);
}

bool ViewTransitionStyleTracker::HasActiveAnimations() const {
  auto pseudo_has_animation = [](PseudoElement* pseudo_element) {
    auto* animations = pseudo_element->GetElementAnimations();
    if (!animations) {
      return false;
    }

    for (auto& animation_pair : animations->Animations()) {
      auto animation_play_state =
          animation_pair.key->CalculateAnimationPlayState();
      if (animation_play_state == Animation::kRunning ||
          animation_play_state == Animation::kPaused) {
        return true;
      }
    }
    return false;
  };
  return !!ViewTransitionUtils::FindPseudoIf(*document_, pseudo_has_animation);
}

PaintPropertyChangeType ViewTransitionStyleTracker::UpdateCaptureClip(
    const Element& element,
    const ClipPaintPropertyNodeOrAlias* current_clip,
    const TransformPaintPropertyNodeOrAlias* current_transform) {
  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (element_data->target_element != &element) {
      continue;
    }

    ClipPaintPropertyNode::State state(
        *current_transform, *element_data->captured_rect_in_layout_space,
        FloatRoundedRect(*element_data->captured_rect_in_layout_space));

    if (!element_data->clip_node) {
      element_data->clip_node =
          ClipPaintPropertyNode::Create(*current_clip, std::move(state));
#if DCHECK_IS_ON()
      element_data->clip_node->SetDebugName(element.DebugName() +
                                            "ViewTransition");
#endif
      return PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    return element_data->clip_node->Update(*current_clip, std::move(state));
  }
  NOTREACHED_IN_MIGRATION();
  return PaintPropertyChangeType::kUnchanged;
}

const ClipPaintPropertyNode* ViewTransitionStyleTracker::GetCaptureClip(
    const Element& element) const {
  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (element_data->target_element != &element) {
      continue;
    }
    DCHECK(element_data->clip_node);
    return element_data->clip_node.Get();
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool ViewTransitionStyleTracker::IsTransitionElement(
    const Element& element) const {
  // In stable states, we don't have transition elements.
  if (state_ == State::kIdle || state_ == State::kCaptured)
    return false;

  if (element.IsDocumentElement()) {
    return is_root_transitioning_;
  }

  for (auto& entry : element_data_map_) {
    if (entry.value->target_element == &element) {
      return true;
    }
  }
  return false;
}

bool ViewTransitionStyleTracker::NeedsCaptureClipNode(
    const Element& node) const {
  if (state_ == State::kIdle || state_ == State::kCaptured) {
    return false;
  }

  for (auto& entry : element_data_map_) {
    if (entry.value->target_element != &node) {
      continue;
    }

    DCHECK(!entry.value->captured_rect_in_layout_space.has_value() ||
           !entry.value->target_element->IsDocumentElement())
        << "The root element should never need a clip node";
    return entry.value->captured_rect_in_layout_space.has_value();
  }
  return false;
}

StyleRequest::RulesToInclude ViewTransitionStyleTracker::StyleRulesToInclude()
    const {
  switch (state_) {
    case State::kIdle:
    case State::kCapturing:
    case State::kCaptured:
      return StyleRequest::kUAOnly;
    case State::kStarted:
    case State::kFinished:
      return StyleRequest::kAll;
  }

  NOTREACHED_IN_MIGRATION();
  return StyleRequest::kAll;
}

namespace {

// Returns the outsets applied by browser UI on the fixed viewport that will
// transform it into the snapshot viewport.
gfx::Outsets GetFixedToSnapshotViewportOutsets(Document& document) {
  DCHECK(document.View());
  DCHECK(document.GetPage());
  DCHECK(document.GetFrame());
  DCHECK(document.GetLayoutView());

  int top = 0;
  int right = 0;
  int bottom = 0;
  int left = 0;

  if (document.GetFrame()->IsOutermostMainFrame()) {
    BrowserControls& controls = document.GetPage()->GetBrowserControls();
    // If Blink's size is currently smaller to accommodate the browser controls,
    // outset the snapshot root to include the area occupied by browser
    // controls. Note: for cross-document transitions, this relies on the
    // browser resizing Blink before requesting the outgoing document snapshot.
    if (controls.ShrinkViewport()) {
      top += controls.TopHeight() - controls.TopMinHeight();
      bottom += controls.BottomHeight() - controls.BottomMinHeight();
    }

    bottom += document.GetFrame()
                  ->GetWidgetForLocalRoot()
                  ->GetVirtualKeyboardResizeHeight();
  }

  PhysicalBoxStrut scrollbar_strut =
      document.GetLayoutView()->ComputeScrollbars();
  // A left-side scrollbar (i.e. in an RTL writing-mode) should overlay the
  // snapshot viewport as well. This cannot currently happen in Chrome but it
  // can in other browsers. Handle this case in the event
  // https://crbug.com/249860 is ever fixed.
  // This includes outsets for scrollbar-gutter; both sides could include
  // scrollbar space simultaneously.
  left += scrollbar_strut.left.ToInt();
  right += scrollbar_strut.right.ToInt();
  bottom += scrollbar_strut.bottom.ToInt();
  top += scrollbar_strut.top.ToInt();

  gfx::Outsets outsets;
  outsets.set_top(top);
  outsets.set_right(right);
  outsets.set_bottom(bottom);
  outsets.set_left(left);
  return outsets;
}
}  // namespace

gfx::Rect ViewTransitionStyleTracker::GetSnapshotRootInFixedViewport() const {
  DCHECK(document_->View());
  DCHECK(document_->GetLayoutView());

  LayoutView& layout_view = *document_->GetLayoutView();
  LocalFrameView& frame_view = *document_->View();

  // Start with the position: fixed viewport and expand it by any
  // insetting UI such as the mobile URL bar, virtual-keyboard, scrollbars,
  // etc.
  // TODO(bokan): Differing behavior based on ViewportEnabled is a bit of a
  // kludge but is required since with ViewportEnabled the frame size may
  // actually be larger than than the LayoutView (the ICB) so we must use it.
  // However, LayoutView::ClientWidth/Height is the only way I know to get the
  // correct content size when the frame is inset by a scrollbar-gutter.
  // Luckily these two cases are mutually exclusive: ViewportEnabled is only
  // used with overlay scrollbars which have no gutter, however, it'd be better
  // if we could query a single property directly from layout information.
  gfx::Rect snapshot_viewport_rect =
      document_->GetSettings()->GetViewportEnabled()
          ? gfx::Rect(frame_view.Size().width(), frame_view.Size().height())
          : gfx::Rect(layout_view.ClientWidth().ToInt(),
                      layout_view.ClientHeight().ToInt());
  snapshot_viewport_rect.Outset(GetFixedToSnapshotViewportOutsets(*document_));

  return snapshot_viewport_rect;
}

gfx::Size ViewTransitionStyleTracker::GetSnapshotRootSize() const {
  return GetSnapshotRootInFixedViewport().size();
}

gfx::Vector2d ViewTransitionStyleTracker::GetFixedToSnapshotRootOffset() const {
  return GetSnapshotRootInFixedViewport().OffsetFromOrigin();
}

gfx::Vector2d ViewTransitionStyleTracker::GetFrameToSnapshotRootOffset() const {
  DCHECK(document_->GetLayoutView());
  DCHECK(document_->View());

  gfx::Outsets outsets = GetFixedToSnapshotViewportOutsets(*document_);
  gfx::Vector2d fixed_to_snapshot(-outsets.left(), -outsets.top());

  // A scrollbar (or gutter) on the left or top is placed within the frame but
  // offsets the fixed viewport so remove its size from the fixed-to-snapshot
  // offset to get the frame-to-snapshot offset.
  gfx::Vector2d frame_to_snapshot =
      fixed_to_snapshot +
      document_->GetLayoutView()->OriginAdjustmentForScrollbars();

  return frame_to_snapshot;
}

ViewTransitionState ViewTransitionStyleTracker::GetViewTransitionState() const {
  DCHECK_EQ(state_, State::kCaptured);

  ViewTransitionState transition_state;

  transition_state.device_pixel_ratio = device_pixel_ratio_;
  DCHECK(snapshot_root_layout_size_at_capture_);
  transition_state.snapshot_root_size_at_capture =
      *snapshot_root_layout_size_at_capture_;

  for (const auto& entry : element_data_map_) {
    const auto& element_data = entry.value;
    DCHECK_EQ(element_data->container_properties.size(), 1u)
        << "Multiple container properties are only created in the Animate "
           "phase";

    auto& element = transition_state.elements.emplace_back();
    element.tag_name = entry.key.Utf8();
    element.border_box_size_in_css_space = gfx::SizeF(
        element_data->container_properties[0].border_box_size_in_css_space);
    element.viewport_matrix =
        element_data->container_properties[0].snapshot_matrix;
    element.overflow_rect_in_layout_space =
        gfx::RectF(element_data->visual_overflow_rect_in_layout_space);
    element.snapshot_id = element_data->old_snapshot_id;
    element.paint_order = element_data->element_index;
    element.captured_rect_in_layout_space =
        element_data->captured_rect_in_layout_space;

    FlatMapBuilder<mojom::blink::ViewTransitionPropertyId, std::string>
        css_property_builder(element_data->captured_css_properties.size());
    for (const auto& [id, value] : element_data->captured_css_properties) {
      css_property_builder.Insert(ToTranstionPropertyId(id), value.Utf8());
    }
    element.captured_css_properties = std::move(css_property_builder).Finish();
    for (const auto& class_name : element_data->class_list) {
      element.class_list.push_back(class_name.Utf8());
    }
    element.containing_group_name =
        element_data->containing_group_name
            ? element_data->containing_group_name.Utf8()
            : "";
  }

  // Preserve the transition id for the new document.
  transition_state.transition_token = transition_token_;

  // To ensure the any new resources generated by the new document don't
  // collide in id with this document's resources, pass the next sequence id so
  // the new document can continue the sequence.
  transition_state.next_element_resource_id = GenerateResourceId().local_id();

  if (subframe_snapshot_layer_) {
    transition_state.subframe_snapshot_id =
        subframe_snapshot_layer_->ViewTransitionResourceId();
  }

  state_extracted_ = true;

  // TODO(khushalsagar): Need to send offsets to retain positioning of
  // ::view-transition.

  return transition_state;
}

bool ViewTransitionStyleTracker::SnapshotRootDidChangeSize() const {
  if (!snapshot_root_layout_size_at_capture_.has_value()) {
    return false;
  }

  gfx::Size current_size = GetSnapshotRootSize();

  // Allow 1px of diff since the snapshot root can be adjusted by
  // viewport-resizing UI (e.g. the virtual keyboard insets the viewport but
  // then outsets the viewport rect to get the snapshot root). These
  // adjustments can be off by a pixel due to different pixel snapping.
  if (std::abs(snapshot_root_layout_size_at_capture_->width() -
               current_size.width()) <= 1 &&
      std::abs(snapshot_root_layout_size_at_capture_->height() -
               current_size.height()) <= 1) {
    return false;
  }

  return true;
}

void ViewTransitionStyleTracker::InvalidateStyle() {
  ua_style_sheet_ = nullptr;

  if (auto* originating_element = document_->documentElement()) {
    originating_element->SetNeedsStyleRecalc(
        kLocalStyleChange, StyleChangeReasonForTracing::Create(
                               style_change_reason::kViewTransition));
  }

  auto invalidate_style = [](PseudoElement* pseudo_element) {
    pseudo_element->SetNeedsStyleRecalc(
        kLocalStyleChange, StyleChangeReasonForTracing::Create(
                               style_change_reason::kViewTransition));
  };
  ViewTransitionUtils::ForEachTransitionPseudo(*document_, invalidate_style);

  // Invalidate layout view compositing properties.
  if (auto* layout_view = document_->GetLayoutView()) {
    layout_view->SetNeedsPaintPropertyUpdate();
  }

  for (auto& entry : element_data_map_) {
    if (!entry.value->target_element ||
        entry.value->target_element->IsDocumentElement()) {
      continue;
    }

    // We need to recalc style on each of the target elements, because we store
    // whether the element is a view transition participant on the computed
    // style. InvalidateStyle() is an indication that this state may have
    // changed.
    entry.value->target_element->SetNeedsStyleRecalc(
        kLocalStyleChange, StyleChangeReasonForTracing::Create(
                               style_change_reason::kViewTransition));

    auto* object = entry.value->target_element->GetLayoutObject();
    if (!object)
      continue;

    // We propagate the view transition element id on an effect node for the
    // object. This means that we should update the paint properties to update
    // the view transition element id.
    object->SetNeedsPaintPropertyUpdate();

    // All elements participating in a transition are forced to become stacking
    // contexts. This state may change when the transition ends.
    if (auto* layer = object->EnclosingLayer()) {
      layer->DirtyStackingContextZOrderLists();
    }
  }

  document_->GetDisplayLockDocumentState()
      .NotifyViewTransitionPseudoTreeChanged();
}

CSSStyleSheet& ViewTransitionStyleTracker::UAStyleSheet() {
  if (ua_style_sheet_)
    return *ua_style_sheet_;

  // Animations are added in the start phase of the transition.
  // Note that the cached ua_style_sheet_ above is invalidated when |state_|
  // moves to kStarted stage to generate a new stylesheet including styles for
  // animations.
  const bool add_animations = state_ == State::kStarted;

  ViewTransitionStyleBuilder builder;
  builder.AddUAStyle(StaticUAStyles());
  if (add_animations)
    builder.AddUAStyle(AnimationUAStyles());

  for (auto& entry : element_data_map_) {
    const auto& view_transition_name = entry.key.GetString();
    auto& element_data = entry.value;

    // TODO(vmpstr): We will run a style resolution before the first time we get
    // a chance to update our rendering in RunPostPrePaintSteps. There is no
    // point in adding any styles here, because those will be wrong. The TODO
    // here is to skip this step earlier, instead of per each element.
    if (element_data->container_properties.empty())
      continue;

    gfx::Transform old_parent_inverse_transform;
    gfx::Transform new_parent_inverse_transform;
    if (element_data->containing_group_name && HasLiveNewContent()) {
      CHECK(element_data_map_.Contains(element_data->containing_group_name));
      const auto& containing_group_data =
          element_data_map_.at(element_data->containing_group_name);
      old_parent_inverse_transform =
          containing_group_data->cached_container_properties.snapshot_matrix
              .InverseOrIdentity();

      if (!containing_group_data->container_properties.empty()) {
        new_parent_inverse_transform =
            containing_group_data->container_properties.back()
                .snapshot_matrix.InverseOrIdentity();
      }
    }

    // This updates the styles on the pseudo-elements as described in
    // https://drafts.csswg.org/css-view-transitions-1/#style-transition-pseudo-elements-algorithm.
    builder.AddContainerStyles(
        view_transition_name, element_data->container_properties.back(),
        element_data->captured_css_properties, new_parent_inverse_transform);

    // This sets up the styles to animate the pseudo-elements as described in
    // https://drafts.csswg.org/css-view-transitions-1/#setup-transition-pseudo-elements-algorithm.
    if (add_animations) {
      CHECK(element_data->old_snapshot_id.IsValid() ||
            element_data->new_snapshot_id.IsValid());

      auto type = ViewTransitionStyleBuilder::AnimationType::kBoth;
      if (!element_data->old_snapshot_id.IsValid()) {
        type = ViewTransitionStyleBuilder::AnimationType::kNewOnly;
      } else if (!element_data->new_snapshot_id.IsValid()) {
        type = ViewTransitionStyleBuilder::AnimationType::kOldOnly;
      }

      builder.AddAnimations(type, view_transition_name,
                            element_data->cached_container_properties,
                            element_data->cached_animated_css_properties,
                            old_parent_inverse_transform);
    }
  }

  // We can't use the default UA parser, because it doesn't work for CSS URLs.
  // Filters & clip-path can have local (#) URLs and are copied into a UA
  // stylesheet, So we need to parse the stylesheet with a base URL override.
  auto* ua_parser_context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);

  auto* ua_parser_context_with_base_url =
      MakeGarbageCollected<CSSParserContext>(
          ua_parser_context, document_->BaseURL(),
          ua_parser_context->IsOriginClean(), ua_parser_context->GetReferrer(),
          ua_parser_context->Charset(), nullptr);

  auto* sheet =
      MakeGarbageCollected<StyleSheetContents>(ua_parser_context_with_base_url);
  sheet->ParseString(builder.Build());
  ua_style_sheet_ = MakeGarbageCollected<CSSStyleSheet>(sheet);
  return *ua_style_sheet_;
}

bool ViewTransitionStyleTracker::HasLiveNewContent() const {
  return state_ == State::kStarted;
}

void ViewTransitionStyleTracker::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(element_data_map_);
  visitor->Trace(pending_transition_element_names_);
  visitor->Trace(ua_style_sheet_);
}

void ViewTransitionStyleTracker::InvalidateHitTestingCache() {
  // Hit-testing data is cached based on the current DOM version. Normally, this
  // version is incremented any time there is a DOM modification or an attribute
  // change to some element (which can result in a new style). However, with
  // view transitions, we dynamically create and destroy hit-testable
  // pseudo elements based on the current state. This means that we have to
  // manually modify the DOM tree version since there is no other mechanism that
  // will do it.
  document_->IncDOMTreeVersion();
}

void ViewTransitionStyleTracker::ElementData::Trace(Visitor* visitor) const {
  visitor->Trace(target_element);
  visitor->Trace(clip_node);
}

// TODO(vmpstr): We need to write tests for the following:
// * A local transform on the transition element.
// * A transform on an ancestor which changes its screen space transform.
gfx::RectF ViewTransitionStyleTracker::ElementData::GetInkOverflowRect(
    bool use_cached_data) const {
  return gfx::RectF(use_cached_data
                        ? cached_visual_overflow_rect_in_layout_space
                        : visual_overflow_rect_in_layout_space);
}

gfx::RectF ViewTransitionStyleTracker::ElementData::GetCapturedSubrect(
    bool use_cached_data) const {
  auto captured_rect = use_cached_data ? cached_captured_rect_in_layout_space
                                       : captured_rect_in_layout_space;
  return captured_rect.value_or(GetInkOverflowRect(use_cached_data));
}

gfx::RectF ViewTransitionStyleTracker::ElementData::GetBorderBoxRect(
    bool use_cached_data,
    float device_scale_factor) const {
  // TODO(vmpstr): Make container_properties a non-vector non-optional member.
  if (!use_cached_data && container_properties.size() == 0) {
    return gfx::RectF();
  }
  PhysicalSize border_box_size_in_layout_space =
      use_cached_data
          ? cached_container_properties.border_box_size_in_css_space
          : container_properties.back().border_box_size_in_css_space;
  border_box_size_in_layout_space.Scale(device_scale_factor);
  return gfx::RectF(gfx::SizeF(border_box_size_in_layout_space));
}

void ViewTransitionStyleTracker::ElementData::CacheStateForOldSnapshot() {
  // This could be empty if the element was uncontained and was ignored for a
  // transition.
  DCHECK_LT(container_properties.size(), 2u);

  if (!container_properties.empty()) {
    cached_container_properties = container_properties.back();
  }
  cached_visual_overflow_rect_in_layout_space =
      visual_overflow_rect_in_layout_space;
  cached_captured_rect_in_layout_space = captured_rect_in_layout_space;

  FlatMapBuilder<CSSPropertyID, String> builder(
      std::size(kPropertiesToAnimate));
  for (auto& id : kPropertiesToAnimate) {
    auto it = captured_css_properties.find(id);
    if (it != captured_css_properties.end()) {
      builder.Insert(it->first, it->second);
    }
  }
  cached_animated_css_properties = std::move(builder).Finish();
}

// TODO(vmpstr): This could be optimized by caching values for individual layout
// boxes. However, it's unclear when the cache should be cleared.
PhysicalRect ViewTransitionStyleTracker::ComputeVisualOverflowRect(
    LayoutBoxModelObject& box,
    const LayoutBoxModelObject* ancestor) const {
  DCHECK(!box.IsLayoutView());

  if (ancestor) {
    if (auto* element = DynamicTo<Element>(box.GetNode());
        element && IsTransitionElement(*element)) {
      return {};
    }
  }

  const bool visible =
      box.StyleRef().UsedVisibility() == EVisibility::kVisible ||
      !box.VisualRectRespectsVisibility();
  const bool layered_effects_contribute_to_visual_overflow =
      ancestor ||
      !RuntimeEnabledFeatures::ViewTransitionLayeredCaptureEnabled();
  PhysicalRect result;

  if (layered_effects_contribute_to_visual_overflow) {
    if (auto clip_path_bounds =
            ClipPathClipper::LocalClipPathBoundingBox(box)) {
      // TODO(crbug.com/40840594): This is just the bounds of the clip-path, as
      // opposed to the intersection between the clip-path and the border box
      // bounds. This seems suboptimal, but that's the rect that we use further
      // down the pipeline to generate the texture.
      // TODO(khushalsagar): This doesn't account for CSS clip property.
      if (visible) {
        result = PhysicalRect::EnclosingRect(*clip_path_bounds);
        if (ancestor) {
          box.MapToVisualRectInAncestorSpace(ancestor, result,
                                             kUseGeometryMapper);
        }
      }

      return result;
    }
  }

  auto* paint_layer = box.Layer();
  if (!paint_layer || (!box.ChildPaintBlockedByDisplayLock() &&
                       !paint_layer->KnownToClipSubtreeToPaddingBox())) {
    const LayoutBoxModelObject* ancestor_for_recursion =
        ancestor ? ancestor : &box;
    for (auto* child = box.SlowFirstChild(); child;
         child = child->NextSibling()) {
      // Recurse for every child. Doing a paint walk here is insufficient
      // because of visibility considerations on each layout object. See
      // crbug.com/1458568 for more details.
      if (auto* child_box = DynamicTo<LayoutBoxModelObject>(child)) {
        PhysicalRect mapped_overflow_rect =
            ComputeVisualOverflowRect(*child_box, ancestor_for_recursion);
        result.Unite(mapped_overflow_rect);
      } else if (auto* child_text = DynamicTo<LayoutText>(child)) {
        if (box.IsLayoutInline()) {
          continue;
        }

        const bool child_visible =
            child_text->StyleRef().UsedVisibility() == EVisibility::kVisible ||
            !child_text->VisualRectRespectsVisibility();
        if (!child_visible) {
          continue;
        }

        auto overflow_rect = child_text->VisualOverflowRect();
        child_text->MapToVisualRectInAncestorSpace(
            ancestor_for_recursion, overflow_rect, kUseGeometryMapper);
        result.Unite(overflow_rect);
      }
    }
  }

  PhysicalRect overflow_rect;
  if (visible) {
    if (auto* layout_box = DynamicTo<LayoutBox>(box)) {
      overflow_rect = layout_box->PhysicalBorderBoxRect();
      if (layout_box->StyleRef().HasVisualOverflowingEffect()) {
        PhysicalBoxStrut outsets =
            layout_box->ComputeVisualEffectOverflowOutsets();
        overflow_rect.Expand(outsets);
      }
    } else {
      overflow_rect = To<LayoutInline>(box).LinesVisualOverflowBoundingBox();
    }
  }

  if (ancestor) {
    // For any recursive call, we map our overflow rect into the
    // ancestor space and combine that with the result. GeometryMapper should
    // take care of any filters and clips that are necessary between this box
    // and the ancestor.
    if (visible) {
      box.MapToVisualRectInAncestorSpace(ancestor, overflow_rect,
                                         kUseGeometryMapper);
      result.Unite(overflow_rect);
    }
  } else {
    // We're at the root of the recursion, so clip self painting descendant
    // overflow by the overflow clip rect, then add in the visual overflow (with
    // filters) from the own painting layer.
    if (auto* layout_box = DynamicTo<LayoutBox>(&box);
        layout_box && layout_box->ShouldClipOverflowAlongEitherAxis()) {
      result.Intersect(layout_box->OverflowClipRect(PhysicalOffset()));
    } else if (auto* layout_inline = DynamicTo<LayoutInline>(box)) {
      // We need the `overflow_rect` to be relative to the inline's
      // border-box. However, `LayoutInline::LinesVisualOverflowBoundingBox()`
      // is relative to the inline's container's border-box. The offset below
      // removes the translation between the container's border-box and the
      // inline's border-box.
      //
      // This mapping is done internally by
      // `LayoutObject::MapToVisualRectInAncestorSpace` so its not necessary
      // when computing overflow for an ancestor.
      overflow_rect.Move(-layout_inline->PhysicalLinesBoundingBox().offset);
    }

    if (visible) {
      result.Unite(overflow_rect);
    }

    if (layered_effects_contribute_to_visual_overflow) {
      result = box.ApplyFiltersToRect(result);
    }

    // TODO(crbug.com/1432868): This captures a couple of common cases --
    // box-shadow and no box shadow on the element. However, this isn't at all
    // comprehensive. The paint system determines per element whether it
    // should pixel snap or enclosing rect or something else. We need to think
    // of a better way to fix this for all cases.
    result.Move(box.FirstFragment().PaintOffset());
    if (visible && box.StyleRef().BoxShadow()) {
      result = PhysicalRect(ToEnclosingRect(result));
    } else {
      result = PhysicalRect(ToPixelSnappedRect(result));
    }
  }
  return result;
}

const char* ViewTransitionStyleTracker::StateToString(State state) {
  switch (state) {
    case State::kIdle:
      return "Idle";
    case State::kCapturing:
      return "Capturing";
    case State::kCaptured:
      return "Captured";
    case State::kStarted:
      return "Started";
    case State::kFinished:
      return "Finished";
  }
  NOTREACHED_IN_MIGRATION();
  return "???";
}

viz::ViewTransitionElementResourceId
ViewTransitionStyleTracker::GenerateResourceId() const {
  // If we've already send the state to the incoming document, generating a new
  // ID now would collide with IDs generated by that document.
  CHECK(!state_extracted_);
  auto* supplement = ViewTransitionSupplement::FromIfExists(*document_);
  CHECK(supplement);
  return supplement->GenerateResourceId(transition_token_);
}

void ViewTransitionStyleTracker::SnapBrowserControlsToFullyShown() {
  CHECK(document_->GetFrame()->IsOutermostMainFrame());
  BrowserControls& controls = document_->GetPage()->GetBrowserControls();
  ScrollableArea& root_scroller = *document_->View()->GetScrollableArea();

  // If (and only if) the page is scrolled to a non-0 offset, the top controls
  // animation keeps content from moving by producing a "counter-scroll" as the
  // controls animate. Preemptively perform this counter-scroll now, so that it
  // is included when snapshot transforms are computed.
  if (root_scroller.ScrollPosition().y()) {
    float counter_scroll = controls.TopHeight() - controls.ContentOffset();

    // Without FractionalScrollOffsets, the compositor commits only integer
    // values of scroll delta, but it always sends exact browser controls
    // delta. This means our computed counter-scroll does not include the
    // fractional part remaining in the compositor delta. The full counter
    // scroll will be an integer, since the compositor rounds the sent
    // offset, we round the counter-scroll as well which snaps it in the
    // opposing direction the compositor snapped to account for the missing
    // (or additional) pixel in the compositor's committed delta.
    if (!RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
      counter_scroll = base::ClampRound(counter_scroll);
    }

    // Fully show the controls also ensures scroll bounds can accommodate the
    // counter-scroll so do this before scrolling.
    controls.SetShownRatio(1, 1);
    root_scroller.ScrollBy(ScrollOffset(0, counter_scroll),
                           mojom::blink::ScrollType::kCompositor);

    // The next commit should overwrite any scrolling that occurred on the
    // compositor thread since it last committed values. Since the compositor
    // may still be animating the browser controls, and we add the full
    // controls distance here, any deltas that have occurred since this
    // BeginMainFrame would be double-applied. More generally, the snapshot
    // transform matrices will be computed in this Blink frame; any deltas
    // that have occurred on the compositor since this frame was issued won't
    // be accounted for in snapshot transforms.
    root_scroller.DropCompositorScrollDeltaNextCommit();
  } else {
    controls.SetShownRatio(1, 1);
  }
}

}  // namespace blink
