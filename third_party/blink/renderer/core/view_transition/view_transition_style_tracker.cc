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
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
namespace {

const char* kContainmentNotSatisfied =
    "Aborting transition. Element must contain paint or layout for "
    "view-transition-name : ";
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

bool SatisfiesContainment(const LayoutObject& object) {
  return object.ShouldApplyPaintContainment() ||
         object.ShouldApplyLayoutContainment();
}

absl::optional<String> ComputeInsetDifference(PhysicalRect reference_rect,
                                              const LayoutRect& target_rect,
                                              float device_pixel_ratio) {
  if (reference_rect.IsEmpty()) {
    DCHECK(target_rect.IsEmpty());
    return absl::nullopt;
  }

  // Reference rect is given to us in layout space, but target_rect is in css
  // space. Note that this currently relies on the fact that object-view-box
  // scales its parameters from CSS to layout space. However, that's a bug.
  // TODO(crbug.com/1324618): Fix this when the object-view-box bug is fixed.
  reference_rect.Scale(1.f / device_pixel_ratio);
  LayoutRect reference_layout_rect = reference_rect.ToLayoutRect();

  if (reference_layout_rect == target_rect)
    return absl::nullopt;

  float top_offset = (target_rect.Y() - reference_layout_rect.Y()).ToFloat();
  float right_offset =
      (reference_layout_rect.MaxX() - target_rect.MaxX()).ToFloat();
  float bottom_offset =
      (reference_layout_rect.MaxY() - target_rect.MaxY()).ToFloat();
  float left_offset = (target_rect.X() - reference_layout_rect.X()).ToFloat();

  return String::Format("inset(%.3fpx %.3fpx %.3fpx %.3fpx);", top_offset,
                        right_offset, bottom_offset, left_offset);
}

// TODO(vmpstr): This could be optimized by caching values for individual layout
// boxes. However, it's unclear when the cache should be cleared.
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
        // it's not an element shared element, it must mean we have it as a new
        // root name.
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
        // not an element shared element, it must mean we have it as an old root
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
    : document_(document) {}

ViewTransitionStyleTracker::ViewTransitionStyleTracker(
    Document& document,
    ViewTransitionState transition_state)
    : document_(document), state_(State::kCaptured), deserialized_(true) {
  captured_name_count_ = static_cast<int>(transition_state.elements.size());

  VectorOf<AtomicString> transition_names;
  transition_names.ReserveInitialCapacity(captured_name_count_);
  for (const auto& transition_state_element : transition_state.elements) {
    AtomicString name(transition_state_element.tag_name.c_str());
    transition_names.push_back(name);

    if (transition_state_element.is_root) {
      DCHECK(!old_root_data_);

      old_root_data_.emplace();
      old_root_data_->snapshot_id = transition_state_element.snapshot_id;
      old_root_data_->names.push_back(name);

      // TODO(khushalsagar): We should keep track of the snapshot viewport rect
      // size to handle changes in its bounds.
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

    element_data->CacheGeometryState();

    element_data_map_.insert(name, std::move(element_data));
  }
}

ViewTransitionStyleTracker::~ViewTransitionStyleTracker() = default;

void ViewTransitionStyleTracker::AddConsoleError(
    String message,
    Vector<DOMNodeId> related_nodes) {
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kRendering,
      mojom::blink::ConsoleMessageLevel::kError, std::move(message));
  console_message->SetNodes(document_->GetFrame(), std::move(related_nodes));
  document_->AddConsoleMessage(console_message);
}

void ViewTransitionStyleTracker::AddSharedElement(Element* element,
                                                  const AtomicString& name) {
  DCHECK(element);

  // Insert an empty hash set for the element if it doesn't exist, or get it if
  // it does.
  auto& value = pending_shared_element_names_
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
    AtomicString view_transition_name) const {
  DCHECK(view_transition_name);

  switch (pseudo_id) {
    case kPseudoIdViewTransitionGroup: {
      const bool has_root = old_root_data_ || new_root_data_;
      if (has_root) {
        return element_data_map_.empty();
      } else {
        DCHECK(!element_data_map_.empty());
        return element_data_map_.size() == 1;
      }
    }
    case kPseudoIdViewTransitionImagePair:
      return true;
    case kPseudoIdViewTransitionOld: {
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

void ViewTransitionStyleTracker::AddSharedElementsFromCSS() {
  DCHECK(document_ && document_->View());

  // We need our paint layers, and z-order lists which is done during
  // compositing inputs update.
  DCHECK_GE(document_->Lifecycle().GetState(),
            DocumentLifecycle::kCompositingInputsClean);

  AddSharedElementsFromCSSRecursive(
      document_->GetLayoutView()->PaintingLayer());
}

void ViewTransitionStyleTracker::AddSharedElementsFromCSSRecursive(
    PaintLayer* root) {
  // We want to call AddSharedElements in the order in which
  // PaintLayerPaintOrderIterator would cause us to paint the elements.
  // Specifically, parents are added before their children, and lower z-index
  // children are added before higher z-index children. Given that, what we
  // need to do is to first add `root`'s element, and then recurse using the
  // PaintLayerPaintOrderIterator which will return values in the correct
  // z-index order.
  //
  // Note that the order of calls to AddSharedElement determines the DOM order
  // of pseudo-elements constructed to represent the shared elements, which by
  // default will also represent the paint order of the pseudo-elements (unless
  // changed by something like z-index on the pseudo-elements).
  auto& root_object = root->GetLayoutObject();
  auto& root_style = root_object.StyleRef();
  if (root_style.ViewTransitionName()) {
    DCHECK(root_object.GetNode());
    DCHECK(root_object.GetNode()->IsElementNode());
    AddSharedElement(DynamicTo<Element>(root_object.GetNode()),
                     root_style.ViewTransitionName());
  }

  if (root_object.ChildPaintBlockedByDisplayLock())
    return;

  PaintLayerPaintOrderIterator child_iterator(root, kAllChildren);
  while (auto* child = child_iterator.Next()) {
    AddSharedElementsFromCSSRecursive(child);
  }
}

bool ViewTransitionStyleTracker::FlattenAndVerifyElements(
    VectorOf<Element>& elements,
    VectorOf<AtomicString>& transition_names,
    absl::optional<RootData>& root_data) {
  for (const auto& element : ViewTransitionSupplement::From(*document_)
                                 ->ElementsWithViewTransitionName()) {
    DCHECK(element->ComputedStyleRef().ViewTransitionName());

    // Ignore elements which are not rendered.
    if (!element->GetLayoutObject())
      continue;

    // Skip the transition if containment is not satisfied.
    if (!element->IsDocumentElement() &&
        !SatisfiesContainment(*element->GetLayoutObject())) {
      StringBuilder message;
      message.Append(kContainmentNotSatisfied);
      message.Append(element->ComputedStyleRef().ViewTransitionName());
      AddConsoleError(message.ReleaseString());
      return false;
    }
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
  for (auto& [element, names] : pending_shared_element_names_) {
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

  // Flatten `pending_shared_element_names_` into a vector of names and
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
  int next_index = OldRootDataTagSize();
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
  }
  for (const auto& root_name : AllRootTags())
    transition_names.push_front(root_name);

  // This informs the style engine the set of names we have, which will be used
  // to create the pseudo element tree.
  document_->GetStyleEngine().SetViewTransitionNames(transition_names);

  // We need a style invalidation to generate the pseudo element tree.
  InvalidateStyle();

  set_element_sequence_id_ = 0;
  pending_shared_element_names_.clear();

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
  // just the shared elements. We can split InvalidateStyle() into two functions
  // as an optimization.
  InvalidateStyle();

  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    element_data->target_element = nullptr;
    element_data->effect_node = nullptr;
  }
  root_effect_node_ = nullptr;
}

VectorOf<Element> ViewTransitionStyleTracker::GetTransitioningElements() const {
  // In stable states, we don't have shared elements.
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

  // Flatten `pending_shared_element_names_` into a vector of names and
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
    DCHECK_GT(captured_name_count_, 0);
    found_new_names = true;
  }

  int next_index =
      element_data_map_.size() + OldRootDataTagSize() + NewRootDataTagSize();
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
    element_data->target_element = element;
    element_data->new_snapshot_id = snapshot_id;
    DCHECK_LT(element_data->element_index, next_index);
  }

  // If the old and new root names have different size that means we likely have
  // at least one new name.
  found_new_names |= OldRootDataTagSize() != NewRootDataTagSize();
  if (!found_new_names && new_root_data_) {
    DCHECK(old_root_data_);
    for (const auto& new_name : new_root_data_->names) {
      // If the new root name is not also an old root name and it isn't a shared
      // element name, then we have a new name.
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
    for (const auto& root_name : AllRootTags())
      new_name_pairs.push_back(std::make_pair(root_name, ++next_name_index));
    for (auto& [name, data] : element_data_map_)
      new_name_pairs.push_back(std::make_pair(name, data->element_index));

    std::sort(new_name_pairs.begin(), new_name_pairs.end(),
              [](const std::pair<AtomicString, int>& left,
                 const std::pair<AtomicString, int>& right) {
                return left.second < right.second;
              });

    VectorOf<AtomicString> new_names;
    for (auto& [name, index] : new_name_pairs)
      new_names.push_back(name);

    document_->GetStyleEngine().SetViewTransitionNames(new_names);
  }

  DCHECK_GE(document_->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  // We need to run post prepaint steps here to ensure that the style would be
  // correct if computed by either the main frame or by getComputedStyle call.
  // TODO(vmpstr): Rename to something like UpdatePseudoGeometry.
  const bool continue_transition = RunPostPrePaintSteps();
  DCHECK(continue_transition)
      << "The transition should've been skipped by FlattenAndVerifyElements";

  // We need a style invalidation to generate new content pseudo elements for
  // new elements in the DOM.
  InvalidateStyle();

  if (auto* page = document_->GetPage())
    page->Animator().SetHasSharedElementTransition(true);
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
  state_ = State::kFinished;
  InvalidateHitTestingCache();

  // We need a style invalidation to remove the pseudo element tree. This needs
  // to be done before we clear the data, since we need to invalidate the shared
  // elements stored in `element_data_map_`.
  InvalidateStyle();

  element_data_map_.clear();
  pending_shared_element_names_.clear();
  set_element_sequence_id_ = 0;
  old_root_data_.reset();
  new_root_data_.reset();
  document_->GetStyleEngine().SetViewTransitionNames({});
  if (auto* page = document_->GetPage())
    page->Animator().SetHasSharedElementTransition(false);
}

void ViewTransitionStyleTracker::UpdateElementIndicesAndSnapshotId(
    Element* element,
    ViewTransitionElementId& index,
    viz::ViewTransitionElementResourceId& resource_id) const {
  DCHECK(element);

  for (const auto& entry : element_data_map_) {
    if (entry.value->target_element == element) {
      index.AddIndex(entry.value->element_index);
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
      LayoutSize size;
      viz::ViewTransitionElementResourceId snapshot_id;
      if (old_root_data_ &&
          old_root_data_->names.Contains(view_transition_name)) {
        size = LayoutSize(GetSnapshotViewportRect().size());
        snapshot_id = old_root_data_->snapshot_id;
      } else {
        DCHECK(view_transition_name);
        const auto& element_data =
            element_data_map_.find(view_transition_name)->value;
        // If live data is tracking new elements then use the cached data for
        // the pseudo element displaying snapshot of old element.
        bool use_cached_data = HasLiveNewContent();
        size = element_data->GetIntrinsicSize(use_cached_data);
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
      pseudo_element->SetIntrinsicSize(size);
      return pseudo_element;
    }
    case kPseudoIdViewTransitionNew: {
      LayoutSize size;
      viz::ViewTransitionElementResourceId snapshot_id;
      if (new_root_data_ &&
          new_root_data_->names.Contains(view_transition_name)) {
        size = LayoutSize(GetSnapshotViewportRect().size());
        snapshot_id = new_root_data_->snapshot_id;
      } else {
        DCHECK(view_transition_name);
        const auto& element_data =
            element_data_map_.find(view_transition_name)->value;
        bool use_cached_data = false;
        size = element_data->GetIntrinsicSize(use_cached_data);
        snapshot_id = element_data->new_snapshot_id;
      }
      auto* pseudo_element = MakeGarbageCollected<ViewTransitionContentElement>(
          parent, pseudo_id, view_transition_name, snapshot_id,
          /*is_live_content_element=*/true, this);
      pseudo_element->SetIntrinsicSize(size);
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
  bool needs_style_invalidation = false;

  // Use the document element's effective zoom, since that's what the parent
  // effective zoom would be.
  float device_pixel_ratio = document_->documentElement()
                                 ->GetLayoutObject()
                                 ->StyleRef()
                                 .EffectiveZoom();
  if (device_pixel_ratio_ != device_pixel_ratio) {
    device_pixel_ratio_ = device_pixel_ratio;
    needs_style_invalidation = true;
  }

  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (!element_data->target_element)
      continue;

    DCHECK_NE(element_data->target_element, document_->documentElement());
    auto* layout_object = element_data->target_element->GetLayoutObject();
    if (!layout_object || !SatisfiesContainment(*layout_object)) {
      StringBuilder message;
      message.Append(kContainmentNotSatisfied);
      message.Append(entry.key);
      AddConsoleError(message.ReleaseString());
      return false;
    }

    gfx::Transform snapshot_matrix = layout_object->LocalToAbsoluteTransform();

    if (document_->GetLayoutView()
            ->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
      // The SnapshotViewportRect offset below takes points from the fixed
      // viewport into the snapshot viewport. However, the transform is
      // currently into absolute coordinates; when the scrollbar appears on the
      // left, the fixed viewport origin is actually at (15, 0) in absolute
      // coordinates (assuming 15px scrollbars). Therefore we must first shift
      // by the scrollbar width so we're in fixed viewport coordinates.
      ScrollableArea& viewport = *document_->View()->LayoutViewport();
      snapshot_matrix.PostTranslate(-viewport.VerticalScrollbarWidth(), 0);
    }

    gfx::Vector2d snapshot_to_fixed_offset =
        -GetSnapshotViewportRect().OffsetFromOrigin();
    snapshot_matrix.PostTranslate(snapshot_to_fixed_offset.x(),
                                  snapshot_to_fixed_offset.y());

    snapshot_matrix.Zoom(1.0 / device_pixel_ratio_);

    // ResizeObserverEntry is created to reuse the logic for parsing object size
    // for different types of LayoutObjects.
    auto* resize_observer_entry =
        MakeGarbageCollected<ResizeObserverEntry>(element_data->target_element);
    auto entry_size = resize_observer_entry->borderBoxSize()[0];
    LayoutSize border_box_size_in_css_space =
        layout_object->IsHorizontalWritingMode()
            ? LayoutSize(LayoutUnit(entry_size->inlineSize()),
                         LayoutUnit(entry_size->blockSize()))
            : LayoutSize(LayoutUnit(entry_size->blockSize()),
                         LayoutUnit(entry_size->inlineSize()));
    if (float effective_zoom = layout_object->StyleRef().EffectiveZoom();
        std::abs(effective_zoom - device_pixel_ratio_) >=
        std::numeric_limits<float>::epsilon()) {
      border_box_size_in_css_space.Scale(effective_zoom / device_pixel_ratio_);
    }

    PhysicalRect visual_overflow_rect_in_layout_space;
    if (auto* box = DynamicTo<LayoutBox>(layout_object))
      visual_overflow_rect_in_layout_space = ComputeVisualOverflowRect(*box);

    WritingMode writing_mode = layout_object->StyleRef().GetWritingMode();

    ContainerProperties container_properties(border_box_size_in_css_space,
                                             snapshot_matrix);
    if (!element_data->container_properties.empty() &&
        element_data->container_properties.back() == container_properties &&
        visual_overflow_rect_in_layout_space ==
            element_data->visual_overflow_rect_in_layout_space &&
        writing_mode == element_data->container_writing_mode) {
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

    PseudoId live_content_element = HasLiveNewContent()
                                        ? kPseudoIdViewTransitionNew
                                        : kPseudoIdViewTransitionOld;
    if (auto* pseudo_element =
            document_->documentElement()->GetNestedPseudoElement(
                live_content_element, entry.key)) {
      // A pseudo element of type |tansition*content| must be created using
      // ViewTransitionContentElement.
      bool use_cached_data = false;
      LayoutSize size = element_data->GetIntrinsicSize(use_cached_data);
      static_cast<ViewTransitionContentElement*>(pseudo_element)
          ->SetIntrinsicSize(size);
    }

    // Ensure that the cached state stays in sync with the current state while
    // we're capturing.
    if (state_ == State::kCapturing)
      element_data->CacheGeometryState();

    needs_style_invalidation = true;
  }

  if (needs_style_invalidation)
    InvalidateStyle();

  return true;
}

bool ViewTransitionStyleTracker::HasActiveAnimations() const {
  bool has_animations = false;
  auto accumulate_pseudo = [&has_animations](PseudoElement* pseudo_element) {
    if (has_animations)
      return;

    auto* animations = pseudo_element->GetElementAnimations();
    if (!animations)
      return;

    for (auto& animation_pair : animations->Animations()) {
      if (auto* effect = animation_pair.key->effect())
        has_animations = has_animations || effect->IsCurrent();
    }
  };
  ViewTransitionUtils::ForEachTransitionPseudo(*document_, accumulate_pseudo);
  return has_animations;
}

PaintPropertyChangeType ViewTransitionStyleTracker::UpdateEffect(
    Element* element,
    EffectPaintPropertyNode::State state,
    const EffectPaintPropertyNodeOrAlias& current_effect) {
  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (element_data->target_element != element)
      continue;

    if (!element_data->effect_node) {
      element_data->effect_node =
          EffectPaintPropertyNode::Create(current_effect, std::move(state));
#if DCHECK_IS_ON()
      element_data->effect_node->SetDebugName("SharedElementTransition");
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
    root_effect_node_->SetDebugName("SharedElementTransition");
#endif
    return PaintPropertyChangeType::kNodeAddedOrRemoved;
  }
  return root_effect_node_->Update(current_effect, std::move(state), {});
}

EffectPaintPropertyNode* ViewTransitionStyleTracker::GetEffect(
    Element* element) const {
  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (element_data->target_element != element)
      continue;
    DCHECK(element_data->effect_node);
    return element_data->effect_node.get();
  }
  NOTREACHED();
  return nullptr;
}

EffectPaintPropertyNode* ViewTransitionStyleTracker::GetRootEffect() const {
  DCHECK(root_effect_node_);
  return root_effect_node_.get();
}

bool ViewTransitionStyleTracker::IsSharedElement(Node* node) const {
  // In stable states, we don't have shared elements.
  if (state_ == State::kIdle || state_ == State::kCaptured)
    return false;

  for (auto& entry : element_data_map_) {
    if (entry.value->target_element == node)
      return true;
  }
  return false;
}

bool ViewTransitionStyleTracker::IsRootTransitioning() const {
  switch (state_) {
    case State::kIdle:
      return false;
    case State::kCapturing:
    case State::kCaptured:
      return !!old_root_data_;
    case State::kStarted:
    case State::kFinished:
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

gfx::Rect ViewTransitionStyleTracker::GetSnapshotViewportRect() const {
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

gfx::Vector2d ViewTransitionStyleTracker::GetRootSnapshotPaintOffset() const {
  DCHECK(document_->GetLayoutView());
  DCHECK(document_->View());

  gfx::Outsets outsets = GetFixedToSnapshotViewportOutsets(*document_);
  int left = outsets.left();
  int top = outsets.top();

  // Paint already applies an offset for a left-side vertical scrollbar so
  // don't offset by it here again.
  if (document_->GetLayoutView()
          ->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
    left -= document_->View()->LayoutViewport()->VerticalScrollbarWidth();
  }

  return gfx::Vector2d(left, top);
}

ViewTransitionState ViewTransitionStyleTracker::GetViewTransitionState() const {
  DCHECK_EQ(state_, State::kCaptured);

  ViewTransitionState transition_state;
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
    // TODO(khushalsagar): Also writing mode.

    DCHECK_GT(element.paint_order, 0);
  }

  if (old_root_data_) {
    auto& element = transition_state.elements.emplace_back();
    // TODO(khushalsagar): What about non utf8 strings?
    element.tag_name = old_root_data_->names[0].Utf8();
    element.border_box_size_in_css_space =
        gfx::SizeF(GetSnapshotViewportRect().size());
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

  auto* originating_element = document_->documentElement();
  originating_element->SetNeedsStyleRecalc(
      kLocalStyleChange, StyleChangeReasonForTracing::Create(
                             style_change_reason::kViewTransition));

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
    auto* object = entry.value->target_element->GetLayoutObject();
    if (!object)
      continue;

    // We propagate the shared element id on an effect node for the object. This
    // means that we should update the paint properties to update the shared
    // element id.
    object->SetNeedsPaintPropertyUpdate();
  }

  document_->GetDisplayLockDocumentState()
      .NotifySharedElementPseudoTreeChanged();
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
  // 3. A name is an old root and a new shared element. The AllRootTags loop
  // skips this name. The element map loop updates the container for the new
  // shared element size and transform. The animation code of that loop adds an
  // animation from old root size and identity matrix.
  //
  // 4. A name is a new root only (entry animation for root). Its only visited
  // in AllRootTags and its a default fade-in.
  //
  // 5. A name is a new root and old shared element. We visit it in AllRootTags
  // to set up the destination state. We skip setting its styles in the
  // `element_data_map_` loop since latest value comes from AllRootTags. We do
  // set the animation in that loop since we need the "from" state.
  //
  // 6. A name is a new and old shared element (or maybe exit/enter for shared
  // element only -- no roots involved. Everything is done in the
  // `element_data_map_` loop.

  // Size and position the root container behind any viewport insetting widgets
  // (such as the URL bar) so that it's stable across a transition. This rect
  // is called the "snapshot viewport".  Since this is applied in style,
  // convert from physical pixels to CSS pixels.
  gfx::RectF snapshot_viewport_css_pixels = gfx::ScaleRect(
      gfx::RectF(GetSnapshotViewportRect()), 1.f / device_pixel_ratio_);

  // If adjusted, the root is always translated up and left underneath any UI
  // so the direction must always be negative.
  DCHECK_LE(snapshot_viewport_css_pixels.x(), 0.f);
  DCHECK_LE(snapshot_viewport_css_pixels.y(), 0.f);

  builder.AddRootStyles(snapshot_viewport_css_pixels);

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

      // Incoming inset also only makes sense if the name is a new shared
      // element (not a new root).
      const bool has_new_image = element_data->new_snapshot_id.IsValid();
      absl::optional<String> incoming_inset =
          has_new_image
              ? ComputeInsetDifference(
                    element_data->visual_overflow_rect_in_layout_space,
                    LayoutRect(LayoutPoint(),
                               element_data->container_properties.back()
                                   .border_box_size_in_css_space),
                    device_pixel_ratio_)
              : absl::nullopt;

      if (incoming_inset) {
        builder.AddIncomingObjectViewBox(view_transition_name, *incoming_inset);
      }
    }

    // Outgoing inset only makes sense if the name is an old shared element (not
    // an old root).
    const bool has_old_image = element_data->old_snapshot_id.IsValid();
    if (has_old_image && !name_is_old_root) {
      absl::optional<String> outgoing_inset = ComputeInsetDifference(
          element_data->cached_visual_overflow_rect_in_layout_space,
          LayoutRect(LayoutPoint(), element_data->cached_container_properties
                                        .border_box_size_in_css_space),
          device_pixel_ratio_);

      if (outgoing_inset) {
        builder.AddOutgoingObjectViewBox(view_transition_name, *outgoing_inset);
      }
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
        auto layout_view_size = LayoutSize(GetSnapshotViewportRect().size());
        // Note that we want the size in css space, which means we need to undo
        // the effective zoom.
        layout_view_size.Scale(
            1 / document_->GetLayoutView()->StyleRef().EffectiveZoom());
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
  visitor->Trace(pending_shared_element_names_);
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
// * A local transform on the shared element.
// * A transform on an ancestor which changes its screen space transform.
LayoutSize ViewTransitionStyleTracker::ElementData::GetIntrinsicSize(
    bool use_cached_data) {
  LayoutSize box_size =
      use_cached_data
          ? cached_visual_overflow_rect_in_layout_space.size.ToLayoutSize()
          : visual_overflow_rect_in_layout_space.size.ToLayoutSize();
  return box_size;
}

void ViewTransitionStyleTracker::ElementData::CacheGeometryState() {
  // This could be empty if the element was uncontained and was ignored for a
  // transition.
  DCHECK_LT(container_properties.size(), 2u);

  if (!container_properties.empty())
    cached_container_properties = container_properties.back();
  cached_visual_overflow_rect_in_layout_space =
      visual_overflow_rect_in_layout_space;
}

PhysicalRect ViewTransitionStyleTracker::ComputeVisualOverflowRect(
    LayoutBoxModelObject& box,
    LayoutBoxModelObject* ancestor) {
  if (ancestor && IsSharedElement(box.GetNode())) {
    return {};
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
      if (!child_layer->IsSelfPaintingLayer())
        continue;
      LayoutBoxModelObject& child_box = child_layer->GetLayoutObject();

      PhysicalRect mapped_overflow_rect =
          ComputeVisualOverflowRect(child_box, ancestor ? ancestor : &box);
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
  }
  return result;
}

}  // namespace blink
