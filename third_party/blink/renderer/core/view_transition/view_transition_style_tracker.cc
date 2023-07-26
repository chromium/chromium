// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"

#include <limits>

#include "base/containers/contains.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/layout_view_transition_root.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_content_element.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_style_builder.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
namespace {

// The non-root elements start their index counting from this number. This is to
// avoid unstable sorts when the index of a root element conflicts with a
// non-root element.
constexpr const int kElementIndexOffset = 1000;

const char* kDuplicateTagBaseError =
    "Unexpected duplicate view-transition-name: ";

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
absl::optional<gfx::RectF> ComputeCaptureRect(
    const int max_capture_size,
    const PhysicalRect& ink_overflow_rect_in_border_box_space,
    const gfx::Transform& element_to_snapshot_root,
    const gfx::Size& snapshot_root_size) {
  if (ink_overflow_rect_in_border_box_space.Width() <= max_capture_size &&
      ink_overflow_rect_in_border_box_space.Height() <= max_capture_size) {
    return absl::nullopt;
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

int ComputeMaxCaptureSize(absl::optional<int> max_texture_size,
                          const gfx::Size& snapshot_root_size) {
  // While we can render up to the max texture size, that would significantly
  // add to the memory overhead. So limit to up to a viewport worth of
  // additional content.
  const int max_bounds_based_on_viewport =
      2 * std::max(snapshot_root_size.width(), snapshot_root_size.height());

  // If the max texture size is not known yet, clip to the size of the snapshot
  // root. The snapshot root corresponds to the maximum screen bounds, we must
  // be able to allocate a buffer of that size.
  const int computed_max_texture_size = max_texture_size.value_or(
      std::max(snapshot_root_size.width(), snapshot_root_size.height()));
  DCHECK_LE(snapshot_root_size.width(), computed_max_texture_size);
  DCHECK_LE(snapshot_root_size.height(), computed_max_texture_size);

  return std::min(max_bounds_based_on_viewport, computed_max_texture_size);
}

gfx::Transform ComputeViewportTransform(const LayoutObject& object) {
  DCHECK(object.HasLayer());
  auto& first_fragment = object.FirstFragment();
  DCHECK(ToRoundedPoint(first_fragment.PaintOffset()).IsOrigin())
      << first_fragment.PaintOffset();
  auto paint_properties = first_fragment.LocalBorderBoxProperties();

  auto& root_fragment = object.GetDocument().GetLayoutView()->FirstFragment();
  const auto& root_properties = root_fragment.LocalBorderBoxProperties();

  auto transform = GeometryMapper::SourceToDestinationProjection(
      paint_properties.Transform(), root_properties.Transform());

  if (!transform.HasPerspective()) {
    transform.Round2dTranslationComponents();
  }

  return transform;
}

gfx::Transform ConvertFromTopLeftToCenter(
    const gfx::Transform& transform_from_top_left,
    const LayoutSize& box_size) {
  gfx::Transform transform_from_center;
  transform_from_center.Translate(-box_size.Width() / 2,
                                  -box_size.Height() / 2);
  transform_from_center.PreConcat(transform_from_top_left);
  transform_from_center.Translate(box_size.Width() / 2, box_size.Height() / 2);

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
    viz::ViewTransitionElementResourceId snapshot_id;
    if (pseudo_id == kPseudoIdViewTransitionOld) {
      if (style_tracker_->old_root_data_ &&
          style_tracker_->old_root_data_->names.Contains(
              view_transition_name())) {
        snapshot_id = style_tracker_->old_root_data_->snapshot_id;
        DCHECK(snapshot_id.IsValid());
      } else if (auto it = style_tracker_->element_data_map_.find(
                     view_transition_name());
                 it != style_tracker_->element_data_map_.end()) {
        snapshot_id = it->value->old_snapshot_id;
      } else {
        // If we're being called with a name that isn't an old_root name and
        // it's not a transition element, it must mean we have it as a new root
        // name.
        DCHECK(style_tracker_->new_root_data_);
        DCHECK(style_tracker_->new_root_data_->names.Contains(
            view_transition_name()));
      }
    } else {
      if (style_tracker_->new_root_data_ &&
          style_tracker_->new_root_data_->names.Contains(
              view_transition_name())) {
        snapshot_id = style_tracker_->new_root_data_->snapshot_id;
        DCHECK(snapshot_id.IsValid());
      } else if (auto it = style_tracker_->element_data_map_.find(
                     view_transition_name());
                 it != style_tracker_->element_data_map_.end()) {
        snapshot_id = it->value->new_snapshot_id;
      } else {
        // If we're being called with a name that isn't a new_root name and it's
        // not a transition element, it must mean we have it as an old root
        // name.
        DCHECK(style_tracker_->old_root_data_);
        DCHECK(style_tracker_->old_root_data_->names.Contains(
            view_transition_name()));
      }
    }
    return snapshot_id.IsValid();
  }
};

ViewTransitionStyleTracker::ViewTransitionStyleTracker(Document& document)
    : document_(document),
      device_pixel_ratio_(DevicePixelRatioFromDocument(document)) {}

ViewTransitionStyleTracker::ViewTransitionStyleTracker(
    Document& document,
    ViewTransitionState transition_state)
    : document_(document), state_(State::kCaptured), deserialized_(true) {
  device_pixel_ratio_ = transition_state.device_pixel_ratio;
  captured_name_count_ = static_cast<int>(transition_state.elements.size());
  snapshot_root_size_at_capture_ =
      transition_state.snapshot_root_size_at_capture;

  VectorOf<AtomicString> transition_names;
  transition_names.ReserveInitialCapacity(captured_name_count_);
  for (const auto& transition_state_element : transition_state.elements) {
    auto name =
        AtomicString::FromUTF8(transition_state_element.tag_name.c_str());
    transition_names.push_back(name);

    if (transition_state_element.is_root) {
      DCHECK(!old_root_data_);

      old_root_data_.emplace();
      old_root_data_->snapshot_id = transition_state_element.snapshot_id;
      old_root_data_->names.push_back(name);

      // TODO(khushalsagar): We should keep track of the snapshot viewport rect
      // size to handle changes in its bounds.
      // https://crbug.com/1404957.
      continue;
    }

    DCHECK(!element_data_map_.Contains(name));
    auto* element_data = MakeGarbageCollected<ElementData>();

    element_data->container_properties.emplace_back(
        LayoutSize(transition_state_element.border_box_size_in_css_space),
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

    element_data->CacheGeometryState();

    element_data_map_.insert(name, std::move(element_data));
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
    const AtomicString& name) {
  DCHECK(element);

  // Insert an empty hash set for the element if it doesn't exist, or get it if
  // it does.
  auto& value = pending_transition_element_names_
                    .insert(element, HashSet<std::pair<AtomicString, int>>())
                    .stored_value->value;
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
      const bool has_root = old_root_data_ || new_root_data_;
      if (has_root) {
        return element_data_map_.empty();
      } else {
        DCHECK(!element_data_map_.empty());
        return element_data_map_.size() == 1;
      }
    }
    case kPseudoIdViewTransitionImagePair:
      DCHECK(view_transition_name);
      return true;
    case kPseudoIdViewTransitionOld: {
      DCHECK(view_transition_name);
      if (new_root_data_ &&
          new_root_data_->names.Contains(view_transition_name)) {
        return false;
      }

      auto it = element_data_map_.find(view_transition_name);
      if (it == element_data_map_.end()) {
        DCHECK(old_root_data_ &&
               old_root_data_->names.Contains(view_transition_name));
        return true;
      }

      const auto& element_data = it->value;
      return !element_data->new_snapshot_id.IsValid();
    }
    case kPseudoIdViewTransitionNew: {
      DCHECK(view_transition_name);
      if (old_root_data_ &&
          old_root_data_->names.Contains(view_transition_name)) {
        return false;
      }

      auto it = element_data_map_.find(view_transition_name);
      if (it == element_data_map_.end()) {
        DCHECK(new_root_data_ &&
               new_root_data_->names.Contains(view_transition_name));
        return true;
      }

      const auto& element_data = it->value;
      return !element_data->old_snapshot_id.IsValid();
    }
    default:
      NOTREACHED();
  }

  return false;
}

void ViewTransitionStyleTracker::AddTransitionElementsFromCSS() {
  DCHECK(document_ && document_->View());

  // We need our paint layers, and z-order lists which is done during
  // compositing inputs update.
  DCHECK_GE(document_->Lifecycle().GetState(),
            DocumentLifecycle::kCompositingInputsClean);

  AddTransitionElementsFromCSSRecursive(
      document_->GetLayoutView()->PaintingLayer());
}

void ViewTransitionStyleTracker::AddTransitionElementsFromCSSRecursive(
    PaintLayer* root) {
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
  if (root_style.ViewTransitionName() && !root_object.IsFragmented()) {
    DCHECK(root_object.GetNode());
    DCHECK(root_object.GetNode()->IsElementNode());
    AddTransitionElement(DynamicTo<Element>(root_object.GetNode()),
                         root_style.ViewTransitionName());
  }

  if (root_object.ChildPaintBlockedByDisplayLock())
    return;

  PaintLayerPaintOrderIterator child_iterator(root, kAllChildren);
  while (auto* child = child_iterator.Next()) {
    AddTransitionElementsFromCSSRecursive(child);
  }
}

bool ViewTransitionStyleTracker::FlattenAndVerifyElements(
    VectorOf<Element>& elements,
    VectorOf<AtomicString>& transition_names,
    absl::optional<RootData>& root_data) {
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

    const bool is_root = element->IsDocumentElement();
    if (is_root && !root_data)
      root_data.emplace();

    for (auto& name_pair : names) {
      if (is_root) {
        // The order of the root names doesn't matter, so we don't keep the
        // ordering.
        root_data->names.push_back(name_pair.first);
      } else {
        flat_list.push_back(MakeGarbageCollected<FlatData>(
            element, name_pair.first, name_pair.second));
      }
    }
  }

  // Sort it.
  std::sort(flat_list.begin(), flat_list.end(),
            [](const FlatData* a, const FlatData* b) {
              return a->ordering < b->ordering;
            });
  DCHECK(!root_data || !root_data->names.empty());

  auto have_root_name = [&root_data](const AtomicString& name) {
    return root_data && root_data->names.Contains(name);
  };

  // Verify it.
  for (auto& flat_data : flat_list) {
    auto& name = flat_data->name;
    auto& element = flat_data->element;

    if (UNLIKELY(transition_names.Contains(name) || have_root_name(name))) {
      StringBuilder message;
      message.Append(kDuplicateTagBaseError);
      message.Append(name);
      AddConsoleError(message.ReleaseString());
      return false;
    }

    transition_names.push_back(name);
    elements.push_back(element);
  }
  return true;
}

bool ViewTransitionStyleTracker::Capture() {
  DCHECK_EQ(state_, State::kIdle);

  // Flatten `pending_transition_element_names_` into a vector of names and
  // elements. This process also verifies that the name-element combinations are
  // valid.
  VectorOf<AtomicString> transition_names;
  VectorOf<Element> elements;
  bool success =
      FlattenAndVerifyElements(elements, transition_names, old_root_data_);
  if (!success)
    return false;

  // Now we know that we can start a transition. Update the state and populate
  // `element_data_map_`.
  state_ = State::kCapturing;
  InvalidateHitTestingCache();

  captured_name_count_ = transition_names.size() + OldRootDataTagSize();

  element_data_map_.ReserveCapacityForSize(captured_name_count_);
  HeapHashMap<Member<Element>, viz::ViewTransitionElementResourceId>
      element_snapshot_ids;
  int next_index = kElementIndexOffset;
  for (wtf_size_t i = 0; i < transition_names.size(); ++i) {
    const auto& name = transition_names[i];
    const auto& element = elements[i];

    // Reuse any previously generated snapshot_id for this element. If there was
    // none yet, then generate the resource id.
    auto& snapshot_id =
        element_snapshot_ids
            .insert(element, viz::ViewTransitionElementResourceId())
            .stored_value->value;
    if (!snapshot_id.IsValid()) {
      snapshot_id = viz::ViewTransitionElementResourceId::Generate();
      capture_resource_ids_.push_back(snapshot_id);
    }

    auto* element_data = MakeGarbageCollected<ElementData>();
    element_data->target_element = element;
    element_data->element_index = next_index++;
    element_data->old_snapshot_id = snapshot_id;
    element_data_map_.insert(name, std::move(element_data));
  }

  if (old_root_data_) {
    old_root_data_->snapshot_id =
        viz::ViewTransitionElementResourceId::Generate();
    capture_resource_ids_.push_back(old_root_data_->snapshot_id);
  }
  for (const auto& root_name : AllRootTags())
    transition_names.push_front(root_name);

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

  DCHECK(!snapshot_root_size_at_capture_.has_value());
  snapshot_root_size_at_capture_ = GetSnapshotRootSize();

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
    element_data->effect_node = nullptr;
  }
  root_effect_node_ = nullptr;
}

VectorOf<Element> ViewTransitionStyleTracker::GetTransitioningElements() const {
  // In stable states, we don't have transitioning elements.
  if (state_ == State::kIdle || state_ == State::kCaptured)
    return {};

  VectorOf<Element> result;
  for (auto& entry : element_data_map_) {
    if (entry.value->target_element)
      result.push_back(entry.value->target_element);
  }
  return result;
}

bool ViewTransitionStyleTracker::Start() {
  DCHECK_EQ(state_, State::kCaptured);

  // Flatten `pending_transition_element_names_` into a vector of names and
  // elements. This process also verifies that the name-element combinations are
  // valid.
  VectorOf<AtomicString> transition_names;
  VectorOf<Element> elements;
  bool success =
      FlattenAndVerifyElements(elements, transition_names, new_root_data_);
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
  // entries, which in turn would start from kElementIndexOffset.
  int next_index = element_data_map_.size() + kElementIndexOffset;
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
    if (!snapshot_id.IsValid())
      snapshot_id = viz::ViewTransitionElementResourceId::Generate();

    auto& element_data = element_data_map_.find(name)->value;
    DCHECK(!element_data->target_element);
    element_data->target_element = element;
    element_data->new_snapshot_id = snapshot_id;
    // Verify that the element_index assigned in Capture is less than next_index
    // here, just as a sanity check.
    DCHECK_LT(element_data->element_index, next_index);
  }

  // If the old and new root names have different size that means we likely have
  // at least one new name.
  found_new_names |= OldRootDataTagSize() != NewRootDataTagSize();
  if (!found_new_names && new_root_data_) {
    DCHECK(old_root_data_);
    for (const auto& new_name : new_root_data_->names) {
      // If the new root name is not also an old root name and it isn't a
      // transition element name, then we have a new name.
      if (!old_root_data_->names.Contains(new_name) &&
          !element_data_map_.Contains(new_name)) {
        found_new_names = true;
        break;
      }
    }
  }

  if (new_root_data_) {
    new_root_data_->snapshot_id =
        viz::ViewTransitionElementResourceId::Generate();
  }

  if (found_new_names) {
    VectorOf<std::pair<AtomicString, int>> new_name_pairs;
    int next_name_index = 0;
    HashSet<AtomicString> unique_names;
    for (const auto& root_name : AllRootTags()) {
      new_name_pairs.push_back(std::make_pair(root_name, ++next_name_index));
      DCHECK(!unique_names.Contains(root_name));
      unique_names.insert(root_name);
    }
    for (auto& [name, data] : element_data_map_) {
      if (!unique_names.Contains(name)) {
        new_name_pairs.push_back(std::make_pair(name, data->element_index));
        unique_names.insert(name);
      }
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
  old_root_data_.reset();
  new_root_data_.reset();
  document_->GetStyleEngine().SetViewTransitionNames({});
  if (auto* page = document_->GetPage())
    page->Animator().SetHasViewTransition(false);
}

void ViewTransitionStyleTracker::UpdateElementIndicesAndSnapshotId(
    Element* element,
    ViewTransitionElementId& index,
    viz::ViewTransitionElementResourceId& resource_id) const {
  DCHECK(element);

  // In cc, the index is matched against the elements based on the 0 based
  // position in a vector, so the index here really does need to be a 0-n range.
  // This means we need to subtract back the kElementIndexOffset for elements.
  // However, at this point we know that a root is either transitioning or not,
  // so we might need to reserve a single slot for the root.
  int index_offset = -kElementIndexOffset + IsRootTransitioning();
  for (const auto& entry : element_data_map_) {
    if (entry.value->target_element == element) {
      index.AddIndex(entry.value->element_index + index_offset);
      const auto& snapshot_id = HasLiveNewContent()
                                    ? entry.value->new_snapshot_id
                                    : entry.value->old_snapshot_id;
      DCHECK(!resource_id.IsValid() || resource_id == snapshot_id);
      if (!resource_id.IsValid())
        resource_id = snapshot_id;
    }
  }
  DCHECK(resource_id.IsValid());
}

auto ViewTransitionStyleTracker::GetCurrentRootData() const
    -> absl::optional<RootData> {
  return HasLiveNewContent() ? new_root_data_ : old_root_data_;
}

void ViewTransitionStyleTracker::UpdateRootIndexAndSnapshotId(
    ViewTransitionElementId& index,
    viz::ViewTransitionElementResourceId& resource_id) const {
  if (!IsRootTransitioning())
    return;

  index.AddIndex(0);
  const auto& root_data = GetCurrentRootData();
  DCHECK(root_data);
  resource_id = root_data->snapshot_id;
  DCHECK(resource_id.IsValid());
}

PseudoElement* ViewTransitionStyleTracker::CreatePseudoElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) {
  DCHECK(IsTransitionPseudoElement(pseudo_id));
  DCHECK(pseudo_id == kPseudoIdViewTransition || view_transition_name);

  switch (pseudo_id) {
    case kPseudoIdViewTransition:
    case kPseudoIdViewTransitionGroup:
      return MakeGarbageCollected<ViewTransitionPseudoElementBase>(
          parent, pseudo_id, view_transition_name, this);
    case kPseudoIdViewTransitionImagePair:
      return MakeGarbageCollected<ImageWrapperPseudoElement>(
          parent, pseudo_id, view_transition_name, this);
    case kPseudoIdViewTransitionOld: {
      gfx::RectF captured_rect;
      gfx::RectF border_box_rect;
      viz::ViewTransitionElementResourceId snapshot_id;
      if (old_root_data_ &&
          old_root_data_->names.Contains(view_transition_name)) {
        captured_rect = gfx::RectF(gfx::SizeF(GetSnapshotRootSize()));
        border_box_rect = captured_rect;
        snapshot_id = old_root_data_->snapshot_id;
      } else {
        DCHECK(view_transition_name);
        const auto& element_data =
            element_data_map_.find(view_transition_name)->value;
        // If live data is tracking new elements then use the cached data for
        // the pseudo element displaying snapshot of old element.
        bool use_cached_data = HasLiveNewContent();
        captured_rect = element_data->GetCapturedSubrect(use_cached_data);
        border_box_rect = element_data->GetBorderBoxRect(use_cached_data,
                                                         device_pixel_ratio_);
        snapshot_id = element_data->old_snapshot_id;
      }
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
      gfx::RectF captured_rect;
      gfx::RectF border_box_rect;
      viz::ViewTransitionElementResourceId snapshot_id;
      if (new_root_data_ &&
          new_root_data_->names.Contains(view_transition_name)) {
        captured_rect = gfx::RectF(gfx::SizeF(GetSnapshotRootSize()));
        border_box_rect = captured_rect;
        snapshot_id = new_root_data_->snapshot_id;
      } else {
        DCHECK(view_transition_name);
        const auto& element_data =
            element_data_map_.find(view_transition_name)->value;
        bool use_cached_data = false;
        captured_rect = element_data->GetCapturedSubrect(use_cached_data);
        border_box_rect = element_data->GetBorderBoxRect(use_cached_data,
                                                         device_pixel_ratio_);
        snapshot_id = element_data->new_snapshot_id;
      }
      auto* pseudo_element = MakeGarbageCollected<ViewTransitionContentElement>(
          parent, pseudo_id, view_transition_name, snapshot_id,
          /*is_live_content_element=*/true, this);
      pseudo_element->SetIntrinsicSize(captured_rect, border_box_rect);
      return pseudo_element;
    }
    default:
      NOTREACHED();
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
    if (new_root_data_) {
      return false;
    }

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

  // Check if the root element participates in a transition and has been
  // fragmented.
  if (new_root_data_ &&
      document_->documentElement()->GetLayoutObject()->IsFragmented()) {
    return false;
  }

  const int max_capture_size = ComputeMaxCaptureSize(
      document_->GetPage()->GetChromeClient().GetMaxRenderBufferBounds(
          *document_->GetFrame()),
      *snapshot_root_size_at_capture_);

  bool needs_style_invalidation = false;
  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (!element_data->target_element)
      continue;

    DCHECK(document_->documentElement());
    DCHECK_NE(element_data->target_element, document_->documentElement());
    auto* layout_object = element_data->target_element->GetLayoutObject();
    // TODO(khushalsagar): Verify that skipping a transition when things become
    // display none is aligned with spec.
    if (!layout_object) {
      return false;
    }

    // End the transition if any of the objects have become fragmented.
    if (layout_object->IsFragmented()) {
      return false;
    }

    // TODO(bokan): This doesn't account for the local offset of an inline
    // element within its container. The object-view-box inset will ensure the
    // snapshot is rendered in the correct place but the pseudo is positioned
    // w.r.t. to the container. This can look awkward since the opposing
    // snapshot may have a different object-view-box. Inline positioning and
    // scaling more generally might use some improvements.
    // https://crbug.com/1416951.
    auto snapshot_matrix_in_layout_space =
        ComputeViewportTransform(*layout_object);

    if (document_->GetLayoutView()
            ->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
      // The SnapshotViewportRect offset below takes points from the fixed
      // viewport into the snapshot viewport. However, the transform is
      // currently into absolute coordinates; when the scrollbar appears on the
      // left, the fixed viewport origin is actually at (15, 0) in absolute
      // coordinates (assuming 15px scrollbars). Therefore we must first shift
      // by the scrollbar width so we're in fixed viewport coordinates.
      ScrollableArea& viewport = *document_->View()->LayoutViewport();
      snapshot_matrix_in_layout_space.PostTranslate(
          -viewport.VerticalScrollbarWidth(), 0);
    }

    gfx::Vector2d snapshot_to_fixed_offset = -GetFixedToSnapshotRootOffset();
    snapshot_matrix_in_layout_space.PostTranslate(snapshot_to_fixed_offset.x(),
                                                  snapshot_to_fixed_offset.y());

    auto snapshot_matrix_in_css_space = snapshot_matrix_in_layout_space;
    snapshot_matrix_in_css_space.Zoom(1.0 / device_pixel_ratio_);

    LayoutSize border_box_size_in_css_space;
    if (layout_object->IsSVGChild() || IsA<LayoutBox>(layout_object)) {
      // ResizeObserverEntry is created to reuse the logic for parsing object
      // size for different types of LayoutObjects. However, this works only
      // for SVGChild and LayoutBox.
      auto* resize_observer_entry = MakeGarbageCollected<ResizeObserverEntry>(
          element_data->target_element);
      auto entry_size = resize_observer_entry->borderBoxSize()[0];
      border_box_size_in_css_space =
          layout_object->IsHorizontalWritingMode()
              ? LayoutSize(LayoutUnit(entry_size->inlineSize()),
                           LayoutUnit(entry_size->blockSize()))
              : LayoutSize(LayoutUnit(entry_size->blockSize()),
                           LayoutUnit(entry_size->inlineSize()));
    } else if (auto* box_model =
                   DynamicTo<LayoutBoxModelObject>(layout_object)) {
      border_box_size_in_css_space =
          LayoutSize(box_model->BorderBoundingBox().size());
    }

    // If the object's effective zoom differs from device_pixel_ratio, adjust
    // the border box size by that difference to get the css space size.
    if (float effective_zoom = layout_object->StyleRef().EffectiveZoom();
        std::abs(effective_zoom - device_pixel_ratio_) >=
        std::numeric_limits<float>::epsilon()) {
      border_box_size_in_css_space.Scale(effective_zoom / device_pixel_ratio_);
    }

    snapshot_matrix_in_css_space = ConvertFromTopLeftToCenter(
        snapshot_matrix_in_css_space, border_box_size_in_css_space);

    PhysicalRect visual_overflow_rect_in_layout_space;
    if (auto* box = DynamicTo<LayoutBoxModelObject>(layout_object)) {
      visual_overflow_rect_in_layout_space =
          RuntimeEnabledFeatures::
                  ViewTransitionLayoutObjectVisualOverflowEnabled()
              ? ComputeVisualOverflowRect(*box)
              : ComputeVisualOverflowRectWithPaintLayers(*box);
    }

    // This is intentionally computed in layout space to include scaling from
    // device scale factor. The element's texture will be in physical pixel
    // bounds which includes this scale.
    auto captured_rect_in_layout_space = ComputeCaptureRect(
        max_capture_size, visual_overflow_rect_in_layout_space,
        snapshot_matrix_in_layout_space, *snapshot_root_size_at_capture_);

    WritingMode writing_mode = layout_object->StyleRef().GetWritingMode();

    ContainerProperties container_properties(border_box_size_in_css_space,
                                             snapshot_matrix_in_css_space);
    if (!element_data->container_properties.empty() &&
        element_data->container_properties.back() == container_properties &&
        visual_overflow_rect_in_layout_space ==
            element_data->visual_overflow_rect_in_layout_space &&
        writing_mode == element_data->container_writing_mode &&
        captured_rect_in_layout_space ==
            element_data->captured_rect_in_layout_space) {
      continue;
    }

    // Only add a new container properties entry if it differs from the last
    // one.
    if (element_data->container_properties.empty()) {
      element_data->container_properties.push_back(container_properties);
    } else if (element_data->container_properties.back() !=
               container_properties) {
      if (state_ == State::kStarted)
        element_data->container_properties.push_back(container_properties);
      else
        element_data->container_properties.back() = container_properties;
    }

    element_data->visual_overflow_rect_in_layout_space =
        visual_overflow_rect_in_layout_space;
    element_data->container_writing_mode = writing_mode;
    element_data->captured_rect_in_layout_space = captured_rect_in_layout_space;

    PseudoId live_content_element = HasLiveNewContent()
                                        ? kPseudoIdViewTransitionNew
                                        : kPseudoIdViewTransitionOld;
    DCHECK(document_->documentElement());
    if (auto* pseudo_element =
            document_->documentElement()->GetNestedPseudoElement(
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
    if (state_ == State::kCapturing)
      element_data->CacheGeometryState();

    needs_style_invalidation = true;
  }

  if (LayoutViewTransitionRoot* snapshot_containing_block =
          document_->GetLayoutView()->GetViewTransitionRoot()) {
    snapshot_containing_block->UpdateSnapshotStyle(*this);
  }

  if (needs_style_invalidation)
    InvalidateStyle();

  return true;
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

PaintPropertyChangeType ViewTransitionStyleTracker::UpdateEffect(
    const Element& element,
    EffectPaintPropertyNode::State state,
    const EffectPaintPropertyNodeOrAlias& current_effect) {
  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (element_data->target_element != &element) {
      continue;
    }

    if (!element_data->effect_node) {
      element_data->effect_node =
          EffectPaintPropertyNode::Create(current_effect, std::move(state));
#if DCHECK_IS_ON()
      element_data->effect_node->SetDebugName(element.DebugName() +
                                              "ViewTransition");
#endif
      return PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    return element_data->effect_node->Update(current_effect, std::move(state),
                                             {});
  }
  NOTREACHED();
  return PaintPropertyChangeType::kUnchanged;
}

PaintPropertyChangeType ViewTransitionStyleTracker::UpdateRootEffect(
    EffectPaintPropertyNode::State state,
    const EffectPaintPropertyNodeOrAlias& current_effect) {
  if (!root_effect_node_) {
    root_effect_node_ =
        EffectPaintPropertyNode::Create(current_effect, std::move(state));
#if DCHECK_IS_ON()
    root_effect_node_->SetDebugName("ViewTransition");
#endif
    return PaintPropertyChangeType::kNodeAddedOrRemoved;
  }
  return root_effect_node_->Update(current_effect, std::move(state), {});
}

const EffectPaintPropertyNode* ViewTransitionStyleTracker::GetEffect(
    const Element& element) const {
  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (element_data->target_element != &element) {
      continue;
    }
    DCHECK(element_data->effect_node);
    return element_data->effect_node.get();
  }
  NOTREACHED();
  return nullptr;
}

const EffectPaintPropertyNode* ViewTransitionStyleTracker::GetRootEffect()
    const {
  DCHECK(root_effect_node_);
  return root_effect_node_.get();
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
        current_transform, *element_data->captured_rect_in_layout_space,
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
  NOTREACHED();
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
    return element_data->clip_node.get();
  }
  NOTREACHED();
  return nullptr;
}

bool ViewTransitionStyleTracker::IsTransitionElement(
    const Element& element) const {
  // In stable states, we don't have transition elements.
  if (state_ == State::kIdle || state_ == State::kCaptured)
    return false;

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
    return entry.value->captured_rect_in_layout_space.has_value();
  }
  return false;
}

bool ViewTransitionStyleTracker::IsRootTransitioning() const {
  switch (state_) {
    case State::kIdle:
    case State::kCaptured:
    case State::kFinished:
      return false;
    case State::kCapturing:
      return !!old_root_data_;
    case State::kStarted:
      return !!new_root_data_;
  }
  NOTREACHED();
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

  NOTREACHED();
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
    // TODO(bokan): This assumes any shown ratio implies controls are shown. We
    // many need to do some synchronization to make this work seamlessly with
    // URL bar animations.
    BrowserControls& controls = document.GetPage()->GetBrowserControls();
    if (controls.TopShownRatio())
      top += controls.TopHeight() - controls.TopMinHeight();
    if (controls.BottomShownRatio())
      bottom += controls.BottomHeight() - controls.BottomMinHeight();

    bottom += document.GetFrame()
                  ->GetWidgetForLocalRoot()
                  ->GetVirtualKeyboardResizeHeight();
  }

  // A left-side scrollbar (i.e. in an RTL writing-mode) should overlay the
  // snapshot viewport as well. This cannot currently happen in Chrome but it
  // can in other browsers. Handle this case in the event
  // https://crbug.com/249860 is ever fixed.
  LocalFrameView& view = *document.View();
  if (document.GetLayoutView()
          ->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    left += view.LayoutViewport()->VerticalScrollbarWidth();
  } else {
    right += view.LayoutViewport()->VerticalScrollbarWidth();
  }

  bottom += view.LayoutViewport()->HorizontalScrollbarHeight();

  gfx::Outsets outsets;
  outsets.set_top(top);
  outsets.set_right(right);
  outsets.set_bottom(bottom);
  outsets.set_left(left);
  return outsets;
}
}  // namespace

gfx::Rect ViewTransitionStyleTracker::GetSnapshotRootInFixedViewport() const {
  DCHECK(document_->GetLayoutView());
  DCHECK(document_->View());
  DCHECK(document_->GetFrame());

  LocalFrameView& view = *document_->View();

  // Start with the FrameView size, i.e. the position: fixed viewport, and
  // expand the viewport by any insetting UI such as the mobile URL bar,
  // virtual-keyboard, scrollbars, etc.
  gfx::Rect snapshot_viewport_rect(
      view.LayoutViewport()->ExcludeScrollbars(view.Size()));
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
  int left = outsets.left();
  int top = outsets.top();

  // Left-side vertical scrollbars are placed within the frame but offset the
  // fixed viewport so remove its width from the fixed-to-snapshot offset to
  // get the frame-to-snapshot offset.
  if (document_->GetLayoutView()
          ->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    left -= document_->View()->LayoutViewport()->VerticalScrollbarWidth();
  }

  return gfx::Vector2d(-left, -top);
}

ViewTransitionState ViewTransitionStyleTracker::GetViewTransitionState() const {
  DCHECK_EQ(state_, State::kCaptured);

  ViewTransitionState transition_state;

  transition_state.device_pixel_ratio = device_pixel_ratio_;
  DCHECK(snapshot_root_size_at_capture_);
  transition_state.snapshot_root_size_at_capture =
      *snapshot_root_size_at_capture_;

  for (const auto& entry : element_data_map_) {
    const auto& element_data = entry.value;
    DCHECK_EQ(element_data->container_properties.size(), 1u)
        << "Multiple container properties are only created in the Animate "
           "phase";

    auto& element = transition_state.elements.emplace_back();
    // TODO(khushalsagar): What about non utf8 strings?
    element.tag_name = entry.key.Utf8();
    element.border_box_size_in_css_space =
        gfx::SizeF(element_data->container_properties[0]
                       .border_box_size_in_css_space.Width(),
                   element_data->container_properties[0]
                       .border_box_size_in_css_space.Height());
    element.viewport_matrix =
        element_data->container_properties[0].snapshot_matrix;
    element.overflow_rect_in_layout_space =
        gfx::RectF(element_data->visual_overflow_rect_in_layout_space);
    element.snapshot_id = element_data->old_snapshot_id;
    element.paint_order = element_data->element_index;
    element.is_root = false;
    element.captured_rect_in_layout_space =
        element_data->captured_rect_in_layout_space;

    // TODO(khushalsagar): Also writing mode.

    DCHECK(!old_root_data_ || element.paint_order > 0);
  }

  if (old_root_data_) {
    auto& element = transition_state.elements.emplace_back();
    // TODO(khushalsagar): What about non utf8 strings?
    element.tag_name = old_root_data_->names[0].Utf8();
    element.border_box_size_in_css_space = gfx::SizeF(GetSnapshotRootSize());
    element.snapshot_id = old_root_data_->snapshot_id;
    element.paint_order = 0;
    element.is_root = true;
  }

  // TODO(khushalsagar): Need to send offsets to retain positioning of
  // ::view-transition.

  return transition_state;
}

void ViewTransitionStyleTracker::InvalidateStyle() {
  ua_style_sheet_.reset();
  document_->GetStyleEngine().InvalidateUAViewTransitionStyle();

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
  if (auto* layout_view = document_->GetLayoutView())
    layout_view->SetNeedsPaintPropertyUpdate();

  for (auto& entry : element_data_map_) {
    if (!entry.value->target_element)
      continue;

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

HashSet<AtomicString> ViewTransitionStyleTracker::AllRootTags() const {
  HashSet<AtomicString> all_root_names;
  if (old_root_data_) {
    for (auto& name : old_root_data_->names)
      all_root_names.insert(name);
  }
  if (new_root_data_) {
    for (auto& name : new_root_data_->names)
      all_root_names.insert(name);
  }
  return all_root_names;
}

const String& ViewTransitionStyleTracker::UAStyleSheet() {
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

  // SUBTLETY AHEAD!
  // There are several situations to consider when creating the styles and
  // animation styles below:
  //
  // 1. A name is both an old and new root. We will only visit the AllRootTags
  // loop and correctly append styles (modulo TODO in that loop). Note that this
  // name will not be in the `element_data_map_` (DCHECKed in that loop).
  //
  // 2. A name is an old root only (exit animation for root). The style is set
  // up in the AllrootTags loop and fades out through AnimationUAStyles.
  //
  // 3. A name is an old root and a new transition element. The AllRootTags loop
  // skips this name. The element map loop updates the container for the new
  // transition element size and transform. The animation code of that loop adds
  // an animation from old root size and identity matrix.
  //
  // 4. A name is a new root only (entry animation for root). Its only visited
  // in AllRootTags and its a default fade-in.
  //
  // 5. A name is a new root and old transition element. We visit it in
  // AllRootTags to set up the destination state. We skip setting its styles in
  // the `element_data_map_` loop since latest value comes from AllRootTags. We
  // do set the animation in that loop since we need the "from" state.
  //
  // 6. A name is a new and old transition element (or maybe exit/enter for
  // transition element only -- no roots involved. Everything is done in the
  // `element_data_map_` loop.

  for (auto& root_name : AllRootTags()) {
    // This is case 3 above.
    bool name_is_old_root =
        old_root_data_ && old_root_data_->names.Contains(root_name);
    if (name_is_old_root && element_data_map_.Contains(root_name)) {
      DCHECK(
          element_data_map_.find(root_name)->value->new_snapshot_id.IsValid());
      continue;
    }

    // TODO(vmpstr): For animations, we need to re-target the layout size if it
    // changes, but right now we only use the latest layout view size.
    // Note that we don't set the writing-mode since it would inherit from the
    // :root anyway, so there is no reason to put it on the pseudo elements.
    builder.AddContainerStyles(root_name, "right: 0; bottom: 0;");

    bool name_is_new_root =
        new_root_data_ && new_root_data_->names.Contains(root_name);
    if (name_is_old_root && name_is_new_root)
      builder.AddPlusLighter(root_name);
  }

  for (auto& entry : element_data_map_) {
    const auto& view_transition_name = entry.key.GetString();
    auto& element_data = entry.value;

    // TODO(vmpstr): We will run a style resolution before the first time we get
    // a chance to update our rendering in RunPostPrePaintSteps. There is no
    // point in adding any styles here, because those will be wrong. The TODO
    // here is to skip this step earlier, instead of per each element.
    if (element_data->container_properties.empty())
      continue;

    const bool name_is_old_root =
        old_root_data_ && old_root_data_->names.Contains(view_transition_name);
    const bool name_is_new_root =
        new_root_data_ && new_root_data_->names.Contains(view_transition_name);
    // The name can't be both old and new root, since it shouldn't be in the
    // `element_data_map_`. This is case 1 above.
    DCHECK(!name_is_old_root || !name_is_new_root);

    // Skipping this if a name is a new root. This is case 5 above.
    if (!name_is_new_root) {
      // ::view-transition-group styles using computed properties for each
      // element.
      builder.AddContainerStyles(view_transition_name,
                                 element_data->container_properties.back(),
                                 element_data->container_writing_mode);
    }

    // TODO(khushalsagar) : We'll need to retarget the animation if the final
    // value changes during the start phase.
    if (add_animations) {
      // If the old snapshot is valid, then we add a transition if we have
      // either the new snapshot (case 6 above) or the name is a new root (case
      // 5 above).
      //
      // The else-if case is case 3 above: if we have the new snapshot and the
      // name is an old root, in which case we also add an animation but sourced
      // from the old root, rather than from the cached element data.
      if (element_data->old_snapshot_id.IsValid() &&
          (element_data->new_snapshot_id.IsValid() || name_is_new_root)) {
        builder.AddAnimationAndBlending(
            view_transition_name, element_data->cached_container_properties);
      } else if (element_data->new_snapshot_id.IsValid() && name_is_old_root) {
        auto layout_view_size = LayoutSize(GetSnapshotRootSize());
        // Note that we want the size in css space, which means we need to undo
        // the effective zoom.
        layout_view_size.Scale(1 / device_pixel_ratio_);
        builder.AddAnimationAndBlending(
            view_transition_name,
            ContainerProperties(layout_view_size, gfx::Transform()));
      }
    }
  }

  ua_style_sheet_ = builder.Build();
  return *ua_style_sheet_;
}

bool ViewTransitionStyleTracker::HasLiveNewContent() const {
  return state_ == State::kStarted;
}

void ViewTransitionStyleTracker::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(element_data_map_);
  visitor->Trace(pending_transition_element_names_);
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
  LayoutSize border_box_size_in_layout_space =
      use_cached_data
          ? cached_container_properties.border_box_size_in_css_space
          : container_properties.back().border_box_size_in_css_space;
  border_box_size_in_layout_space.Scale(device_scale_factor);
  return gfx::RectF(LayoutRect(LayoutPoint(), border_box_size_in_layout_space));
}

void ViewTransitionStyleTracker::ElementData::CacheGeometryState() {
  // This could be empty if the element was uncontained and was ignored for a
  // transition.
  DCHECK_LT(container_properties.size(), 2u);

  if (!container_properties.empty()) {
    cached_container_properties = container_properties.back();
  }
  cached_visual_overflow_rect_in_layout_space =
      visual_overflow_rect_in_layout_space;
  cached_captured_rect_in_layout_space = captured_rect_in_layout_space;
}

// TODO(vmpstr): This could be optimized by caching values for individual layout
// boxes. However, it's unclear when the cache should be cleared.
PhysicalRect ViewTransitionStyleTracker::ComputeVisualOverflowRect(
    LayoutBoxModelObject& box,
    LayoutBoxModelObject* ancestor) {
  if (ancestor) {
    if (auto* element = DynamicTo<Element>(box.GetNode());
        element && IsTransitionElement(*element)) {
      return {};
    }
  }

  const bool visible = box.StyleRef().Visibility() == EVisibility::kVisible ||
                       !box.VisualRectRespectsVisibility();
  if (auto clip_path_bounds = ClipPathClipper::LocalClipPathBoundingBox(box)) {
    // TODO(crbug.com/1326514): This is just the bounds of the clip-path, as
    // opposed to the intersection between the clip-path and the border box
    // bounds. This seems suboptimal, but that's the rect that we use further
    // down the pipeline to generate the texture.
    // TODO(khushalsagar): This doesn't account for CSS clip property.
    PhysicalRect bounds;
    if (visible) {
      bounds = PhysicalRect::EnclosingRect(*clip_path_bounds);
      if (ancestor) {
        box.MapToVisualRectInAncestorSpace(ancestor, bounds,
                                           kUseGeometryMapper);
      }
    }
    return bounds;
  }

  PhysicalRect result;
  auto* paint_layer = box.Layer();
  if (!paint_layer || (!box.ChildPaintBlockedByDisplayLock() &&
                       !paint_layer->KnownToClipSubtreeToPaddingBox())) {
    LayoutBoxModelObject* ancestor_for_recursion = ancestor ? ancestor : &box;
    for (auto* child = box.SlowFirstChild(); child;
         child = child->NextSibling()) {
      // Recurse for every child. Doing a paint walk here is insufficient
      // because of visibility considerations on each layout object. See
      // crbug.com/1458568 for more details.
      if (auto* child_box = DynamicTo<LayoutBoxModelObject>(child)) {
        PhysicalRect mapped_overflow_rect =
            ComputeVisualOverflowRect(*child_box, ancestor_for_recursion);
        result.Unite(mapped_overflow_rect);
      }
    }
  }

  PhysicalRect overflow_rect;
  if (visible) {
    if (auto* layout_box = DynamicTo<LayoutBox>(box)) {
      overflow_rect = layout_box->PhysicalBorderBoxRect();
      if (layout_box->StyleRef().HasVisualOverflowingEffect()) {
        NGPhysicalBoxStrut outsets =
            layout_box->ComputeVisualEffectOverflowOutsets();
        overflow_rect.Expand(outsets);
      }
    } else if (auto* layout_inline = DynamicTo<LayoutInline>(box)) {
      overflow_rect = layout_inline->PhysicalLinesBoundingBox();
    } else {
      overflow_rect = PhysicalRect(box.BorderBoundingBox());
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
    }

    if (visible) {
      result.Unite(overflow_rect);
    }
    result = box.ApplyFiltersToRect(result);

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

PhysicalRect
ViewTransitionStyleTracker::ComputeVisualOverflowRectWithPaintLayers(
    LayoutBoxModelObject& box,
    LayoutBoxModelObject* ancestor) {
  if (ancestor) {
    if (auto* element = DynamicTo<Element>(box.GetNode());
        element && IsTransitionElement(*element)) {
      return {};
    }
  }

  if (auto clip_path_bounds = ClipPathClipper::LocalClipPathBoundingBox(box)) {
    // TODO(crbug.com/1326514): This is just the bounds of the clip-path, as
    // opposed to the intersection between the clip-path and the border box
    // bounds. This seems suboptimal, but that's the rect that we use further
    // down the pipeline to generate the texture.
    // TODO(khushalsagar): This doesn't account for CSS clip property.
    auto bounds = PhysicalRect::EnclosingRect(*clip_path_bounds);
    if (ancestor) {
      box.MapToVisualRectInAncestorSpace(ancestor, bounds, kUseGeometryMapper);
    }
    return bounds;
  }

  PhysicalRect result;
  auto* paint_layer = box.Layer();
  if (!box.ChildPaintBlockedByDisplayLock() &&
      paint_layer->HasSelfPaintingLayerDescendant() &&
      !paint_layer->KnownToClipSubtreeToPaddingBox()) {
    PaintLayerPaintOrderIterator iterator(paint_layer, kAllChildren);
    while (PaintLayer* child_layer = iterator.Next()) {
      if (!child_layer->IsSelfPaintingLayer()) {
        continue;
      }
      LayoutBoxModelObject& child_box = child_layer->GetLayoutObject();

      PhysicalRect mapped_overflow_rect =
          ComputeVisualOverflowRectWithPaintLayers(child_box,
                                                   ancestor ? ancestor : &box);
      result.Unite(mapped_overflow_rect);
    }
  }

  if (ancestor) {
    // For any recursive call, we instead map our overflow rect into the
    // ancestor space and combine that with the result. GeometryMapper should
    // take care of any filters and clips that are necessary between this box
    // and the ancestor.
    auto overflow_rect = box.PhysicalVisualOverflowRect();
    box.MapToVisualRectInAncestorSpace(ancestor, overflow_rect,
                                       kUseGeometryMapper);
    result.Unite(overflow_rect);
  } else {
    // We're at the root of the recursion, so clip self painting descendant
    // overflow by the overflow clip rect, then add in the visual overflow (with
    // filters) from the own painting layer.
    if (auto* layout_box = DynamicTo<LayoutBox>(&box);
        layout_box && layout_box->ShouldClipOverflowAlongEitherAxis()) {
      result.Intersect(layout_box->OverflowClipRect(PhysicalOffset()));
    }
    result.Unite(box.PhysicalVisualOverflowRectIncludingFilters());

    // TODO(crbug.com/1432868): This captures a couple of common cases --
    // box-shadow and no box shadow on the element. However, this isn't at all
    // comprehensive. The paint system determines per element whether it
    // should pixel snap or enclosing rect or something else. We need to think
    // of a better way to fix this for all cases.
    result.Move(box.FirstFragment().PaintOffset());
    if (box.StyleRef().BoxShadow()) {
      result = PhysicalRect(ToEnclosingRect(result));
    } else {
      result = PhysicalRect(ToPixelSnappedRect(result));
    }
  }
  return result;
}

bool ViewTransitionStyleTracker::SnapshotRootDidChangeSize() const {
  if (!snapshot_root_size_at_capture_.has_value()) {
    return false;
  }

  gfx::Size current_size = GetSnapshotRootSize();

  // Allow 1px of diff since the snapshot root can be adjusted by
  // viewport-resizing UI (e.g. the virtual keyboard insets the viewport but
  // then outsets the viewport rect to get the snapshot root). These
  // adjustments can be off by a pixel due to different pixel snapping.
  if (std::abs(snapshot_root_size_at_capture_->width() -
               current_size.width()) <= 1 &&
      std::abs(snapshot_root_size_at_capture_->height() -
               current_size.height()) <= 1) {
    return false;
  }

  return true;
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
  NOTREACHED();
  return "???";
}

}  // namespace blink
