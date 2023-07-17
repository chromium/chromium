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

#include "third_party/blink/renderer/core/layout/layout_text.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/text.h"
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
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_span.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/hyphenation.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

namespace {

struct SameSizeAsLayoutText : public LayoutObject {
  uint32_t bitfields : 12;
  DOMNodeId node_id;
  float widths[4];
  String text;
  LogicalOffset previous_starting_point;
  NGInlineItemSpan inline_items;
  wtf_size_t first_fragment_item_index_;
};

ASSERT_SIZE(LayoutText, SameSizeAsLayoutText);

class SecureTextTimer;
typedef HeapHashMap<WeakMember<LayoutText>, Member<SecureTextTimer>>
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
    layout_text_->ForceSetText(layout_text_->GetText());
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
      has_tab_(false),
      lines_dirty_(false),
      valid_ng_items_(false),
      has_bidi_control_items_(false),
      contains_reversed_text_(false),
      known_to_have_no_overflow_and_no_fallback_fonts_(false),
      contains_only_whitespace_or_nbsp_(
          static_cast<unsigned>(OnlyWhitespaceOrNbsp::kUnknown)),
      is_text_fragment_(false),
      has_abstract_inline_text_box_(false),
      min_width_(-1),
      max_width_(-1),
      first_line_min_width_(0),
      last_line_line_min_width_(0),
      text_(std::move(str)) {
  DCHECK(text_);
  DCHECK(!node || !node->IsDocumentNode());

  SetIsText();

  if (node)
    GetFrameView()->IncrementVisuallyNonEmptyCharacterCount(text_.length());

  // Call GetSecureTextTimers() and GetSelectionDisplayItemClientMap() to ensure
  // map exists. They are called in pre-finalizer where allocation is not
  // allowed.
  // TODO(yukiy): Remove these if FormattedTextRun::Dispose() can be
  // removed.
  GetSecureTextTimers();
  GetSelectionDisplayItemClientMap();
}

void LayoutText::Trace(Visitor* visitor) const {
  visitor->Trace(inline_items_);
  LayoutObject::Trace(visitor);
}

LayoutText* LayoutText::CreateEmptyAnonymous(
    Document& doc,
    scoped_refptr<const ComputedStyle> style) {
  auto* text = MakeGarbageCollected<LayoutText>(nullptr, StringImpl::empty_);
  text->SetDocumentForAnonymous(&doc);
  text->SetStyle(std::move(style));
  return text;
}

LayoutText* LayoutText::CreateAnonymousForFormattedText(
    Document& doc,
    scoped_refptr<const ComputedStyle> style,
    String text) {
  auto* layout_text =
      MakeGarbageCollected<LayoutText>(nullptr, std::move(text));
  layout_text->SetDocumentForAnonymous(&doc);
  layout_text->SetStyleInternal(std::move(style));
  return layout_text;
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
    if (current_style->Visibility() != new_style.Visibility() ||
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
    known_to_have_no_overflow_and_no_fallback_fonts_ = false;
  }

  const ComputedStyle& new_style = StyleRef();
  ETextTransform old_transform =
      old_style ? old_style->TextTransform() : ETextTransform::kNone;
  ETextSecurity old_security =
      old_style ? old_style->TextSecurity() : ETextSecurity::kNone;
  if (old_transform != new_style.TextTransform() ||
      old_security != new_style.TextSecurity())
    TransformText();

  // This is an optimization that kicks off font load before layout.
  if (!GetText().ContainsOnlyWhitespaceOrEmpty())
    new_style.GetFont().WillUseFontData(GetText());

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
      NGFragmentItems::LayoutObjectWillBeDestroyed(*this);
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
  // NGAbstractInlineTextBox for them.
  DCHECK(has_abstract_inline_text_box_);
  has_abstract_inline_text_box_ = false;
  // TODO(yosin): Make sure we call this function within valid containg block
  // of |this|.
  NGInlineCursor cursor;
  for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject())
    NGAbstractInlineTextBox::WillDestroy(cursor);
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
  // TODO(yosin): Call |NGAbstractInlineTextBox::WillDestroy()|.
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
  if (const NGOffsetMapping* mapping = GetNGOffsetMapping()) {
    bool in_hidden_for_paint = false;
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      // TODO(yosin): We should introduce |NGFragmentItem::IsTruncated()| to
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
      const NGTextOffsetRange offset = cursor.Current().TextOffset();
      for (const NGOffsetMappingUnit& unit :
           mapping->GetMappingUnitsForTextContentOffsetRange(offset.start,
                                                             offset.end)) {
        DCHECK_EQ(unit.GetLayoutObject(), this);
        if (unit.GetType() == NGOffsetMappingUnitType::kCollapsed)
          continue;
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
          if (const absl::optional<unsigned> box_start = CaretOffsetForPosition(
                  mapping->GetLastPosition(clamped_start))) {
            results.push_back(TextBoxInfo{rect, *box_start, box_length});
            continue;
          }
          NOTREACHED();
          continue;
        }
        // Handle CSS generated content, e.g. ::before/::after
        const NGOffsetMappingUnit* const mapping_unit =
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

String LayoutText::PlainText() const {
  NOT_DESTROYED();
  if (GetNode()) {
    if (const NGOffsetMapping* mapping = GetNGOffsetMapping()) {
      StringBuilder result;
      for (const NGOffsetMappingUnit& unit :
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
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      if (UNLIKELY(option != ClippingOption::kNoClipping)) {
        DCHECK_EQ(option, ClippingOption::kClipToEllipsis);
        if (cursor.Current().IsHiddenForPaint())
          continue;
      }
      yield(cursor.Current().RectInContainerFragment());
    }
    return;
  }
}

void LayoutText::AbsoluteQuads(Vector<gfx::QuadF>& quads,
                               MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  CollectLineBoxRects([this, &quads, mode](const PhysicalRect& r) {
    quads.push_back(LocalRectToAbsoluteQuad(r, mode));
  });
}

bool LayoutText::MapDOMOffsetToTextContentOffset(const NGOffsetMapping& mapping,
                                                 unsigned* start,
                                                 unsigned* end) const {
  NOT_DESTROYED();
  DCHECK_LE(*start, *end);

  // Adjust |start| to the next non-collapsed offset if |start| is collapsed.
  Position start_position =
      PositionForCaretOffset(std::min(*start, TextLength()));
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
  Position end_position = PositionForCaretOffset(std::min(*end, TextLength()));
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

  if (auto* mapping = GetNGOffsetMapping()) {
    if (!MapDOMOffsetToTextContentOffset(*mapping, &start, &end))
      return;

    const auto* const text_combine = DynamicTo<LayoutNGTextCombine>(Parent());

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
    NGInlineCursor cursor;
    bool is_last_end_included = false;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      const NGFragmentItem& item = *cursor.Current();
      DCHECK(item.IsText());
      bool is_collapsed = false;
      PhysicalRect rect;
      if (!item.IsGeneratedText()) {
        const NGTextOffsetRange& offset = item.TextOffset();
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
      if (UNLIKELY(text_combine))
        rect = text_combine->AdjustRectForBoundingBox(rect);
      gfx::QuadF quad;
      if (item.Type() == NGFragmentItem::kSvgText) {
        gfx::RectF float_rect(rect);
        float_rect.Offset(item.SvgFragmentData()->rect.OffsetFromOrigin());
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
  const LayoutBlock* block_for_flipping =
      UNLIKELY(HasFlippedBlocksWritingMode()) ? ContainingBlock() : nullptr;
  CollectLineBoxRects(
      [this, &result, block_for_flipping](const PhysicalRect& r) {
        LayoutRect rect = FlipForWritingMode(r, block_for_flipping);
        result.Union(gfx::RectF(rect));
      },
      kClipToEllipsis);
  // TODO(wangxianzhu): This is one of a few cases that a gfx::RectF is required
  // to be in flipped blocks direction. Should eliminite them.
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
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    const LayoutBlockFlow* containing_block_flow = cursor.GetLayoutBlockFlow();
    DCHECK(containing_block_flow);
    PhysicalOffset point_in_contents = point;
    if (containing_block_flow->IsScrollContainer()) {
      point_in_contents += PhysicalOffset(
          containing_block_flow->PixelSnappedScrolledContentOffset());
    }
    const auto* const text_combine = DynamicTo<LayoutNGTextCombine>(Parent());
    const NGPhysicalBoxFragment* container_fragment = nullptr;
    PhysicalOffset point_in_container_fragment;
    DCHECK(!IsSVGInlineText());
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      DCHECK(&cursor.ContainerFragment());
      if (container_fragment != &cursor.ContainerFragment()) {
        container_fragment = &cursor.ContainerFragment();
        point_in_container_fragment =
            point_in_contents - container_fragment->OffsetFromOwnerLayoutBox();
        if (UNLIKELY(text_combine)) {
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

LayoutRect LayoutText::LocalCaretRect(
    int caret_offset,
    LayoutUnit* extra_width_to_end_of_line) const {
  NOT_DESTROYED();
  return LayoutRect();
}

bool LayoutText::IsAllCollapsibleWhitespace() const {
  NOT_DESTROYED();
  unsigned length = TextLength();
  if (Is8Bit()) {
    for (unsigned i = 0; i < length; ++i) {
      if (!StyleRef().IsCollapsibleWhiteSpace(Characters8()[i]))
        return false;
    }
    return true;
  }
  for (unsigned i = 0; i < length; ++i) {
    if (!StyleRef().IsCollapsibleWhiteSpace(Characters16()[i]))
      return false;
  }
  return true;
}

bool LayoutText::ContainsOnlyWhitespace(unsigned from, unsigned len) const {
  NOT_DESTROYED();
  DCHECK(text_);
  unsigned curr_pos;
  for (curr_pos = from;
       curr_pos < from + len && (text_[curr_pos] == kNewlineCharacter ||
                                 text_[curr_pos] == kSpaceCharacter ||
                                 text_[curr_pos] == kTabulationCharacter);
       curr_pos++) {
  }
  return curr_pos >= (from + len);
}

UChar32 LayoutText::FirstCharacterAfterWhitespaceCollapsing() const {
  NOT_DESTROYED();
  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
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
    NGInlineCursor cursor;
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
    if (UNLIKELY(!IsFirstInlineFragmentSafe()))
      return PhysicalOffset();
    NGInlineCursor cursor;
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
    NGInlineCursor cursor;
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

void LayoutText::SetTextWithOffset(String text, unsigned offset, unsigned len) {
  NOT_DESTROYED();
  if (text_ == text) {
    return;
  }

  if (NGInlineNode::SetTextWithOffset(this, text, offset, len)) {
    DCHECK(!NeedsCollectInlines());
    // Prevent |TextDidChange()| to propagate |NeedsCollectInlines|
    SetNeedsCollectInlines(true);
    TextDidChange();
    valid_ng_items_ = true;
    ClearNeedsCollectInlines();
    return;
  }

  bool dirtied_lines = false;

  // If the text node is empty, dirty the line where new text will be inserted.
  if (!HasInlineFragments() && Parent()) {
    Parent()->DirtyLinesFromChangedChild(this);
    dirtied_lines = true;
  }

  lines_dirty_ = dirtied_lines;
  ForceSetText(std::move(text));

  // TODO(layout-dev): Invalidation is currently all or nothing in LayoutNG,
  // this is probably fine for NGInlineItem reuse as recreating the individual
  // items is relatively cheap. If partial relayout performance improvement are
  // needed partial re-shapes are likely to be sufficient. Revisit as needed.
  valid_ng_items_ = false;
}

void LayoutText::TransformText() {
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
  return To<LayoutText>(o)->GetText().empty();
}

OnlyWhitespaceOrNbsp LayoutText::ContainsOnlyWhitespaceOrNbsp() const {
  NOT_DESTROYED();
  return IntrinsicLogicalWidthsDirty() ? OnlyWhitespaceOrNbsp::kUnknown
                                       : static_cast<OnlyWhitespaceOrNbsp>(
                                             contains_only_whitespace_or_nbsp_);
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
            To<LayoutText>(previous_text)->GetText()) {
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
  DCHECK(!IsBR() || (TextLength() == 1 && text_[0] == kNewlineCharacter));
}

void LayoutText::ApplyTextTransform() {
  NOT_DESTROYED();
  if (const ComputedStyle* style = Style()) {
    style->ApplyTextTransform(&text_, PreviousCharacter());

    // We use the same characters here as for list markers.
    // See CollectUACounterStyleRules() in ua_counter_style_map.cc.
    switch (style->TextSecurity()) {
      case ETextSecurity::kNone:
        break;
      case ETextSecurity::kCircle:
        SecureText(kWhiteBulletCharacter);
        break;
      case ETextSecurity::kDisc:
        SecureText(kBulletCharacter);
        break;
      case ETextSecurity::kSquare:
        SecureText(kBlackSquareCharacter);
    }
  }
}

void LayoutText::SecureText(UChar mask) {
  NOT_DESTROYED();
  if (!text_.length())
    return;

  int last_typed_character_offset_to_reveal = -1;
  UChar revealed_text;
  auto it = GetSecureTextTimers().find(this);
  SecureTextTimer* secure_text_timer =
      it != GetSecureTextTimers().end() ? it->value : nullptr;
  if (secure_text_timer && secure_text_timer->IsActive()) {
    last_typed_character_offset_to_reveal =
        secure_text_timer->LastTypedCharacterOffset();
    if (last_typed_character_offset_to_reveal >= 0)
      revealed_text = text_[last_typed_character_offset_to_reveal];
  }

  text_.Fill(mask);
  if (last_typed_character_offset_to_reveal >= 0) {
    text_.replace(last_typed_character_offset_to_reveal, 1,
                  String(&revealed_text, 1u));
    // text_ may be updated later before timer fires. We invalidate the
    // last_typed_character_offset_ to avoid inconsistency.
    secure_text_timer->Invalidate();
  }
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
  auto* const text_combine = DynamicTo<LayoutNGTextCombine>(Parent());
  if (UNLIKELY(text_combine)) {
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
  ApplyTextTransform();
  known_to_have_no_overflow_and_no_fallback_fonts_ = false;

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
  SetNeedsCollectInlines();
}

void LayoutText::InvalidateSubtreeLayoutForFontUpdates() {
  NOT_DESTROYED();
  if (IsFontFallbackValid())
    return;

  known_to_have_no_overflow_and_no_fallback_fonts_ = false;
  valid_ng_items_ = false;
  SetNeedsCollectInlines();
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kFontsChanged);
}

void LayoutText::DirtyOrDeleteLineBoxesIfNeeded(bool full_layout) {
  NOT_DESTROYED();
  if (full_layout)
    DeleteTextBoxes();
  else if (!lines_dirty_)
    DirtyLineBoxes();
  lines_dirty_ = false;
  valid_ng_items_ = false;
}

void LayoutText::DirtyLineBoxes() {
  NOT_DESTROYED();
  lines_dirty_ = false;
  valid_ng_items_ = false;
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
  const auto* const text_combine = DynamicTo<LayoutNGTextCombine>(Parent());
  if (UNLIKELY(text_combine))
    return text_combine->AdjustRectForBoundingBox(result);
  return result;
}

PhysicalRect LayoutText::PhysicalVisualOverflowRect() const {
  NOT_DESTROYED();
  DCHECK(IsInLayoutNGInlineFormattingContext());
  return NGFragmentItem::LocalVisualRectFor(*this);
}

PhysicalRect LayoutText::LocalVisualRectIgnoringVisibility() const {
  NOT_DESTROYED();
  return UnionRect(PhysicalVisualOverflowRect(), LocalSelectionVisualRect());
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
    NGInlineCursor cursor(*FragmentItemsContainer());
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
        const NGFragmentItem& item = *cursor.CurrentItem();
        float_rect.Offset(item.SvgFragmentData()->rect.OffsetFromOrigin());
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

  const LayoutTextSelectionStatus& selection_status =
      frame_selection.ComputeLayoutSelectionStatus(*this);
  const unsigned start_pos = selection_status.start;
  const unsigned end_pos = selection_status.end;
  DCHECK_LE(start_pos, end_pos);
  LayoutRect rect;
  return FlipForWritingMode(rect);
}

void LayoutText::InvalidateVisualOverflow() {
  DCHECK(IsInLayoutNGInlineFormattingContext());
  NGInlineCursor cursor;
  for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject())
    cursor.Current()->GetMutableForPainting().InvalidateInkOverflow();
}

const NGOffsetMapping* LayoutText::GetNGOffsetMapping() const {
  NOT_DESTROYED();
  return NGOffsetMapping::GetFor(this);
}

Position LayoutText::PositionForCaretOffset(unsigned offset) const {
  NOT_DESTROYED();
  // ::first-letter handling should be done by LayoutTextFragment override.
  DCHECK(!IsTextFragment());
  // BR handling should be done by LayoutBR override.
  DCHECK(!IsBR());
  // WBR handling should be done by LayoutWordBreak override.
  DCHECK(!IsWordBreak());
  DCHECK_LE(offset, TextLength());
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

absl::optional<unsigned> LayoutText::CaretOffsetForPosition(
    const Position& position) const {
  NOT_DESTROYED();
  // ::first-letter handling should be done by LayoutTextFragment override.
  DCHECK(!IsTextFragment());
  // BR handling should be done by LayoutBR override.
  DCHECK(!IsBR());
  // WBR handling should be done by LayoutWordBreak override.
  DCHECK(!IsWordBreak());
  if (position.IsNull() || position.AnchorNode() != GetNode())
    return absl::nullopt;
  DCHECK(GetNode()->IsTextNode());
  if (position.IsBeforeAnchor())
    return 0;
  // TODO(layout-dev): Support offset change due to text-transform.
  if (position.IsAfterAnchor())
    return TextLength();
  DCHECK(position.IsOffsetInAnchor()) << position;
  DCHECK_LE(position.OffsetInContainerNode(), static_cast<int>(TextLength()))
      << position;
  return position.OffsetInContainerNode();
}

int LayoutText::CaretMinOffset() const {
  NOT_DESTROYED();
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  if (auto* mapping = GetNGOffsetMapping()) {
    const Position first_position = PositionForCaretOffset(0);
    if (first_position.IsNull())
      return 0;
    absl::optional<unsigned> candidate = CaretOffsetForPosition(
        mapping->StartOfNextNonCollapsedContent(first_position));
    // Align with the legacy behavior that 0 is returned if the entire node
    // contains only collapsed whitespaces.
    const bool fully_collapsed = !candidate || *candidate == TextLength();
    return fully_collapsed ? 0 : *candidate;
  }

  return 0;
}

int LayoutText::CaretMaxOffset() const {
  NOT_DESTROYED();
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  if (auto* mapping = GetNGOffsetMapping()) {
    const Position last_position = PositionForCaretOffset(TextLength());
    if (last_position.IsNull())
      return TextLength();
    absl::optional<unsigned> candidate = CaretOffsetForPosition(
        mapping->EndOfLastNonCollapsedContent(last_position));
    // Align with the legacy behavior that |TextLenght()| is returned if the
    // entire node contains only collapsed whitespaces.
    const bool fully_collapsed = !candidate || *candidate == 0u;
    return fully_collapsed ? TextLength() : *candidate;
  }

  return TextLength();
}

unsigned LayoutText::ResolvedTextLength() const {
  NOT_DESTROYED();
  if (auto* mapping = GetNGOffsetMapping()) {
    const Position start_position = PositionForCaretOffset(0);
    const Position end_position = PositionForCaretOffset(TextLength());
    if (start_position.IsNull()) {
      DCHECK(end_position.IsNull()) << end_position;
      return 0;
    }
    DCHECK(end_position.IsNotNull()) << start_position;
    absl::optional<unsigned> start =
        mapping->GetTextContentOffset(start_position);
    absl::optional<unsigned> end = mapping->GetTextContentOffset(end_position);
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
  if (GetNGOffsetMapping())
    return ResolvedTextLength();
  return false;
}

bool LayoutText::ContainsCaretOffset(int text_offset) const {
  NOT_DESTROYED();
  DCHECK_GE(text_offset, 0);
  if (auto* mapping = GetNGOffsetMapping()) {
    if (text_offset > static_cast<int>(TextLength()))
      return false;
    const Position position = PositionForCaretOffset(text_offset);
    if (position.IsNull())
      return false;
    if (text_offset < static_cast<int>(TextLength()) &&
        mapping->IsBeforeNonCollapsedContent(position))
      return true;
    if (!text_offset || !mapping->IsAfterNonCollapsedContent(position))
      return false;
    return *mapping->GetCharacterBefore(position) != kNewlineCharacter;
  }

  return false;
}

bool LayoutText::IsBeforeNonCollapsedCharacter(unsigned text_offset) const {
  NOT_DESTROYED();
  if (auto* mapping = GetNGOffsetMapping()) {
    if (text_offset >= TextLength())
      return false;
    const Position position = PositionForCaretOffset(text_offset);
    if (position.IsNull())
      return false;
    return mapping->IsBeforeNonCollapsedContent(position);
  }

  return false;
}

bool LayoutText::IsAfterNonCollapsedCharacter(unsigned text_offset) const {
  NOT_DESTROYED();
  if (auto* mapping = GetNGOffsetMapping()) {
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

NGAbstractInlineTextBox* LayoutText::FirstAbstractInlineTextBox() {
  NOT_DESTROYED();
  DCHECK(IsInLayoutNGInlineFormattingContext());
  NGInlineCursor cursor;
  cursor.MoveTo(*this);
  return NGAbstractInlineTextBox::GetOrCreate(cursor);
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
  ObjectPaintInvalidator invalidator(*this);
  invalidator.InvalidateDisplayItemClient(*this, reason);

  if (const auto* selection_client = GetSelectionDisplayItemClient())
    invalidator.InvalidateDisplayItemClient(*selection_client, reason);

  if (IsInLayoutNGInlineFormattingContext()) {
#if DCHECK_IS_ON()
    NGInlineCursor cursor;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject())
      DCHECK_EQ(cursor.Current().GetDisplayItemClient(), this);
#endif
    return;
  }
}

const DisplayItemClient* LayoutText::GetSelectionDisplayItemClient() const {
  NOT_DESTROYED();
  if (UNLIKELY(!IsInLayoutNGInlineFormattingContext()))
    return nullptr;
  // When |this| is in text-combine box, we should use text-combine box as
  // display client item to paint caret with affine transform.
  const auto* const text_combine = DynamicTo<LayoutNGTextCombine>(Parent());
  if (UNLIKELY(text_combine) && text_combine->NeedsAffineTransformInPaint())
    return text_combine;
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
        node_id_ = DOMNodeIds::IdForNode(node);
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

void LayoutText::SetInlineItems(NGInlineItemsData* data,
                                wtf_size_t begin,
                                wtf_size_t size) {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  for (wtf_size_t i = begin; i < begin + size; i++) {
    DCHECK_EQ(data->items[i].GetLayoutObject(), this);
  }
#endif
  auto* items = GetNGInlineItems();
  if (!items)
    return;
  valid_ng_items_ = true;
  items->SetItems(data, begin, size);
}

void LayoutText::ClearInlineItems() {
  NOT_DESTROYED();
  has_bidi_control_items_ = false;
  valid_ng_items_ = false;
  if (auto* items = GetNGInlineItems())
    items->Clear();
}

const NGInlineItemSpan& LayoutText::InlineItems() const {
  NOT_DESTROYED();
  DCHECK(valid_ng_items_);
  DCHECK(GetNGInlineItems());
  DCHECK(!GetNGInlineItems()->empty());
  return *GetNGInlineItems();
}

#if DCHECK_IS_ON()
void LayoutText::RecalcVisualOverflow() {
  // We should never reach here, because |PaintLayer| calls
  // |RecalcVisualOverflow| for each layer, and the containing |LayoutObject|
  // should recalculate its |NGFragmentItem|s without traversing descendant
  // |LayoutObject|s.
  if (IsInline() && IsInLayoutNGInlineFormattingContext())
    NOTREACHED();

  LayoutObject::RecalcVisualOverflow();
}
#endif

}  // namespace blink
