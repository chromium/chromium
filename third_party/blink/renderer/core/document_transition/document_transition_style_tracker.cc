// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_style_tracker.h"

#include "components/viz/common/shared_element_resource_id.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_content_element.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_utils.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
namespace {

const char* kElementSetModificationError =
    "The element set cannot be modified at this transition state.";
const char* kPaintContainmentNotSatisfied =
    "Dropping element from transition. Shared element must contain paint";
const char* kDuplicateTagBaseError =
    "Unexpected duplicate page transition tag: ";
const char* kReservedTagBaseError = "Unexpected reserved page transition tag: ";

const AtomicString& RootTag() {
  DEFINE_STATIC_LOCAL(AtomicString, kRootTag, ("root"));
  return kRootTag;
}

constexpr int root_index = 0;

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
                                              const gfx::Rect& target_rect,
                                              float device_pixel_ratio) {
  if (reference_rect.IsEmpty()) {
    DCHECK(target_rect.IsEmpty());
    return absl::nullopt;
  }

  // Reference rect is given to us in layout space, but target_rect is in css
  // space. Note that this currently relies on the fact that object-view-box
  // scales its parameters from CSS to layout space. However, that's a bug.
  // TODO(crbug.com/1324618): Fix this when the object-view-box bug is fixed.
  gfx::Rect reference_bounding_rect = gfx::ToEnclosingRect(gfx::ScaleRect(
      static_cast<gfx::RectF>(reference_rect), 1.0 / device_pixel_ratio));

  if (reference_bounding_rect == target_rect)
    return absl::nullopt;

  int top_offset = target_rect.y() - reference_bounding_rect.y();
  int right_offset = reference_bounding_rect.right() - target_rect.right();
  int bottom_offset = reference_bounding_rect.bottom() - target_rect.bottom();
  int left_offset = target_rect.x() - reference_bounding_rect.x();

  DCHECK_GE(top_offset, 0);
  DCHECK_GE(right_offset, 0);
  DCHECK_GE(bottom_offset, 0);
  DCHECK_GE(left_offset, 0);

  return String::Format("inset(%dpx %dpx %dpx %dpx)", top_offset, right_offset,
                        bottom_offset, left_offset);
}

// TODO(vmpstr): This could be optimized by caching values for individual layout
// boxes. However, it's unclear when the cache should be cleared.
PhysicalRect ComputeVisualOverflowRect(LayoutBox* box) {
  PhysicalRect result;
  for (auto* child = box->Layer()->FirstChild(); child;
       child = child->NextSibling()) {
    auto* child_box = child->GetLayoutBox();
    PhysicalRect overflow_rect = ComputeVisualOverflowRect(child_box);
    child_box->MapToVisualRectInAncestorSpace(box, overflow_rect);
    result.Unite(overflow_rect);
  }
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
                                            document_transition_tag),
        style_tracker_(style_tracker) {}

  ~ImageWrapperPseudoElement() override = default;

  void Trace(Visitor* visitor) const override {
    PseudoElement::Trace(visitor);
    visitor->Trace(style_tracker_);
  }

 private:
  bool CanGeneratePseudoElement(PseudoId pseudo_id) const override {
    if (!DocumentTransitionPseudoElementBase::CanGeneratePseudoElement(
            pseudo_id)) {
      return false;
    }

    viz::SharedElementResourceId snapshot_id;
    if (document_transition_tag() == RootTag()) {
      snapshot_id = pseudo_id == kPseudoIdPageTransitionOutgoingImage
                        ? style_tracker_->old_root_snapshot_id_
                        : style_tracker_->new_root_snapshot_id_;
    } else {
      auto& element_data =
          style_tracker_->element_data_map_.find(document_transition_tag())
              ->value;
      snapshot_id = pseudo_id == kPseudoIdPageTransitionOutgoingImage
                        ? element_data->old_snapshot_id
                        : element_data->new_snapshot_id;
    }
    return snapshot_id.IsValid();
  }

  Member<const DocumentTransitionStyleTracker> style_tracker_;
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
  //
  // TODO(vmpstr): If root object is the layout view, we shouldn't append it as
  // a shared element. It's unlikely to work correctly here.
  auto& root_object = root->GetLayoutObject();
  auto& root_style = root_object.StyleRef();
  if (root_style.PageTransitionTag()) {
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
    VectorOf<AtomicString>& transition_tags) {
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
    for (auto& tag_pair : tags) {
      flat_list.push_back(MakeGarbageCollected<FlatData>(
          element, tag_pair.first, tag_pair.second));
    }
  }

  // Sort it.
  std::sort(flat_list.begin(), flat_list.end(),
            [](const FlatData* a, const FlatData* b) {
              return a->ordering < b->ordering;
            });

  // Verify it.
  for (auto& flat_data : flat_list) {
    auto& tag = flat_data->tag;
    auto& element = flat_data->element;

    if (UNLIKELY(transition_tags.Contains(tag))) {
      StringBuilder message;
      message.Append(kDuplicateTagBaseError);
      message.Append(tag);
      AddConsoleError(message.ReleaseString());
      return false;
    }
    if (UNLIKELY(tag == RootTag())) {
      StringBuilder message;
      message.Append(kReservedTagBaseError);
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
  bool success = FlattenAndVerifyElements(elements, transition_tags);
  if (!success)
    return false;

  // Now we know that we can start a transition. Update the state and populate
  // `element_data_map_`.
  state_ = State::kCapturing;
  captured_tag_count_ = transition_tags.size();

  old_root_snapshot_id_ = viz::SharedElementResourceId::Generate();
  element_data_map_.ReserveCapacityForSize(captured_tag_count_);
  HeapHashMap<Member<Element>, viz::SharedElementResourceId>
      element_snapshot_ids;
  int next_index = root_index + 1;
  for (int i = 0; i < captured_tag_count_; ++i) {
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

  // Don't forget to add the root tag!
  transition_tags.push_front(RootTag());

  // Add one to the captured tag count to account for root. We don't add it
  // earlier, since we're doing an iteration over captured tag counts from the
  // shared non-root elements.
  ++captured_tag_count_;

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
    element_data->cached_device_pixel_ratio = element_data->device_pixel_ratio;
    element_data->cached_visual_overflow_rect_in_layout_space =
        element_data->visual_overflow_rect_in_layout_space;
    element_data->effect_node = nullptr;
  }
  root_effect_node_ = nullptr;
}

bool DocumentTransitionStyleTracker::Start() {
  DCHECK_EQ(state_, State::kCaptured);

  // Flatten `pending_shared_element_tags_` into a vector of tags and elements.
  // This process also verifies that the tag-element combinations are valid.
  VectorOf<AtomicString> transition_tags;
  VectorOf<Element> elements;
  bool success = FlattenAndVerifyElements(elements, transition_tags);
  if (!success)
    return false;

  state_ = State::kStarted;
  new_root_snapshot_id_ = viz::SharedElementResourceId::Generate();
  HeapHashMap<Member<Element>, viz::SharedElementResourceId>
      element_snapshot_ids;
  bool found_new_tags = false;
  int next_index = root_index + element_data_map_.size() + 1;
  for (wtf_size_t i = 0; i < elements.size(); ++i) {
    const auto& tag = transition_tags[i];
    const auto& element = elements[i];

    // Insert a new tag data if there is no data for this tag yet.
    if (element_data_map_.find(tag) == element_data_map_.end()) {
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

  if (found_new_tags) {
    VectorOf<std::pair<AtomicString, int>> new_tag_pairs;
    new_tag_pairs.push_back(std::make_pair(RootTag(), root_index));
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

  // We need a style invalidation to remove the pseudo element tree. This needs
  // to be done before we clear the data, since we need to invalidate the shared
  // elements stored in `element_data_map_`.
  InvalidateStyle();

  element_data_map_.clear();
  pending_shared_element_tags_.clear();
  set_element_sequence_id_ = 0;
  document_->GetStyleEngine().SetDocumentTransitionTags({});
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

void DocumentTransitionStyleTracker::UpdateRootIndexAndSnapshotId(
    DocumentTransitionSharedElementId& index,
    viz::SharedElementResourceId& resource_id) const {
  index.AddIndex(root_index);
  resource_id =
      HasLiveNewContent() ? new_root_snapshot_id_ : old_root_snapshot_id_;
  DCHECK(resource_id.IsValid());
}

PseudoElement* DocumentTransitionStyleTracker::CreatePseudoElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag) {
  DCHECK(IsTransitionPseudoElement(pseudo_id));
  DCHECK(pseudo_id == kPseudoIdPageTransition || document_transition_tag);

  const auto& element_data =
      (document_transition_tag && document_transition_tag != RootTag())
          ? element_data_map_.find(document_transition_tag)->value
          : nullptr;

  switch (pseudo_id) {
    case kPseudoIdPageTransition:
    case kPseudoIdPageTransitionContainer:
      return MakeGarbageCollected<DocumentTransitionPseudoElementBase>(
          parent, pseudo_id, document_transition_tag);
    case kPseudoIdPageTransitionImageWrapper:
      return MakeGarbageCollected<ImageWrapperPseudoElement>(
          parent, pseudo_id, document_transition_tag, this);
    case kPseudoIdPageTransitionOutgoingImage: {
      LayoutSize size;
      viz::SharedElementResourceId snapshot_id;
      if (document_transition_tag == RootTag()) {
        // Always use the the current layout view's size.
        // TODO(vmpstr): We might want to consider caching the size when we
        // capture it, in case the layout view sizes change.
        size = LayoutSize(
            document_->GetLayoutView()->GetLayoutSize(kIncludeScrollbars));
        snapshot_id = old_root_snapshot_id_;
      } else {
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
              /*is_live_content_element=*/false);
      pseudo_element->SetIntrinsicSize(size);
      return pseudo_element;
    }
    case kPseudoIdPageTransitionIncomingImage: {
      LayoutSize size;
      viz::SharedElementResourceId snapshot_id;
      if (document_transition_tag == RootTag()) {
        size = LayoutSize(
            document_->GetLayoutView()->GetLayoutSize(kIncludeScrollbars));
        snapshot_id = new_root_snapshot_id_;
      } else {
        bool use_cached_data = false;
        size = element_data->GetIntrinsicSize(use_cached_data);
        snapshot_id = element_data->new_snapshot_id;
      }
      auto* pseudo_element =
          MakeGarbageCollected<DocumentTransitionContentElement>(
              parent, pseudo_id, document_transition_tag, snapshot_id,
              /*is_live_content_element=*/true);
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
    if (!layout_object || !layout_object->ShouldApplyPaintContainment()) {
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

    float device_pixel_ratio = document_->DevicePixelRatio();
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

    PhysicalRect visual_overflow_rect_in_layout_space;
    if (auto* box = DynamicTo<LayoutBox>(layout_object))
      visual_overflow_rect_in_layout_space = ComputeVisualOverflowRect(box);

    if (viewport_matrix == element_data->viewport_matrix &&
        border_box_size_in_css_space ==
            element_data->border_box_size_in_css_space &&
        device_pixel_ratio == element_data->device_pixel_ratio &&
        visual_overflow_rect_in_layout_space ==
            element_data->visual_overflow_rect_in_layout_space) {
      continue;
    }

    element_data->viewport_matrix = viewport_matrix;
    element_data->border_box_size_in_css_space = border_box_size_in_css_space;
    element_data->device_pixel_ratio = device_pixel_ratio;
    element_data->visual_overflow_rect_in_layout_space =
        visual_overflow_rect_in_layout_space;

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
      if (object->ShouldApplyPaintContainment())
        continue;

      AddConsoleError(kPaintContainmentNotSatisfied,
                      {DOMNodeIds::IdForNode(active_element)});
    }

    // Clear the shared element. Note that we don't remove the element from the
    // vector, since we need to preserve the order of the elements and we
    // support nulls as a valid active element.

    // Invalidate the element since we should no longer be compositing it.
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

// static
bool DocumentTransitionStyleTracker::IsReservedTransitionTag(
    const StringView& value) {
  return value == RootTag();
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

    auto* box = entry.value->target_element->GetLayoutBox();
    if (!box || !box->HasSelfPaintingLayer())
      continue;
  }
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

  for (auto& entry : element_data_map_) {
    auto document_transition_tag = entry.key.GetString().Utf8();
    auto& element_data = entry.value;

    gfx::Rect border_box_in_css_space = gfx::Rect(
        gfx::Size(element_data->border_box_size_in_css_space.Width().ToInt(),
                  element_data->border_box_size_in_css_space.Height().ToInt()));
    gfx::Rect cached_border_box_in_css_space = gfx::Rect(gfx::Size(
        element_data->cached_border_box_size_in_css_space.Width().ToInt(),
        element_data->cached_border_box_size_in_css_space.Height().ToInt()));

    // ::page-transition-container styles using computed properties for each
    // element.
    builder.AppendFormat(
        R"CSS(
        html::page-transition-container(%s) {
          width: %dpx;
          height: %dpx;
          transform: %s;
        }
        )CSS",
        document_transition_tag.c_str(), border_box_in_css_space.width(),
        border_box_in_css_space.height(),
        ComputedStyleUtils::ValueForTransformationMatrix(
            element_data->viewport_matrix, 1, false)
            ->CssText()
            .Utf8()
            .c_str());

    float device_pixel_ratio = document_->DevicePixelRatio();
    absl::optional<String> incoming_inset = ComputeInsetDifference(
        element_data->visual_overflow_rect_in_layout_space,
        border_box_in_css_space, device_pixel_ratio);
    if (incoming_inset) {
      builder.AppendFormat(
          R"CSS(
          html::page-transition-incoming-image(%s) {
            object-view-box: %s;
          }
          )CSS",
          document_transition_tag.c_str(), incoming_inset->Utf8().c_str());
    }

    absl::optional<String> outgoing_inset = ComputeInsetDifference(
        element_data->cached_visual_overflow_rect_in_layout_space,
        cached_border_box_in_css_space, device_pixel_ratio);
    if (outgoing_inset) {
      builder.AppendFormat(
          R"CSS(
          html::page-transition-outgoing-image(%s) {
            object-view-box: %s;
          }
          )CSS",
          document_transition_tag.c_str(), outgoing_inset->Utf8().c_str());
    }

    // TODO(khushalsagar) : We'll need to retarget the animation if the final
    // value changes during the start phase.
    if (add_animations && element_data->old_snapshot_id.IsValid() &&
        element_data->new_snapshot_id.IsValid()) {
      builder.AppendFormat(
          R"CSS(
          @keyframes page-transition-container-anim-%s {
            from {
             transform: %s;
             width: %dpx;
             height: %dpx;
            }
           }
           )CSS",
          document_transition_tag.c_str(),
          ComputedStyleUtils::ValueForTransformationMatrix(
              element_data->cached_viewport_matrix, 1, false)
              ->CssText()
              .Utf8()
              .c_str(),
          element_data->cached_border_box_size_in_css_space.Width().ToInt(),
          element_data->cached_border_box_size_in_css_space.Height().ToInt());

      // TODO(khushalsagar) : The duration/delay in the UA stylesheet will need
      // to be the duration from TransitionConfig. See crbug.com/1275727.
      builder.AppendFormat(
          R"CSS(
          html::page-transition-container(%s) {
            animation: page-transition-container-anim-%s 0.25s both
          }
          )CSS",
          document_transition_tag.c_str(), document_transition_tag.c_str());
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
