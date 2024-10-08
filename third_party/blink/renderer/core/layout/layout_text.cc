/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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
 *
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/layout_text.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/text_diff_range.h"
#include "third_party/blink/renderer/core/editing/bidi_adjustment.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_span.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/hyphenation.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

namespace {

struct SameSizeAsLayoutText : public LayoutObject {
  uint32_t bitfields : 4;
  DOMNodeId node_id;
  String text;
  LogicalOffset previous_starting_point;
  InlineItemSpan inline_items;
  wtf_size_t first_fragment_item_index_;
};

ASSERT_SIZE(LayoutText, SameSizeAsLayoutText);

class SecureTextTimer;
typedef HeapHashMap<WeakMember<const LayoutText>, Member<SecureTextTimer>>
    SecureTextTimerMap;
static SecureTextTimerMap& GetSecureTextTimers() {
  DEFINE_STATIC_LOCAL(const Persistent<SecureTextTimerMap>, map,
                      (MakeGarbageCollected<SecureTextTimerMap>()));
  return *map;
}

class SecureTextTimer final : public GarbageCollected<SecureTextTimer>,
                              public TimerBase {
 public:
  explicit SecureTextTimer(LayoutText* layout_text)
      : TimerBase(layout_text->GetDocument().GetTaskRunner(
            TaskType::kUserInteraction)),
        layout_text_(layout_text),
        last_typed_character_offset_(-1) {}

  static SecureTextTimer* ActiveInstanceFor(const LayoutText* layout_text) {
    auto it = GetSecureTextTimers().find(layout_text);
    if (it != GetSecureTextTimers().end()) {
      SecureTextTimer* secure_text_timer = it->value;
      if (secure_text_timer && secure_text_timer->IsActive()) {
        return secure_text_timer;
      }
    }
    return nullptr;
  }

  void RestartWithNewText(unsigned last_typed_character_offset) {
    last_typed_character_offset_ = last_typed_character_offset;
    if (Settings* settings = layout_text_->GetDocument().GetSettings()) {
      StartOneShot(base::Seconds(settings->GetPasswordEchoDurationInSeconds()),
                   FROM_HERE);
    }
  }
  void Invalidate() { last_typed_character_offset_ = -1; }
  unsigned LastTypedCharacterOffset() { return last_typed_character_offset_; }

  void Trace(Visitor* visitor) const { visitor->Trace(layout_text_); }

 private:
  void Fired() override {
    DCHECK(GetSecureTextTimers().Contains(layout_text_));
    // Forcing setting text as it may be masked later
    layout_text_->ForceSetText(layout_text_->TransformedText());
  }

  Member<LayoutText> layout_text_;
  int last_typed_character_offset_;
};

class SelectionDisplayItemClient
    : public GarbageCollected<SelectionDisplayItemClient>,
      public DisplayItemClient {
 public:
  String DebugName() const final { return "Selection"; }
  void Trace(Visitor* visitor) const override {
    DisplayItemClient::Trace(visitor);
  }
};

using SelectionDisplayItemClientMap =
    HeapHashMap<WeakMember<const LayoutText>,
                Member<SelectionDisplayItemClient>>;
SelectionDisplayItemClientMap& GetSelectionDisplayItemClientMap() {
  DEFINE_STATIC_LOCAL(Persistent<SelectionDisplayItemClientMap>, map,
                      (MakeGarbageCollected<SelectionDisplayItemClientMap>()));
  return *map;
}

}  // anonymous namespace

LayoutText::LayoutText(Node* node, String str)
    : LayoutObject(node),
      valid_ng_items_(false),
      has_bidi_control_items_(false),
      is_text_fragment_(false),
      has_abstract_inline_text_box_(false),
      text_(std::move(str)) {
  DCHECK(text_);
  DCHECK(!node || !node->IsDocumentNode());

  if (node)
    GetFrameView()->IncrementVisuallyNonEmptyCharacterCount(text_.length());
}

void LayoutText::Trace(Visitor* visitor) const {
  visitor->Trace(inline_items_);
  LayoutObject::Trace(visitor);
}

LayoutText* LayoutText::CreateEmptyAnonymous(Document& doc,
                                             const ComputedStyle* style) {
  auto* text = MakeGarbageCollected<LayoutText>(nullptr, StringImpl::empty_);
  text->SetDocumentForAnonymous(&doc);
  text->SetStyle(style);
  return text;
}

bool LayoutText::IsWordBreak() const {
  NOT_DESTROYED();
  return false;
}

void LayoutText::StyleWillChange(StyleDifference diff,
                                 const ComputedStyle& new_style) {
  NOT_DESTROYED();

  if (const ComputedStyle* current_style = Style()) {
    // Process accessibility for style changes that affect text.
    if (current_style->UsedVisibility() != new_style.UsedVisibility() ||
        current_style->IsInert() != new_style.IsInert()) {
      if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
        cache->StyleChanged(this, /*visibility_or_inertness_changed*/ true);
      }
    }
  }
}

void LayoutText::StyleDidChange(StyleDifference diff,
                                const ComputedStyle* old_style) {
  NOT_DESTROYED();
  // There is no need to ever schedule paint invalidations from a style change
  // of a text run, since we already did this for the parent of the text run.
  // We do have to schedule layouts, though, since a style change can force us
  // to need to relayout.
  if (diff.NeedsFullLayout()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kStyleChange);
  }

  const ComputedStyle& new_style = StyleRef();
  ETextTransform old_transform =
      old_style ? old_style->TextTransform() : ETextTransform::kNone;
  ETextSecurity old_security =
      old_style ? old_style->TextSecurity() : ETextSecurity::kNone;
  if (old_transform != new_style.TextTransform() ||
      old_security != new_style.TextSecurity()) {
    TransformAndSecureOriginalText();
  } else if (old_transform == new_style.TextTransform() &&
             new_style.TextTransform() != ETextTransform::kNone &&
             old_style->Locale() != new_style.Locale()) {
    TransformAndSecureOriginalText();
  }

  // This is an optimization that kicks off font load before layout.
  if (!TransformedText().ContainsOnlyWhitespaceOrEmpty()) {
    new_style.GetFont().WillUseFontData(TransformedText());
  }

  TextAutosizer* text_autosizer = GetDocument().GetTextAutosizer();
  if (!old_style && text_autosizer)
    text_autosizer->Record(this);

  if (diff.NeedsReshape()) {
    valid_ng_items_ = false;
    SetNeedsCollectInlines();
  }

  SetHorizontalWritingMode(new_style.IsHorizontalWritingMode());
}

void LayoutText::RemoveAndDestroyTextBoxes() {
  NOT_DESTROYED();
  if (!DocumentBeingDestroyed()) {
    if (Parent()) {
      Parent()->DirtyLinesFromChangedChild(this);
    }
    if (FirstInlineFragmentItemIndex()) {
      DetachAbstractInlineTextBoxesIfNeeded();
      FragmentItems::LayoutObjectWillBeDestroyed(*this);
      ClearFirstInlineFragmentItemIndex();
    }
  } else if (FirstInlineFragmentItemIndex()) {
    DetachAbstractInlineTextBoxesIfNeeded();
    ClearFirstInlineFragmentItemIndex();
  }
  DeleteTextBoxes();
}

void LayoutText::WillBeDestroyed() {
  NOT_DESTROYED();

  if (SecureTextTimer* timer = GetSecureTextTimers().Take(this))
    timer->Stop();

  GetSelectionDisplayItemClientMap().erase(this);

  if (node_id_ != kInvalidDOMNodeId) {
    if (auto* manager = GetOrResetContentCaptureManager())
      manager->OnLayoutTextWillBeDestroyed(*GetNode());
    node_id_ = kInvalidDOMNodeId;
  }

  RemoveAndDestroyTextBoxes();
  LayoutObject::WillBeDestroyed();
  valid_ng_items_ = false;

#if DCHECK_IS_ON()
  if (IsInLayoutNGInlineFormattingContext())
    DCHECK(!first_fragment_item_index_);
#endif
}

void LayoutText::DeleteTextBoxes() {
  NOT_DESTROYED();
  DetachAbstractInlineTextBoxesIfNeeded();
}

void LayoutText::DetachAbstractInlineTextBoxes() {
  NOT_DESTROYED();
  // TODO(layout-dev): Because We should call |WillDestroy()| once for
  // associated fragments, when you reuse fragments, you should construct
  // AbstractInlineTextBox for them.
  DCHECK(has_abstract_inline_text_box_);
  has_abstract_inline_text_box_ = false;
  // TODO(yosin): Make sure we call this function within valid containg block
  // of |this|.
  InlineCursor cursor;
  for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject())
    AbstractInlineTextBox::WillDestroy(cursor);
}

void LayoutText::ClearFirstInlineFragmentItemIndex() {
  NOT_DESTROYED();
  CHECK(IsInLayoutNGInlineFormattingContext()) << *this;
  DetachAbstractInlineTextBoxesIfNeeded();
  first_fragment_item_index_ = 0u;
}

void LayoutText::SetFirstInlineFragmentItemIndex(wtf_size_t index) {
  NOT_DESTROYED();
  CHECK(IsInLayoutNGInlineFormattingContext());
  // TODO(yosin): Call |AbstractInlineTextBox::WillDestroy()|.
  DCHECK_NE(index, 0u);
  DetachAbstractInlineTextBoxesIfNeeded();
  // Changing the first fragment item index causes
  // LayoutText::FirstAbstractInlineTextBox to return a box,
  // so notify the AX object for this LayoutText that it might need to
  // recompute its text child.
  if (index > 0 && first_fragment_item_index_ == 0) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      cache->TextChanged(this);
    }
  }
  first_fragment_item_index_ = index;
}

void LayoutText::InLayoutNGInlineFormattingContextWillChange(bool new_value) {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext())
    ClearFirstInlineFragmentItemIndex();
  else
    DeleteTextBoxes();

  // Because there are no inline boxes associated to this text, we should not
  // have abstract inline text boxes too.
  DCHECK(!has_abstract_inline_text_box_);
}

Vector<LayoutText::TextBoxInfo> LayoutText::GetTextBoxInfo() const {
  NOT_DESTROYED();
  // This function may kick the layout (e.g., |LocalRect()|), but Inspector may
  // call this function outside of the layout phase.
  FontCachePurgePreventer fontCachePurgePreventer;

  Vector<TextBoxInfo> results;
  if (const OffsetMapping* mapping = GetOffsetMapping()) {
    bool in_hidden_for_paint = false;
    InlineCursor cursor;
    cursor.MoveTo(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      // TODO(yosin): We should introduce |FragmentItem::IsTruncated()| to
      // skip them instead of using |IsHiddenForPaint()| with ordering of
      // fragments.
      if (cursor.Current().IsHiddenForPaint()) {
        in_hidden_for_paint = true;
      } else if (in_hidden_for_paint) {
        // Because of we finished original fragments (not painted), we should
        // ignore truncated fragments (actually painted).
        break;
      }
      // We don't put generated texts, e.g. ellipsis, hyphen, etc. not in text
      // content, into results. Note: CSS "content" aren't categorized this.
      if (cursor.Current().IsLayoutGeneratedText())
        continue;
      // When the corresponding DOM range contains collapsed whitespaces, NG
      // produces one fragment but legacy produces multiple text boxes broken at
      // collapsed whitespaces. We break the fragment at collapsed whitespaces
      // to match the legacy output.
      const TextOffsetRange offset = cursor.Current().TextOffset();
      for (const OffsetMappingUnit& unit :
           mapping->GetMappingUnitsForTextContentOffsetRange(offset.start,
                                                             offset.end)) {
        DCHECK_EQ(unit.GetLayoutObject(), this);
        if (unit.GetType() == OffsetMappingUnitType::kCollapsed) {
          continue;
        }
        // [clamped_start, clamped_end] of |fragment| matches a legacy text box.
        const unsigned clamped_start =
            std::max(unit.TextContentStart(), offset.start);
        const unsigned clamped_end =
            std::min(unit.TextContentEnd(), offset.end);
        DCHECK_LT(clamped_start, clamped_end);
        const unsigned box_length = clamped_end - clamped_start;

        // Compute rect of the legacy text box.
        PhysicalRect rect = cursor.CurrentLocalRect(clamped_start, clamped_end);
        rect.offset += cursor.Current().OffsetInContainerFragment();

        // Compute start of the legacy text box.
        if (unit.AssociatedNode()) {
          // In case of |text_| comes from DOM node.
          if (const std::optional<unsigned> box_start = CaretOffsetForPosition(
                  mapping->GetLastPosition(clamped_start))) {
            results.push_back(TextBoxInfo{rect, *box_start, box_length});
            continue;
          }
          NOTREACHED_IN_MIGRATION();
          continue;
        }
        // Handle CSS generated content, e.g. ::before/::after
        const OffsetMappingUnit* const mapping_unit =
            mapping->GetLastMappingUnit(clamped_start);
        DCHECK(mapping_unit) << this << " at " << clamped_start;
        const unsigned dom_offset =
            mapping_unit->ConvertTextContentToLastDOMOffset(clamped_start);
        results.push_back(TextBoxInfo{rect, dom_offset, box_length});
      }
    }
    return results;
  }

  return results;
}

bool LayoutText::HasInlineFragments() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext())
    return first_fragment_item_index_;
  return false;
}

String LayoutText::OriginalText() const {
  NOT_DESTROYED();
  auto* text_node = DynamicTo<Text>(GetNode());
  return text_node ? text_node->data() : String();
}

unsigned LayoutText::OriginalTextLength() const {
  NOT_DESTROYED();
  DCHECK(!IsBR());
  return OriginalText().length();
}

String LayoutText::PlainText() const {
  NOT_DESTROYED();
  if (GetNode()) {
    if (const OffsetMapping* mapping = GetOffsetMapping()) {
      StringBuilder result;
      for (const OffsetMappingUnit& unit :
           mapping->GetMappingUnitsForNode(*GetNode())) {
        result.Append(
            StringView(mapping->GetText(), unit.TextContentStart(),
                       unit.TextContentEnd() - unit.TextContentStart()));
      }
      return result.ToString();
    }
    // TODO(crbug.com/591099): Remove this branch when legacy layout is removed.
    return blink::PlainText(EphemeralRange::RangeOfContents(*GetNode()));
  }

  // FIXME: this is just a stopgap until TextIterator is adapted to support
  // generated text.
  StringBuilder plain_text_builder;
  unsigned last_end_offset = 0;
  for (const auto& text_box : GetTextBoxInfo()) {
    if (!text_box.dom_length)
      continue;

    // Append a trailing space of the last |text_box| if it was collapsed.
    const unsigned end_offset = text_box.dom_start_offset + text_box.dom_length;
    if (last_end_offset && text_box.dom_start_offset > last_end_offset &&
        !IsASCIISpace(text_[end_offset - 1])) {
      plain_text_builder.Append(kSpaceCharacter);
    }
    last_end_offset = end_offset;

    String text =
        text_.Substring(text_box.dom_start_offset, text_box.dom_length)
            .SimplifyWhiteSpace(WTF::kDoNotStripWhiteSpace);
    plain_text_builder.Append(text);
  }
  return plain_text_builder.ToString();
}

template <typename PhysicalRectCollector>
void LayoutText::CollectLineBoxRects(const PhysicalRectCollector& yield,
                                     ClippingOption option) const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveTo(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      if (option != ClippingOption::kNoClipping) [[unlikely]] {
        DCHECK_EQ(option, ClippingOption::kClipToEllipsis);
        if (cursor.Current().IsHiddenForPaint())
          continue;
      }
      yield(cursor.Current().RectInContainerFragment());
    }
    return;
  }
}

void LayoutText::QuadsInAncestorInternal(Vector<gfx::QuadF>& quads,
                                         const LayoutBoxModelObject* ancestor,
                                         MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  CollectLineBoxRects([this, &quads, ancestor, mode](const PhysicalRect& r) {
    quads.push_back(LocalRectToAncestorQuad(r, ancestor, mode));
  });
}

bool LayoutText::MapDOMOffsetToTextContentOffset(const OffsetMapping& mapping,
                                                 unsigned* start,
                                                 unsigned* end) const {
  NOT_DESTROYED();
  DCHECK_LE(*start, *end);

  // Adjust |start| to the next non-collapsed offset if |start| is collapsed.
  Position start_position =
      PositionForCaretOffset(std::min(*start, OriginalTextLength()));
  Position non_collapsed_start_position =
      mapping.StartOfNextNonCollapsedContent(start_position);

  // If all characters after |start| are collapsed, adjust to the last
  // non-collapsed offset.
  if (non_collapsed_start_position.IsNull()) {
    non_collapsed_start_position =
        mapping.EndOfLastNonCollapsedContent(start_position);

    // If all characters are collapsed, return false.
    if (non_collapsed_start_position.IsNull())
      return false;
  }

  *start = mapping.GetTextContentOffset(non_collapsed_start_position).value();

  // Adjust |end| to the last non-collapsed offset if |end| is collapsed.
  Position end_position =
      PositionForCaretOffset(std::min(*end, OriginalTextLength()));
  Position non_collpased_end_position =
      mapping.EndOfLastNonCollapsedContent(end_position);

  // Note: `non_collpased_{start,end}_position}` can be position before/after
  // non-`Text` node. See http://crbug.com/1389193
  if (non_collpased_end_position.IsNull() ||
      non_collpased_end_position <= non_collapsed_start_position) {
    // If all characters in the range are collapsed, make |end| = |start|.
    *end = *start;
  } else {
    *end = mapping.GetTextContentOffset(non_collpased_end_position).value();
  }

  DCHECK_LE(*start, *end);
  return true;
}

void LayoutText::AbsoluteQuadsForRange(Vector<gfx::QuadF>& quads,
                                       unsigned start,
                                       unsigned end) const {
  NOT_DESTROYED();
  // Work around signed/unsigned issues. This function takes unsigneds, and is
  // often passed UINT_MAX to mean "all the way to the end". InlineTextBox
  // coordinates are unsigneds, so changing this function to take ints causes
  // various internal mismatches. But selectionRect takes ints, and passing
  // UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take
  // unsigneds, but that would cause many ripple effects, so for now we'll just
  // clamp our unsigned parameters to INT_MAX.
  DCHECK(end == UINT_MAX || end <= INT_MAX);
  DCHECK_LE(start, static_cast<unsigned>(INT_MAX));
  start = std::min(start, static_cast<unsigned>(INT_MAX));
  end = std::min(end, static_cast<unsigned>(INT_MAX));

  if (auto* mapping = GetOffsetMapping()) {
    if (!MapDOMOffsetToTextContentOffset(*mapping, &start, &end))
      return;

    const auto* const text_combine = DynamicTo<LayoutTextCombine>(Parent());

    // We don't want to add collapsed (i.e., start == end) quads from text
    // fragments that intersect [start, end] only at the boundary, unless they
    // are the only quads found. For example, when we have
    // - text fragments: ABC  DEF  GHI
    // - text offsets:   012  345  678
    // and input range [3, 6], since fragment "DEF" gives non-collapsed quad,
    // we no longer add quads from "ABC" and "GHI" since they are collapsed.
    // TODO(layout-dev): This heuristic doesn't cover all cases, as we return
    // 2 collapsed quads (instead of 1) for range [3, 3] in the above example.
    bool found_non_collapsed_quad = false;
    Vector<gfx::QuadF, 1> collapsed_quads_candidates;

    // Find fragments that have text for the specified range.
    DCHECK_LE(start, end);
    InlineCursor cursor;
    bool is_last_end_included = false;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      const FragmentItem& item = *cursor.Current();
      DCHECK(item.IsText());
      bool is_collapsed = false;
      PhysicalRect rect;
      if (!item.IsGeneratedText()) {
        const TextOffsetRange& offset = item.TextOffset();
        if (start > offset.end || end < offset.start) {
          is_last_end_included = false;
          continue;
        }
        is_last_end_included = offset.end <= end;
        const unsigned clamped_start = std::max(start, offset.start);
        const unsigned clamped_end = std::min(end, offset.end);
        rect = cursor.CurrentLocalRect(clamped_start, clamped_end);
        is_collapsed = clamped_start >= clamped_end;
      } else if (item.IsEllipsis()) {
        continue;
      } else {
        // Hyphens. Include if the last end was included.
        if (!is_last_end_included)
          continue;
        rect = item.LocalRect();
      }
      if (text_combine) [[unlikely]] {
        rect = text_combine->AdjustRectForBoundingBox(rect);
      }
      gfx::QuadF quad;
      if (const SvgFragmentData* svg_data = item.GetSvgFragmentData()) {
        gfx::RectF float_rect(rect);
        float_rect.Offset(svg_data->rect.OffsetFromOrigin());
        quad = item.BuildSvgTransformForBoundingBox().MapQuad(
            gfx::QuadF(float_rect));
        const float scaling_factor = item.SvgScalingFactor();
        quad.Scale(1 / scaling_factor, 1 / scaling_factor);
        quad = LocalToAbsoluteQuad(quad);
      } else {
        rect.Move(cursor.CurrentOffsetInBlockFlow());
        quad = LocalRectToAbsoluteQuad(rect);
      }
      if (!is_collapsed) {
        quads.push_back(quad);
        found_non_collapsed_quad = true;
      } else {
        collapsed_quads_candidates.push_back(quad);
      }
    }
    if (!found_non_collapsed_quad)
      quads.AppendVector(collapsed_quads_candidates);
    return;
  }
}

gfx::RectF LayoutText::LocalBoundingBoxRectForAccessibility() const {
  NOT_DESTROYED();
  gfx::RectF result;
  CollectLineBoxRects(
      [&result](const PhysicalRect& rect) { result.Union(gfx::RectF(rect)); },
      kClipToEllipsis);
  return result;
}

PositionWithAffinity LayoutText::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  // NG codepath requires |kPrePaintClean|.
  // |SelectionModifier| calls this only in legacy codepath.
  DCHECK(!IsLayoutNGObject() || GetDocument().Lifecycle().GetState() >=
                                    DocumentLifecycle::kPrePaintClean);

  if (IsInLayoutNGInlineFormattingContext()) {
    // Because of Texts in "position:relative" can be outside of line box, we
    // attempt to find a fragment containing |point|.
    // See All/LayoutViewHitTestTest.HitTestHorizontal/* and
    // All/LayoutViewHitTestTest.HitTestVerticalRL/*
    InlineCursor cursor;
    cursor.MoveTo(*this);
    const LayoutBlockFlow* containing_block_flow = cursor.GetLayoutBlockFlow();
    DCHECK(containing_block_flow);
    PhysicalOffset point_in_contents = point;
    if (containing_block_flow->IsScrollContainer()) {
      point_in_contents += PhysicalOffset(
          containing_block_flow->PixelSnappedScrolledContentOffset());
    }
    const auto* const text_combine = DynamicTo<LayoutTextCombine>(Parent());
    const PhysicalBoxFragment* container_fragment = nullptr;
    PhysicalOffset point_in_container_fragment;
    DCHECK(!IsSVGInlineText());
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      DCHECK(&cursor.ContainerFragment());
      if (container_fragment != &cursor.ContainerFragment()) {
        container_fragment = &cursor.ContainerFragment();
        point_in_container_fragment =
            point_in_contents - container_fragment->OffsetFromOwnerLayoutBox();
        if (text_combine) [[unlikely]] {
          point_in_container_fragment =
              text_combine->AdjustOffsetForHitTest(point_in_container_fragment);
        }
      }
      if (!ToEnclosingRect(cursor.Current().RectInContainerFragment())
               .Contains(ToFlooredPoint(point_in_container_fragment)))
        continue;
      if (auto position_with_affinity =
              cursor.PositionForPointInChild(point_in_container_fragment)) {
        // Note: Due by Bidi adjustment, |position_with_affinity| isn't
        // relative to this.
        return AdjustForEditingBoundary(position_with_affinity);
      }
    }
    // Try for leading and trailing spaces between lines.
    return containing_block_flow->PositionForPoint(point);
  }

  return CreatePositionWithAffinity(0);
}

PhysicalRect LayoutText::LocalCaretRect(int caret_offset) const {
  NOT_DESTROYED();
  return PhysicalRect();
}

bool LayoutText::IsAllCollapsibleWhitespace() const {
  NOT_DESTROYED();
  unsigned length = text_.length();
  if (text_.Is8Bit()) {
    for (unsigned i = 0; i < length; ++i) {
      if (!StyleRef().IsCollapsibleWhiteSpace(text_.Characters8()[i])) {
        return false;
      }
    }
    return true;
  }
  for (unsigned i = 0; i < length; ++i) {
    if (!StyleRef().IsCollapsibleWhiteSpace(text_.Characters16()[i])) {
      return false;
    }
  }
  return true;
}

UChar32 LayoutText::FirstCharacterAfterWhitespaceCollapsing() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveTo(*this);
    if (cursor) {
      const StringView text = cursor.Current().Text(cursor);
      return text.length() ? text.CodepointAt(0) : 0;
    }
  }
  return 0;
}

UChar32 LayoutText::LastCharacterAfterWhitespaceCollapsing() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveTo(*this);
    if (cursor) {
      const StringView text = cursor.Current().Text(cursor);
      return text.length() ? text.CodepointAt(text.length() - 1) : 0;
    }
  }
  return 0;
}

PhysicalOffset LayoutText::FirstLineBoxTopLeft() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    // TODO(kojii): Some clients call this against dirty-tree, but NG fragments
    // are not safe to read for dirty-tree. crbug.com/963103
    if (!IsFirstInlineFragmentSafe()) [[unlikely]] {
      return PhysicalOffset();
    }
    InlineCursor cursor;
    cursor.MoveTo(*this);
    return cursor ? cursor.Current().OffsetInContainerFragment()
                  : PhysicalOffset();
  }
  return PhysicalOffset();
}

void LayoutText::LogicalStartingPointAndHeight(
    LogicalOffset& logical_starting_point,
    LayoutUnit& logical_height) const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    cursor.MoveTo(*this);
    if (!cursor)
      return;
    PhysicalOffset physical_offset =
        cursor.Current().OffsetInContainerFragment();
    if (StyleRef().GetWritingDirection().IsHorizontalLtr()) {
      cursor.MoveToLastForSameLayoutObject();
      logical_height = cursor.Current().RectInContainerFragment().Bottom() -
                       physical_offset.top;
      logical_starting_point = {physical_offset.left, physical_offset.top};
      return;
    }
    PhysicalSize outer_size = ContainingBlock()->Size();
    logical_starting_point = physical_offset.ConvertToLogical(
        StyleRef().GetWritingDirection(), outer_size, cursor.Current().Size());
    cursor.MoveToLastForSameLayoutObject();
    PhysicalRect last_physical_rect =
        cursor.Current().RectInContainerFragment();
    LogicalOffset logical_ending_point =
        WritingModeConverter(StyleRef().GetWritingDirection(), outer_size)
            .ToLogical(last_physical_rect)
            .EndOffset();
    logical_height =
        logical_ending_point.block_offset - logical_starting_point.block_offset;
  }
}

void LayoutText::SetTextWithOffset(String text, const TextDiffRange& diff) {
  NOT_DESTROYED();
  if (text_ == text) {
    return;
  }

  if (InlineNode::SetTextWithOffset(this, text, diff)) {
    DCHECK(!NeedsCollectInlines());
    // Prevent |TextDidChange()| to propagate |NeedsCollectInlines|
    SetNeedsCollectInlines(true);
    TextDidChange();
    valid_ng_items_ = true;
    ClearNeedsCollectInlines();
    return;
  }

  // If the text node is empty, dirty the line where new text will be inserted.
  if (!HasInlineFragments() && Parent()) {
    Parent()->DirtyLinesFromChangedChild(this);
  }

  ForceSetText(std::move(text));

  // TODO(layout-dev): Invalidation is currently all or nothing in LayoutNG,
  // this is probably fine for InlineItem reuse as recreating the individual
  // items is relatively cheap. If partial relayout performance improvement are
  // needed partial re-shapes are likely to be sufficient. Revisit as needed.
  valid_ng_items_ = false;
}

void LayoutText::TransformAndSecureOriginalText() {
  NOT_DESTROYED();
  if (String text_to_transform = OriginalText()) {
    ForceSetText(std::move(text_to_transform));
  }
}

static inline bool IsInlineFlowOrEmptyText(const LayoutObject* o) {
  if (o->IsLayoutInline())
    return true;
  if (!o->IsText())
    return false;
  return To<LayoutText>(o)->HasEmptyText();
}

UChar LayoutText::PreviousCharacter() const {
  NOT_DESTROYED();
  // find previous text layoutObject if one exists
  const LayoutObject* previous_text = PreviousInPreOrder();
  for (; previous_text; previous_text = previous_text->PreviousInPreOrder()) {
    if (!IsInlineFlowOrEmptyText(previous_text))
      break;
  }
  UChar prev = kSpaceCharacter;
  if (previous_text && previous_text->IsText()) {
    if (const String& previous_string =
            To<LayoutText>(previous_text)->TransformedText()) {
      prev = previous_string[previous_string.length() - 1];
    }
  }
  return prev;
}

void LayoutText::SetTextInternal(String text) {
  NOT_DESTROYED();
  DCHECK(text);
  text_ = String(std::move(text));
  DCHECK(text_);
  DCHECK(!IsBR() ||
         (TransformedTextLength() == 1 && text_[0] == kNewlineCharacter));
}

String LayoutText::TransformAndSecureText(const String& original,
                                          TextOffsetMap& offset_map) const {
  NOT_DESTROYED();
  if (const ComputedStyle* style = Style()) {
    String transformed =
        style->ApplyTextTransform(original, PreviousCharacter(), &offset_map);

    UChar mask = 0;
    // We use the same characters here as for list markers.
    // See CollectUACounterStyleRules() in ua_counter_style_map.cc.
    switch (style->TextSecurity()) {
      case ETextSecurity::kNone:
        return transformed;
      case ETextSecurity::kCircle:
        mask = kWhiteBulletCharacter;
        break;
      case ETextSecurity::kDisc:
        mask = kBulletCharacter;
        break;
      case ETextSecurity::kSquare:
        mask = kBlackSquareCharacter;
        break;
    }
    auto [masked, secure_map] = SecureText(transformed, mask);
    if (!secure_map.IsEmpty()) {
      offset_map = TextOffsetMap(offset_map, secure_map);
    }
    return masked;
  }
  return original;
}

std::pair<String, TextOffsetMap> LayoutText::SecureText(const String& plain,
                                                        UChar mask) const {
  NOT_DESTROYED();
  if (!plain.length()) {
    return std::make_pair(plain, TextOffsetMap());
  }

  int last_typed_character_offset_to_reveal = -1;
  if (auto* secure_text_timer = SecureTextTimer::ActiveInstanceFor(this)) {
    last_typed_character_offset_to_reveal =
        secure_text_timer->LastTypedCharacterOffset();
  }

  StringBuilder builder;
  // `mask` always needs a 16bit buffer.
  builder.Reserve16BitCapacity(plain.length());
  TextOffsetMap offset_map;
  for (unsigned offset = 0; offset < plain.length();) {
    unsigned cluster_size = LengthOfGraphemeCluster(plain, offset);
    unsigned next_offset = offset + cluster_size;
    if (last_typed_character_offset_to_reveal >= 0) {
      unsigned last_typed_offset =
          base::checked_cast<unsigned>(last_typed_character_offset_to_reveal);
      if (offset <= last_typed_offset && last_typed_offset < next_offset) {
        builder.Append(StringView(plain, offset, cluster_size));
        offset = next_offset;
        continue;
      }
    }
    builder.Append(mask);
    offset = next_offset;
    if (cluster_size != 1) {
      offset_map.Append(offset, builder.length());
    }
  }
  return std::make_pair(builder.ToString(), offset_map);
}

void LayoutText::SetVariableLengthTransformResult(
    wtf_size_t original_length,
    const TextOffsetMap& offset_map) {
  if (offset_map.IsEmpty()) {
    ClearHasVariableLengthTransform();
    return;
  }
  has_variable_length_transform_ = true;
  View()->RegisterVariableLengthTransformResult(*this,
                                                {original_length, offset_map});
}

VariableLengthTransformResult LayoutText::GetVariableLengthTransformResult()
    const {
  return View()->GetVariableLengthTransformResult(*this);
}

void LayoutText::ClearHasVariableLengthTransform() {
  NOT_DESTROYED();
  if (has_variable_length_transform_) {
    View()->UnregisterVariableLengthTransformResult(*this);
  }
  has_variable_length_transform_ = false;
}

void LayoutText::SetTextIfNeeded(String text) {
  NOT_DESTROYED();
  DCHECK(text);

  if (text_ == text) {
    return;
  }
  ForceSetText(std::move(text));
}

void LayoutText::ForceSetText(String text) {
  NOT_DESTROYED();
  DCHECK(text);
  SetTextInternal(std::move(text));
  TextDidChange();
}

void LayoutText::SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
    LayoutInvalidationReasonForTracing reason) {
  auto* const text_combine = DynamicTo<LayoutTextCombine>(Parent());
  if (text_combine) [[unlikely]] {
    // Number of characters in text may change compressed font or scaling of
    // text combine. So, we should invalidate |LayoutNGTextCombine| to repaint.
    text_combine
        ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
            reason);
    return;
  }
  LayoutObject::SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      reason);
}

void LayoutText::TextDidChange() {
  NOT_DESTROYED();
  // If intrinsic_logical_widths_dirty_ of an orphan child is true,
  // LayoutObjectChildList::InsertChildNode() fails to set true to owner.
  // To avoid that, we call SetNeedsLayoutAndIntrinsicWidthsRecalc() only if
  // this LayoutText has parent.
  if (Parent()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kTextChanged);
  }
  TextDidChangeWithoutInvalidation();
}

void LayoutText::TextDidChangeWithoutInvalidation() {
  NOT_DESTROYED();
  TextOffsetMap offset_map;
  wtf_size_t original_length = text_.length();
  text_ = TransformAndSecureText(text_, offset_map);
  SetVariableLengthTransformResult(original_length, offset_map);
  if (auto* secure_text_timer = SecureTextTimer::ActiveInstanceFor(this)) {
    // text_ may be updated later before timer fires. We invalidate the
    // last_typed_character_offset_ to avoid inconsistency.
    secure_text_timer->Invalidate();
  }

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->TextChanged(this);

  TextAutosizer* text_autosizer = GetDocument().GetTextAutosizer();
  if (text_autosizer)
    text_autosizer->Record(this);

  if (HasNodeId()) {
    if (auto* content_capture_manager = GetOrResetContentCaptureManager())
      content_capture_manager->OnNodeTextChanged(*GetNode());
  }

  valid_ng_items_ = false;
  ClearHasNoControlItems();
  SetNeedsCollectInlines();
}

void LayoutText::InvalidateSubtreeLayoutForFontUpdates() {
  NOT_DESTROYED();
  if (IsFontFallbackValid())
    return;

  valid_ng_items_ = false;
  SetNeedsCollectInlines();
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kFontsChanged);
}

PhysicalRect LayoutText::PhysicalLinesBoundingBox() const {
  NOT_DESTROYED();
  PhysicalRect result;
  CollectLineBoxRects(
      [&result](const PhysicalRect& r) { result.UniteIfNonZero(r); });
  // Some callers expect correct offset even if the rect is empty.
  if (result == PhysicalRect())
    result.offset = FirstLineBoxTopLeft();
  // Note: |result.offset| is relative to container fragment.
  const auto* const text_combine = DynamicTo<LayoutTextCombine>(Parent());
  if (text_combine) [[unlikely]] {
    return text_combine->AdjustRectForBoundingBox(result);
  }
  return result;
}

PhysicalRect LayoutText::VisualOverflowRect() const {
  NOT_DESTROYED();
  DCHECK(IsInLayoutNGInlineFormattingContext());
  return FragmentItem::LocalVisualRectFor(*this);
}

PhysicalRect LayoutText::LocalSelectionVisualRect() const {
  NOT_DESTROYED();
  DCHECK(!NeedsLayout());

  if (!IsSelected())
    return PhysicalRect();

  const FrameSelection& frame_selection = GetFrame()->Selection();
  if (IsInLayoutNGInlineFormattingContext()) {
    const auto* svg_inline_text = DynamicTo<LayoutSVGInlineText>(this);
    float scaling_factor =
        svg_inline_text ? svg_inline_text->ScalingFactor() : 1.0f;
    PhysicalRect rect;
    InlineCursor cursor(*FragmentItemsContainer());
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      if (cursor.Current().IsHiddenForPaint())
        continue;
      const LayoutSelectionStatus status =
          frame_selection.ComputeLayoutSelectionStatus(cursor);
      if (status.start == status.end)
        continue;
      PhysicalRect item_rect = cursor.CurrentLocalSelectionRectForText(status);
      if (svg_inline_text) {
        gfx::RectF float_rect(item_rect);
        const FragmentItem& item = *cursor.CurrentItem();
        float_rect.Offset(item.GetSvgFragmentData()->rect.OffsetFromOrigin());
        if (item.HasSvgTransformForBoundingBox()) {
          float_rect =
              item.BuildSvgTransformForBoundingBox().MapRect(float_rect);
        }
        if (scaling_factor != 1.0f)
          float_rect.Scale(1 / scaling_factor);
        item_rect = PhysicalRect::EnclosingRect(float_rect);
      } else {
        item_rect.offset += cursor.Current().OffsetInContainerFragment();
      }
      rect.Unite(item_rect);
    }
    return rect;
  }

  return PhysicalRect();
}

void LayoutText::InvalidateVisualOverflow() {
  DCHECK(IsInLayoutNGInlineFormattingContext());
  InlineCursor cursor;
  for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject())
    cursor.Current()->GetMutableForPainting().InvalidateInkOverflow();
}

const OffsetMapping* LayoutText::GetOffsetMapping() const {
  NOT_DESTROYED();
  return OffsetMapping::GetFor(this);
}

Position LayoutText::PositionForCaretOffset(unsigned offset) const {
  NOT_DESTROYED();
  // ::first-letter handling should be done by LayoutTextFragment override.
  DCHECK(!IsTextFragment());
  // BR handling should be done by LayoutBR override.
  DCHECK(!IsBR());
  // WBR handling should be done by LayoutWordBreak override.
  DCHECK(!IsWordBreak());
  DCHECK_LE(offset, OriginalTextLength());
  const Node* node = GetNode();
  if (!node)
    return Position();
  auto* text_node = To<Text>(node);
  // TODO(layout-dev): Support offset change due to text-transform.
#if DCHECK_IS_ON()
  // Ensures that the clamping hack kicks in only with text-transform.
  if (StyleRef().TextTransform() == ETextTransform::kNone)
    DCHECK_LE(offset, text_node->length());
#endif
  const unsigned clamped_offset = std::min(offset, text_node->length());
  return Position(node, clamped_offset);
}

std::optional<unsigned> LayoutText::CaretOffsetForPosition(
    const Position& position) const {
  NOT_DESTROYED();
  // ::first-letter handling should be done by LayoutTextFragment override.
  DCHECK(!IsTextFragment());
  // BR handling should be done by LayoutBR override.
  DCHECK(!IsBR());
  // WBR handling should be done by LayoutWordBreak override.
  DCHECK(!IsWordBreak());
  if (position.IsNull() || position.AnchorNode() != GetNode())
    return std::nullopt;
  DCHECK(GetNode()->IsTextNode());
  if (position.IsBeforeAnchor())
    return 0;
  if (position.IsAfterAnchor())
    return OriginalTextLength();
  DCHECK(position.IsOffsetInAnchor()) << position;
  DCHECK_LE(position.OffsetInContainerNode(),
            static_cast<int>(OriginalTextLength()))
      << position;
  return position.OffsetInContainerNode();
}

int LayoutText::CaretMinOffset() const {
  NOT_DESTROYED();
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  if (auto* mapping = GetOffsetMapping()) {
    const Position first_position = PositionForCaretOffset(0);
    if (first_position.IsNull())
      return 0;
    std::optional<unsigned> candidate = CaretOffsetForPosition(
        mapping->StartOfNextNonCollapsedContent(first_position));
    // Align with the legacy behavior that 0 is returned if the entire node
    // contains only collapsed whitespaces.
    const bool fully_collapsed =
        !candidate || *candidate == TransformedTextLength();
    return fully_collapsed ? 0 : *candidate;
  }

  return 0;
}

int LayoutText::CaretMaxOffset() const {
  NOT_DESTROYED();
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  const unsigned text_length = OriginalTextLength();
  if (auto* mapping = GetOffsetMapping()) {
    const Position last_position = PositionForCaretOffset(text_length);
    if (last_position.IsNull())
      return text_length;
    std::optional<unsigned> candidate = CaretOffsetForPosition(
        mapping->EndOfLastNonCollapsedContent(last_position));
    // Align with the legacy behavior that |TextLenght()| is returned if the
    // entire node contains only collapsed whitespaces.
    const bool fully_collapsed = !candidate || *candidate == 0u;
    return fully_collapsed ? text_length : *candidate;
  }

  return text_length;
}

unsigned LayoutText::NonCollapsedCaretMaxOffset() const {
  NOT_DESTROYED();
  return OriginalTextLength();
}

unsigned LayoutText::ResolvedTextLength() const {
  NOT_DESTROYED();
  if (auto* mapping = GetOffsetMapping()) {
    const Position start_position = PositionForCaretOffset(0);
    const Position end_position =
        PositionForCaretOffset(NonCollapsedCaretMaxOffset());
    if (start_position.IsNull()) {
      DCHECK(end_position.IsNull()) << end_position;
      return 0;
    }
    DCHECK(end_position.IsNotNull()) << start_position;
    std::optional<unsigned> start =
        mapping->GetTextContentOffset(start_position);
    std::optional<unsigned> end = mapping->GetTextContentOffset(end_position);
    if (!start.has_value() || !end.has_value()) {
      DCHECK(!start.has_value()) << this;
      DCHECK(!end.has_value()) << this;
      return 0;
    }
    DCHECK_LE(*start, *end);
    return *end - *start;
  }

  return 0;
}

bool LayoutText::HasNonCollapsedText() const {
  NOT_DESTROYED();
  if (GetOffsetMapping()) {
    return ResolvedTextLength();
  }
  return false;
}

bool LayoutText::ContainsCaretOffset(int text_offset) const {
  NOT_DESTROYED();
  DCHECK_GE(text_offset, 0);
  if (auto* mapping = GetOffsetMapping()) {
    const int text_length = static_cast<int>(NonCollapsedCaretMaxOffset());
    if (text_offset > text_length) {
      return false;
    }
    const Position position = PositionForCaretOffset(text_offset);
    if (position.IsNull()) {
      return false;
    }
    // Return `true` if the position is not collapsed.
    if (text_offset < text_length &&
        mapping->IsBeforeNonCollapsedContent(position)) {
      return true;
    }
    // The position is collapsed. Return `false` if this is the first character,
    // or the previous character is also collapsed.
    if (!text_offset || !mapping->IsAfterNonCollapsedContent(position)) {
      return false;
    }
    // The previous character isn't collapsed. Return `false` if it's a newline,
    // otherwise `true`.
    if (std::optional<UChar> ch = mapping->GetCharacterBefore(position)) {
      return *ch != kNewlineCharacter;
    }
    // TODO(crbug.com/326745564): It's not clear when the code reaches here, and
    // thus it's not clear whether it should return `true` or `false`.
  }

  return false;
}

bool LayoutText::IsBeforeNonCollapsedCharacter(unsigned text_offset) const {
  NOT_DESTROYED();
  if (auto* mapping = GetOffsetMapping()) {
    if (text_offset >= NonCollapsedCaretMaxOffset()) {
      return false;
    }
    const Position position = PositionForCaretOffset(text_offset);
    if (position.IsNull())
      return false;
    return mapping->IsBeforeNonCollapsedContent(position);
  }

  return false;
}

bool LayoutText::IsAfterNonCollapsedCharacter(unsigned text_offset) const {
  NOT_DESTROYED();
  if (auto* mapping = GetOffsetMapping()) {
    if (!text_offset)
      return false;
    const Position position = PositionForCaretOffset(text_offset);
    if (position.IsNull())
      return false;
    return mapping->IsAfterNonCollapsedContent(position);
  }

  return false;
}

void LayoutText::MomentarilyRevealLastTypedCharacter(
    unsigned last_typed_character_offset) {
  NOT_DESTROYED();
  auto it = GetSecureTextTimers().find(this);
  SecureTextTimer* secure_text_timer =
      it != GetSecureTextTimers().end() ? it->value : nullptr;
  if (!secure_text_timer) {
    secure_text_timer = MakeGarbageCollected<SecureTextTimer>(this);
    GetSecureTextTimers().insert(this, secure_text_timer);
  }
  secure_text_timer->RestartWithNewText(last_typed_character_offset);
}

AbstractInlineTextBox* LayoutText::FirstAbstractInlineTextBox() {
  NOT_DESTROYED();
  DCHECK(IsInLayoutNGInlineFormattingContext());
  InlineCursor cursor;
  cursor.MoveTo(*this);
  return AbstractInlineTextBox::GetOrCreate(cursor);
}

void LayoutText::InvalidatePaint(const PaintInvalidatorContext& context) const {
  NOT_DESTROYED();
  if (ShouldInvalidateSelection() && !IsSelected())
    GetSelectionDisplayItemClientMap().erase(this);
  LayoutObject::InvalidatePaint(context);
}

void LayoutText::InvalidateDisplayItemClients(
    PaintInvalidationReason reason) const {
  NOT_DESTROYED();
  LayoutObject::InvalidateDisplayItemClients(reason);

  if (const auto* selection_client = GetSelectionDisplayItemClient()) {
    ObjectPaintInvalidator(*this).InvalidateDisplayItemClient(*selection_client,
                                                              reason);
  }

#if DCHECK_IS_ON()
  if (IsInLayoutNGInlineFormattingContext()) {
    InlineCursor cursor;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      DCHECK_EQ(cursor.Current().GetDisplayItemClient(), this);
    }
  }
#endif
}

const DisplayItemClient* LayoutText::GetSelectionDisplayItemClient() const {
  NOT_DESTROYED();
  if (!IsInLayoutNGInlineFormattingContext()) [[unlikely]] {
    return nullptr;
  }
  // When |this| is in text-combine box, we should use text-combine box as
  // display client item to paint caret with affine transform.
  const auto* const text_combine = DynamicTo<LayoutTextCombine>(Parent());
  if (text_combine && text_combine->NeedsAffineTransformInPaint())
      [[unlikely]] {
    return text_combine;
  }
  if (!IsSelected())
    return nullptr;
  auto it = GetSelectionDisplayItemClientMap().find(this);
  if (it != GetSelectionDisplayItemClientMap().end())
    return &*it->value;
  return GetSelectionDisplayItemClientMap()
      .insert(this, MakeGarbageCollected<SelectionDisplayItemClient>())
      .stored_value->value.Get();
}

PhysicalRect LayoutText::DebugRect() const {
  NOT_DESTROYED();
  return PhysicalRect(ToEnclosingRect(PhysicalLinesBoundingBox()));
}

DOMNodeId LayoutText::EnsureNodeId() {
  NOT_DESTROYED();
  if (node_id_ == kInvalidDOMNodeId) {
    if (auto* content_capture_manager = GetOrResetContentCaptureManager()) {
      if (auto* node = GetNode()) {
        content_capture_manager->ScheduleTaskIfNeeded(*node);
        node_id_ = node->GetDomNodeId();
      }
    }
  }
  return node_id_;
}

ContentCaptureManager* LayoutText::GetOrResetContentCaptureManager() {
  NOT_DESTROYED();
  if (auto* node = GetNode()) {
    if (auto* frame = node->GetDocument().GetFrame()) {
      return frame->LocalFrameRoot().GetOrResetContentCaptureManager();
    }
  }
  return nullptr;
}

void LayoutText::SetInlineItems(InlineItemsData* data,
                                wtf_size_t begin,
                                wtf_size_t size) {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  for (wtf_size_t i = begin; i < begin + size; i++) {
    DCHECK_EQ(data->items[i].GetLayoutObject(), this);
  }
#endif
  auto* items = GetInlineItems();
  if (!items)
    return;
  valid_ng_items_ = true;
  items->SetItems(data, begin, size);
}

void LayoutText::ClearInlineItems() {
  NOT_DESTROYED();
  has_bidi_control_items_ = false;
  valid_ng_items_ = false;
  if (auto* items = GetInlineItems()) {
    items->Clear();
  }
}

const InlineItemSpan& LayoutText::InlineItems() const {
  NOT_DESTROYED();
  DCHECK(valid_ng_items_);
  DCHECK(GetInlineItems());
  DCHECK(!GetInlineItems()->empty());
  return *GetInlineItems();
}

#if DCHECK_IS_ON()
void LayoutText::RecalcVisualOverflow() {
  // We should never reach here, because |PaintLayer| calls
  // |RecalcVisualOverflow| for each layer, and the containing |LayoutObject|
  // should recalculate its |FragmentItem|s without traversing descendant
  // |LayoutObject|s.
  if (IsInline() && IsInLayoutNGInlineFormattingContext())
    NOTREACHED_IN_MIGRATION();

  LayoutObject::RecalcVisualOverflow();
}
#endif

}  // namespace blink
