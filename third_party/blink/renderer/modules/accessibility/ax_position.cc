// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_position.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// static
const AXPosition AXPosition::CreatePositionBeforeObject(
    const AXObject& child,
    const AXPositionAdjustmentBehavior adjustment_behavior) {
  if (child.IsDetached() || !child.IsIncludedInTree())
    return {};

  // If |child| is a text object, but not a text control, make behavior the same
  // as |CreateFirstPositionInObject| so that equality would hold. Text controls
  // behave differently because you should be able to set a position before the
  // text control in case you want to e.g. select it as a whole.
  if (child.IsTextObject())
    return CreateFirstPositionInObject(child, adjustment_behavior);

  const AXObject* parent = child.ParentObjectIncludedInTree();

  if (!parent || parent->IsDetached())
    return {};

  DCHECK(parent);
  AXPosition position(*parent);
  position.text_offset_or_child_index_ = child.IndexInParent();
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(position.IsValid(&failure_reason)) << failure_reason;
#endif
  return position.AsUnignoredPosition(adjustment_behavior);
}

// static
const AXPosition AXPosition::CreatePositionAfterObject(
    const AXObject& child,
    const AXPositionAdjustmentBehavior adjustment_behavior) {
  if (child.IsDetached() || !child.IsIncludedInTree())
    return {};

  // If |child| is a text object, but not a text control, make behavior the same
  // as |CreateLastPositionInObject| so that equality would hold. Text controls
  // behave differently because you should be able to set a position after the
  // text control in case you want to e.g. select it as a whole.
  if (child.IsTextObject())
    return CreateLastPositionInObject(child, adjustment_behavior);

  const AXObject* parent = child.ParentObjectIncludedInTree();

  if (!parent || parent->IsDetached())
    return {};

  DCHECK(parent);
  AXPosition position(*parent);
  position.text_offset_or_child_index_ = child.IndexInParent() + 1;
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(position.IsValid(&failure_reason)) << failure_reason;
#endif
  return position.AsUnignoredPosition(adjustment_behavior);
}

// static
const AXPosition AXPosition::CreateFirstPositionInObject(
    const AXObject& container,
    const AXPositionAdjustmentBehavior adjustment_behavior) {
  if (container.IsDetached())
    return {};

  if (container.IsTextObject() || container.IsAtomicTextField()) {
    AXPosition position(container);
    position.text_offset_or_child_index_ = 0;
#if DCHECK_IS_ON()
    String failure_reason;
    DCHECK(position.IsValid(&failure_reason)) << failure_reason;
#endif
    return position.AsUnignoredPosition(adjustment_behavior);
  }

  // If the container is not a text object, creating a position inside an
  // object that is excluded from the accessibility tree will result in an
  // invalid position, because child count is not always accurate for such
  // objects.
  const AXObject* unignored_container =
      !container.IsIncludedInTree()
          ? container.ParentObjectIncludedInTree()
          : &container;
  DCHECK(unignored_container);
  AXPosition position(*unignored_container);
  position.text_offset_or_child_index_ = 0;
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(position.IsValid(&failure_reason)) << failure_reason;
#endif
  return position.AsUnignoredPosition(adjustment_behavior);
}

// static
const AXPosition AXPosition::CreateLastPositionInObject(
    const AXObject& container,
    const AXPositionAdjustmentBehavior adjustment_behavior) {
  if (container.IsDetached())
    return {};

  if (container.IsTextObject() || container.IsAtomicTextField()) {
    AXPosition position(container);
    position.text_offset_or_child_index_ = position.MaxTextOffset();
#if DCHECK_IS_ON()
    String failure_reason;
    DCHECK(position.IsValid(&failure_reason)) << failure_reason;
#endif
    return position.AsUnignoredPosition(adjustment_behavior);
  }

  // If the container is not a text object, creating a position inside an
  // object that is excluded from the accessibility tree will result in an
  // invalid position, because child count is not always accurate for such
  // objects.
  const AXObject* unignored_container =
      !container.IsIncludedInTree()
          ? container.ParentObjectIncludedInTree()
          : &container;
  DCHECK(unignored_container);
  AXPosition position(*unignored_container);
  position.text_offset_or_child_index_ =
      unignored_container->ChildCountIncludingIgnored();
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(position.IsValid(&failure_reason)) << failure_reason;
#endif
  return position.AsUnignoredPosition(adjustment_behavior);
}

// static
const AXPosition AXPosition::CreatePositionInTextObject(
    const AXObject& container,
    const int offset,
    const TextAffinity affinity,
    const AXPositionAdjustmentBehavior adjustment_behavior) {
  if (container.IsDetached() ||
      !(container.IsTextObject() || container.IsTextField())) {
    return {};
  }

  AXPosition position(container);
  position.text_offset_or_child_index_ = offset;
  position.affinity_ = affinity;
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(position.IsValid(&failure_reason)) << failure_reason;
#endif
  return position.AsUnignoredPosition(adjustment_behavior);
}

// static
const AXPosition AXPosition::FromPosition(
    const Position& position,
    const TextAffinity affinity,
    const AXPositionAdjustmentBehavior adjustment_behavior) {
  if (position.IsNull() || position.IsOrphan())
    return {};

  const Document* document = position.GetDocument();
  // Non orphan positions always have a document.
  DCHECK(document);

  AXObjectCache* ax_object_cache = document->ExistingAXObjectCache();
  if (!ax_object_cache)
    return {};

  auto* ax_object_cache_impl = static_cast<AXObjectCacheImpl*>(ax_object_cache);
  const Position& parent_anchored_position = position.ToOffsetInAnchor();
  const Node* container_node = parent_anchored_position.AnchorNode();
  DCHECK(container_node);
  const AXObject* container = ax_object_cache_impl->Get(container_node);
  if (!container)
    return {};

  if (container_node->IsTextNode()) {
    if (!container->IsIncludedInTree()) {
      // Find the closest DOM sibling that is unignored in the accessibility
      // tree.
      switch (adjustment_behavior) {
        case AXPositionAdjustmentBehavior::kMoveRight: {
          const AXObject* next_container = FindNeighboringUnignoredObject(
              *document, *container_node, container_node->parentNode(),
              adjustment_behavior);
          if (next_container) {
            return CreatePositionBeforeObject(*next_container,
                                              adjustment_behavior);
          }

          // Do the next best thing by moving up to the unignored parent if it
          // exists.
          if (!container || !container->ParentObjectIncludedInTree())
            return {};
          return CreateLastPositionInObject(
              *container->ParentObjectIncludedInTree(), adjustment_behavior);
        }

        case AXPositionAdjustmentBehavior::kMoveLeft: {
          const AXObject* previous_container = FindNeighboringUnignoredObject(
              *document, *container_node, container_node->parentNode(),
              adjustment_behavior);
          if (previous_container) {
            return CreatePositionAfterObject(*previous_container,
                                             adjustment_behavior);
          }

          // Do the next best thing by moving up to the unignored parent if it
          // exists.
          if (!container || !container->ParentObjectIncludedInTree())
            return {};
          return CreateFirstPositionInObject(
              *container->ParentObjectIncludedInTree(), adjustment_behavior);
        }
      }
    }

    AXPosition ax_position(*container);
    // Convert from a DOM offset that may have uncompressed white space to a
    // character offset.
    //
    // Note that OffsetMapping::GetInlineFormattingContextOf will reject DOM
    // positions that it does not support, so we don't need to explicitly check
    // this before calling the method.)
    LayoutBlockFlow* formatting_context =
        OffsetMapping::GetInlineFormattingContextOf(parent_anchored_position);
    const OffsetMapping* container_offset_mapping =
        formatting_context ? InlineNode::GetOffsetMapping(formatting_context)
                           : nullptr;
    if (!container_offset_mapping) {
      // We are unable to compute the text offset in the accessibility tree that
      // corresponds to the DOM offset. We do the next best thing by returning
      // either the first or the last AX position in |container| based on the
      // |adjustment_behavior|.
      switch (adjustment_behavior) {
        case AXPositionAdjustmentBehavior::kMoveRight:
          return CreateLastPositionInObject(*container, adjustment_behavior);
        case AXPositionAdjustmentBehavior::kMoveLeft:
          return CreateFirstPositionInObject(*container, adjustment_behavior);
      }
    }

    // We can now compute the text offset that corresponds to the given DOM
    // position from the beginning of our formatting context. We also need to
    // subtract the text offset of our |container| from the beginning of the
    // same formatting context.
    int container_offset = container->TextOffsetInFormattingContext(0);
    std::optional<unsigned> content_offset =
        container_offset_mapping->GetTextContentOffset(
            parent_anchored_position);
    int text_offset = 0;
    if (content_offset.has_value()) {
      text_offset = content_offset.value() - container_offset;
      // Adjust the offset for characters that are not in the accessible text.
      // These can include zero-width breaking opportunities inserted after
      // preserved preliminary whitespace and isolate characters inserted when
      // positioning SVG text at a specific x coordinate.
      int adjustment = ax_position.GetLeadingIgnoredCharacterCount(
          container_offset_mapping, container->GetClosestNode(),
          container_offset, content_offset.value());
      text_offset -= adjustment;
    }
    DCHECK_GE(text_offset, 0);
    ax_position.text_offset_or_child_index_ = text_offset;
    ax_position.affinity_ = affinity;
#if DCHECK_IS_ON()
    String failure_reason;
    DCHECK(ax_position.IsValid(&failure_reason)) << failure_reason;
#endif
    return ax_position;
  }

  DCHECK(container_node->IsContainerNode());
  if (!container->IsIncludedInTree()) {
    container = container->ParentObjectIncludedInTree();
    if (!container)
      return {};

    // |container_node| could potentially become nullptr if the unignored
    // parent is an anonymous layout block.
    container_node = container->GetClosestNode();
  }

  AXPosition ax_position(*container);
  // |ComputeNodeAfterPosition| returns nullptr for "after children"
  // positions.
  const Node* node_after_position = position.ComputeNodeAfterPosition();
  if (!node_after_position) {
    ax_position.text_offset_or_child_index_ =
        container->ChildCountIncludingIgnored();

    } else {
      const AXObject* ax_child = ax_object_cache_impl->Get(node_after_position);
      // |ax_child| might be nullptr because not all DOM nodes can have AX
      // objects. For example, the "head" element has no corresponding AX
      // object.
      if (!ax_child || !ax_child->IsIncludedInTree()) {
        // Find the closest DOM sibling that is present and unignored in the
        // accessibility tree.
        switch (adjustment_behavior) {
          case AXPositionAdjustmentBehavior::kMoveRight: {
            const AXObject* next_child = FindNeighboringUnignoredObject(
                *document, *node_after_position,
                DynamicTo<ContainerNode>(container_node), adjustment_behavior);
            if (next_child) {
              return CreatePositionBeforeObject(*next_child,
                                                adjustment_behavior);
            }

            return CreateLastPositionInObject(*container, adjustment_behavior);
          }

          case AXPositionAdjustmentBehavior::kMoveLeft: {
            const AXObject* previous_child = FindNeighboringUnignoredObject(
                *document, *node_after_position,
                DynamicTo<ContainerNode>(container_node), adjustment_behavior);
            if (previous_child) {
              // |CreatePositionAfterObject| cannot be used here because it will
              // try to create a position before the object that comes after
              // |previous_child|, which in this case is the ignored object
              // itself.
              return CreateLastPositionInObject(*previous_child,
                                                adjustment_behavior);
            }

            return CreateFirstPositionInObject(*container, adjustment_behavior);
          }
        }
      }

      if (!container->ChildrenIncludingIgnored().Contains(ax_child)) {
        // The |ax_child| is aria-owned by another object.
        return CreatePositionBeforeObject(*ax_child, adjustment_behavior);
      }

      if (ax_child->IsTextObject()) {
        // The |ax_child| is a text object. In order that equality between
        // seemingly identical positions would hold, i.e. a "before object"
        // position before the text object and a "text position" before the
        // first character of the text object, we would need to convert to the
        // deep equivalent position.
        return CreateFirstPositionInObject(*ax_child, adjustment_behavior);
      }

      ax_position.text_offset_or_child_index_ = ax_child->IndexInParent();
    }

    return ax_position;
}

// static
const AXPosition AXPosition::FromPosition(
    const PositionWithAffinity& position_with_affinity,
    const AXPositionAdjustmentBehavior adjustment_behavior) {
  return FromPosition(position_with_affinity.GetPosition(),
                      position_with_affinity.Affinity(), adjustment_behavior);
}

AXPosition::AXPosition()
    : container_object_(nullptr),
      text_offset_or_child_index_(0),
      affinity_(TextAffinity::kDownstream) {
#if DCHECK_IS_ON()
  dom_tree_version_ = 0;
  style_version_ = 0;
#endif
}

AXPosition::AXPosition(const AXObject& container)
    : container_object_(&container),
      text_offset_or_child_index_(0),
      affinity_(TextAffinity::kDownstream) {
  const Document* document = container_object_->GetDocument();
  DCHECK(document);
#if DCHECK_IS_ON()
  dom_tree_version_ = document->DomTreeVersion();
  style_version_ = document->StyleVersion();
#endif
}

const AXObject* AXPosition::ChildAfterTreePosition() const {
  if (!IsValid() || IsTextPosition())
    return nullptr;
  if (ChildIndex() == container_object_->ChildCountIncludingIgnored())
    return nullptr;
  DCHECK_LT(ChildIndex(), container_object_->ChildCountIncludingIgnored());
  return container_object_->ChildAtIncludingIgnored(ChildIndex());
}

int AXPosition::ChildIndex() const {
  if (!IsTextPosition())
    return text_offset_or_child_index_;
  DUMP_WILL_BE_NOTREACHED() << *this << " should be a tree position.";
  return 0;
}

int AXPosition::TextOffset() const {
  if (IsTextPosition())
    return text_offset_or_child_index_;
  NOTREACHED_IN_MIGRATION() << *this << " should be a text position.";
  return 0;
}

int AXPosition::MaxTextOffset() const {
  if (!IsTextPosition()) {
    NOTREACHED_IN_MIGRATION() << *this << " should be a text position.";
    return 0;
  }

  // TODO(nektar): Make AXObject::TextLength() public and use throughout this
  // method.
  if (container_object_->IsAtomicTextField())
    return container_object_->GetValueForControl().length();

  if (!container_object_->GetNode()) {
    // 1. The |Node| associated with an inline text box contains all the text in
    // the static text object parent, whilst the inline text box might contain
    // only part of it.
    // 2. Some accessibility objects, such as those used for CSS "::before" and
    // "::after" content, don't have an associated text node. We retrieve the
    // text from the inline text box or layout object itself.
    return container_object_->ComputedName().length();
  }

  const LayoutObject* layout_object = container_object_->GetLayoutObject();
  if (!layout_object)
    return container_object_->ComputedName().length();
  // TODO(nektar): Remove all this logic once we switch to
  // AXObject::TextLength().
  const bool is_atomic_inline_level =
      layout_object->IsInline() && layout_object->IsAtomicInlineLevel();
  if (!is_atomic_inline_level && !layout_object->IsText())
    return container_object_->ComputedName().length();

  // TODO(crbug.com/1149171): OffsetMappingBuilder does not properly
  // compute offset mappings for empty LayoutText objects. Other text objects
  // (such as some list markers) are not affected.
  if (const LayoutText* layout_text = DynamicTo<LayoutText>(layout_object)) {
    if (layout_text->HasEmptyText()) {
      return container_object_->ComputedName().length();
    }
  }

  LayoutBlockFlow* formatting_context =
      OffsetMapping::GetInlineFormattingContextOf(*layout_object);
  const OffsetMapping* container_offset_mapping =
      formatting_context ? InlineNode::GetOffsetMapping(formatting_context)
                         : nullptr;
  if (!container_offset_mapping)
    return container_object_->ComputedName().length();
  const base::span<const OffsetMappingUnit> mapping_units =
      container_offset_mapping->GetMappingUnitsForNode(
          *container_object_->GetClosestNode());
  if (mapping_units.empty())
    return container_object_->ComputedName().length();
  return static_cast<int>(mapping_units.back().TextContentEnd() -
                          mapping_units.front().TextContentStart());
}

TextAffinity AXPosition::Affinity() const {
  if (!IsTextPosition()) {
    NOTREACHED_IN_MIGRATION() << *this << " should be a text position.";
    return TextAffinity::kDownstream;
  }

  return affinity_;
}

bool AXPosition::IsValid(String* failure_reason) const {
  if (!container_object_) {
    if (failure_reason)
      *failure_reason = "\nPosition invalid: no container object.";
    return false;
  }
  if (container_object_->IsDetached()) {
    if (failure_reason)
      *failure_reason = "\nPosition invalid: detached container object.";
    return false;
  }
  if (!container_object_->GetDocument()) {
    if (failure_reason) {
      *failure_reason = "\nPosition invalid: no document for container object.";
    }
    return false;
  }

  // Some container objects, such as those for CSS "::before" and "::after"
  // text, don't have associated DOM nodes.
  if (container_object_->GetClosestNode() &&
      !container_object_->GetClosestNode()->isConnected()) {
    if (failure_reason) {
      *failure_reason =
          "\nPosition invalid: container object node is disconnected.";
    }
    return false;
  }

  const Document* document = container_object_->GetDocument();
  DCHECK(document->IsActive());
  DCHECK(!document->NeedsLayoutTreeUpdate());
  if (!document->IsActive() || document->NeedsLayoutTreeUpdate()) {
    if (failure_reason) {
      *failure_reason =
          "\nPosition invalid: document is either not active or it needs "
          "layout tree update.";
    }
    return false;
  }

  if (IsTextPosition()) {
    if (text_offset_or_child_index_ > MaxTextOffset()) {
      if (failure_reason) {
        *failure_reason = String::Format(
            "\nPosition invalid: text offset too large.\n%d vs. %d.",
            text_offset_or_child_index_, MaxTextOffset());
      }
      return false;
    }
  } else {
    if (text_offset_or_child_index_ >
        container_object_->ChildCountIncludingIgnored()) {
      if (failure_reason) {
        *failure_reason = String::Format(
            "\nPosition invalid: child index too large.\n%d vs. %d.",
            text_offset_or_child_index_,
            container_object_->ChildCountIncludingIgnored());
      }
      return false;
    }
  }

#if DCHECK_IS_ON()
  DCHECK_EQ(container_object_->GetDocument()->DomTreeVersion(),
            dom_tree_version_);
  DCHECK_EQ(container_object_->GetDocument()->StyleVersion(), style_version_);
#endif  // DCHECK_IS_ON()
  return true;
}

bool AXPosition::IsTextPosition() const {
  // We don't call |IsValid| from here because |IsValid| uses this method.
  if (!container_object_)
    return false;
  return container_object_->IsTextObject() ||
         container_object_->IsAtomicTextField();
}

const AXPosition AXPosition::CreateNextPosition() const {
  if (!IsValid())
    return {};

  if (IsTextPosition() && TextOffset() < MaxTextOffset()) {
    return CreatePositionInTextObject(*container_object_, (TextOffset() + 1),
                                      TextAffinity::kDownstream,
                                      AXPositionAdjustmentBehavior::kMoveRight);
  }

  // Handles both an "after children" position, or a text position that is right
  // after the last character.
  const AXObject* child = ChildAfterTreePosition();
  if (!child) {
    // If this is a static text object, we should not descend into its inline
    // text boxes when present, because we'll just be creating a text position
    // in the same piece of text.
    const AXObject* next_in_order =
        container_object_->ChildCountIncludingIgnored()
            ? container_object_->DeepestLastChildIncludingIgnored()
                  ->NextInPreOrderIncludingIgnored()
            : container_object_->NextInPreOrderIncludingIgnored();
    if (!next_in_order || !next_in_order->ParentObjectIncludedInTree())
      return {};

    return CreatePositionBeforeObject(*next_in_order,
                                      AXPositionAdjustmentBehavior::kMoveRight);
  }

  if (!child->ParentObjectIncludedInTree())
    return {};

  return CreatePositionAfterObject(*child,
                                   AXPositionAdjustmentBehavior::kMoveRight);
}

const AXPosition AXPosition::CreatePreviousPosition() const {
  if (!IsValid())
    return {};

  if (IsTextPosition() && TextOffset() > 0) {
    return CreatePositionInTextObject(*container_object_, (TextOffset() - 1),
                                      TextAffinity::kDownstream,
                                      AXPositionAdjustmentBehavior::kMoveLeft);
  }

  const AXObject* child = ChildAfterTreePosition();
  const AXObject* object_before_position = nullptr;
  // Handles both an "after children" position, or a text position that is
  // before the first character.
  if (!child) {
    // If this is a static text object, we should not descend into its inline
    // text boxes when present, because we'll just be creating a text position
    // in the same piece of text.
    if (!container_object_->IsTextObject() &&
        container_object_->ChildCountIncludingIgnored()) {
      const AXObject* last_child =
          container_object_->LastChildIncludingIgnored();
      // Dont skip over any intervening text.
      if (last_child->IsTextObject() || last_child->IsAtomicTextField()) {
        return CreatePositionAfterObject(
            *last_child, AXPositionAdjustmentBehavior::kMoveLeft);
      }

      return CreatePositionBeforeObject(
          *last_child, AXPositionAdjustmentBehavior::kMoveLeft);
    }

    object_before_position =
        container_object_->PreviousInPreOrderIncludingIgnored();
  } else {
    object_before_position = child->PreviousInPreOrderIncludingIgnored();
  }

  if (!object_before_position ||
      !object_before_position->ParentObjectIncludedInTree()) {
    return {};
  }

  // Dont skip over any intervening text.
  if (object_before_position->IsTextObject() ||
      object_before_position->IsAtomicTextField()) {
    return CreatePositionAfterObject(*object_before_position,
                                     AXPositionAdjustmentBehavior::kMoveLeft);
  }

  return CreatePositionBeforeObject(*object_before_position,
                                    AXPositionAdjustmentBehavior::kMoveLeft);
}

const AXPosition AXPosition::AsUnignoredPosition(
    const AXPositionAdjustmentBehavior adjustment_behavior) const {
  if (!IsValid())
    return {};

  // There are five possibilities:
  //
  // 1. The container object is ignored and this is not a text position or an
  // "after children" position. Try to find the equivalent position in the
  // unignored parent.
  //
  // 2. The position is a text position and the container object is ignored.
  // Return a "before children" or an "after children" position anchored at the
  // container's unignored parent.
  //
  // 3. The container object is ignored and this is an "after children"
  // position. Find the previous or the next object in the tree and recurse.
  //
  // 4. The child after a tree position is ignored, but the container object is
  // not. Return a "before children" or an "after children" position.
  //
  // 5. We arbitrarily decided to ignore positions that are anchored to before a
  // text object. We move such positions to before the first character of the
  // text object. This is in an effort to ensure that two positions, one a
  // "before object" position anchored to a text object, and one a "text
  // position" anchored to before the first character of the same text object,
  // compare as equivalent.

  const AXObject* container = container_object_;
  const AXObject* child = ChildAfterTreePosition();

  // Case 1.
  // Neither text positions nor "after children" positions have a |child|
  // object.
  if (!container->IsIncludedInTree() && child) {
    // |CreatePositionBeforeObject| already finds the unignored parent before
    // creating the new position, so we don't need to replicate the logic here.
    return CreatePositionBeforeObject(*child, adjustment_behavior);
  }

  // Cases 2 and 3.
  if (!container->IsIncludedInTree()) {
    // Case 2.
    if (IsTextPosition()) {
      if (!container->ParentObjectIncludedInTree())
        return {};

      // Calling |CreateNextPosition| or |CreatePreviousPosition| is not
      // appropriate here because they will go through the text position
      // character by character which is unnecessary, in addition to skipping
      // any unignored siblings.
      switch (adjustment_behavior) {
        case AXPositionAdjustmentBehavior::kMoveRight:
          return CreateLastPositionInObject(
              *container->ParentObjectIncludedInTree(), adjustment_behavior);
        case AXPositionAdjustmentBehavior::kMoveLeft:
          return CreateFirstPositionInObject(
              *container->ParentObjectIncludedInTree(), adjustment_behavior);
      }
    }

    // Case 3.
    switch (adjustment_behavior) {
      case AXPositionAdjustmentBehavior::kMoveRight:
        return CreateNextPosition().AsUnignoredPosition(adjustment_behavior);
      case AXPositionAdjustmentBehavior::kMoveLeft:
        return CreatePreviousPosition().AsUnignoredPosition(
            adjustment_behavior);
    }
  }

  // Case 4.
  if (child && !child->IsIncludedInTree()) {
    switch (adjustment_behavior) {
      case AXPositionAdjustmentBehavior::kMoveRight:
        return CreateLastPositionInObject(*container);
      case AXPositionAdjustmentBehavior::kMoveLeft:
        return CreateFirstPositionInObject(*container);
    }
  }

  // Case 5.
  if (child && child->IsTextObject())
    return CreateFirstPositionInObject(*child);

  // The position is not ignored.
  return *this;
}

const AXPosition AXPosition::AsValidDOMPosition(
    const AXPositionAdjustmentBehavior adjustment_behavior) const {
  if (!IsValid())
    return {};

  // We adjust to the next or previous position if the container or the child
  // object after a tree position are mock or virtual objects, since mock or
  // virtual objects will not be present in the DOM tree. Alternatively, in the
  // case of an "after children" position, we need to check if the last child of
  // the container object is mock or virtual and adjust accordingly. Abstract
  // inline text boxes and static text nodes for CSS "::before" and "::after"
  // positions are also considered to be virtual since they don't have an
  // associated DOM node.

  // In more detail:
  // If the child after a tree position doesn't have an associated node in the
  // DOM tree, we adjust to the next or previous position because a
  // corresponding child node will not be found in the DOM tree. We need a
  // corresponding child node in the DOM tree so that we can anchor the DOM
  // position before it. We can't ask the layout tree for the child's container
  // block node, because this might change the placement of the AX position
  // drastically. However, if the container doesn't have a corresponding DOM
  // node, we need to use the layout tree to find its corresponding container
  // block node, because no AX positions inside an anonymous layout block could
  // be represented in the DOM tree anyway.

  const AXObject* container = container_object_;
  DCHECK(container);
  const AXObject* child = ChildAfterTreePosition();
  const AXObject* last_child = container->LastChildIncludingIgnored();
  if ((IsTextPosition() &&
       (!container->GetClosestNode() ||
        container->GetClosestNode()->IsMarkerPseudoElement())) ||
      (!child && last_child &&
       (!last_child->GetClosestNode() ||
        last_child->GetClosestNode()->IsMarkerPseudoElement())) ||
      (child && (!child->GetClosestNode() ||
                 child->GetClosestNode()->IsMarkerPseudoElement()))) {
    AXPosition result;
    if (adjustment_behavior == AXPositionAdjustmentBehavior::kMoveRight)
      result = CreateNextPosition();
    else
      result = CreatePreviousPosition();

    if (result && result != *this)
      return result.AsValidDOMPosition(adjustment_behavior);
    return {};
  }

  // At this point, if a non-pseudo element DOM node is associated with our
  // container, then the corresponding DOM position should be valid.
  const Node* container_node = container->GetClosestNode();
  if (container_node->IsPseudoElement()) {
    container_node = LayoutTreeBuilderTraversal::Parent(*container_node);
  } else {
    return *this;
  }
  DCHECK(container_node) << "All anonymous layout objects and list markers "
                            "should have a containing block element.";
  DCHECK(!container->IsDetached());
  if (!container_node || container->IsDetached())
    return {};

  auto& ax_object_cache_impl = container->AXObjectCache();
  const AXObject* new_container = ax_object_cache_impl.Get(container_node);
  DCHECK(new_container);
  if (!new_container)
    return {};

  AXPosition position(*new_container);
  if (new_container == container->ParentObjectIncludedInTree()) {
    position.text_offset_or_child_index_ = container->IndexInParent();
  } else {
    switch (adjustment_behavior) {
      case AXPositionAdjustmentBehavior::kMoveRight:
        position.text_offset_or_child_index_ =
            new_container->ChildCountIncludingIgnored();
        break;
      case AXPositionAdjustmentBehavior::kMoveLeft:
        position.text_offset_or_child_index_ = 0;
        break;
    }
  }
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(position.IsValid(&failure_reason)) << failure_reason;
#endif
  return position.AsValidDOMPosition(adjustment_behavior);
}

const PositionWithAffinity AXPosition::ToPositionWithAffinity(
    const AXPositionAdjustmentBehavior adjustment_behavior) const {
  const AXPosition adjusted_position = AsValidDOMPosition(adjustment_behavior);
  if (!adjusted_position.IsValid())
    return {};

  const Node* container_node =
      adjusted_position.container_object_->GetClosestNode();
  DCHECK(container_node) << "AX positions that are valid DOM positions should "
                            "always be connected to their DOM nodes.";
  if (!container_node)
    return {};

  if (!adjusted_position.IsTextPosition()) {
    // AX positions that are unumbiguously at the start or end of a container,
    // should convert to the corresponding DOM positions at the start or end of
    // their parent node. Other child positions in the accessibility tree should
    // recompute their parent in the DOM tree, because they might be ARIA owned
    // by a different object in the accessibility tree than in the DOM tree, or
    // their parent in the accessibility tree might be ignored.

    const AXObject* child = adjusted_position.ChildAfterTreePosition();
    if (child) {
      const Node* child_node = child->GetClosestNode();
      DCHECK(child_node) << "AX objects used in AX positions that are valid "
                            "DOM positions should always be connected to their "
                            "DOM nodes.";
      if (!child_node)
        return {};

      if (!child_node->previousSibling()) {
        // Creates a |PositionAnchorType::kBeforeChildren| position.
        container_node = child_node->parentNode();
        DCHECK(container_node);
        if (!container_node)
          return {};

        return PositionWithAffinity(
            Position::FirstPositionInNode(*container_node), affinity_);
      }

      // Creates a |PositionAnchorType::kOffsetInAnchor| position.
      return PositionWithAffinity(Position::InParentBeforeNode(*child_node),
                                  affinity_);
    }

    // "After children" positions.
    const AXObject* last_child = container_object_->LastChildIncludingIgnored();
    if (last_child) {
      const Node* last_child_node = last_child->GetClosestNode();
      DCHECK(last_child_node) << "AX objects used in AX positions that are "
                                 "valid DOM positions should always be "
                                 "connected to their DOM nodes.";
      if (!last_child_node)
        return {};

      // Check if this is an "after children" position in the DOM as well.
      if (!last_child_node->nextSibling()) {
        // Creates a |PositionAnchorType::kAfterChildren| position.
        container_node = last_child_node->parentNode();
        DCHECK(container_node);
        if (!container_node)
          return {};

        return PositionWithAffinity(
            Position::LastPositionInNode(*container_node), affinity_);
      }

      // Do the next best thing by creating a
      // |PositionAnchorType::kOffsetInAnchor| position after the last unignored
      // child.
      return PositionWithAffinity(Position::InParentAfterNode(*last_child_node),
                                  affinity_);
    }

    // The |AXObject| container has no children. Do the next best thing by
    // creating a |PositionAnchorType::kBeforeChildren| position.
    return PositionWithAffinity(Position::FirstPositionInNode(*container_node),
                                affinity_);
  }

  // If OffsetMapping supports it, convert from a text offset, which may have
  // white space collapsed, to a DOM offset which should have uncompressed white
  // space. OffsetMapping supports layout text, layout replaced, ruby columns,
  // list markers, and layout block flow at inline-level, i.e. "display=inline"
  // or "display=inline-block". It also supports out-of-flow elements, which
  // should not be relevant to text positions in the accessibility tree.
  const LayoutObject* layout_object = container_node->GetLayoutObject();
  // TODO(crbug.com/567964): LayoutObject::IsAtomicInlineLevel() also includes
  // block-level replaced elements. We need to explicitly exclude them via
  // LayoutObject::IsInline().
  const bool supports_ng_offset_mapping =
      layout_object &&
      ((layout_object->IsInline() && layout_object->IsAtomicInlineLevel()) ||
       layout_object->IsText());
  const OffsetMapping* container_offset_mapping = nullptr;
  if (supports_ng_offset_mapping) {
    LayoutBlockFlow* formatting_context =
        OffsetMapping::GetInlineFormattingContextOf(*layout_object);
    container_offset_mapping =
        formatting_context ? InlineNode::GetOffsetMapping(formatting_context)
                           : nullptr;
  }

  if (!container_offset_mapping) {
    // We are unable to compute the text offset in the accessibility tree that
    // corresponds to the DOM offset. We do the next best thing by returning
    // either the first or the last DOM position in |container_node| based on
    // the |adjustment_behavior|.
    switch (adjustment_behavior) {
      case AXPositionAdjustmentBehavior::kMoveRight:
        return PositionWithAffinity(
            Position::LastPositionInNode(*container_node), affinity_);
      case AXPositionAdjustmentBehavior::kMoveLeft:
        return PositionWithAffinity(
            Position::FirstPositionInNode(*container_node), affinity_);
    }
  }

  int text_offset_in_formatting_context =
      adjusted_position.container_object_->TextOffsetInFormattingContext(
          adjusted_position.TextOffset());
  DCHECK_GE(text_offset_in_formatting_context, 0);

  // An "after text" position in the accessibility tree should map to a text
  // position in the DOM tree that is after the DOM node's text, but before any
  // collapsed white space at the node's end. In all other cases, the text
  // offset in the accessibility tree should be translated to a DOM offset that
  // is after any collapsed white space. For example, look at the inline text
  // box with the word "Hello" and observe how the white space in the DOM, both
  // before and after the word, is mapped from the equivalent accessibility
  // position.
  //
  // AX text position in "InlineTextBox" name="Hello", 0
  // DOM position #text "   Hello   "@offsetInAnchor[3]
  // AX text position in "InlineTextBox" name="Hello", 5
  // DOM position #text "   Hello   "@offsetInAnchor[8]
  Position dom_position =
      adjusted_position.TextOffset() < adjusted_position.MaxTextOffset()
          ? container_offset_mapping->GetLastPosition(
                static_cast<unsigned int>(text_offset_in_formatting_context))
          : container_offset_mapping->GetFirstPosition(
                static_cast<unsigned int>(text_offset_in_formatting_context));

  // When there is no uncompressed white space at the end of our
  // |container_node|, and this is an "after text" position, we might get back
  // the NULL position if this is the last node in the DOM.
  if (dom_position.IsNull())
    dom_position = Position::LastPositionInNode(*container_node);
  return PositionWithAffinity(dom_position, affinity_);
}

const Position AXPosition::ToPosition(
    const AXPositionAdjustmentBehavior adjustment_behavior) const {
  return ToPositionWithAffinity(adjustment_behavior).GetPosition();
}

String AXPosition::ToString() const {
  if (!IsValid())
    return "Invalid AXPosition";

  StringBuilder builder;
  if (IsTextPosition()) {
    builder.Append("AX text position in ");
    builder.Append(container_object_->ToString(/*verbose*/false));
    builder.AppendFormat(", %d", TextOffset());
    return builder.ToString();
  }

  builder.Append("AX object anchored position in ");
  builder.Append(container_object_->ToString(/*verbose*/false));
  builder.AppendFormat(", %d", ChildIndex());
  return builder.ToString();
}

// static
bool AXPosition::IsIgnoredCharacter(UChar character) {
  switch (character) {
    case kZeroWidthSpaceCharacter:
    case kLeftToRightIsolateCharacter:
    case kRightToLeftIsolateCharacter:
    case kPopDirectionalIsolateCharacter:
      return true;
    default:
      return false;
  }
}

int AXPosition::GetLeadingIgnoredCharacterCount(const OffsetMapping* mapping,
                                                const Node* node,
                                                int container_offset,
                                                int content_offset) const {
  if (!mapping) {
    return content_offset;
  }

  String text = mapping->GetText();
  int count = 0;
  unsigned previous_content_end = container_offset;
  for (auto unit : mapping->GetMappingUnitsForNode(*node)) {
    if (unit.TextContentStart() > static_cast<unsigned>(content_offset)) {
      break;
    }

    if (unit.TextContentStart() != previous_content_end) {
      String substring = text.Substring(
          previous_content_end, unit.TextContentStart() - previous_content_end);
      String unignored = substring.RemoveCharacters(IsIgnoredCharacter);
      count += substring.length() - unignored.length();
    }
    previous_content_end = unit.TextContentEnd();
  }

  return count;
}

// static
const AXObject* AXPosition::FindNeighboringUnignoredObject(
    const Document& document,
    const Node& child_node,
    const ContainerNode* container_node,
    const AXPositionAdjustmentBehavior adjustment_behavior) {
  AXObjectCache* ax_object_cache = document.ExistingAXObjectCache();
  if (!ax_object_cache)
    return nullptr;

  auto* ax_object_cache_impl = static_cast<AXObjectCacheImpl*>(ax_object_cache);
  switch (adjustment_behavior) {
    case AXPositionAdjustmentBehavior::kMoveRight: {
      const Node* next_node = &child_node;
      while ((next_node = NodeTraversal::NextIncludingPseudo(*next_node,
                                                             container_node))) {
        const AXObject* next_object = ax_object_cache_impl->Get(next_node);
        if (next_object && next_object->IsIncludedInTree())
          return next_object;
      }
      return nullptr;
    }

    case AXPositionAdjustmentBehavior::kMoveLeft: {
      const Node* previous_node = &child_node;
      // Since this is a pre-order traversal,
      // "NodeTraversal::PreviousIncludingPseudo" will eventually reach
      // |container_node| if |container_node| is not nullptr. We should exclude
      // this as we are strictly interested in |container_node|'s unignored
      // descendantsin the accessibility tree.
      while ((previous_node = NodeTraversal::PreviousIncludingPseudo(
                  *previous_node, container_node)) &&
             previous_node != container_node) {
        const AXObject* previous_object =
            ax_object_cache_impl->Get(previous_node);
        if (previous_object && previous_object->IsIncludedInTree())
          return previous_object;
      }
      return nullptr;
    }
  }
}

bool operator==(const AXPosition& a, const AXPosition& b) {
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(a.IsValid(&failure_reason) && b.IsValid(&failure_reason))
      << failure_reason;
#endif
  if (*a.ContainerObject() != *b.ContainerObject())
    return false;
  if (a.IsTextPosition() && b.IsTextPosition())
    return a.TextOffset() == b.TextOffset() && a.Affinity() == b.Affinity();
  if (!a.IsTextPosition() && !b.IsTextPosition())
    return a.ChildIndex() == b.ChildIndex();
  NOTREACHED_IN_MIGRATION()
      << "AXPosition objects having the same container object should "
         "have the same type.";
  return false;
}

bool operator!=(const AXPosition& a, const AXPosition& b) {
  return !(a == b);
}

bool operator<(const AXPosition& a, const AXPosition& b) {
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(a.IsValid(&failure_reason) && b.IsValid(&failure_reason))
      << failure_reason;
#endif

  if (a.ContainerObject() == b.ContainerObject()) {
    if (a.IsTextPosition() && b.IsTextPosition())
      return a.TextOffset() < b.TextOffset();
    if (!a.IsTextPosition() && !b.IsTextPosition())
      return a.ChildIndex() < b.ChildIndex();
    NOTREACHED_IN_MIGRATION()
        << "AXPosition objects having the same container object should "
           "have the same type.";
    return false;
  }

  int index_in_ancestor1, index_in_ancestor2;
  const AXObject* ancestor =
      AXObject::LowestCommonAncestor(*a.ContainerObject(), *b.ContainerObject(),
                                     &index_in_ancestor1, &index_in_ancestor2);
  DCHECK_GE(index_in_ancestor1, -1);
  DCHECK_GE(index_in_ancestor2, -1);
  if (!ancestor)
    return false;
  if (ancestor == a.ContainerObject()) {
    DCHECK(!a.IsTextPosition());
    index_in_ancestor1 = a.ChildIndex();
  }
  if (ancestor == b.ContainerObject()) {
    DCHECK(!b.IsTextPosition());
    index_in_ancestor2 = b.ChildIndex();
  }
  return index_in_ancestor1 < index_in_ancestor2;
}

bool operator<=(const AXPosition& a, const AXPosition& b) {
  return a < b || a == b;
}

bool operator>(const AXPosition& a, const AXPosition& b) {
#if DCHECK_IS_ON()
  String failure_reason;
  DCHECK(a.IsValid(&failure_reason) && b.IsValid(&failure_reason))
      << failure_reason;
#endif

  if (a.ContainerObject() == b.ContainerObject()) {
    if (a.IsTextPosition() && b.IsTextPosition())
      return a.TextOffset() > b.TextOffset();
    if (!a.IsTextPosition() && !b.IsTextPosition())
      return a.ChildIndex() > b.ChildIndex();
    NOTREACHED_IN_MIGRATION()
        << "AXPosition objects having the same container object should "
           "have the same type.";
    return false;
  }

  int index_in_ancestor1, index_in_ancestor2;
  const AXObject* ancestor =
      AXObject::LowestCommonAncestor(*a.ContainerObject(), *b.ContainerObject(),
                                     &index_in_ancestor1, &index_in_ancestor2);
  DCHECK_GE(index_in_ancestor1, -1);
  DCHECK_GE(index_in_ancestor2, -1);
  if (!ancestor)
    return false;
  if (ancestor == a.ContainerObject()) {
    DCHECK(!a.IsTextPosition());
    index_in_ancestor1 = a.ChildIndex();
  }
  if (ancestor == b.ContainerObject()) {
    DCHECK(!b.IsTextPosition());
    index_in_ancestor2 = b.ChildIndex();
  }
  return index_in_ancestor1 > index_in_ancestor2;
}

bool operator>=(const AXPosition& a, const AXPosition& b) {
  return a > b || a == b;
}

std::ostream& operator<<(std::ostream& ostream, const AXPosition& position) {
  return ostream << position.ToString().Utf8();
}

}  // namespace blink
