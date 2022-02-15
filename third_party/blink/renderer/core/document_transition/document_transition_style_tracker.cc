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
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"

namespace blink {
namespace {

const AtomicString& RootTag() {
  DEFINE_STATIC_LOCAL(AtomicString, kRootTag, ("root"));
  return kRootTag;
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

AtomicString IdFromIndex(wtf_size_t index) {
  StringBuilder builder;
  builder.AppendFormat("shared-%d", index);
  return builder.ToAtomicString();
}

}  // namespace

class DocumentTransitionStyleTracker::ContainerPseudoElement
    : public DocumentTransitionPseudoElementBase {
 public:
  ContainerPseudoElement(Element* parent,
                         PseudoId pseudo_id,
                         const AtomicString& document_transition_tag,
                         const DocumentTransitionStyleTracker* style_tracker)
      : DocumentTransitionPseudoElementBase(parent,
                                            pseudo_id,
                                            document_transition_tag),
        style_tracker_(style_tracker) {}

  ~ContainerPseudoElement() override = default;

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
      snapshot_id = pseudo_id == kPseudoIdTransitionOldContent
                        ? style_tracker_->old_root_snapshot_id_
                        : style_tracker_->new_root_snapshot_id_;
    } else {
      auto& element_data =
          style_tracker_->element_data_map_.find(document_transition_tag())
              ->value;
      snapshot_id = pseudo_id == kPseudoIdTransitionOldContent
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

void DocumentTransitionStyleTracker::Prepare(
    const HeapVector<Member<Element>>& old_elements) {
  DCHECK_EQ(state_, State::kIdle);

  state_ = State::kPreparing;

  // An id for each shared element + root.
  pseudo_document_transition_tags_.resize(old_elements.size() + 1);

  // The order of IDs in this list defines the DOM order and as a result the
  // paint order of these elements. This is why root needs to be first in the
  // list.
  pseudo_document_transition_tags_[0] = RootTag();
  old_root_snapshot_id_ = viz::SharedElementResourceId::Generate();
  element_data_map_.ReserveCapacityForSize(old_elements.size());
  for (wtf_size_t i = 0; i < old_elements.size(); ++i) {
    auto document_transition_tag = IdFromIndex(i);

    auto* element_data = MakeGarbageCollected<ElementData>();
    element_data->target_element = old_elements[i];
    if (old_elements[i])
      element_data->old_snapshot_id = viz::SharedElementResourceId::Generate();
    element_data_map_.insert(document_transition_tag, std::move(element_data));

    pseudo_document_transition_tags_[i + 1] =
        std::move(document_transition_tag);
  }

  document_->GetStyleEngine().SetDocumentTransitionTags(
      pseudo_document_transition_tags_);

  // We need a style invalidation to generate the pseudo element tree.
  InvalidateStyle();
}

void DocumentTransitionStyleTracker::PrepareResolved() {
  DCHECK_EQ(state_, State::kPreparing);

  state_ = State::kPrepared;

  for (auto& entry : element_data_map_) {
    auto& element_data = entry.value;
    element_data->target_element = nullptr;
    element_data->cached_border_box_size = element_data->border_box_size;
    element_data->cached_viewport_matrix = element_data->viewport_matrix;
  }
}

void DocumentTransitionStyleTracker::Start(
    const HeapVector<Member<Element>>& new_elements) {
  DCHECK_EQ(state_, State::kPrepared);
  DCHECK_EQ(element_data_map_.size(), new_elements.size());

  state_ = State::kStarted;
  new_root_snapshot_id_ = viz::SharedElementResourceId::Generate();
  for (wtf_size_t i = 0; i < new_elements.size(); ++i) {
    auto document_transition_tag = IdFromIndex(i);

    auto& element_data = element_data_map_.find(document_transition_tag)->value;
    element_data->target_element = new_elements[i];
    if (new_elements[i])
      element_data->new_snapshot_id = viz::SharedElementResourceId::Generate();
    element_data->effect_node = nullptr;
  }
  root_effect_node_ = nullptr;

  // We need a style invalidation to generate new content pseudo elements for
  // new elements in the DOM.
  InvalidateStyle();
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

  element_data_map_.clear();
  pseudo_document_transition_tags_.clear();
  document_->GetStyleEngine().SetDocumentTransitionTags({});

  // We need a style invalidation to remove the pseudo element tree.
  InvalidateStyle();
}

viz::SharedElementResourceId DocumentTransitionStyleTracker::GetLiveSnapshotId(
    const Element* element) const {
  DCHECK(element);

  for (const auto& entry : element_data_map_) {
    if (entry.value->target_element == element) {
      auto snapshot_id = HasLiveNewContent() ? entry.value->new_snapshot_id
                                             : entry.value->old_snapshot_id;
      DCHECK(snapshot_id.IsValid());
      return snapshot_id;
    }
  }

  NOTREACHED();
  return viz::SharedElementResourceId();
}

viz::SharedElementResourceId
DocumentTransitionStyleTracker::GetLiveRootSnapshotId() const {
  return HasLiveNewContent() ? new_root_snapshot_id_ : old_root_snapshot_id_;
}

PseudoElement* DocumentTransitionStyleTracker::CreatePseudoElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag) {
  DCHECK(IsTransitionPseudoElement(pseudo_id));
  DCHECK(pseudo_id == kPseudoIdTransition || document_transition_tag);

  const auto& element_data =
      (document_transition_tag && document_transition_tag != RootTag())
          ? element_data_map_.find(document_transition_tag)->value
          : nullptr;

  switch (pseudo_id) {
    case kPseudoIdTransition:
      return MakeGarbageCollected<DocumentTransitionPseudoElementBase>(
          parent, pseudo_id, document_transition_tag);
    case kPseudoIdTransitionContainer:
      return MakeGarbageCollected<ContainerPseudoElement>(
          parent, pseudo_id, document_transition_tag, this);
    case kPseudoIdTransitionOldContent: {
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
        // If live data is tracking new elements then use the cached size for
        // the pseudo element displaying snapshot of old element.
        size = HasLiveNewContent() ? element_data->border_box_size
                                   : element_data->cached_border_box_size;
        snapshot_id = element_data->old_snapshot_id;
      }
      auto* pseudo_element =
          MakeGarbageCollected<DocumentTransitionContentElement>(
              parent, pseudo_id, document_transition_tag, snapshot_id);
      pseudo_element->SetIntrinsicSize(size);
      return pseudo_element;
    }
    case kPseudoIdTransitionNewContent: {
      LayoutSize size;
      viz::SharedElementResourceId snapshot_id;
      if (document_transition_tag == RootTag()) {
        size = LayoutSize(
            document_->GetLayoutView()->GetLayoutSize(kIncludeScrollbars));
        snapshot_id = new_root_snapshot_id_;
      } else {
        size = element_data->border_box_size;
        snapshot_id = element_data->new_snapshot_id;
      }
      auto* pseudo_element =
          MakeGarbageCollected<DocumentTransitionContentElement>(
              parent, pseudo_id, document_transition_tag, snapshot_id);
      pseudo_element->SetIntrinsicSize(size);
      return pseudo_element;
    }
    default:
      NOTREACHED();
  }

  return nullptr;
}

void DocumentTransitionStyleTracker::RunPostLayoutSteps() {
  // TODO(khushalsagar) : This callback needs to be switched to PostPrepaint or
  // PostPaint since we need to paint the pseudo elements in the same order in
  // which they appear in the DOM. See crbug.com/1275740.

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

    const auto& viewport_matrix = layout_object->LocalToAbsoluteTransform();

    // ResizeObserverEntry is created to reuse the logic for parsing object size
    // for different types of LayoutObjects.
    auto* resize_observer_entry =
        MakeGarbageCollected<ResizeObserverEntry>(element_data->target_element);
    auto entry_size = resize_observer_entry->borderBoxSize()[0];
    LayoutSize border_box_size(LayoutUnit(entry_size->inlineSize()),
                               LayoutUnit(entry_size->blockSize()));

    if (viewport_matrix == element_data->viewport_matrix &&
        border_box_size == element_data->border_box_size) {
      continue;
    }

    element_data->viewport_matrix = viewport_matrix;
    element_data->border_box_size = border_box_size;

    PseudoId live_content_element = HasLiveNewContent()
                                        ? kPseudoIdTransitionNewContent
                                        : kPseudoIdTransitionOldContent;
    if (auto* pseudo_element =
            document_->documentElement()->GetNestedPseudoElement(
                live_content_element, entry.key)) {
      // A pseudo element of type |tansition*content| must be created using
      // DocumentTransitionContentElement.
      static_cast<DocumentTransitionContentElement*>(pseudo_element)
          ->SetIntrinsicSize(border_box_size);
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
  if (auto* layout_view = document_->GetLayoutView()) {
    layout_view->SetNeedsPaintPropertyUpdate();
    if (layout_view->HasSelfPaintingLayer())
      layout_view->Layer()->SetNeedsCompositingInputsUpdate();
  }
  for (auto& entry : element_data_map_) {
    if (!entry.value->target_element)
      continue;
    auto* object = entry.value->target_element->GetLayoutObject();
    if (!object)
      continue;
    object->SetNeedsPaintPropertyUpdate();
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

    // ::transition-container styles using computed properties for each element.
    builder.AppendFormat(
        R"CSS(
        html::transition-container(%s) {
          width: %dpx;
          height: %dpx;
          transform:%s;
        }
        )CSS",
        document_transition_tag.c_str(),
        element_data->border_box_size.Width().ToInt(),
        element_data->border_box_size.Height().ToInt(),
        ComputedStyleUtils::ValueForTransformationMatrix(
            element_data->viewport_matrix, 1, false)
            ->CssText()
            .Utf8()
            .c_str());

    // TODO(khushalsagar) : We'll need to retarget the animation if the final
    // value changes during the start phase.
    if (add_animations && element_data->old_snapshot_id.IsValid() &&
        element_data->new_snapshot_id.IsValid()) {
      builder.AppendFormat(
          R"CSS(
          @keyframes transition-container-anim-%s {
            from {
             transform:%s;
             width:%dpx;
             height:%dpx;
            }
           }
           )CSS",
          document_transition_tag.c_str(),
          ComputedStyleUtils::ValueForTransformationMatrix(
              element_data->cached_viewport_matrix, 1, false)
              ->CssText()
              .Utf8()
              .c_str(),
          element_data->cached_border_box_size.Width().ToInt(),
          element_data->cached_border_box_size.Height().ToInt());

      // TODO(khushalsagar) : The duration/delay in the UA stylesheet will need
      // to be the duration from TransitionConfig. See crbug.com/1275727.
      builder.AppendFormat(
          R"CSS(
          html::transition-container(%s) {
            animation: transition-container-anim-%s 0.25s
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
}

void DocumentTransitionStyleTracker::ElementData::Trace(
    Visitor* visitor) const {
  visitor->Trace(target_element);
}

}  // namespace blink
