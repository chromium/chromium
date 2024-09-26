/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/editing/layout_selection.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_wbr_element.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

// TODO(yoichio): Share condition between OffsetMapping::AcceptsPosition.
// TODO(1229581): Do we need this function anymore?
bool ShouldUseLayoutNGTextContent(const Node& node) {
  LayoutObject* layout_object = node.GetLayoutObject();
  DCHECK(layout_object);
  if (layout_object->IsInline())
    return layout_object->IsInLayoutNGInlineFormattingContext();
  return IsA<LayoutBlockFlow>(layout_object);
}

}  // namespace

// The current selection to be painted is represented as 2 pairs of
// (Node, offset).
// Each offset represents text offsets on selection edge if it is text.
// For example, suppose we select "f^oo<br><img>|",
// |start_offset_| is 1 and |end_offset_| is nullopt.
// If on NG, offset is on text content offset rather than each text node.
class SelectionPaintRange : public GarbageCollected<SelectionPaintRange> {
 public:
  SelectionPaintRange() = default;
  SelectionPaintRange(const Node& passed_start_node,
                      std::optional<unsigned> passed_start_offset,
                      const Node& passed_end_node,
                      std::optional<unsigned> passed_end_offset)
      : start_node(passed_start_node),
        start_offset(passed_start_offset),
        end_node(passed_end_node),
        end_offset(passed_end_offset) {}
  void Trace(Visitor* visitor) const {
    visitor->Trace(start_node);
    visitor->Trace(end_node);
  }

  bool IsNull() const { return !start_node; }
  void AssertSanity() const {
#if DCHECK_IS_ON()
    if (start_node) {
      DCHECK(end_node);
      DCHECK(start_node->GetLayoutObject()->GetSelectionState() ==
                 SelectionState::kStart ||
             start_node->GetLayoutObject()->GetSelectionState() ==
                 SelectionState::kStartAndEnd);
      DCHECK(end_node->GetLayoutObject()->GetSelectionState() ==
                 SelectionState::kEnd ||
             end_node->GetLayoutObject()->GetSelectionState() ==
                 SelectionState::kStartAndEnd);
      return;
    }
    DCHECK(!end_node);
    DCHECK(!start_offset.has_value());
    DCHECK(!end_offset.has_value());
#endif
  }

  Member<const Node> start_node;
  std::optional<unsigned> start_offset;
  Member<const Node> end_node;
  std::optional<unsigned> end_offset;
};

LayoutSelection::LayoutSelection(FrameSelection& frame_selection)
    : frame_selection_(&frame_selection),
      has_pending_selection_(false),
      paint_range_(MakeGarbageCollected<SelectionPaintRange>()) {}

enum class SelectionMode {
  kNone,
  kRange,
};

void LayoutSelection::AssertIsValid() const {
  const Document& document = frame_selection_->GetDocument();
  DCHECK_GE(document.Lifecycle().GetState(), DocumentLifecycle::kLayoutClean);
  DCHECK(!document.IsSlotAssignmentDirty());
  DCHECK(!has_pending_selection_);
}

static SelectionMode ComputeSelectionMode(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  if (selection_in_dom.IsRange())
    return SelectionMode::kRange;
  DCHECK(selection_in_dom.IsCaret());
  return SelectionMode::kNone;
}

static EphemeralRangeInFlatTree CalcSelectionInFlatTree(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  switch (ComputeSelectionMode(frame_selection)) {
    case SelectionMode::kNone:
      return {};
    case SelectionMode::kRange: {
      const PositionInFlatTree& anchor =
          ToPositionInFlatTree(selection_in_dom.Anchor());
      const PositionInFlatTree& focus =
          ToPositionInFlatTree(selection_in_dom.Focus());
      if (anchor.IsNull() || focus.IsNull() || anchor == focus ||
          !anchor.IsValidFor(frame_selection.GetDocument()) ||
          !focus.IsValidFor(frame_selection.GetDocument())) {
        return {};
      }
      return anchor <= focus ? EphemeralRangeInFlatTree(anchor, focus)
                             : EphemeralRangeInFlatTree(focus, anchor);
    }
  }
  NOTREACHED_IN_MIGRATION();
  return {};
}

// OldSelectedNodes is current selected Nodes with
// current SelectionState which is kStart, kEnd, kStartAndEnd or kInside.
struct OldSelectedNodes {
  STACK_ALLOCATED();

 public:
  OldSelectedNodes()
      : paint_range(MakeGarbageCollected<SelectionPaintRange>()) {}
  OldSelectedNodes(OldSelectedNodes&& other) {
    paint_range = other.paint_range;
    selected_map = std::move(other.selected_map);
  }

  OldSelectedNodes(const OldSelectedNodes&) = delete;
  OldSelectedNodes& operator=(const OldSelectedNodes&) = delete;

  SelectionPaintRange* paint_range;
  HeapHashMap<Member<const Node>, SelectionState> selected_map;
};

std::ostream& operator<<(std::ostream&, const OldSelectedNodes&);

// This struct represents a selection range in layout tree and each
// Node is SelectionState-marked.
struct NewPaintRangeAndSelectedNodes {
  STACK_ALLOCATED();

 public:
  NewPaintRangeAndSelectedNodes()
      : paint_range(MakeGarbageCollected<SelectionPaintRange>()) {}
  NewPaintRangeAndSelectedNodes(
      SelectionPaintRange* passed_paint_range,
      HeapHashSet<Member<const Node>>&& passed_selected_objects)
      : paint_range(passed_paint_range),
        selected_objects(std::move(passed_selected_objects)) {}
  NewPaintRangeAndSelectedNodes(NewPaintRangeAndSelectedNodes&& other) {
    paint_range = other.paint_range;
    selected_objects = std::move(other.selected_objects);
  }

  NewPaintRangeAndSelectedNodes(const NewPaintRangeAndSelectedNodes&) = delete;
  NewPaintRangeAndSelectedNodes& operator=(
      const NewPaintRangeAndSelectedNodes&) = delete;

  void AssertSanity() const {
#if DCHECK_IS_ON()
    paint_range->AssertSanity();
    if (paint_range->start_node) {
      DCHECK(selected_objects.Contains(paint_range->start_node)) << this;
      DCHECK(selected_objects.Contains(paint_range->end_node)) << this;
      return;
    }
    DCHECK(selected_objects.empty()) << this;
#endif
  }

  SelectionPaintRange* paint_range;
  HeapHashSet<Member<const Node>> selected_objects;
};

std::ostream& operator<<(std::ostream&, const NewPaintRangeAndSelectedNodes&);

static void SetShouldInvalidateIfNeeded(LayoutObject* layout_object) {
  if (layout_object->ShouldInvalidateSelection())
    return;
  layout_object->SetShouldInvalidateSelection();

  // We should invalidate if ancestor of |layout_object| is LayoutSVGText
  // because SVGRootInlineBoxPainter::Paint() paints selection for
  // |layout_object| in/ LayoutSVGText and it is invoked when parent
  // LayoutSVGText is invalidated.
  // That is different from InlineTextBoxPainter::Paint() which paints
  // LayoutText selection when LayoutText is invalidated.
  if (!layout_object->IsSVG())
    return;
  for (LayoutObject* parent = layout_object->Parent(); parent;
       parent = parent->Parent()) {
    if (parent->IsSVGRoot())
      return;
    if (parent->IsSVGText()) {
      if (!parent->ShouldInvalidateSelection())
        parent->SetShouldInvalidateSelection();
      return;
    }
  }
}

static LayoutTextFragment* FirstLetterPartFor(
    const LayoutObject* layout_object) {
  // TODO(yoichio): LayoutText::GetFirstLetterPart() should be typed
  // LayoutTextFragment.
  if (const auto* layout_text = DynamicTo<LayoutText>(layout_object))
    return To<LayoutTextFragment>(layout_text->GetFirstLetterPart());
  return nullptr;
}

static void SetShouldInvalidateIfNeeded(const Node& node) {
  LayoutObject* layout_object = node.GetLayoutObject();
  if (!layout_object)
    return;
  if (LayoutTextFragment* first_letter = FirstLetterPartFor(layout_object))
    SetShouldInvalidateIfNeeded(first_letter);
  SetShouldInvalidateIfNeeded(layout_object);
}

static void SetSelectionStateIfNeeded(const Node& node, SelectionState state) {
  DCHECK_NE(state, SelectionState::kContain) << node;
  DCHECK_NE(state, SelectionState::kNone) << node;
  LayoutObject* layout_object = node.GetLayoutObject();
  if (layout_object->GetSelectionState() == state)
    return;
  layout_object->SetSelectionState(state);

  // Set ancestors SelectionState kContain for CSS ::selection style.
  // See LayoutObject::InvalidateSelectedChildrenOnStyleChange().
  for (Node& ancestor : FlatTreeTraversal::AncestorsOf(node)) {
    LayoutObject* ancestor_layout = ancestor.GetLayoutObject();
    if (!ancestor_layout)
      continue;
    if (ancestor_layout->GetSelectionState() == SelectionState::kContain)
      return;
    ancestor_layout->LayoutObject::SetSelectionState(SelectionState::kContain);
  }
}

// Set ShouldInvalidateSelection flag of LayoutObjects
// comparing them in |new_range| and |old_range|.
static void SetShouldInvalidateSelection(
    const NewPaintRangeAndSelectedNodes& new_range,
    const OldSelectedNodes& old_selected_objects) {
  // We invalidate each LayoutObject in
  // MakeGarbageCollected<SelectionPaintRange> which has SelectionState of
  // kStart, kEnd, kStartAndEnd, or kInside and is not in old
  // SelectionPaintRange.
  for (const Node* node : new_range.selected_objects) {
    if (old_selected_objects.selected_map.Contains(node))
      continue;
    const SelectionState new_state =
        node->GetLayoutObject()->GetSelectionState();
    DCHECK_NE(new_state, SelectionState::kContain) << node;
    DCHECK_NE(new_state, SelectionState::kNone) << node;
    SetShouldInvalidateIfNeeded(*node);
  }
  // For LayoutObject in old SelectionPaintRange, we invalidate LayoutObjects
  // each of:
  // 1. LayoutObject was painted and would not be painted.
  // 2. LayoutObject was not painted and would be painted.
  for (const auto& key_value : old_selected_objects.selected_map) {
    const Node* const node = key_value.key;
    const SelectionState old_state = key_value.value;
    const SelectionState new_state =
        node->GetLayoutObject()->GetSelectionState();
    if (new_state == old_state)
      continue;
    DCHECK(new_state != SelectionState::kNone ||
           old_state != SelectionState::kNone)
        << node;
    DCHECK_NE(new_state, SelectionState::kContain) << node;
    DCHECK_NE(old_state, SelectionState::kContain) << node;
    SetShouldInvalidateIfNeeded(*node);
  }

  // Invalidate Selection start/end is moving on a same node.
  const SelectionPaintRange& new_paint_range = *new_range.paint_range;
  const SelectionPaintRange& old_paint_range =
      *old_selected_objects.paint_range;
  if (new_paint_range.IsNull())
    return;
  if (new_paint_range.start_node->IsTextNode() &&
      new_paint_range.start_node == old_paint_range.start_node &&
      new_paint_range.start_offset != old_paint_range.start_offset)
    SetShouldInvalidateIfNeeded(*new_paint_range.start_node);
  if (new_paint_range.end_node->IsTextNode() &&
      new_paint_range.end_node == old_paint_range.end_node &&
      new_paint_range.end_offset != old_paint_range.end_offset)
    SetShouldInvalidateIfNeeded(*new_paint_range.end_node);
}

static bool IsDisplayContentElement(const Node& node) {
  if (!node.IsElementNode())
    return false;
  const ComputedStyle* const style = node.GetComputedStyle();
  return style && style->Display() == EDisplay::kContents;
}

template <typename Visitor>
static void VisitSelectedInclusiveDescendantsOfInternal(Node& node,
                                                        Visitor* visitor) {
  // Display:content element appears in a flat tree even it doesn't have
  // a LayoutObject but we need to visit its children.
  if (!IsDisplayContentElement(node)) {
    LayoutObject* layout_object = node.GetLayoutObject();
    if (!layout_object)
      return;
    if (layout_object->GetSelectionState() == SelectionState::kNone)
      return;
    visitor->Visit(node);
  }

  for (Node& child : FlatTreeTraversal::ChildrenOf(node))
    VisitSelectedInclusiveDescendantsOfInternal(child, visitor);
}

static inline bool IsFlatTreeClean(const Node& node) {
  return !node.GetDocument().IsSlotAssignmentDirty();
}

template <typename Visitor>
static void VisitSelectedInclusiveDescendantsOf(Node& node, Visitor* visitor) {
  DCHECK(IsFlatTreeClean(node));
  return VisitSelectedInclusiveDescendantsOfInternal(node, visitor);
}

static OldSelectedNodes ResetOldSelectedNodes(
    Node& root,
    std::optional<unsigned> old_start_offset,
    std::optional<unsigned> old_end_offset) {
  class OldSelectedVisitor {
    STACK_ALLOCATED();

   public:
    OldSelectedVisitor(std::optional<unsigned> passed_old_start_offset,
                       std::optional<unsigned> passed_old_end_offset)
        : old_start_offset(passed_old_start_offset),
          old_end_offset(passed_old_end_offset) {}

    void Visit(Node& node) {
      LayoutObject* layout_object = node.GetLayoutObject();
      const SelectionState old_state = layout_object->GetSelectionState();
      DCHECK_NE(old_state, SelectionState::kNone) << node;
      layout_object->SetSelectionState(SelectionState::kNone);
      if (old_state == SelectionState::kContain)
        return;
      old_selected_objects.selected_map.insert(&node, old_state);
      if (old_state == SelectionState::kInside)
        return;
      switch (old_state) {
        case SelectionState::kStart: {
          DCHECK(!old_selected_objects.paint_range->start_node);
          old_selected_objects.paint_range->start_node = node;
          old_selected_objects.paint_range->start_offset = old_start_offset;
          break;
        }
        case SelectionState::kEnd: {
          DCHECK(!old_selected_objects.paint_range->end_node);
          old_selected_objects.paint_range->end_node = node;
          old_selected_objects.paint_range->end_offset = old_end_offset;
          break;
        }
        case SelectionState::kStartAndEnd: {
          DCHECK(!old_selected_objects.paint_range->start_node);
          DCHECK(!old_selected_objects.paint_range->end_node);
          old_selected_objects.paint_range->start_node = node;
          old_selected_objects.paint_range->start_offset = old_start_offset;
          old_selected_objects.paint_range->end_node = node;
          old_selected_objects.paint_range->end_offset = old_end_offset;
          break;
        }
        default: {
          NOTREACHED_IN_MIGRATION();
          break;
        }
      }
    }

    OldSelectedNodes old_selected_objects;
    const std::optional<unsigned> old_start_offset;
    const std::optional<unsigned> old_end_offset;
  } visitor(old_start_offset, old_end_offset);
  VisitSelectedInclusiveDescendantsOf(root, &visitor);
  return std::move(visitor.old_selected_objects);
}

static std::optional<unsigned> ComputeStartOffset(
    const Node& node,
    const PositionInFlatTree& selection_start) {
  if (!node.IsTextNode())
    return std::nullopt;

  if (&node == selection_start.AnchorNode())
    return selection_start.OffsetInContainerNode();
  return 0;
}

static std::optional<unsigned> ComputeEndOffset(
    const Node& node,
    const PositionInFlatTree& selection_end) {
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return std::nullopt;

  if (&node == selection_end.AnchorNode())
    return selection_end.OffsetInContainerNode();
  return text_node->length();
}

#if DCHECK_IS_ON()
// Position should be offset on text or before/after a break element.
static bool IsPositionValidText(const Position& position) {
  if (position.AnchorNode()->IsTextNode() && position.IsOffsetInAnchor())
    return true;
  if ((IsA<HTMLBRElement>(position.AnchorNode()) ||
       IsA<HTMLWBRElement>(position.AnchorNode())) &&
      (position.IsBeforeAnchor() || position.IsAfterAnchor()))
    return true;
  return false;
}
#endif

static std::optional<unsigned> GetTextContentOffset(const Position& position) {
  if (position.IsNull())
    return std::nullopt;
#if DCHECK_IS_ON()
  DCHECK(IsPositionValidText(position));
#endif
  DCHECK(ShouldUseLayoutNGTextContent(*position.AnchorNode()));
  const OffsetMapping* const offset_mapping = OffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  if (offset_mapping == nullptr)
    return std::nullopt;
  const std::optional<unsigned>& ng_offset =
      offset_mapping->GetTextContentOffset(position);
  return ng_offset;
}

// Computes text content offset of selection start if |layout_object| is
// LayoutText.
static std::optional<unsigned> GetTextContentOffsetStart(
    const Node& node,
    std::optional<unsigned> node_offset) {
  if (!node.GetLayoutObject()->IsText())
    return std::nullopt;
  if (node.IsTextNode()) {
    DCHECK(node_offset.has_value()) << node;
    return GetTextContentOffset(Position(node, node_offset.value()));
  }

  DCHECK(IsA<HTMLWBRElement>(node) || IsA<HTMLBRElement>(node)) << node;
  DCHECK(!node_offset.has_value()) << node;
  return GetTextContentOffset(Position::BeforeNode(node));
}

// Computes text content offset of selection end if |layout_object| is
// LayoutText.
static std::optional<unsigned> GetTextContentOffsetEnd(
    const Node& node,
    std::optional<unsigned> node_offset) {
  if (!node.GetLayoutObject()->IsText())
    return {};
  if (node.IsTextNode()) {
    DCHECK(node_offset.has_value()) << node;
    return GetTextContentOffset(Position(node, node_offset.value()));
  }

  DCHECK(IsA<HTMLWBRElement>(node) || IsA<HTMLBRElement>(node)) << node;
  DCHECK(!node_offset.has_value()) << node;
  return GetTextContentOffset(Position::AfterNode(node));
}

static SelectionPaintRange* ComputeNewPaintRange(
    const SelectionPaintRange& paint_range) {
  DCHECK(!paint_range.IsNull());

  const Node& start_node = *paint_range.start_node;
  // If LayoutObject is not in NG, use legacy offset.
  const std::optional<unsigned> start_offset =
      ShouldUseLayoutNGTextContent(start_node)
          ? GetTextContentOffsetStart(start_node, paint_range.start_offset)
          : paint_range.start_offset;

  const Node& end_node = *paint_range.end_node;
  const std::optional<unsigned> end_offset =
      ShouldUseLayoutNGTextContent(end_node)
          ? GetTextContentOffsetEnd(end_node, paint_range.end_offset)
          : paint_range.end_offset;

  return MakeGarbageCollected<SelectionPaintRange>(
      *paint_range.start_node, start_offset, *paint_range.end_node, end_offset);
}

static unsigned ClampOffset(unsigned offset,
                            unsigned start_offset,
                            unsigned end_offset) {
  DCHECK_LE(start_offset, end_offset);
  return std::min(std::max(offset, start_offset), end_offset);
}

static Text* AssociatedTextNode(const LayoutText& text) {
  if (const auto* fragment = DynamicTo<LayoutTextFragment>(text))
    return fragment->AssociatedTextNode();
  if (Node* node = text.GetNode())
    return DynamicTo<Text>(node);
  return nullptr;
}

static SelectionState GetSelectionStateFor(const LayoutText& layout_text) {
  if (const auto* text_fragment = DynamicTo<LayoutTextFragment>(layout_text)) {
    Node* node = text_fragment->AssociatedTextNode();
    if (!node)
      return SelectionState::kNone;
    return node->GetLayoutObject()->GetSelectionState();
  }
  return layout_text.GetSelectionState();
}

static SelectionState GetSelectionStateFor(
    const InlineCursorPosition& position) {
  DCHECK(position.GetLayoutObject());
  return GetSelectionStateFor(To<LayoutText>(*position.GetLayoutObject()));
}

static SelectionState GetPaintingSelectionStateFor(
    const LayoutText& layout_text) {
  if (const auto* text_fragment = DynamicTo<LayoutTextFragment>(layout_text)) {
    Node* node = text_fragment->AssociatedTextNode();
    if (!node)
      return SelectionState::kNone;
    return node->GetLayoutObject()->GetSelectionStateForPaint();
  }
  return layout_text.GetSelectionStateForPaint();
}

bool LayoutSelection::IsSelected(const LayoutObject& layout_object) {
  if (const auto* layout_text = DynamicTo<LayoutText>(layout_object))
    return GetSelectionStateFor(*layout_text) != SelectionState::kNone;
  return layout_object.GetSelectionState() != SelectionState::kNone;
}

static inline unsigned ClampOffset(unsigned node_offset,
                                   const LayoutTextFragment& fragment) {
  if (fragment.Start() > node_offset)
    return 0;
  return std::min(node_offset - fragment.Start(), fragment.FragmentLength());
}

static LayoutTextSelectionStatus ComputeSelectionStatusForNode(
    const Text& text,
    SelectionState selection_state,
    std::optional<unsigned> start_offset,
    std::optional<unsigned> end_offset) {
  switch (selection_state) {
    case SelectionState::kInside:
      return {0, text.length(), SelectionIncludeEnd::kInclude};
    case SelectionState::kStart:
      return {start_offset.value(), text.length(),
              SelectionIncludeEnd::kInclude};
    case SelectionState::kEnd:
      return {0, end_offset.value(), SelectionIncludeEnd::kNotInclude};
    case SelectionState::kStartAndEnd:
      return {start_offset.value(), end_offset.value(),
              SelectionIncludeEnd::kNotInclude};
    default:
      NOTREACHED_IN_MIGRATION();
      return {0, 0, SelectionIncludeEnd::kNotInclude};
  }
}

LayoutTextSelectionStatus LayoutSelection::ComputeSelectionStatus(
    const LayoutText& layout_text) const {
  AssertIsValid();
  const SelectionState selection_state = GetSelectionStateFor(layout_text);
  if (selection_state == SelectionState::kNone)
    return {0, 0, SelectionIncludeEnd::kNotInclude};
  if (Text* text = AssociatedTextNode(layout_text)) {
    const LayoutTextSelectionStatus text_status = ComputeSelectionStatusForNode(
        *text, selection_state, paint_range_->start_offset,
        paint_range_->end_offset);
    if (const auto* text_fragment =
            DynamicTo<LayoutTextFragment>(layout_text)) {
      return {ClampOffset(text_status.start, *text_fragment),
              ClampOffset(text_status.end, *text_fragment),
              text_status.include_end};
    }
    return text_status;
  }
  // TODO(yoichio): This is really weird legacy behavior. Remove this.
  if (layout_text.IsBR() && selection_state == SelectionState::kEnd)
    return {0, 0, SelectionIncludeEnd::kNotInclude};
  return {0, layout_text.TransformedTextLength(),
          SelectionIncludeEnd::kInclude};
}

LayoutTextSelectionStatus FrameSelection::ComputeLayoutSelectionStatus(
    const LayoutText& text) const {
  return layout_selection_->ComputeSelectionStatus(text);
}

// FrameSelection holds selection offsets in layout block flow at
// LayoutSelection::Commit() if selection starts/ends within Text that
// each LayoutObject::SelectionState indicates.
// These offset can be out of fragment because SelectionState is of each
// LayoutText and not of each fragment for it.
LayoutSelectionStatus LayoutSelection::ComputeSelectionStatus(
    const InlineCursor& cursor) const {
  const InlineCursorPosition& current = cursor.Current();
  if (!current.IsLayoutGeneratedText())
    return ComputeSelectionStatus(cursor, current.TextOffset());

  // We don't paint selection on ellipsis.
  if (current.IsEllipsis())
    return {0, 0, SelectSoftLineBreak::kNotSelected};

  // Layout-generated text does not have corresponding text in the DOM. Find if
  // the previous character is selected. This is a soft-hyphen character if the
  // hyphen is generated from it, or the character before the hyphen if
  // automatic hyphenation.
  const unsigned offset = current->StartOffsetInContainer(cursor);
  DCHECK_GT(offset, 0u);
  LayoutSelectionStatus status =
      ComputeSelectionStatus(cursor, {offset - 1, offset});
  if (!status.HasValidRange())
    return status;
  // Make |LayoutSelectionStatus| to select the whole text of the hyphen.
  status.start = 0;
  status.end = current->TextLength();
  return status;
}

LayoutSelectionStatus LayoutSelection::ComputeSelectionStatus(
    const InlineCursor& cursor,
    const TextOffsetRange& offset) const {
  const unsigned start_offset = offset.start;
  const unsigned end_offset = offset.end;
  switch (GetSelectionStateFor(cursor.Current())) {
    case SelectionState::kStart: {
      const unsigned start_in_block = paint_range_->start_offset.value();
      const bool is_continuous = start_in_block <= end_offset;
      return {ClampOffset(start_in_block, start_offset, end_offset), end_offset,
              (is_continuous && cursor.IsBeforeSoftLineBreak())
                  ? SelectSoftLineBreak::kSelected
                  : SelectSoftLineBreak::kNotSelected};
    }
    case SelectionState::kEnd: {
      const unsigned end_in_block = paint_range_->end_offset.value();
      const unsigned end_in_fragment =
          ClampOffset(end_in_block, start_offset, end_offset);
      const bool is_continuous = end_offset < end_in_block;
      return {start_offset, end_in_fragment,
              (is_continuous && cursor.IsBeforeSoftLineBreak())
                  ? SelectSoftLineBreak::kSelected
                  : SelectSoftLineBreak::kNotSelected};
    }
    case SelectionState::kStartAndEnd: {
      const unsigned start_in_block = paint_range_->start_offset.value();
      const unsigned end_in_block = paint_range_->end_offset.value();
      const unsigned end_in_fragment =
          ClampOffset(end_in_block, start_offset, end_offset);
      const bool is_continuous =
          start_in_block <= end_offset && end_offset < end_in_block;
      return {ClampOffset(start_in_block, start_offset, end_offset),
              end_in_fragment,
              (is_continuous && cursor.IsBeforeSoftLineBreak())
                  ? SelectSoftLineBreak::kSelected
                  : SelectSoftLineBreak::kNotSelected};
    }
    case SelectionState::kInside: {
      return {start_offset, end_offset,
              cursor.IsBeforeSoftLineBreak()
                  ? SelectSoftLineBreak::kSelected
                  : SelectSoftLineBreak::kNotSelected};
    }
    default:
      // This block is not included in selection.
      return {0, 0, SelectSoftLineBreak::kNotSelected};
  }
}

// Given |state| that describes the provided offsets relationship to the
// |paint_range_| (and thus which comparisons are valid), returns a
// SelectionState that reflects where the endpoints of the selection fall,
// relative to the range expressed by the offsets.
SelectionState LayoutSelection::ComputeSelectionStateFromOffsets(
    SelectionState state,
    unsigned start_offset,
    unsigned end_offset) const {
  switch (state) {
    case SelectionState::kStart: {
      const unsigned start_in_block =
          paint_range_->start_offset.value_or(start_offset);
      return start_offset <= start_in_block && start_in_block <= end_offset
                 ? SelectionState::kStart
                 : SelectionState::kNone;
    }
    case SelectionState::kEnd: {
      const unsigned end_in_block =
          paint_range_->end_offset.value_or(end_offset);
      return start_offset <= end_in_block && end_in_block <= end_offset
                 ? SelectionState::kEnd
                 : SelectionState::kNone;
    }
    case SelectionState::kStartAndEnd: {
      const unsigned start_in_block =
          paint_range_->start_offset.value_or(start_offset);
      const unsigned end_in_block =
          paint_range_->end_offset.value_or(end_offset);
      const bool is_start_in_current_cursor =
          start_offset <= start_in_block && start_in_block <= end_offset;
      const bool is_end_in_current_cursor =
          start_offset <= end_in_block && end_in_block <= end_offset;
      if (is_start_in_current_cursor && is_end_in_current_cursor)
        return SelectionState::kStartAndEnd;
      else if (is_start_in_current_cursor)
        return SelectionState::kStart;
      else if (is_end_in_current_cursor)
        return SelectionState::kEnd;
      else
        return SelectionState::kInside;
    }
    case SelectionState::kInside: {
      return SelectionState::kInside;
    }
    default:
      return SelectionState::kNone;
  }
}

SelectionState LayoutSelection::ComputePaintingSelectionStateForCursor(
    const InlineCursorPosition& position) const {
  if (!position)
    return SelectionState::kNone;

  DCHECK(position.IsText());

  // Selection on ellipsis is not supported.
  if (position.IsEllipsis())
    return SelectionState::kNone;

  const TextOffsetRange offset = position.TextOffset();
  const unsigned start_offset = offset.start;
  const unsigned end_offset = offset.end;
  // Determine the state of the overall selection, relative to the LayoutObject
  // associated with the current cursor position. This state will allow us know
  // which offset comparisons are valid, and determine if the selection
  // endpoints fall within the current cursor position.

  SelectionState state =
      GetPaintingSelectionStateFor(To<LayoutText>(*position.GetLayoutObject()));
  return ComputeSelectionStateFromOffsets(state, start_offset, end_offset);
}

static void SetSelectionStateForPaint(
    const EphemeralRangeInFlatTree& selection) {
  const Node* start_node = nullptr;
  const Node* end_node = nullptr;
  for (Node& node : selection.Nodes()) {
    LayoutObject* const layout_object = node.GetLayoutObject();
    if (!layout_object || !layout_object->CanBeSelectionLeaf())
      continue;

    // If there's a LayoutText without a FragmentItem, it won't be painted, so
    // we skip the object for the purposes of determining the selection state
    // during paint (i.e. if selection starts or ends on one of these, the
    // adjacent node should be the one to get the actual kStart/kEnd state).
    if (const auto* layout_text = DynamicTo<LayoutText>(layout_object)) {
      if (!layout_text->FirstInlineFragmentItemIndex())
        continue;
    }

    if (!start_node) {
      DCHECK(!end_node);
      start_node = end_node = &node;
      continue;
    }

    if (end_node != start_node) {
      end_node->GetLayoutObject()->SetSelectionStateForPaint(
          SelectionState::kInside);
    }
    end_node = &node;
  }

  // No valid LayOutObject found.
  if (!start_node) {
    DCHECK(!end_node);
    return;
  }

  if (start_node == end_node) {
    start_node->GetLayoutObject()->SetSelectionStateForPaint(
        SelectionState::kStartAndEnd);
  } else {
    start_node->GetLayoutObject()->SetSelectionStateForPaint(
        SelectionState::kStart);
    end_node->GetLayoutObject()->SetSelectionStateForPaint(
        SelectionState::kEnd);
  }
}

static NewPaintRangeAndSelectedNodes CalcSelectionRangeAndSetSelectionState(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  if (selection_in_dom.IsNone())
    return {};

  const EphemeralRangeInFlatTree& selection =
      CalcSelectionInFlatTree(frame_selection);
  if (selection.IsCollapsed() || frame_selection.IsHidden())
    return {};

  // Find first/last Node which has a visible LayoutObject while
  // marking SelectionState and collecting invalidation candidate LayoutObjects.
  const Node* start_node = nullptr;
  const Node* end_node = nullptr;
  HeapHashSet<Member<const Node>> selected_objects;
  for (Node& node : selection.Nodes()) {
    LayoutObject* const layout_object = node.GetLayoutObject();
    if (!layout_object || !layout_object->CanBeSelectionLeaf())
      continue;

    if (!start_node) {
      DCHECK(!end_node);
      start_node = end_node = &node;
      continue;
    }

    // In this loop, |end_node| is pointing current last candidate
    // LayoutObject and if it is not start and we find next, we mark the
    // current one as kInside.
    if (end_node != start_node) {
      SetSelectionStateIfNeeded(*end_node, SelectionState::kInside);
      selected_objects.insert(end_node);
    }
    end_node = &node;
  }

  // No valid LayOutObject found.
  if (!start_node) {
    DCHECK(!end_node);
    return {};
  }

  SetSelectionStateForPaint(selection);

  // Compute offset. It has value iff start/end is text.
  const std::optional<unsigned> start_offset = ComputeStartOffset(
      *start_node, selection.StartPosition().ToOffsetInAnchor());
  const std::optional<unsigned> end_offset =
      ComputeEndOffset(*end_node, selection.EndPosition().ToOffsetInAnchor());
  if (start_node == end_node) {
    SetSelectionStateIfNeeded(*start_node, SelectionState::kStartAndEnd);
    selected_objects.insert(start_node);
  } else {
    SetSelectionStateIfNeeded(*start_node, SelectionState::kStart);
    selected_objects.insert(start_node);
    SetSelectionStateIfNeeded(*end_node, SelectionState::kEnd);
    selected_objects.insert(end_node);
  }

  SelectionPaintRange* new_range = MakeGarbageCollected<SelectionPaintRange>(
      *start_node, start_offset, *end_node, end_offset);
  return {ComputeNewPaintRange(*new_range), std::move(selected_objects)};
}

void LayoutSelection::SetHasPendingSelection() {
  has_pending_selection_ = true;
}

void LayoutSelection::Commit() {
  if (!has_pending_selection_)
    return;
  has_pending_selection_ = false;

  DCHECK(!frame_selection_->GetDocument().NeedsLayoutTreeUpdate());
  DCHECK_GE(frame_selection_->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_selection_->GetDocument().Lifecycle());

  const OldSelectedNodes& old_selected_objects = ResetOldSelectedNodes(
      frame_selection_->GetDocument(), paint_range_->start_offset,
      paint_range_->end_offset);
  const NewPaintRangeAndSelectedNodes& new_range =
      CalcSelectionRangeAndSetSelectionState(*frame_selection_);
  new_range.AssertSanity();
  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());
  SetShouldInvalidateSelection(new_range, old_selected_objects);

  paint_range_ = new_range.paint_range;
}

void LayoutSelection::ContextDestroyed() {
  has_pending_selection_ = false;
  paint_range_->start_node = nullptr;
  paint_range_->start_offset = std::nullopt;
  paint_range_->end_node = nullptr;
  paint_range_->end_offset = std::nullopt;
}

static PhysicalRect SelectionRectForLayoutObject(const LayoutObject* object) {
  if (!object->IsRooted())
    return PhysicalRect();

  if (!object->CanUpdateSelectionOnRootLineBoxes())
    return PhysicalRect();

  return object->AbsoluteSelectionRect();
}

template <typename Visitor>
static void VisitLayoutObjectsOf(const Node& node, Visitor* visitor) {
  LayoutObject* layout_object = node.GetLayoutObject();
  if (!layout_object)
    return;
  if (layout_object->GetSelectionState() == SelectionState::kContain)
    return;
  if (LayoutTextFragment* first_letter = FirstLetterPartFor(layout_object))
    visitor->Visit(first_letter);
  visitor->Visit(layout_object);
}

gfx::Rect LayoutSelection::AbsoluteSelectionBounds() {
  Commit();
  if (paint_range_->IsNull())
    return gfx::Rect();

  // Create a single bounding box rect that encloses the whole selection.
  class SelectionBoundsVisitor {
    STACK_ALLOCATED();

   public:
    void Visit(const Node& node) { VisitLayoutObjectsOf(node, this); }
    void Visit(LayoutObject* layout_object) {
      selected_rect.Unite(SelectionRectForLayoutObject(layout_object));
    }
    PhysicalRect selected_rect;
  } visitor;
  VisitSelectedInclusiveDescendantsOf(frame_selection_->GetDocument(),
                                      &visitor);
  return ToPixelSnappedRect(visitor.selected_rect);
}

void LayoutSelection::InvalidateStyleAndPaintForSelection() {
  if (paint_range_->IsNull())
    return;

  class InvalidatingVisitor {
    STACK_ALLOCATED();

   public:
    void Visit(Node& node) {
      if (!node.GetLayoutObject()) {
        return;
      }

      // Invalidate style to force an update to ::selection pseudo
      // elements so that ::selection::inactive-window style is applied
      // (or removed).
      if (auto* this_element = DynamicTo<Element>(node)) {
        const ComputedStyle* element_style = node.GetComputedStyle();
        if (element_style &&
            element_style->HasPseudoElementStyle(kPseudoIdSelection)) {
          node.SetNeedsStyleRecalc(
              kLocalStyleChange,
              StyleChangeReasonForTracing::CreateWithExtraData(
                  style_change_reason::kPseudoClass,
                  style_change_extra_data::g_active));
          this_element->PseudoStateChanged(CSSSelector::kPseudoSelection);
        }
      }

      VisitLayoutObjectsOf(node, this);
    }
    void Visit(LayoutObject* layout_object) {
      layout_object->SetShouldInvalidateSelection();
    }
  } visitor;
  VisitSelectedInclusiveDescendantsOf(frame_selection_->GetDocument(),
                                      &visitor);
}

void LayoutSelection::Trace(Visitor* visitor) const {
  visitor->Trace(frame_selection_);
  visitor->Trace(paint_range_);
}

void PrintSelectionStatus(std::ostream& ostream, const Node& node) {
  ostream << (void*)&node;
  if (node.IsTextNode())
    ostream << "#text";
  else if (const auto* element = DynamicTo<Element>(node))
    ostream << element->tagName().Utf8();
  LayoutObject* layout_object = node.GetLayoutObject();
  if (!layout_object) {
    ostream << " <null LayoutObject>";
    return;
  }
  ostream << ' ' << layout_object->GetSelectionState();
}

#if DCHECK_IS_ON()
std::ostream& operator<<(std::ostream& ostream,
                         const std::optional<unsigned>& offset) {
  if (offset.has_value())
    ostream << offset.value();
  else
    ostream << "<nullopt>";
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectionPaintRange& range) {
  ostream << range.start_node << ": " << range.start_offset << ", "
          << range.end_node << ": " << range.end_offset;
  return ostream;
}

std::ostream& operator<<(
    std::ostream& ostream,
    const HeapHashMap<Member<const Node>, SelectionState>& map) {
  ostream << "[";
  const char* comma = "";
  for (const auto& key_value : map) {
    const Node* const node = key_value.key;
    const SelectionState old_state = key_value.value;
    ostream << comma << node << "." << old_state;
    comma = ", ";
  }
  ostream << "]";
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const OldSelectedNodes& old_node) {
  ostream << old_node.paint_range << ". " << old_node.selected_map;
  return ostream;
}

void PrintOldSelectedNodes(const OldSelectedNodes& old_node) {
  std::stringstream stream;
  stream << std::endl << old_node;
  LOG(INFO) << stream.str();
}

std::ostream& operator<<(
    std::ostream& ostream,
    const HeapHashSet<Member<const Node>>& selected_objects) {
  ostream << "[";
  const char* comma = "";
  for (const Node* node : selected_objects) {
    ostream << comma;
    PrintSelectionStatus(ostream, *node);
    comma = ", ";
  }
  ostream << "]";
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const NewPaintRangeAndSelectedNodes& new_range) {
  ostream << new_range.paint_range << ". " << new_range.selected_objects;
  return ostream;
}

void PrintSelectedNodes(const NewPaintRangeAndSelectedNodes& new_range) {
  std::stringstream stream;
  stream << std::endl << new_range;
  LOG(INFO) << stream.str();
}

void PrintSelectionStateInDocument(const FrameSelection& selection) {
  class PrintVisitor {
    STACK_ALLOCATED();

   public:
    void Visit(const Node& node) { PrintSelectionStatus(stream, node); }
    std::stringstream stream;
  } visitor;
  VisitSelectedInclusiveDescendantsOf(selection.GetDocument(), &visitor);
  LOG(INFO) << std::endl << visitor.stream.str();
}
#endif

}  // namespace blink
