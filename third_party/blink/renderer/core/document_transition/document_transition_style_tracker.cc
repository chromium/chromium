// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_style_tracker.h"

#include <limits>

#include "components/viz/common/shared_element_resource_id.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_content_element.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_utils.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
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
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
namespace {

const char* kElementSetModificationError =
    "The element set cannot be modified at this transition state.";
const char* kContainmentNotSatisfied =
    "Dropping element from transition. Shared element must contain paint or "
    "layout";
const char* kDuplicateTagBaseError =
    "Unexpected duplicate page transition tag: ";

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

  return String::Format("inset(%.3fpx %.3fpx %.3fpx %.3fpx)", top_offset,
                        right_offset, bottom_offset, left_offset);
}

// TODO(vmpstr): This could be optimized by caching values for individual layout
// boxes. However, it's unclear when the cache should be cleared.
PhysicalRect ComputeVisualOverflowRect(LayoutBox* box) {
  if (auto clip_path_bounds = ClipPathClipper::LocalClipPathBoundingBox(*box)) {
    // TODO(crbug.com/1326514): This is just the bounds of the clip-path, as
    // opposed to the intersection between the clip-path and the border box
    // bounds. This seems suboptimal, but that's the rect that we use further
    // down the pipeline to generate the texture.
    return PhysicalRect::EnclosingRect(*clip_path_bounds);
  }

  PhysicalRect result;
  for (auto* child = box->Layer()->FirstChild(); child;
       child = child->NextSibling()) {
    auto* child_box = child->GetLayoutBox();
    PhysicalRect overflow_rect = ComputeVisualOverflowRect(child_box);
    child_box->MapToVisualRectInAncestorSpace(box, overflow_rect);
    result.Unite(overflow_rect);
  }
  // Clip self painting descendant overflow by the overflow clip rect, then add
  // in the visual overflow from the own painting layer.
  result.Intersect(box->OverflowClipRect(PhysicalOffset()));
  result.Unite(box->PhysicalVisualOverflowRectIncludingFilters());
  return result;
}

}  // namespace

class DocumentTransitionStyleTracker::ImageWrapperPseudoElement
    : public DocumentTransitionPseudoElementBase {
 public:
  ImageWrapperPseudoElement(Element* parent,
                            PseudoId pseudo_id,
                            const AtomicString& document_transition_tag,
                            const DocumentTransitionStyleTracker* style_tracker)
      : DocumentTransitionPseudoElementBase(parent,
                                            pseudo_id,
                                            document_transition_tag,
                                            style_tracker) {}

  ~ImageWrapperPseudoElement() override = default;

 private:
  bool CanGeneratePseudoElement(PseudoId pseudo_id) const override {
    if (!DocumentTransitionPseudoElementBase::CanGeneratePseudoElement(
            pseudo_id)) {
      return false;
    }
    viz::SharedElementResourceId snapshot_id;
    if (pseudo_id == kPseudoIdPageTransitionOutgoingImage) {
      if (style_tracker_->old_root_data_ &&
          style_tracker_->old_root_data_->tags.Contains(
              document_transition_tag())) {
        snapshot_id = style_tracker_->old_root_data_->snapshot_id;
        DCHECK(snapshot_id.IsValid());
      } else if (auto it = style_tracker_->element_data_map_.find(
                     document_transition_tag());
                 it != style_tracker_->element_data_map_.end()) {
        snapshot_id = it->value->old_snapshot_id;
      } else {
        // If we're being called with a tag that isn't an old_root tag and it's
        // not an element shared element, it must mean we have it as a new root
        // tag.
        DCHECK(style_tracker_->new_root_data_);
        DCHECK(style_tracker_->new_root_data_->tags.Contains(
            document_transition_tag()));
      }
    } else {
      if (style_tracker_->new_root_data_ &&
          style_tracker_->new_root_data_->tags.Contains(
              document_transition_tag())) {
        snapshot_id = style_tracker_->new_root_data_->snapshot_id;
        DCHECK(snapshot_id.IsValid());
      } else if (auto it = style_tracker_->element_data_map_.find(
                     document_transition_tag());
                 it != style_tracker_->element_data_map_.end()) {
        snapshot_id = it->value->new_snapshot_id;
      } else {
        // If we're being called with a tag that isn't a new_root tag and it's
        // not an element shared element, it must mean we have it as an old root
        // tag.
        DCHECK(style_tracker_->old_root_data_);
        DCHECK(style_tracker_->old_root_data_->tags.Contains(
            document_transition_tag()));
      }
    }
    return snapshot_id.IsValid();
  }
};

DocumentTransitionStyleTracker::DocumentTransitionStyleTracker(
    Document& document)
    : document_(document) {}

DocumentTransitionStyleTracker::~DocumentTransitionStyleTracker() = default;

void DocumentTransitionStyleTracker::AddConsoleError(
    String message,
    Vector<DOMNodeId> related_nodes) {
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kRendering,
      mojom::blink::ConsoleMessageLevel::kError, std::move(message));
  console_message->SetNodes(document_->GetFrame(), std::move(related_nodes));
  document_->AddConsoleMessage(console_message);
}

void DocumentTransitionStyleTracker::AddSharedElement(Element* element,
                                                      const AtomicString& tag) {
  DCHECK(element);
  if (state_ == State::kCapturing || state_ == State::kStarted) {
    AddConsoleError(kElementSetModificationError,
                    {DOMNodeIds::IdForNode(element)});
    return;
  }

  // Insert an empty hash set for the element if it doesn't exist, or get it if
  // it does.
  auto& value = pending_shared_element_tags_
                    .insert(element, HashSet<std::pair<AtomicString, int>>())
                    .stored_value->value;
  // Find the existing tag if one is there.
  auto it = std::find_if(
      value.begin(), value.end(),
      [&tag](const std::pair<AtomicString, int>& p) { return p.first == tag; });
  // If it is there, do nothing.
  if (it != value.end())
    return;
  // Otherwise, insert a new sequence id with this tag. We'll use the sequence
  // to sort later.
  value.insert(std::make_pair(tag, set_element_sequence_id_));
  ++set_element_sequence_id_;
}

void DocumentTransitionStyleTracker::RemoveSharedElement(Element* element) {
  if (state_ == State::kCapturing || state_ == State::kStarted) {
    AddConsoleError(kElementSetModificationError,
                    {DOMNodeIds::IdForNode(element)});
    return;
  }

  pending_shared_element_tags_.erase(element);
}

void DocumentTransitionStyleTracker::AddSharedElementsFromCSS() {
  DCHECK(document_ && document_->View());

  // TODO(vmpstr): This needs some thought :(
  // From khushalsagar:
  // We have to change this such that discovering of tags happens at the end of
  // reaching the paint phase of the lifecycle update at the next frame. So the
  // way this would be setup is:
  // - At the next frame, acquire the scope before dispatching raf callbacks.
  // - When we hit paint, discover all the tags and then release the scope.
  // We can have recursive lifecycle updates after this to invalidate the pseudo
  // DOM but the decision for which elements will be shared is not changeable
  // after that point.
  auto scope =
      document_->GetDisplayLockDocumentState().GetScopedForceActivatableLocks();

  // We need our paint layers, and z-order lists which is done during
  // compositing inputs update.
  document_->View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kDocumentTransition);

  AddSharedElementsFromCSSRecursive(
      document_->GetLayoutView()->PaintingLayer());
}

void DocumentTransitionStyleTracker::AddSharedElementsFromCSSRecursive(
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
  if (root_style.PageTransitionTag()) {
    DCHECK(root_object.GetNode());
    DCHECK(root_object.GetNode()->IsElementNode());
    AddSharedElement(DynamicTo<Element>(root_object.GetNode()),
                     root_style.PageTransitionTag());
  }

  PaintLayerPaintOrderIterator child_iterator(root, kAllChildren);
  while (auto* child = child_iterator.Next()) {
    AddSharedElementsFromCSSRecursive(child);
  }
}

bool DocumentTransitionStyleTracker::FlattenAndVerifyElements(
    VectorOf<Element>& elements,
    VectorOf<AtomicString>& transition_tags,
    absl::optional<RootData>& root_data) {
  // We need to flatten the data first, and sort it by ordering which reflects
  // the setElement ordering.
  struct FlatData : public GarbageCollected<FlatData> {
    FlatData(Element* element, const AtomicString& tag, int ordering)
        : element(element), tag(tag), ordering(ordering) {}
    Member<Element> element;
    AtomicString tag;
    int ordering;

    void Trace(Visitor* visitor) const { visitor->Trace(element); }
  };
  VectorOf<FlatData> flat_list;

  // Flatten it.
  for (auto& [element, tags] : pending_shared_element_tags_) {
    bool is_root = element->IsDocumentElement();
    if (is_root && !root_data)
      root_data.emplace();

    for (auto& tag_pair : tags) {
      if (is_root) {
        // The order of the root tags doesn't matter, so we don't keep the
        // ordering.
        root_data->tags.push_back(tag_pair.first);
      } else {
        flat_list.push_back(MakeGarbageCollected<FlatData>(
            element, tag_pair.first, tag_pair.second));
      }
    }
  }

  // Sort it.
  std::sort(flat_list.begin(), flat_list.end(),
            [](const FlatData* a, const FlatData* b) {
              return a->ordering < b->ordering;
            });
  DCHECK(!root_data || !root_data->tags.IsEmpty());

  auto have_root_tag = [&root_data](const AtomicString& tag) {
    return root_data && root_data->tags.Contains(tag);
  };

  // Verify it.
  for (auto& flat_data : flat_list) {
    auto& tag = flat_data->tag;
    auto& element = flat_data->element;

    if (UNLIKELY(transition_tags.Contains(tag) || have_root_tag(tag))) {
      StringBuilder message;
      message.Append(kDuplicateTagBaseError);
      message.Append(tag);
      AddConsoleError(message.ReleaseString());
      return false;
    }
    transition_tags.push_back(tag);
    elements.push_back(element);
  }
  return true;
}

bool DocumentTransitionStyleTracker::Capture() {
  DCHECK_EQ(state_, State::kIdle);

  // Flatten `pending_shared_element_tags_` into a vector of tags and elements.
  // This process also verifies that the tag-element combinations are valid.
  VectorOf<AtomicString> transition_tags;
  VectorOf<Element> elements;
  bool success =
      FlattenAndVerifyElements(elements, transition_tags, old_root_data_);
  if (!success)
    return false;

  // Now we know that we can start a transition. Update the state and populate
  // `element_data_map_`.
  state_ = State::kCapturing;
  InvalidateHitTestingCache();

  captured_tag_count_ = transition_tags.size() + OldRootDataTagSize();

  element_data_map_.ReserveCapacityForSize(captured_tag_count_);
  HeapHashMap<Member<Element>, viz::SharedElementResourceId>
      element_snapshot_ids;
  int next_index = OldRootDataTagSize();
  for (wtf_size_t i = 0; i < transition_tags.size(); ++i) {
    const auto& tag = transition_tags[i];
    const auto& element = elements[i];

    // Reuse any previously generated snapshot_id for this element. If there was
    // none yet, then generate the resource id.
    auto& snapshot_id =
        element_snapshot_ids.insert(element, viz::SharedElementResourceId())
            .stored_value->value;
    if (!snapshot_id.IsValid()) {
      snapshot_id = viz::SharedElementResourceId::Generate();
      capture_resource_ids_.push_back(snapshot_id);
    }

    auto* element_data = MakeGarbageCollected<ElementData>();
    element_data->target_element = element;
    element_data->element_index = next_index++;
    element_data->old_snapshot_id = snapshot_id;
    element_data_map_.insert(tag, std::move(element_data));
  }

  if (old_root_data_) {
    old_root_data_->snapshot_id = viz::SharedElementResourceId::Generate();
  }
  for (const auto& root_tag : AllRootTags())
    transition_tags.push_front(root_tag);

  // This informs the style engine the set of tags we have, which will be used
  // to create the pseudo element tree.
  document_->GetStyleEngine().SetDocumentTransitionTags(transition_tags);

  // We need a style invalidation to generate the pseudo element tree.
  InvalidateStyle();

  set_element_sequence_id_ = 0;
  pending_shared_element_tags_.clear();

  return true;
}

void DocumentTransitionStyleTracker::CaptureResolved() {
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
    element_data->cached_border_box_size_in_css_space =
        element_data->border_box_size_in_css_space;
    element_data->cached_viewport_matrix = element_data->viewport_matrix;
    element_data->cached_visual_overflow_rect_in_layout_space =
        element_data->visual_overflow_rect_in_layout_space;
    element_data->effect_node = nullptr;
  }
  root_effect_node_ = nullptr;
}

VectorOf<Element> DocumentTransitionStyleTracker::GetTransitioningElements()
    const {
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

bool DocumentTransitionStyleTracker::Start() {
  DCHECK_EQ(state_, State::kCaptured);

  // Flatten `pending_shared_element_tags_` into a vector of tags and elements.
  // This process also verifies that the tag-element combinations are valid.
  VectorOf<AtomicString> transition_tags;
  VectorOf<Element> elements;
  bool success =
      FlattenAndVerifyElements(elements, transition_tags, new_root_data_);
  if (!success)
    return false;

  state_ = State::kStarted;
  InvalidateHitTestingCache();

  HeapHashMap<Member<Element>, viz::SharedElementResourceId>
      element_snapshot_ids;
  bool found_new_tags = false;
  int next_index =
      element_data_map_.size() + OldRootDataTagSize() + NewRootDataTagSize();
  for (wtf_size_t i = 0; i < elements.size(); ++i) {
    const auto& tag = transition_tags[i];
    const auto& element = elements[i];

    // Insert a new tag data if there is no data for this tag yet.
    if (!element_data_map_.Contains(tag)) {
      found_new_tags = true;
      auto* data = MakeGarbageCollected<ElementData>();
      data->element_index = next_index++;
      element_data_map_.insert(tag, data);
    }

    // Reuse any previously generated snapshot_id for this element. If there was
    // none yet, then generate the resource id.
    auto& snapshot_id =
        element_snapshot_ids.insert(element, viz::SharedElementResourceId())
            .stored_value->value;
    if (!snapshot_id.IsValid())
      snapshot_id = viz::SharedElementResourceId::Generate();

    auto& element_data = element_data_map_.find(tag)->value;
    element_data->target_element = element;
    element_data->new_snapshot_id = snapshot_id;
    DCHECK_LT(element_data->element_index, next_index);
  }

  // If the old and new root tags have different size that means we likely have
  // at least one new tag.
  found_new_tags |= OldRootDataTagSize() != NewRootDataTagSize();
  if (!found_new_tags && new_root_data_) {
    DCHECK(old_root_data_);
    for (const auto& new_tag : new_root_data_->tags) {
      // If the new root tag is not also an old root tag and it isn't a shared
      // element tag, then we have a new tag.
      if (!old_root_data_->tags.Contains(new_tag) &&
          !element_data_map_.Contains(new_tag)) {
        found_new_tags = true;
        break;
      }
    }
  }

  if (new_root_data_)
    new_root_data_->snapshot_id = viz::SharedElementResourceId::Generate();

  if (found_new_tags) {
    VectorOf<std::pair<AtomicString, int>> new_tag_pairs;
    int next_tag_index = 0;
    for (const auto& root_tag : AllRootTags())
      new_tag_pairs.push_back(std::make_pair(root_tag, ++next_tag_index));
    for (auto& [tag, data] : element_data_map_)
      new_tag_pairs.push_back(std::make_pair(tag, data->element_index));

    std::sort(new_tag_pairs.begin(), new_tag_pairs.end(),
              [](const std::pair<AtomicString, int>& left,
                 const std::pair<AtomicString, int>& right) {
                return left.second < right.second;
              });

    VectorOf<AtomicString> new_tags;
    for (auto& [tag, index] : new_tag_pairs)
      new_tags.push_back(tag);

    document_->GetStyleEngine().SetDocumentTransitionTags(new_tags);
  }

  // We need a style invalidation to generate new content pseudo elements for
  // new elements in the DOM.
  InvalidateStyle();

  if (auto* page = document_->GetPage())
    page->Animator().SetHasSharedElementTransition(true);
  return true;
}

void DocumentTransitionStyleTracker::StartFinished() {
  DCHECK_EQ(state_, State::kStarted);
  EndTransition();
}

void DocumentTransitionStyleTracker::Abort() {
  EndTransition();
}

void DocumentTransitionStyleTracker::EndTransition() {
  state_ = State::kFinished;
  InvalidateHitTestingCache();

  // We need a style invalidation to remove the pseudo element tree. This needs
  // to be done before we clear the data, since we need to invalidate the shared
  // elements stored in `element_data_map_`.
  InvalidateStyle();

  element_data_map_.clear();
  pending_shared_element_tags_.clear();
  set_element_sequence_id_ = 0;
  old_root_data_.reset();
  new_root_data_.reset();
  document_->GetStyleEngine().SetDocumentTransitionTags({});
  if (auto* page = document_->GetPage())
    page->Animator().SetHasSharedElementTransition(false);
}

void DocumentTransitionStyleTracker::UpdateElementIndicesAndSnapshotId(
    Element* element,
    DocumentTransitionSharedElementId& index,
    viz::SharedElementResourceId& resource_id) const {
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

auto DocumentTransitionStyleTracker::GetCurrentRootData() const
    -> absl::optional<RootData> {
  return HasLiveNewContent() ? new_root_data_ : old_root_data_;
}

void DocumentTransitionStyleTracker::UpdateRootIndexAndSnapshotId(
    DocumentTransitionSharedElementId& index,
    viz::SharedElementResourceId& resource_id) const {
  if (!IsRootTransitioning())
    return;

  index.AddIndex(0);
  const auto& root_data = GetCurrentRootData();
  DCHECK(root_data);
  resource_id = root_data->snapshot_id;
  DCHECK(resource_id.IsValid());
}

PseudoElement* DocumentTransitionStyleTracker::CreatePseudoElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag) {
  DCHECK(IsTransitionPseudoElement(pseudo_id));
  DCHECK(pseudo_id == kPseudoIdPageTransition || document_transition_tag);

  switch (pseudo_id) {
    case kPseudoIdPageTransition:
    case kPseudoIdPageTransitionContainer:
      return MakeGarbageCollected<DocumentTransitionPseudoElementBase>(
          parent, pseudo_id, document_transition_tag, this);
    case kPseudoIdPageTransitionImageWrapper:
      return MakeGarbageCollected<ImageWrapperPseudoElement>(
          parent, pseudo_id, document_transition_tag, this);
    case kPseudoIdPageTransitionOutgoingImage: {
      LayoutSize size;
      viz::SharedElementResourceId snapshot_id;
      if (old_root_data_ && old_root_data_->tags.Contains(document_transition_tag)) {
        // Always use the the current layout view's size.
        // TODO(vmpstr): We might want to consider caching the size when we
        // capture it, in case the layout view sizes change.
        size = LayoutSize(
            document_->GetLayoutView()->GetLayoutSize(kIncludeScrollbars));
        snapshot_id = old_root_data_->snapshot_id;
      } else {
        DCHECK(document_transition_tag);
        const auto& element_data = element_data_map_.find(document_transition_tag)->value;
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
      auto* pseudo_element =
          MakeGarbageCollected<DocumentTransitionContentElement>(
              parent, pseudo_id, document_transition_tag, snapshot_id,
              /*is_live_content_element=*/false, this);
      pseudo_element->SetIntrinsicSize(size);
      return pseudo_element;
    }
    case kPseudoIdPageTransitionIncomingImage: {
      LayoutSize size;
      viz::SharedElementResourceId snapshot_id;
      if (new_root_data_ && new_root_data_->tags.Contains(document_transition_tag)) {
        size = LayoutSize(
            document_->GetLayoutView()->GetLayoutSize(kIncludeScrollbars));
        snapshot_id = new_root_data_->snapshot_id;
      } else {
        DCHECK(document_transition_tag);
        const auto& element_data = element_data_map_.find(document_transition_tag)->value;
        bool use_cached_data = false;
        size = element_data->GetIntrinsicSize(use_cached_data);
        snapshot_id = element_data->new_snapshot_id;
      }
      auto* pseudo_element =
          MakeGarbageCollected<DocumentTransitionContentElement>(
              parent, pseudo_id, document_transition_tag, snapshot_id,
              /*is_live_content_element=*/true, this);
      pseudo_element->SetIntrinsicSize(size);
      return pseudo_element;
    }
    default:
      NOTREACHED();
  }

  return nullptr;
}

void DocumentTransitionStyleTracker::RunPostPrePaintSteps() {
  bool needs_style_invalidation = false;

  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (!element_data->target_element)
      continue;

    // TODO(khushalsagar) : Switch paint containment and disallow fragmentation
    // to implicit constraints. See crbug.com/1277121.
    auto* layout_object = element_data->target_element->GetLayoutObject();
    if (!layout_object || (!layout_object->ShouldApplyPaintContainment() &&
                           !layout_object->ShouldApplyLayoutContainment())) {
      element_data->target_element = nullptr;

      // If we had a valid |target_element| there must be an associated snapshot
      // ID. Remove it since there is no corresponding DOM element to produce
      // its snapshot.
      auto& live_snapshot_id = HasLiveNewContent()
                                   ? element_data->new_snapshot_id
                                   : element_data->old_snapshot_id;
      DCHECK(live_snapshot_id.IsValid());
      live_snapshot_id = viz::SharedElementResourceId();
      continue;
    }

    // Use the document element's effective zoom, since that's what the parent
    // effective zoom would be.
    const float device_pixel_ratio = document_->documentElement()
                                         ->GetLayoutObject()
                                         ->StyleRef()
                                         .EffectiveZoom();
    TransformationMatrix viewport_matrix =
        layout_object->LocalToAbsoluteTransform();
    viewport_matrix.Zoom(1.0 / device_pixel_ratio);

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
        std::abs(effective_zoom - device_pixel_ratio) >=
        std::numeric_limits<float>::epsilon()) {
      border_box_size_in_css_space.Scale(effective_zoom / device_pixel_ratio);
    }

    PhysicalRect visual_overflow_rect_in_layout_space;
    if (auto* box = DynamicTo<LayoutBox>(layout_object))
      visual_overflow_rect_in_layout_space = ComputeVisualOverflowRect(box);

    WritingMode writing_mode = layout_object->StyleRef().GetWritingMode();

    if (viewport_matrix == element_data->viewport_matrix &&
        border_box_size_in_css_space ==
            element_data->border_box_size_in_css_space &&
        visual_overflow_rect_in_layout_space ==
            element_data->visual_overflow_rect_in_layout_space &&
        writing_mode == element_data->container_writing_mode) {
      continue;
    }

    element_data->viewport_matrix = viewport_matrix;
    element_data->border_box_size_in_css_space = border_box_size_in_css_space;
    element_data->visual_overflow_rect_in_layout_space =
        visual_overflow_rect_in_layout_space;
    element_data->container_writing_mode = writing_mode;

    PseudoId live_content_element = HasLiveNewContent()
                                        ? kPseudoIdPageTransitionIncomingImage
                                        : kPseudoIdPageTransitionOutgoingImage;
    if (auto* pseudo_element =
            document_->documentElement()->GetNestedPseudoElement(
                live_content_element, entry.key)) {
      // A pseudo element of type |tansition*content| must be created using
      // DocumentTransitionContentElement.
      bool use_cached_data = false;
      LayoutSize size = element_data->GetIntrinsicSize(use_cached_data);
      static_cast<DocumentTransitionContentElement*>(pseudo_element)
          ->SetIntrinsicSize(size);
    }

    needs_style_invalidation = true;
  }

  if (needs_style_invalidation)
    InvalidateStyle();
}

bool DocumentTransitionStyleTracker::HasActiveAnimations() const {
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
  DocumentTransitionUtils::ForEachTransitionPseudo(*document_,
                                                   accumulate_pseudo);
  return has_animations;
}

PaintPropertyChangeType DocumentTransitionStyleTracker::UpdateEffect(
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

PaintPropertyChangeType DocumentTransitionStyleTracker::UpdateRootEffect(
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

EffectPaintPropertyNode* DocumentTransitionStyleTracker::GetEffect(
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

EffectPaintPropertyNode* DocumentTransitionStyleTracker::GetRootEffect() const {
  DCHECK(root_effect_node_);
  return root_effect_node_.get();
}

void DocumentTransitionStyleTracker::VerifySharedElements() {
  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    if (!element_data->target_element)
      continue;
    auto& active_element = element_data->target_element;

    auto* object = active_element->GetLayoutObject();

    // TODO(vmpstr): Should this work for replaced elements as well?
    if (object) {
      if (object->ShouldApplyPaintContainment() ||
          object->ShouldApplyLayoutContainment()) {
        continue;
      }

      AddConsoleError(kContainmentNotSatisfied,
                      {DOMNodeIds::IdForNode(active_element)});
    }

    // Clear the shared element. Note that we don't remove the element from the
    // vector, since we need to preserve the order of the elements and we
    // support nulls as a valid active element.

    // Invalidate the element since we should no longer be compositing it.
    // TODO(vmpstr): Should we abort the transition instead?0
    auto* box = active_element->GetLayoutBox();
    if (box && box->HasSelfPaintingLayer())
      box->SetNeedsPaintPropertyUpdate();
    active_element = nullptr;
  }
}

bool DocumentTransitionStyleTracker::IsSharedElement(Element* element) const {
  // In stable states, we don't have shared elements.
  if (state_ == State::kIdle || state_ == State::kCaptured)
    return false;

  for (auto& entry : element_data_map_) {
    if (entry.value->target_element == element)
      return true;
  }
  return false;
}

bool DocumentTransitionStyleTracker::IsRootTransitioning() const {
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

StyleRequest::RulesToInclude
DocumentTransitionStyleTracker::StyleRulesToInclude() const {
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

void DocumentTransitionStyleTracker::InvalidateStyle() {
  ua_style_sheet_.reset();
  document_->GetStyleEngine().InvalidateUADocumentTransitionStyle();

  auto* originating_element = document_->documentElement();
  originating_element->SetNeedsStyleRecalc(
      kLocalStyleChange, StyleChangeReasonForTracing::Create(
                             style_change_reason::kDocumentTransition));

  auto invalidate_style = [](PseudoElement* pseudo_element) {
    pseudo_element->SetNeedsStyleRecalc(
        kLocalStyleChange, StyleChangeReasonForTracing::Create(
                               style_change_reason::kDocumentTransition));
  };
  DocumentTransitionUtils::ForEachTransitionPseudo(*document_,
                                                   invalidate_style);

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

HashSet<AtomicString> DocumentTransitionStyleTracker::AllRootTags() const {
  HashSet<AtomicString> all_root_tags;
  if (old_root_data_) {
    for (auto& tag : old_root_data_->tags)
      all_root_tags.insert(tag);
  }
  if (new_root_data_) {
    for (auto& tag : new_root_data_->tags)
      all_root_tags.insert(tag);
  }
  return all_root_tags;
}

const String& DocumentTransitionStyleTracker::UAStyleSheet() {
  if (ua_style_sheet_)
    return *ua_style_sheet_;

  // Animations are added in the start phase of the transition.
  // Note that the cached ua_style_sheet_ above is invalidated when |state_|
  // moves to kStarted stage to generate a new stylesheet including styles for
  // animations.
  const bool add_animations = state_ == State::kStarted;

  StringBuilder builder;
  builder.Append(StaticUAStyles());
  if (add_animations)
    builder.Append(AnimationUAStyles());

  auto append_selector = [&builder](const String& name, const String& tag) {
    builder.Append(name);
    builder.Append("(");
    builder.Append(tag);
    builder.Append(")");
  };

  auto add_plus_lighter = [&builder, &append_selector](const String& tag) {
    append_selector("html::page-transition-image-wrapper", tag);
    builder.Append("{ isolation: isolate; }");

    append_selector("html::page-transition-incoming-image", tag);
    builder.Append("{ mix-blend-mode: plus-lighter; }");

    append_selector("html::page-transition-outgoing-image", tag);
    builder.Append("{ mix-blend-mode: plus-lighter; }");
  };

  auto add_animation = [&builder, &append_selector, &add_plus_lighter](
                           const String& tag,
                           const TransformationMatrix& source_matrix,
                           const LayoutSize& source_size) {
    builder.Append("@keyframes -ua-page-transition-container-anim-");
    builder.Append(tag);
    builder.AppendFormat(
        R"CSS({
          from {
           transform: %s;
           width: %.3fpx;
           height: %.3fpx;
          }
        })CSS",
        ComputedStyleUtils::ValueForTransformationMatrix(source_matrix, 1,
                                                         false)
            ->CssText()
            .Utf8()
            .c_str(),
        source_size.Width().ToFloat(), source_size.Height().ToFloat());

    append_selector("html::page-transition-container", tag);
    builder.Append("{ animation: -ua-page-transition-container-anim-");
    builder.Append(tag);
    builder.Append(" 0.25s both }");

    add_plus_lighter(tag);
  };

  // SUBTLETY AHEAD!
  // There are several situations to consider when creating the styles and
  // animation styles below:
  //
  // 1. A tag is both an old and new root. We will only visit the AllRootTags
  // loop and correctly append styles (modulo TODO in that loop). Note that this
  // tag will not be in the `element_data_map_` (DCHECKed in that loop).
  //
  // 2. A tag is an old root only (exit animation for root). The style is set up
  // in the AllrootTags loop and fades out through AnimationUAStyles.
  //
  // 3. A tag is an old root and a new shared element. The AllRootTags loop
  // skips this tag. The element map loop updates the container for the new
  // shared element size and transform. The animation code of that loop adds an
  // animation from old root size and identity matrix.
  //
  // 4. A tag is a new root only (entry animation for root). Its only visited in
  // AllRootTags and its a default fade-in.
  //
  // 5. A tag is a new root and old shared element. We visit it in AllRootTags
  // to set up the destination state. We skip setting its styles in the
  // `element_data_map_` loop since latest value comes from AllRootTags. We do
  // set the animation in that loop since we need the "from" state.
  //
  // 6. A tag is a new and old shared element (or maybe exit/enter for shared
  // element only -- no roots involved. Everything is done in the
  // `element_data_map_` loop.

  for (auto& root_tag : AllRootTags()) {
    // This is case 3 above.
    bool tag_is_old_root =
        old_root_data_ && old_root_data_->tags.Contains(root_tag);
    if (tag_is_old_root && element_data_map_.Contains(root_tag)) {
      DCHECK(
          element_data_map_.find(root_tag)->value->new_snapshot_id.IsValid());
      continue;
    }

    // TODO(vmpstr): For animations, we need to re-target the layout size if it
    // changes, but right now we only use the latest layout view size.
    // Note that we don't set the writing-mode since it would inherit from the
    // :root anyway, so there is no reason to put it on the pseudo elements.
    append_selector("html::page-transition-container", root_tag);
    builder.Append(
        R"CSS({
          right: 0;
          bottom: 0;
        })CSS");

    bool tag_is_new_root =
        new_root_data_ && new_root_data_->tags.Contains(root_tag);
    if (tag_is_old_root && tag_is_new_root)
      add_plus_lighter(root_tag);
  }

  // Use the document element's effective zoom, since that's what the parent
  // effective zoom would be.
  float device_pixel_ratio = document_->documentElement()
                                 ->GetLayoutObject()
                                 ->StyleRef()
                                 .EffectiveZoom();
  for (auto& entry : element_data_map_) {
    const auto& document_transition_tag = entry.key.GetString();
    auto& element_data = entry.value;

    const bool tag_is_old_root =
        old_root_data_ &&
        old_root_data_->tags.Contains(document_transition_tag);
    const bool tag_is_new_root =
        new_root_data_ &&
        new_root_data_->tags.Contains(document_transition_tag);
    // The tag can't be both old and new root, since it shouldn't be in the
    // `element_data_map_`. This is case 1 above.
    DCHECK(!tag_is_old_root || !tag_is_new_root);

    std::ostringstream writing_mode_stream;
    writing_mode_stream << element_data->container_writing_mode;

    // Skipping this if a tag is a new root. This is case 5 above.
    if (!tag_is_new_root) {
      // ::page-transition-container styles using computed properties for each
      // element.
      append_selector("html::page-transition-container",
                      document_transition_tag);
      builder.AppendFormat(
          R"CSS({
            width: %.3fpx;
            height: %.3fpx;
            transform: %s;
            writing-mode: %s;
          })CSS",
          element_data->border_box_size_in_css_space.Width().ToFloat(),
          element_data->border_box_size_in_css_space.Height().ToFloat(),
          ComputedStyleUtils::ValueForTransformationMatrix(
              element_data->viewport_matrix, 1, false)
              ->CssText()
              .Utf8()
              .c_str(),
          writing_mode_stream.str().c_str());

      // Incoming inset also only makes sense if the tag is a new shared element
      // (not a new root).
      absl::optional<String> incoming_inset = ComputeInsetDifference(
          element_data->visual_overflow_rect_in_layout_space,
          LayoutRect(LayoutPoint(), element_data->border_box_size_in_css_space),
          device_pixel_ratio);
      if (incoming_inset) {
        append_selector("html::page-transition-incoming-image",
                        document_transition_tag);
        builder.AppendFormat(
            R"CSS({
              object-view-box: %s;
            })CSS",
            incoming_inset->Utf8().c_str());
      }
    }

    // Outgoing inset only makes sense if the tag is an old shared element (not
    // an old root).
    if (!tag_is_old_root) {
      absl::optional<String> outgoing_inset = ComputeInsetDifference(
          element_data->cached_visual_overflow_rect_in_layout_space,
          LayoutRect(LayoutPoint(),
                     element_data->cached_border_box_size_in_css_space),
          device_pixel_ratio);
      if (outgoing_inset) {
        append_selector("html::page-transition-outgoing-image",
                        document_transition_tag);
        builder.AppendFormat(
            R"CSS({
              object-view-box: %s;
            })CSS",
            outgoing_inset->Utf8().c_str());
      }
    }

    // TODO(khushalsagar) : We'll need to retarget the animation if the final
    // value changes during the start phase.
    if (add_animations) {
      // If the old snapshot is valid, then we add a transition if we have
      // either the new snapshot (case 6 above) or the tag is a new root (case 5
      // above).
      //
      // The else-if case is case 3 above: if we have the new snapshot and the
      // tag is an old root, in which case we also add an animation but sourced
      // from the old root, rather than from the cached element data.
      if (element_data->old_snapshot_id.IsValid() &&
          (element_data->new_snapshot_id.IsValid() || tag_is_new_root)) {
        add_animation(document_transition_tag,
                      element_data->cached_viewport_matrix,
                      element_data->cached_border_box_size_in_css_space);
      } else if (element_data->new_snapshot_id.IsValid() && tag_is_old_root) {
        // TODO(vmpstr): Update the size to be the cached one, here and when
        // constructing outgoing pseudos.
        auto layout_view_size = LayoutSize(
            document_->GetLayoutView()->GetLayoutSize(kIncludeScrollbars));
        // Note that we want the size in css space, which means we need to undo
        // the effective zoom.
        layout_view_size.Scale(
            1 / document_->GetLayoutView()->StyleRef().EffectiveZoom());
        add_animation(document_transition_tag, TransformationMatrix(),
                      layout_view_size);
      }
    }
  }

  ua_style_sheet_ = builder.ReleaseString();
  return *ua_style_sheet_;
}

bool DocumentTransitionStyleTracker::HasLiveNewContent() const {
  return state_ == State::kStarted;
}

void DocumentTransitionStyleTracker::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(element_data_map_);
  visitor->Trace(pending_shared_element_tags_);
}

void DocumentTransitionStyleTracker::InvalidateHitTestingCache() {
  // Hit-testing data is cached based on the current DOM version. Normally, this
  // version is incremented any time there is a DOM modification or an attribute
  // change to some element (which can result in a new style). However, with
  // shared element transitions, we dynamically create and destroy hit-testable
  // pseudo elements based on the current state. This means that we have to
  // manually modify the DOM tree version since there is no other mechanism that
  // will do it.
  document_->IncDOMTreeVersion();
}

void DocumentTransitionStyleTracker::ElementData::Trace(
    Visitor* visitor) const {
  visitor->Trace(target_element);
}

// TODO(vmpstr): We need to write tests for the following:
// * A local transform on the shared element.
// * A transform on an ancestor which changes its screen space transform.
LayoutSize DocumentTransitionStyleTracker::ElementData::GetIntrinsicSize(
    bool use_cached_data) {
  LayoutSize box_size =
      use_cached_data
          ? cached_visual_overflow_rect_in_layout_space.size.ToLayoutSize()
          : visual_overflow_rect_in_layout_space.size.ToLayoutSize();
  return box_size;
}

}  // namespace blink
