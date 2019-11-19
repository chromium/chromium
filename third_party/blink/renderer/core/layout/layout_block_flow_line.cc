/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/bidi_run_for_line.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/breaking_context_inline_headers.h"
#include "third_party/blink/renderer/core/layout/line/glyph_overflow.h"
#include "third_party/blink/renderer/core/layout/line/layout_text_info.h"
#include "third_party/blink/renderer/core/layout/line/line_layout_state.h"
#include "third_party/blink/renderer/core/layout/line/line_width.h"
#include "third_party/blink/renderer/core/layout/line/word_measurement.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_root_inline_box.h"
#include "third_party/blink/renderer/core/layout/vertical_position_cache.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExpansionOpportunities {
 public:
  ExpansionOpportunities() : total_opportunities_(0) {}

  void AddRunWithExpansions(BidiRun& run,
                            bool& is_after_expansion,
                            TextJustify text_justify) {
    LineLayoutText text = LineLayoutText(run.line_layout_item_);
    unsigned opportunities_in_run;
    if (text.Is8Bit()) {
      opportunities_in_run = Character::ExpansionOpportunityCount(
          {text.Characters8() + run.start_, run.stop_ - run.start_},
          run.box_->Direction(), is_after_expansion, text_justify);
    } else if (run.line_layout_item_.IsCombineText()) {
      // Justfication applies to before and after the combined text as if
      // it is an ideographic character, and is prohibited inside the
      // combined text.
      opportunities_in_run = is_after_expansion ? 1 : 2;
      is_after_expansion = true;
    } else {
      opportunities_in_run = Character::ExpansionOpportunityCount(
          {text.Characters16() + run.start_, run.stop_ - run.start_},
          run.box_->Direction(), is_after_expansion, text_justify);
    }
    runs_with_expansions_.push_back(opportunities_in_run);
    total_opportunities_ += opportunities_in_run;
  }
  void RemoveTrailingExpansion() {
    if (!total_opportunities_ || !runs_with_expansions_.back())
      return;
    runs_with_expansions_.back()--;
    total_opportunities_--;
  }

  unsigned Count() { return total_opportunities_; }

  unsigned OpportunitiesInRun(size_t run) { return runs_with_expansions_[run]; }

  void ComputeExpansionsForJustifiedText(BidiRun* first_run,
                                         BidiRun* trailing_space_run,
                                         LayoutUnit& total_logical_width,
                                         LayoutUnit available_logical_width) {
    if (!total_opportunities_ || available_logical_width <= total_logical_width)
      return;

    size_t i = 0;
    for (BidiRun* r = first_run; r; r = r->Next()) {
      if (!r->box_ || r == trailing_space_run)
        continue;

      if (r->line_layout_item_.IsText()) {
        unsigned opportunities_in_run = runs_with_expansions_[i++];

        CHECK_LE(opportunities_in_run, total_opportunities_);

        // Don't justify for white-space: pre.
        if (r->line_layout_item_.StyleRef().WhiteSpace() != EWhiteSpace::kPre) {
          InlineTextBox* text_box = ToInlineTextBox(r->box_);
          CHECK(total_opportunities_);
          int expansion = ((available_logical_width - total_logical_width) *
                           opportunities_in_run / total_opportunities_)
                              .ToInt();
          text_box->SetExpansion(expansion);
          total_logical_width += expansion;
        }
        total_opportunities_ -= opportunities_in_run;
        if (!total_opportunities_)
          break;
      }
    }
  }

 private:
  Vector<unsigned, 16> runs_with_expansions_;
  unsigned total_opportunities_;
};

static inline InlineBox* CreateInlineBoxForLayoutObject(
    LineLayoutItem line_layout_item,
    bool is_root_line_box,
    bool is_only_run = false) {
  // Callers should handle text themselves.
  DCHECK(!line_layout_item.IsText());

  if (is_root_line_box)
    return LineLayoutBlockFlow(line_layout_item).CreateAndAppendRootInlineBox();

  if (line_layout_item.IsBox())
    return LineLayoutBox(line_layout_item).CreateInlineBox();

  return LineLayoutInline(line_layout_item).CreateAndAppendInlineFlowBox();
}

static inline InlineTextBox* CreateInlineBoxForText(BidiRun& run,
                                                    bool is_only_run) {
  DCHECK(run.line_layout_item_.IsText());
  LineLayoutText text = LineLayoutText(run.line_layout_item_);
  InlineTextBox* text_box =
      text.CreateInlineTextBox(run.start_, run.stop_ - run.start_);
  // We only treat a box as text for a <br> if we are on a line by ourself or in
  // strict mode (Note the use of strict mode.  In "almost strict" mode, we
  // don't treat the box for <br> as text.)
  if (text.IsBR())
    text_box->SetIsText(is_only_run || text.GetDocument().InNoQuirksMode());
  text_box->SetDirOverride(
      run.DirOverride(text.StyleRef().RtlOrdering() == EOrder::kVisual));
  if (run.has_hyphen_)
    text_box->SetHasHyphen(true);
  return text_box;
}

static inline void DirtyLineBoxesForObject(LayoutObject* o, bool full_layout) {
  if (o->IsText()) {
    LayoutText* layout_text = ToLayoutText(o);
    layout_text->DirtyOrDeleteLineBoxesIfNeeded(full_layout);
  } else {
    ToLayoutInline(o)->DirtyLineBoxes(full_layout);
  }
}

static bool ParentIsConstructedOrHaveNext(InlineFlowBox* parent_box) {
  do {
    if (parent_box->IsConstructed() || parent_box->NextOnLine())
      return true;
    parent_box = parent_box->Parent();
  } while (parent_box);
  return false;
}

InlineFlowBox* LayoutBlockFlow::CreateLineBoxes(LineLayoutItem line_layout_item,
                                                const LineInfo& line_info,
                                                InlineBox* child_box) {
  // See if we have an unconstructed line box for this object that is also
  // the last item on the line.
  unsigned line_depth = 1;
  InlineFlowBox* parent_box = nullptr;
  InlineFlowBox* result = nullptr;
  do {
    if (line_depth++ >= kCMaxLineDepth ||
        (IsLayoutNGBlockFlow() && line_layout_item.IsLayoutBlockFlow())) {
      // If we've exceeded our line depth, then jump straight to the root and
      // skip all the remaining intermediate inline flows. Additionally, if
      // we're in LayoutNG, abort once we find a block in the ancestry. It may
      // be that it's not |this|. This happens in multicol, because LayoutNG
      // doesn't see the flow thread, and treats DOM children of the multicol
      // container as actual children of the multicol container, without any
      // intervening flow thread block (although that block does exist in the
      // layout tree).
      line_layout_item = LineLayoutItem(this);
    }

    // Get the last box we made for this layout object.
    bool allowed_to_construct_new_box;
    if (line_layout_item.IsLayoutInline()) {
      LineLayoutInline inline_flow(line_layout_item);
      parent_box = inline_flow.LastLineBox();
      allowed_to_construct_new_box = inline_flow.AlwaysCreateLineBoxes();
    } else {
      DCHECK(line_layout_item.IsEqual(this));
      parent_box = LineLayoutBlockFlow(line_layout_item).LastLineBox();
      allowed_to_construct_new_box = true;
    }

    // If this box or its ancestor is constructed then it is from a previous
    // line, and we need to make a new box for our line.  If this box or its
    // ancestor is unconstructed but it has something following it on the line,
    // then we know we have to make a new box as well.  In this situation our
    // inline has actually been split in two on the same line (this can happen
    // with very fancy language mixtures).
    bool constructed_new_box = false;
    bool can_use_existing_parent_box =
        parent_box && !ParentIsConstructedOrHaveNext(parent_box);
    if (allowed_to_construct_new_box && !can_use_existing_parent_box) {
      // We need to make a new box for this layout object.  Once
      // made, we need to place it at the end of the current line.
      InlineBox* new_box = CreateInlineBoxForLayoutObject(
          LineLayoutItem(line_layout_item), line_layout_item.IsEqual(this));
      SECURITY_DCHECK(new_box->IsInlineFlowBox());
      parent_box = ToInlineFlowBox(new_box);
      parent_box->SetFirstLineStyleBit(line_info.IsFirstLine());
      parent_box->SetIsHorizontal(IsHorizontalWritingMode());
      constructed_new_box = true;
    }

    if (constructed_new_box || can_use_existing_parent_box) {
      if (!result)
        result = parent_box;

      // If we have hit the block itself, then |box| represents the root
      // inline box for the line, and it doesn't have to be appended to any
      // parent inline.
      if (child_box)
        parent_box->AddToLine(child_box);

      if (!constructed_new_box || line_layout_item.IsEqual(this))
        break;

      child_box = parent_box;
    }

    line_layout_item = line_layout_item.Parent();
  } while (true);

  return result;
}

template <typename CharacterType>
static inline bool EndsWithASCIISpaces(const CharacterType* characters,
                                       unsigned pos,
                                       unsigned end) {
  while (IsASCIISpace(characters[pos])) {
    pos++;
    if (pos >= end)
      return true;
  }
  return false;
}

static bool ReachedEndOfTextRun(const BidiRunList<BidiRun>& bidi_runs) {
  BidiRun* run = bidi_runs.LogicallyLastRun();
  if (!run)
    return true;
  unsigned pos = run->Stop();
  LineLayoutItem r = run->line_layout_item_;
  if (!r.IsText() || r.IsBR())
    return false;
  LineLayoutText layout_text(r);
  unsigned length = layout_text.TextLength();
  if (pos >= length)
    return true;

  if (layout_text.Is8Bit())
    return EndsWithASCIISpaces(layout_text.Characters8(), pos, length);
  return EndsWithASCIISpaces(layout_text.Characters16(), pos, length);
}

RootInlineBox* LayoutBlockFlow::ConstructLine(BidiRunList<BidiRun>& bidi_runs,
                                              const LineInfo& line_info) {
  DCHECK(bidi_runs.FirstRun());

  InlineFlowBox* parent_box = nullptr;
  int run_count = bidi_runs.RunCount() - line_info.RunsFromLeadingWhitespace();
  for (BidiRun* r = bidi_runs.FirstRun(); r; r = r->Next()) {
    // Create a box for our object.
    bool is_only_run = (run_count == 1);
    if (run_count == 2 && !r->line_layout_item_.IsListMarker()) {
      is_only_run =
          (!StyleRef().IsLeftToRightDirection() ? bidi_runs.LastRun()
                                                : bidi_runs.FirstRun())
              ->line_layout_item_.IsListMarker();
    }

    if (line_info.IsEmpty())
      continue;

    InlineBox* box;
    if (r->line_layout_item_.IsText())
      box = CreateInlineBoxForText(*r, is_only_run);
    else
      box = CreateInlineBoxForLayoutObject(r->line_layout_item_, false,
                                           is_only_run);
    r->box_ = box;

    DCHECK(box);
    if (!box)
      continue;

    // If we have no parent box yet, or if the run is not simply a sibling,
    // then we need to construct inline boxes as necessary to properly enclose
    // the run's inline box. Segments can only be siblings at the root level, as
    // they are positioned separately.
    if (!parent_box ||
        (parent_box->GetLineLayoutItem() != r->line_layout_item_.Parent())) {
      // Create new inline boxes all the way back to the appropriate insertion
      // point.
      parent_box =
          CreateLineBoxes(r->line_layout_item_.Parent(), line_info, box);
    } else {
      // Append the inline box to this line.
      parent_box->AddToLine(box);
    }

    box->SetBidiLevel(r->Level());

    if (box->IsInlineTextBox()) {
      if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
        cache->InlineTextBoxesUpdated(r->line_layout_item_);
    }
  }

  // We should have a root inline box.  It should be unconstructed and
  // be the last continuation of our line list.
  DCHECK(LastLineBox());
  DCHECK(!LastLineBox()->IsConstructed());

  // Set bits on our inline flow boxes that indicate which sides should
  // paint borders/margins/padding.  This knowledge will ultimately be used when
  // we determine the horizontal positions and widths of all the inline boxes on
  // the line.
  bool is_logically_last_run_wrapped =
      bidi_runs.LogicallyLastRun()->line_layout_item_ &&
              bidi_runs.LogicallyLastRun()->line_layout_item_.IsText()
          ? !ReachedEndOfTextRun(bidi_runs)
          : true;
  LastLineBox()->DetermineSpacingForFlowBoxes(
      line_info.IsLastLine(), is_logically_last_run_wrapped,
      bidi_runs.LogicallyLastRun()->line_layout_item_);

  // Now mark the line boxes as being constructed.
  LastLineBox()->SetConstructed();

  // Return the last line.
  return LastRootBox();
}

ETextAlign LayoutBlockFlow::TextAlignmentForLine(
    bool ends_with_soft_break) const {
  return StyleRef().GetTextAlign(!ends_with_soft_break);
}

static bool TextAlignmentNeedsTrailingSpace(ETextAlign text_align,
                                            const ComputedStyle& style) {
  switch (text_align) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      return false;
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
    case ETextAlign::kJustify:
      return style.CollapseWhiteSpace();
    case ETextAlign::kStart:
      return style.CollapseWhiteSpace() && !style.IsLeftToRightDirection();
    case ETextAlign::kEnd:
      return style.CollapseWhiteSpace() && style.IsLeftToRightDirection();
  }
  NOTREACHED();
  return false;
}

static void UpdateLogicalWidthForLeftAlignedBlock(
    bool is_left_to_right_direction,
    BidiRun* trailing_space_run,
    LayoutUnit& logical_left,
    LayoutUnit total_logical_width,
    LayoutUnit available_logical_width) {
  // The direction of the block should determine what happens with wide lines.
  // In particular with RTL blocks, wide lines should still spill out to the
  // left.
  if (is_left_to_right_direction) {
    if (total_logical_width > available_logical_width && trailing_space_run)
      trailing_space_run->box_->SetLogicalWidth(std::max(
          LayoutUnit(), trailing_space_run->box_->LogicalWidth() -
                            total_logical_width + available_logical_width));
    return;
  }

  if (trailing_space_run &&
      trailing_space_run->line_layout_item_.StyleRef().CollapseWhiteSpace())
    trailing_space_run->box_->SetLogicalWidth(LayoutUnit());
  else if (total_logical_width > available_logical_width)
    logical_left -= (total_logical_width - available_logical_width);
}

static void UpdateLogicalWidthForRightAlignedBlock(
    bool is_left_to_right_direction,
    BidiRun* trailing_space_run,
    LayoutUnit& logical_left,
    LayoutUnit& total_logical_width,
    LayoutUnit available_logical_width) {
  // Wide lines spill out of the block based off direction.
  // So even if text-align is right, if direction is LTR, wide lines should
  // overflow out of the right side of the block.
  if (is_left_to_right_direction) {
    if (trailing_space_run &&
        trailing_space_run->line_layout_item_.StyleRef().CollapseWhiteSpace()) {
      total_logical_width -= trailing_space_run->box_->LogicalWidth();
      trailing_space_run->box_->SetLogicalWidth(LayoutUnit());
    }
    if (total_logical_width < available_logical_width)
      logical_left += available_logical_width - total_logical_width;
    return;
  }

  if (total_logical_width > available_logical_width && trailing_space_run) {
    trailing_space_run->box_->SetLogicalWidth(std::max(
        LayoutUnit(), trailing_space_run->box_->LogicalWidth() -
                          total_logical_width + available_logical_width));
    total_logical_width -= trailing_space_run->box_->LogicalWidth();
  } else {
    logical_left += available_logical_width - total_logical_width;
  }
}

static void UpdateLogicalWidthForCenterAlignedBlock(
    bool is_left_to_right_direction,
    BidiRun* trailing_space_run,
    LayoutUnit& logical_left,
    LayoutUnit& total_logical_width,
    LayoutUnit available_logical_width) {
  LayoutUnit trailing_space_width;
  if (trailing_space_run &&
      trailing_space_run->line_layout_item_.StyleRef().CollapseWhiteSpace()) {
    total_logical_width -= trailing_space_run->box_->LogicalWidth();
    trailing_space_width =
        std::min(trailing_space_run->box_->LogicalWidth(),
                 (available_logical_width - total_logical_width + 1) / 2);
    trailing_space_run->box_->SetLogicalWidth(
        std::max(LayoutUnit(), trailing_space_width));
  }
  if (is_left_to_right_direction)
    logical_left += std::max(
        (available_logical_width - total_logical_width) / 2, LayoutUnit());
  else
    logical_left += total_logical_width > available_logical_width
                        ? (available_logical_width - total_logical_width)
                        : (available_logical_width - total_logical_width) / 2 -
                              trailing_space_width;
}

void LayoutBlockFlow::SetMarginsForRubyRun(BidiRun* run,
                                           LayoutRubyRun* layout_ruby_run,
                                           LayoutObject* previous_object,
                                           const LineInfo& line_info) {
  int start_overhang;
  int end_overhang;
  LayoutObject* next_object = nullptr;
  for (BidiRun* run_with_next_object = run->Next(); run_with_next_object;
       run_with_next_object = run_with_next_object->Next()) {
    if (!run_with_next_object->line_layout_item_.IsOutOfFlowPositioned() &&
        !run_with_next_object->box_->IsLineBreak()) {
      next_object = run_with_next_object->line_layout_item_.GetLayoutObject();
      break;
    }
  }
  layout_ruby_run->GetOverhang(
      line_info.IsFirstLine(),
      layout_ruby_run->StyleRef().IsLeftToRightDirection() ? previous_object
                                                           : next_object,
      layout_ruby_run->StyleRef().IsLeftToRightDirection() ? next_object
                                                           : previous_object,
      start_overhang, end_overhang);
  SetMarginStartForChild(*layout_ruby_run, LayoutUnit(-start_overhang));
  SetMarginEndForChild(*layout_ruby_run, LayoutUnit(-end_overhang));
}

static inline size_t FindWordMeasurement(
    LineLayoutText layout_text,
    int offset,
    const WordMeasurements& word_measurements,
    size_t last_index) {
  // In LTR, lastIndex should match since the order of BidiRun (visual) and
  // WordMeasurement (logical) are the same.
  size_t size = word_measurements.size();
  if (last_index < size) {
    const WordMeasurement& word_measurement = word_measurements[last_index];
    if (word_measurement.layout_text == layout_text &&
        word_measurement.start_offset == offset)
      return last_index;
  }

  // In RTL, scan the whole array because they are not the same.
  for (size_t i = 0; i < size; ++i) {
    const WordMeasurement& word_measurement = word_measurements[i];
    if (word_measurement.layout_text != layout_text)
      continue;
    if (word_measurement.start_offset == offset)
      return i;
    if (word_measurement.start_offset > offset)
      break;
  }

  // In RTL with space collpasing or in LTR/RTL mixed lines, there can be no
  // matches because spaces are handled differently in BidiRun and
  // WordMeasurement. This can cause slight performance hit and slight
  // differences in glyph positions since we re-measure the whole run.
  return size;
}

static inline void SetLogicalWidthForTextRun(
    RootInlineBox* line_box,
    BidiRun* run,
    LineLayoutText layout_text,
    LayoutUnit x_pos,
    const LineInfo& line_info,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map,
    VerticalPositionCache& vertical_position_cache,
    const WordMeasurements& word_measurements,
    size_t& word_measurements_index) {
  HashSet<const SimpleFontData*> fallback_fonts;
  GlyphOverflow glyph_overflow;

  const Font& font = layout_text.Style(line_info.IsFirstLine())->GetFont();

  LayoutUnit hyphen_width;
  if (ToInlineTextBox(run->box_)->HasHyphen())
    hyphen_width = LayoutUnit(layout_text.HyphenWidth(font, run->Direction()));

  float measured_width = 0;
  FloatRect glyph_bounds;

  bool kerning_is_enabled =
      font.GetFontDescription().GetTypesettingFeatures() & kKerning;

#if defined(OS_MACOSX)
  // FIXME: Having any font feature settings enabled can lead to selection gaps
  // on Chromium-mac. https://bugs.webkit.org/show_bug.cgi?id=113418
  bool can_use_cached_word_measurements =
      font.CanShapeWordByWord() &&
      !font.GetFontDescription().FeatureSettings() && layout_text.Is8Bit();
#else
  bool can_use_cached_word_measurements =
      font.CanShapeWordByWord() && layout_text.Is8Bit();
#endif

  if (can_use_cached_word_measurements) {
    int last_end_offset = run->start_;
    size_t i = FindWordMeasurement(layout_text, last_end_offset,
                                   word_measurements, word_measurements_index);
    for (size_t size = word_measurements.size();
         i < size && last_end_offset < run->stop_; ++i) {
      const WordMeasurement& word_measurement = word_measurements[i];
      if (word_measurement.start_offset == word_measurement.end_offset)
        continue;
      if (word_measurement.layout_text != layout_text ||
          word_measurement.start_offset != last_end_offset ||
          word_measurement.end_offset > run->stop_)
        break;

      last_end_offset = word_measurement.end_offset;
      if (kerning_is_enabled && last_end_offset == run->stop_) {
        int word_length = last_end_offset - word_measurement.start_offset;
        measured_width +=
            layout_text.Width(word_measurement.start_offset, word_length, x_pos,
                              run->Direction(), line_info.IsFirstLine());
        if (i > 0 && word_length == 1 &&
            layout_text.CharacterAt(word_measurement.start_offset) == ' ')
          measured_width += layout_text.StyleRef().WordSpacing();
      } else {
        FloatRect word_glyph_bounds = word_measurement.glyph_bounds;
        word_glyph_bounds.Move(measured_width, 0);
        glyph_bounds.Unite(word_glyph_bounds);
        measured_width += word_measurement.width;
      }
      if (!word_measurement.fallback_fonts.IsEmpty()) {
        HashSet<const SimpleFontData*>::const_iterator end =
            word_measurement.fallback_fonts.end();
        for (HashSet<const SimpleFontData*>::const_iterator it =
                 word_measurement.fallback_fonts.begin();
             it != end; ++it)
          fallback_fonts.insert(*it);
      }
    }
    word_measurements_index = i;
    if (last_end_offset != run->stop_) {
      // If we don't have enough cached data, we'll measure the run again.
      can_use_cached_word_measurements = false;
      fallback_fonts.clear();
    }
  }

  // Don't put this into 'else' part of the above 'if' because
  // canUseCachedWordMeasurements may be modified in the 'if' block.
  if (!can_use_cached_word_measurements)
    measured_width = layout_text.Width(
        run->start_, run->stop_ - run->start_, x_pos, run->Direction(),
        line_info.IsFirstLine(), &fallback_fonts, &glyph_bounds);

  // Negative word-spacing and/or letter-spacing may cause some glyphs to
  // overflow the left boundary and result negative measured width. Reset
  // measured width to 0.
  if (measured_width < 0) {
    measured_width = 0;
  }

  glyph_overflow.SetFromBounds(glyph_bounds, font, measured_width);

  run->box_->SetLogicalWidth(LayoutUnit(measured_width) + hyphen_width);
  if (!fallback_fonts.IsEmpty()) {
    DCHECK(run->box_->IsText());
    GlyphOverflowAndFallbackFontsMap::ValueType* it =
        text_box_data_map
            .insert(ToInlineTextBox(run->box_),
                    std::make_pair(Vector<const SimpleFontData*>(),
                                   GlyphOverflow()))
            .stored_value;
    DCHECK(it->value.first.IsEmpty());
    CopyToVector(fallback_fonts, it->value.first);
    run->box_->Parent()->ClearDescendantsHaveSameLineHeightAndBaseline();
  }
  if (!glyph_overflow.IsApproximatelyZero()) {
    DCHECK(run->box_->IsText());
    GlyphOverflowAndFallbackFontsMap::ValueType* it =
        text_box_data_map
            .insert(ToInlineTextBox(run->box_),
                    std::make_pair(Vector<const SimpleFontData*>(),
                                   GlyphOverflow()))
            .stored_value;
    it->value.second = glyph_overflow;
    run->box_->ClearKnownToHaveNoOverflow();
  }
}

void LayoutBlockFlow::UpdateLogicalWidthForAlignment(
    const ETextAlign& text_align,
    const RootInlineBox* root_inline_box,
    BidiRun* trailing_space_run,
    LayoutUnit& logical_left,
    LayoutUnit& total_logical_width,
    LayoutUnit& available_logical_width,
    unsigned expansion_opportunity_count) {
  TextDirection direction;
  if (root_inline_box &&
      root_inline_box->GetLineLayoutItem().StyleRef().GetUnicodeBidi() ==
          UnicodeBidi::kPlaintext)
    direction = root_inline_box->Direction();
  else
    direction = StyleRef().Direction();

  // Armed with the total width of the line (without justification),
  // we now examine our text-align property in order to determine where to
  // position the objects horizontally. The total width of the line can be
  // increased if we end up justifying text.
  switch (text_align) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      UpdateLogicalWidthForLeftAlignedBlock(
          StyleRef().IsLeftToRightDirection(), trailing_space_run, logical_left,
          total_logical_width, available_logical_width);
      break;
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      UpdateLogicalWidthForRightAlignedBlock(
          StyleRef().IsLeftToRightDirection(), trailing_space_run, logical_left,
          total_logical_width, available_logical_width);
      break;
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      UpdateLogicalWidthForCenterAlignedBlock(
          StyleRef().IsLeftToRightDirection(), trailing_space_run, logical_left,
          total_logical_width, available_logical_width);
      break;
    case ETextAlign::kJustify:
      AdjustInlineDirectionLineBounds(expansion_opportunity_count, logical_left,
                                      available_logical_width);
      if (expansion_opportunity_count) {
        if (trailing_space_run) {
          total_logical_width -= trailing_space_run->box_->LogicalWidth();
          trailing_space_run->box_->SetLogicalWidth(LayoutUnit());
        }
        break;
      }
      FALLTHROUGH;
    case ETextAlign::kStart:
      if (direction == TextDirection::kLtr) {
        UpdateLogicalWidthForLeftAlignedBlock(
            StyleRef().IsLeftToRightDirection(), trailing_space_run,
            logical_left, total_logical_width, available_logical_width);
      } else {
        UpdateLogicalWidthForRightAlignedBlock(
            StyleRef().IsLeftToRightDirection(), trailing_space_run,
            logical_left, total_logical_width, available_logical_width);
      }
      break;
    case ETextAlign::kEnd:
      if (direction == TextDirection::kLtr) {
        UpdateLogicalWidthForRightAlignedBlock(
            StyleRef().IsLeftToRightDirection(), trailing_space_run,
            logical_left, total_logical_width, available_logical_width);
      } else {
        UpdateLogicalWidthForLeftAlignedBlock(
            StyleRef().IsLeftToRightDirection(), trailing_space_run,
            logical_left, total_logical_width, available_logical_width);
      }
      break;
  }
}

bool LayoutBlockFlow::CanContainFirstFormattedLine() const {
  // The 'text-indent' only affects a line if it is the first formatted
  // line of an element. For example, the first line of an anonymous block
  // box is only affected if it is the first child of its parent element.
  // https://drafts.csswg.org/css-text-3/#text-indent-property
  return !(IsAnonymousBlock() && PreviousSibling());
}

static void UpdateLogicalInlinePositions(LayoutBlockFlow* block,
                                         LayoutUnit& line_logical_left,
                                         LayoutUnit& line_logical_right,
                                         LayoutUnit& available_logical_width,
                                         bool first_line,
                                         IndentTextOrNot indent_text,
                                         LayoutUnit box_logical_height) {
  LayoutUnit line_logical_height =
      block->MinLineHeightForReplacedObject(first_line, box_logical_height);
  line_logical_left = block->LogicalLeftOffsetForLine(
      block->LogicalHeight(), indent_text, line_logical_height);
  line_logical_right = block->LogicalRightOffsetForLine(
      block->LogicalHeight(), indent_text, line_logical_height);
  available_logical_width = line_logical_right - line_logical_left;
}

void LayoutBlockFlow::ComputeInlineDirectionPositionsForLine(
    RootInlineBox* line_box,
    const LineInfo& line_info,
    BidiRun* first_run,
    BidiRun* trailing_space_run,
    bool reached_end,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map,
    VerticalPositionCache& vertical_position_cache,
    const WordMeasurements& word_measurements) {
  bool is_first_line =
      line_info.IsFirstLine() && CanContainFirstFormattedLine();
  bool is_after_hard_line_break =
      line_box->PrevRootBox() && line_box->PrevRootBox()->EndsWithBreak();
  IndentTextOrNot indent_text =
      RequiresIndent(is_first_line, is_after_hard_line_break, StyleRef());
  LayoutUnit line_logical_left;
  LayoutUnit line_logical_right;
  LayoutUnit available_logical_width;
  UpdateLogicalInlinePositions(this, line_logical_left, line_logical_right,
                               available_logical_width, is_first_line,
                               indent_text, LayoutUnit());
  bool needs_word_spacing;

  if (first_run && first_run->line_layout_item_.IsAtomicInlineLevel()) {
    LineLayoutBox layout_box(first_run->line_layout_item_);
    UpdateLogicalInlinePositions(this, line_logical_left, line_logical_right,
                                 available_logical_width, is_first_line,
                                 indent_text, layout_box.LogicalHeight());
  }

  ComputeInlineDirectionPositionsForSegment(
      line_box, line_info, line_logical_left, available_logical_width,
      first_run, trailing_space_run, text_box_data_map, vertical_position_cache,
      word_measurements);
  // The widths of all runs are now known. We can now place every inline box
  // (and compute accurate widths for the inline flow boxes).
  needs_word_spacing = line_box->IsLeftToRightDirection() ? false : true;
  line_box->PlaceBoxesInInlineDirection(line_logical_left, needs_word_spacing);
}

BidiRun* LayoutBlockFlow::ComputeInlineDirectionPositionsForSegment(
    RootInlineBox* line_box,
    const LineInfo& line_info,
    LayoutUnit& logical_left,
    LayoutUnit& available_logical_width,
    BidiRun* first_run,
    BidiRun* trailing_space_run,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map,
    VerticalPositionCache& vertical_position_cache,
    const WordMeasurements& word_measurements) {
  bool needs_word_spacing = true;
  LayoutUnit total_logical_width = line_box->GetFlowSpacingLogicalWidth();
  bool is_after_expansion = true;
  ExpansionOpportunities expansions;
  LayoutObject* previous_object = nullptr;
  ETextAlign text_align = line_info.GetTextAlign();
  TextJustify text_justify = StyleRef().GetTextJustify();

  BidiRun* r = first_run;
  size_t word_measurements_index = 0;
  for (; r; r = r->Next()) {
    if (!r->box_ || r->line_layout_item_.IsOutOfFlowPositioned() ||
        r->box_->IsLineBreak()) {
      continue;  // Positioned objects are only participating to figure out
                 // their correct static x position. They have no effect on the
                 // width. Similarly, line break boxes have no effect on the
                 // width.
    }
    if (r->line_layout_item_.IsText()) {
      LineLayoutText rt(r->line_layout_item_);
      if (text_align == ETextAlign::kJustify && r != trailing_space_run &&
          text_justify != TextJustify::kNone) {
        if (!is_after_expansion)
          ToInlineTextBox(r->box_)->SetCanHaveLeadingExpansion(true);
        expansions.AddRunWithExpansions(*r, is_after_expansion, text_justify);
      }

      if (rt.TextLength()) {
        if (!r->start_ && needs_word_spacing &&
            IsSpaceOrNewline(rt.CharacterAt(r->start_)))
          total_logical_width += rt.Style(line_info.IsFirstLine())
                                     ->GetFont()
                                     .GetFontDescription()
                                     .WordSpacing();
        needs_word_spacing = !IsSpaceOrNewline(rt.CharacterAt(r->stop_ - 1));
      }

      SetLogicalWidthForTextRun(line_box, r, rt, total_logical_width, line_info,
                                text_box_data_map, vertical_position_cache,
                                word_measurements, word_measurements_index);
    } else {
      is_after_expansion = false;
      if (!r->line_layout_item_.IsLayoutInline()) {
        LayoutBox* layout_box =
            ToLayoutBox(r->line_layout_item_.GetLayoutObject());
        if (layout_box->IsRubyRun())
          SetMarginsForRubyRun(r, ToLayoutRubyRun(layout_box), previous_object,
                               line_info);
        r->box_->SetLogicalWidth(LogicalWidthForChild(*layout_box));
        total_logical_width +=
            MarginStartForChild(*layout_box) + MarginEndForChild(*layout_box);
        needs_word_spacing = true;
      }
    }

    total_logical_width += r->box_->LogicalWidth();
    previous_object = r->line_layout_item_.GetLayoutObject();
  }

  if (is_after_expansion)
    expansions.RemoveTrailingExpansion();

  UpdateLogicalWidthForAlignment(text_align, line_box, trailing_space_run,
                                 logical_left, total_logical_width,
                                 available_logical_width, expansions.Count());

  expansions.ComputeExpansionsForJustifiedText(first_run, trailing_space_run,
                                               total_logical_width,
                                               available_logical_width);

  return r;
}

void LayoutBlockFlow::ComputeBlockDirectionPositionsForLine(
    RootInlineBox* line_box,
    BidiRun* first_run,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map,
    VerticalPositionCache& vertical_position_cache) {
  SetLogicalHeight(line_box->AlignBoxesInBlockDirection(
      LogicalHeight(), text_box_data_map, vertical_position_cache));

  // Now make sure we place replaced layout objects correctly.
  for (BidiRun* r = first_run; r; r = r->Next()) {
    DCHECK(r->box_);
    if (!r->box_)
      continue;  // Skip runs with no line boxes.

    // Align positioned boxes with the top of the line box.  This is
    // a reasonable approximation of an appropriate y position.
    if (r->line_layout_item_.IsOutOfFlowPositioned())
      r->box_->SetLogicalTop(LogicalHeight());

    // Position is used to properly position both replaced elements and
    // to update the static normal flow x/y of positioned elements.
    if (r->line_layout_item_.IsText())
      ToLayoutText(r->line_layout_item_.GetLayoutObject())
          ->PositionLineBox(r->box_);
    else if (r->line_layout_item_.IsBox())
      ToLayoutBox(r->line_layout_item_.GetLayoutObject())
          ->PositionLineBox(r->box_);
  }
}

void LayoutBlockFlow::AppendFloatingObjectToLastLine(
    FloatingObject& floating_object) {
  DCHECK(!floating_object.OriginatingLine());
  floating_object.SetOriginatingLine(LastRootBox());
  LastRootBox()->AppendFloat(floating_object.GetLayoutObject());
}

// This function constructs line boxes for all of the text runs in the resolver
// and computes their position.
RootInlineBox* LayoutBlockFlow::CreateLineBoxesFromBidiRuns(
    unsigned bidi_level,
    BidiRunList<BidiRun>& bidi_runs,
    const InlineIterator& end,
    LineInfo& line_info,
    VerticalPositionCache& vertical_position_cache,
    BidiRun* trailing_space_run,
    const WordMeasurements& word_measurements) {
  if (!bidi_runs.RunCount())
    return nullptr;

  // FIXME: Why is this only done when we had runs?
  line_info.SetLastLine(!end.GetLineLayoutItem());

  RootInlineBox* line_box = ConstructLine(bidi_runs, line_info);
  if (!line_box)
    return nullptr;

  line_box->SetBidiLevel(bidi_level);
  line_box->SetEndsWithBreak(line_info.PreviousLineBrokeCleanly());

  bool is_svg_root_inline_box = line_box->IsSVGRootInlineBox();

  GlyphOverflowAndFallbackFontsMap text_box_data_map;

  // Now we position all of our text runs horizontally.
  if (!is_svg_root_inline_box)
    ComputeInlineDirectionPositionsForLine(
        line_box, line_info, bidi_runs.FirstRun(), trailing_space_run,
        end.AtEnd(), text_box_data_map, vertical_position_cache,
        word_measurements);

  // Now position our text runs vertically.
  ComputeBlockDirectionPositionsForLine(line_box, bidi_runs.FirstRun(),
                                        text_box_data_map,
                                        vertical_position_cache);

  // SVG text layout code computes vertical & horizontal positions on its own.
  // Note that we still need to execute computeVerticalPositionsForLine() as
  // it calls InlineTextBox::positionLineBox(), which tracks whether the box
  // contains reversed text or not. If we wouldn't do that editing and thus
  // text selection in RTL boxes would not work as expected.
  if (is_svg_root_inline_box) {
    DCHECK(IsSVGText());
    ToSVGRootInlineBox(line_box)->ComputePerCharacterLayoutInformation();
  }

  // Compute our overflow now.
  line_box->ComputeOverflow(line_box->LineTop(), line_box->LineBottom(),
                            text_box_data_map);

  return line_box;
}

static void DeleteLineRange(LineLayoutState& layout_state,
                            RootInlineBox* start_line,
                            RootInlineBox* stop_line = nullptr) {
  RootInlineBox* box_to_delete = start_line;
  while (box_to_delete && box_to_delete != stop_line) {
    // Note: deleteLineRange(firstRootBox()) is not identical to
    // deleteLineBoxTree(). deleteLineBoxTree uses nextLineBox() instead of
    // nextRootBox() when traversing.
    RootInlineBox* next = box_to_delete->NextRootBox();
    box_to_delete->DeleteLine();
    box_to_delete = next;
  }
}

void LayoutBlockFlow::LayoutRunsAndFloats(LineLayoutState& layout_state) {
  // We want to skip ahead to the first dirty line
  InlineBidiResolver resolver;
  RootInlineBox* start_line = DetermineStartPosition(layout_state, resolver);

  if (ContainsFloats())
    layout_state.SetLastFloat(floating_objects_->Set().back().get());

  // We also find the first clean line and extract these lines.  We will add
  // them back if we determine that we're able to synchronize after handling all
  // our dirty lines.
  InlineIterator clean_line_start;
  BidiStatus clean_line_bidi_status;
  if (!layout_state.IsFullLayout() && start_line)
    DetermineEndPosition(layout_state, start_line, clean_line_start,
                         clean_line_bidi_status);

  if (start_line)
    DeleteLineRange(layout_state, start_line);

  LayoutRunsAndFloatsInRange(layout_state, resolver, clean_line_start,
                             clean_line_bidi_status);
  LinkToEndLineIfNeeded(layout_state);
  MarkDirtyFloatsForPaintInvalidation(layout_state.Floats());
}

// Before restarting the layout loop with a new logicalHeight, remove all floats
// that were added and reset the resolver.
inline const InlineIterator& LayoutBlockFlow::RestartLayoutRunsAndFloatsInRange(
    LayoutUnit old_logical_height,
    LayoutUnit new_logical_height,
    FloatingObject* last_float_from_previous_line,
    InlineBidiResolver& resolver,
    const InlineIterator& old_end) {
  RemoveFloatingObjectsBelow(last_float_from_previous_line, old_logical_height);
  SetLogicalHeight(new_logical_height);
  resolver.SetPositionIgnoringNestedIsolates(old_end);
  return old_end;
}

void LayoutBlockFlow::AppendFloatsToLastLine(
    LineLayoutState& layout_state,
    const InlineIterator& clean_line_start,
    const InlineBidiResolver& resolver,
    const BidiStatus& clean_line_bidi_status) {
  const FloatingObjectSet& floating_object_set = floating_objects_->Set();
  FloatingObjectSetIterator it = floating_object_set.begin();
  FloatingObjectSetIterator end = floating_object_set.end();
  if (layout_state.LastFloat()) {
    FloatingObjectSetIterator last_float_iterator =
        floating_object_set.find(layout_state.LastFloat());
    DCHECK(last_float_iterator != end);
    ++last_float_iterator;
    it = last_float_iterator;
  }
  for (; it != end; ++it) {
    FloatingObject& floating_object = *it->get();
    // If we've reached the start of clean lines any remaining floating children
    // belong to them.
    if (clean_line_start.GetLineLayoutItem().IsEqual(
            floating_object.GetLayoutObject()) &&
        layout_state.EndLine()) {
      layout_state.SetEndLineMatched(layout_state.EndLineMatched() ||
                                     MatchedEndLine(layout_state, resolver,
                                                    clean_line_start,
                                                    clean_line_bidi_status));
      if (layout_state.EndLineMatched()) {
        layout_state.SetLastFloat(&floating_object);
        return;
      }
    }
    AppendFloatingObjectToLastLine(floating_object);
    DCHECK_EQ(floating_object.GetLayoutObject(),
              layout_state.Floats()[layout_state.FloatIndex()].object);
    // If a float's geometry has changed, give up on syncing with clean lines.
    if (layout_state.Floats()[layout_state.FloatIndex()].rect !=
        floating_object.FrameRect()) {
      // Delete all the remaining lines.
      DeleteLineRange(layout_state, layout_state.EndLine());
      layout_state.SetEndLine(nullptr);
    }
    layout_state.SetFloatIndex(layout_state.FloatIndex() + 1);
  }
  layout_state.SetLastFloat(!floating_object_set.IsEmpty()
                                ? floating_object_set.back().get()
                                : nullptr);
}

void LayoutBlockFlow::LayoutRunsAndFloatsInRange(
    LineLayoutState& layout_state,
    InlineBidiResolver& resolver,
    const InlineIterator& clean_line_start,
    const BidiStatus& clean_line_bidi_status) {
  const ComputedStyle& style_to_use = StyleRef();
  bool paginated =
      View()->GetLayoutState() && View()->GetLayoutState()->IsPaginated();
  bool recalculate_struts = layout_state.NeedsPaginationStrutRecalculation();
  LineMidpointState& line_midpoint_state = resolver.GetMidpointState();
  InlineIterator end_of_line = resolver.GetPosition();
  LayoutTextInfo layout_text_info;
  VerticalPositionCache vertical_position_cache;

  // Pagination may require us to delete and re-create a line due to floats.
  // When this happens, we need to know the old offset of the line, to calculate
  // the correct pagination strut.
  LayoutUnit deleted_line_old_offset = LayoutUnit::Min();

  LineBreaker line_breaker(LineLayoutBlockFlow(this));


  while (!end_of_line.AtEnd()) {
    // The runs from the previous line should have been cleaned up.
    DCHECK(!resolver.Runs().RunCount());

    // FIXME: Is this check necessary before the first iteration or can it be
    // moved to the end?
    if (layout_state.EndLine()) {
      layout_state.SetEndLineMatched(layout_state.EndLineMatched() ||
                                     MatchedEndLine(layout_state, resolver,
                                                    clean_line_start,
                                                    clean_line_bidi_status));
      if (layout_state.EndLineMatched()) {
        resolver.SetPosition(
            InlineIterator(resolver.GetPosition().Root(), nullptr, 0), 0);
        break;
      }
    }

    line_midpoint_state.Reset();

    layout_state.GetLineInfo().SetEmpty(true);
    layout_state.GetLineInfo().ResetRunsFromLeadingWhitespace();

    const InlineIterator previous_endof_line = end_of_line;
    bool is_new_uba_paragraph =
        layout_state.GetLineInfo().PreviousLineBrokeCleanly();
    FloatingObject* last_float_from_previous_line =
        (ContainsFloats()) ? floating_objects_->Set().back().get() : nullptr;

    WordMeasurements word_measurements;
    end_of_line =
        line_breaker.NextLineBreak(resolver, layout_state.GetLineInfo(),
                                   layout_text_info, word_measurements);
    layout_text_info.line_break_iterator_.ResetPriorContext();
    if (resolver.GetPosition().AtEnd()) {
      // FIXME: We shouldn't be creating any runs in nextLineBreak to begin
      // with! Once BidiRunList is separated from BidiResolver this will not be
      // needed.
      resolver.Runs().DeleteRuns();
      resolver.MarkCurrentRunEmpty();  // FIXME: This can probably be replaced
                                       // by an ASSERT (or just removed).
      resolver.SetPosition(
          InlineIterator(resolver.GetPosition().Root(), nullptr, 0), 0);
      break;
    }

    DCHECK(end_of_line != resolver.GetPosition());
    RootInlineBox* line_box = nullptr;

    // This is a short-cut for empty lines.
    if (layout_state.GetLineInfo().IsEmpty()) {
      DCHECK_EQ(deleted_line_old_offset, LayoutUnit::Min());
      if (LastRootBox())
        LastRootBox()->SetLineBreakInfo(end_of_line.GetLineLayoutItem(),
                                        end_of_line.Offset(),
                                        resolver.Status());
      resolver.Runs().DeleteRuns();
    } else {
      VisualDirectionOverride override =
          (style_to_use.RtlOrdering() == EOrder::kVisual
               ? (style_to_use.Direction() == TextDirection::kLtr
                      ? kVisualLeftToRightOverride
                      : kVisualRightToLeftOverride)
               : kNoVisualOverride);
      if (is_new_uba_paragraph &&
          style_to_use.GetUnicodeBidi() == UnicodeBidi::kPlaintext &&
          !resolver.Context()->Parent()) {
        TextDirection direction = DeterminePlaintextDirectionality(
            resolver.GetPosition().Root(),
            resolver.GetPosition().GetLineLayoutItem(),
            resolver.GetPosition().Offset());
        resolver.SetStatus(
            BidiStatus(direction, IsOverride(style_to_use.GetUnicodeBidi())));
      }

      ETextAlign text_align = TextAlignmentForLine(
          !end_of_line.AtEnd() &&
          !layout_state.GetLineInfo().PreviousLineBrokeCleanly());
      layout_state.GetLineInfo().SetTextAlign(text_align);
      resolver.SetNeedsTrailingSpace(
          TextAlignmentNeedsTrailingSpace(text_align, style_to_use));

      // FIXME: This ownership is reversed. We should own the BidiRunList and
      // pass it to createBidiRunsForLine.
      BidiRunList<BidiRun>& bidi_runs = resolver.Runs();
      ConstructBidiRunsForLine(
          resolver, bidi_runs, end_of_line, override,
          layout_state.GetLineInfo().PreviousLineBrokeCleanly(),
          is_new_uba_paragraph);
      DCHECK(resolver.GetPosition() == end_of_line);

      BidiRun* trailing_space_run = resolver.TrailingSpaceRun();

      if (bidi_runs.RunCount() && line_breaker.LineWasHyphenated())
        bidi_runs.LogicallyLastRun()->has_hyphen_ = true;

      // Now that the runs have been ordered, we create the line boxes.
      // At the same time we figure out where border/padding/margin should be
      // applied for
      // inline flow boxes.

      LayoutUnit old_logical_height = LogicalHeight();
      line_box = CreateLineBoxesFromBidiRuns(
          resolver.Status().context->Level(), bidi_runs, end_of_line,
          layout_state.GetLineInfo(), vertical_position_cache,
          trailing_space_run, word_measurements);

      bidi_runs.DeleteRuns();
      resolver.MarkCurrentRunEmpty();  // FIXME: This can probably be replaced
                                       // by an ASSERT (or just removed).

      // If we decided to re-create the line due to pagination, we better have a
      // new line now.
      DCHECK(line_box || deleted_line_old_offset == LayoutUnit::Min());

      if (line_box) {
        line_box->SetLineBreakInfo(end_of_line.GetLineLayoutItem(),
                                   end_of_line.Offset(), resolver.Status());
        if (recalculate_struts) {
          LayoutUnit adjustment;
          AdjustLinePositionForPagination(*line_box, adjustment);
          if (adjustment) {
            DCHECK_GT(adjustment, LayoutUnit());
            IndentTextOrNot indent = layout_state.GetLineInfo().IsFirstLine()
                                         ? kIndentText
                                         : kDoNotIndentText;
            LayoutUnit old_line_width =
                AvailableLogicalWidthForLine(old_logical_height, indent);
            LayoutUnit old_logical_top = line_box->LogicalTop();
            line_box->MoveInBlockDirection(adjustment);
            if (AvailableLogicalWidthForLine(old_logical_height + adjustment,
                                             indent) != old_line_width) {
              // We have to delete this line, remove all floats that got added,
              // and let line layout re-run. Store the offset, so that when we
              // eventually get to a location where the line can fit, we
              // calculate the correct pagination strut. Store the offset the
              // first time this happens for a line; it may happen several
              // times. Example: a line is too tall to fit in the current
              // fragmentainer, so we attempt to lay it out into the next
              // one. In the next fragmentainer there may be a float that's too
              // wide to fit anything beside it, so the line will have to go
              // below it. But there may not be enough space to fit the line
              // below the float, so we'll have to skip to the fragmentainer
              // after that, and retry *again* there. And so on.
              if (deleted_line_old_offset == LayoutUnit::Min())
                deleted_line_old_offset = old_logical_top;
              DCHECK_NE(deleted_line_old_offset, LayoutUnit::Min());
              // We're also going to assume that we're right after a page
              // break when re-creating this line, so it better be so.
              DCHECK(line_box->IsFirstAfterPageBreak());
              line_box->DeleteLine();
              line_box = nullptr;
              end_of_line = RestartLayoutRunsAndFloatsInRange(
                  old_logical_height, old_logical_height + adjustment,
                  last_float_from_previous_line, resolver, previous_endof_line);
            }
          }
          if (line_box &&
              (adjustment || deleted_line_old_offset != LayoutUnit::Min())) {
            if (deleted_line_old_offset != LayoutUnit::Min()) {
              // This is a line that got re-created because it got pushed to the
              // next fragmentainer, and there were floats in the vicinity that
              // affected the available width, so we had to re-lay out and
              // re-paginate. We've finally got to a place where the line
              // fits. Calculate a new pagination strut.
              LayoutUnit strut =
                  line_box->LogicalTop() - deleted_line_old_offset;
              line_box->SetIsFirstAfterPageBreak(true);
              line_box->SetPaginationStrut(strut);
              deleted_line_old_offset = LayoutUnit::Min();
            }
            // If the line got adjusted (just now, or in a previous run), we
            // need to encompass its logical bottom in the logical height of the
            // block.
            SetLogicalHeight(line_box->LineBottomWithLeading());
          }
        }
      }
    }

    if (deleted_line_old_offset == LayoutUnit::Min()) {
      for (const auto& positioned_object : line_breaker.PositionedObjects()) {
        if (positioned_object.StyleRef().IsOriginalDisplayInlineType()) {
          // Auto-positioned "inline" out-of-flow objects have already been
          // positioned, but if we're paginated, or just ceased to be so, we
          // need to update their position now, since the line they "belong" to
          // may have been pushed by a pagination strut, or pulled back because
          // a pagination strut was removed.
          if (recalculate_struts && line_box)
            positioned_object.Layer()->SetStaticBlockPosition(
                line_box->LineTopWithLeading());
          continue;
        }
        SetStaticPositions(LineLayoutBlockFlow(this), positioned_object,
                           kDoNotIndentText);
      }

      if (!layout_state.GetLineInfo().IsEmpty())
        layout_state.GetLineInfo().SetFirstLine(false);
      ClearFloats(line_breaker.Clear());

      if (floating_objects_ && LastRootBox()) {
        InlineBidiResolver end_of_line_resolver;
        end_of_line_resolver.SetPosition(end_of_line,
                                         NumberOfIsolateAncestors(end_of_line));
        end_of_line_resolver.SetStatus(resolver.Status());
        AppendFloatsToLastLine(layout_state, clean_line_start,
                               end_of_line_resolver, clean_line_bidi_status);
      }
    }

    line_midpoint_state.Reset();
    resolver.SetPosition(end_of_line, NumberOfIsolateAncestors(end_of_line));
  }

  // The resolver runs should have been cleared, otherwise they're leaking.
  DCHECK(!resolver.Runs().RunCount());

  // In case we already adjusted the line positions during this layout to avoid
  // widows then we need to ignore the possibility of having a new widows
  // situation. Otherwise, we risk leaving empty containers which is against the
  // block fragmentation principles.
  if (paginated && StyleRef().Widows() > 1 && !DidBreakAtLineToAvoidWidow()) {
    // Check the line boxes to make sure we didn't create unacceptable widows.
    // However, we'll prioritize orphans - so nothing we do here should create
    // a new orphan.

    RootInlineBox* line_box = LastRootBox();

    // Count from the end of the block backwards, to see how many hanging
    // lines we have.
    RootInlineBox* first_line_in_block = FirstRootBox();
    int num_lines_hanging = 1;
    while (line_box && line_box != first_line_in_block &&
           !line_box->IsFirstAfterPageBreak()) {
      ++num_lines_hanging;
      line_box = line_box->PrevRootBox();
    }

    // If there were no breaks in the block, we didn't create any widows.
    if (!line_box || !line_box->IsFirstAfterPageBreak() ||
        line_box == first_line_in_block)
      return;

    if (num_lines_hanging < StyleRef().Widows()) {
      // We have detected a widow. Now we need to work out how many
      // lines there are on the previous page, and how many we need
      // to steal.
      int num_lines_needed = StyleRef().Widows() - num_lines_hanging;
      RootInlineBox* current_first_line_of_new_page = line_box;

      // Count the number of lines in the previous page.
      line_box = line_box->PrevRootBox();
      int num_lines_in_previous_page = 1;
      while (line_box && line_box != first_line_in_block &&
             !line_box->IsFirstAfterPageBreak()) {
        ++num_lines_in_previous_page;
        line_box = line_box->PrevRootBox();
      }

      // If there was an explicit value for orphans, respect that. If not, we
      // still shouldn't create a situation where we make an orphan bigger than
      // the initial value. This means that setting widows implies we also care
      // about orphans, but given the specification says the initial orphan
      // value is non-zero, this is ok. The author is always free to set orphans
      // explicitly as well.
      int orphans = StyleRef().Orphans();
      int num_lines_available = num_lines_in_previous_page - orphans;
      if (num_lines_available <= 0)
        return;

      int num_lines_to_take = std::min(num_lines_available, num_lines_needed);
      // Wind back from our first widowed line.
      line_box = current_first_line_of_new_page;
      for (int i = 0; i < num_lines_to_take; ++i)
        line_box = line_box->PrevRootBox();

      // We now want to break at this line. Remember for next layout and trigger
      // relayout.
      SetBreakAtLineToAvoidWidow(LineCount(line_box));
      MarkLinesDirtyInBlockRange(LastRootBox()->LineBottomWithLeading(),
                                 line_box->LineBottomWithLeading(), line_box);
    }
  }

  ClearDidBreakAtLineToAvoidWidow();
}

void LayoutBlockFlow::LinkToEndLineIfNeeded(LineLayoutState& layout_state) {
  if (layout_state.EndLine()) {
    if (layout_state.EndLineMatched()) {
      bool recalculate_struts =
          layout_state.NeedsPaginationStrutRecalculation();
      // Attach all the remaining lines, and then adjust their y-positions as
      // needed.
      LayoutUnit delta = LogicalHeight() - layout_state.EndLineLogicalTop();
      for (RootInlineBox* line = layout_state.EndLine(); line;
           line = line->NextRootBox()) {
        line->AttachLine();
        if (recalculate_struts) {
          delta -= line->PaginationStrut();
          AdjustLinePositionForPagination(*line, delta);
        }
        if (delta)
          line->MoveInBlockDirection(delta);
        if (Vector<LayoutBox*>* clean_line_floats = line->FloatsPtr()) {
          for (auto* box : *clean_line_floats) {
            FloatingObject* floating_object = InsertFloatingObject(*box);
            DCHECK(!floating_object->OriginatingLine());
            floating_object->SetOriginatingLine(line);
            LayoutUnit logical_top =
                LogicalTopForChild(*box) - MarginBeforeForChild(*box) + delta;
            PlaceNewFloats(logical_top);
          }
        }
      }
      SetLogicalHeight(LastRootBox()->LineBottomWithLeading());
    } else {
      // Delete all the remaining lines.
      DeleteLineRange(layout_state, layout_state.EndLine());
    }
  }

  // In case we have a float on the last line, it might not be positioned up to
  // now. This has to be done before adding in the bottom border/padding, or the
  // float will
  // include the padding incorrectly. -dwh
  if (PlaceNewFloats(LogicalHeight()) && LastRootBox())
    AppendFloatsToLastLine(layout_state, InlineIterator(), InlineBidiResolver(),
                           BidiStatus());
}

void LayoutBlockFlow::MarkDirtyFloatsForPaintInvalidation(
    Vector<FloatWithRect>& floats) {
  size_t float_count = floats.size();
  // Floats that did not have layout did not paint invalidations when we laid
  // them out. They would have painted by now if they had moved, but if they
  // stayed at (0, 0), they still need to be painted.
  for (size_t i = 0; i < float_count; ++i) {
    LayoutBox* f = floats[i].object;
    if (!floats[i].ever_had_layout) {
      if (!f->Location().X() && !f->Location().Y())
        f->SetShouldDoFullPaintInvalidation();
    }
    InsertFloatingObject(*f);
  }
  PlaceNewFloats(LogicalHeight());
}

// InlineMinMaxIterator is a class that will iterate over all layout objects
// that contribute to inline min/max width calculations. Note the following
// about the way it walks:
// (1) Positioned content is skipped (since it does not contribute to min/max
//     width of a block)
// (2) We do not drill into the children of floats or replaced elements, since
//     you can't break in the middle of such an element.
// (3) Inline flows (e.g., <a>, <span>, <i>) are walked twice, since each side
//     can have distinct borders/margin/padding that contribute to the min/max
//     width.
struct InlineMinMaxIterator {
  LayoutObject* parent;
  LayoutObject* current;
  bool end_of_inline;

  InlineMinMaxIterator(LayoutObject* p, bool end = false)
      : parent(p), current(p), end_of_inline(end) {}

  LayoutObject* Next();
};

LayoutObject* InlineMinMaxIterator::Next() {
  LayoutObject* result = nullptr;
  bool old_end_of_inline = end_of_inline;
  end_of_inline = false;
  while (current || current == parent) {
    if (!old_end_of_inline &&
        (current == parent ||
         (!current->IsFloating() && !current->IsAtomicInlineLevel() &&
          !current->IsOutOfFlowPositioned())))
      result = current->SlowFirstChild();

    if (!result) {
      // We hit the end of our inline. (It was empty, e.g., <span></span>.)
      if (!old_end_of_inline && current->IsLayoutInline()) {
        result = current;
        end_of_inline = true;
        break;
      }

      while (current && current != parent) {
        result = current->NextSibling();
        if (result)
          break;
        current = current->Parent();
        if (current && current != parent && current->IsLayoutInline()) {
          result = current;
          end_of_inline = true;
          break;
        }
      }
    }

    if (!result)
      break;

    if (!result->IsOutOfFlowPositioned() &&
        (result->IsText() || result->IsFloating() ||
         result->IsAtomicInlineLevel() || result->IsLayoutInline()))
      break;

    current = result;
    result = nullptr;
  }

  // Update our position.
  current = result;
  return current;
}

static LayoutUnit GetBPMWidth(LayoutUnit child_value, const Length& css_unit) {
  if (css_unit.IsFixed())
    return LayoutUnit(css_unit.Value());
  if (css_unit.IsAuto())
    return LayoutUnit();
  return child_value;
}

static LayoutUnit GetBorderPaddingMargin(const LayoutBoxModelObject& child,
                                         bool end_of_inline) {
  const ComputedStyle& child_style = child.StyleRef();
  if (end_of_inline) {
    return GetBPMWidth(child.MarginEnd(), child_style.MarginEnd()) +
           GetBPMWidth(child.PaddingEnd(), child_style.PaddingEnd()) +
           child.BorderEnd();
  }
  return GetBPMWidth(child.MarginStart(), child_style.MarginStart()) +
         GetBPMWidth(child.PaddingStart(), child_style.PaddingStart()) +
         child.BorderStart();
}

static inline void StripTrailingSpace(LayoutUnit& inline_max,
                                      LayoutUnit& inline_min,
                                      LayoutObject* trailing_space_child) {
  if (trailing_space_child && trailing_space_child->IsText()) {
    // Collapse away the trailing space at the end of a block by finding
    // the first white-space character and subtracting its width. Subsequent
    // white-space characters have been collapsed into the first one (which
    // can be either a space or a tab character).
    LayoutText* text = ToLayoutText(trailing_space_child);
    UChar trailing_whitespace_char = ' ';
    for (unsigned i = text->TextLength(); i > 0; i--) {
      UChar c = text->CharacterAt(i - 1);
      if (!Character::TreatAsSpace(c))
        break;
      trailing_whitespace_char = c;
    }

    // FIXME: This ignores first-line.
    const Font& font = text->StyleRef().GetFont();
    TextRun run =
        ConstructTextRun(font, &trailing_whitespace_char, 1, text->StyleRef(),
                         text->StyleRef().Direction());
    float space_width = font.Width(run);
    inline_max -= LayoutUnit::FromFloatCeil(
        space_width + font.GetFontDescription().WordSpacing());
    if (inline_min > inline_max)
      inline_min = inline_max;
  }
}

// When converting between floating point and LayoutUnits we risk losing
// precision with each conversion. When this occurs while accumulating our
// preferred widths, we can wind up with a line width that's larger than our
// maxPreferredWidth due to pure float accumulation.
static inline LayoutUnit AdjustFloatForSubPixelLayout(float value) {
  return LayoutUnit::FromFloatCeil(value);
}

static inline void AdjustMinMaxForInlineFlow(LayoutObject* child,
                                             bool end_of_inline,
                                             LayoutUnit& child_min,
                                             LayoutUnit& child_max) {
  // Add in padding/border/margin from the appropriate side of
  // the element.
  LayoutUnit bpm =
      GetBorderPaddingMargin(ToLayoutInline(*child), end_of_inline);
  child_min += bpm;
  child_max += bpm;
}

static inline void AdjustMarginForInlineReplaced(LayoutObject* child,
                                                 LayoutUnit& child_min,
                                                 LayoutUnit& child_max) {
  // Inline replaced elts add in their margins to their min/max values.
  const ComputedStyle& child_style = child->StyleRef();
  const Length& start_margin = child_style.MarginStart();
  const Length& end_margin = child_style.MarginEnd();
  LayoutUnit margins;
  if (start_margin.IsFixed())
    margins += AdjustFloatForSubPixelLayout(start_margin.Value());
  if (end_margin.IsFixed())
    margins += AdjustFloatForSubPixelLayout(end_margin.Value());
  child_min += margins;
  child_max += margins;
}

// FIXME: This function should be broken into something less monolithic.
// FIXME: The main loop here is very similar to LineBreaker::nextSegmentBreak.
// They can probably reuse code.
DISABLE_CFI_PERF
void LayoutBlockFlow::ComputeInlinePreferredLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) {
  LayoutUnit inline_max;
  LayoutUnit inline_min;

  const ComputedStyle& style_to_use = StyleRef();

  // If we are at the start of a line, we want to ignore all white-space.
  // Also strip spaces if we previously had text that ended in a trailing space.
  bool strip_front_spaces = true;
  LayoutObject* trailing_space_child = nullptr;

  // Firefox and Opera will allow a table cell to grow to fit an image inside it
  // under very specific cirucumstances (in order to match common WinIE
  // layouts). Not supporting the quirk has caused us to mis-layout some real
  // sites. (See Bugzilla 10517.)
  bool allow_images_to_break = !GetDocument().InQuirksMode() ||
                               !IsTableCell() ||
                               !style_to_use.LogicalWidth().IsIntrinsicOrAuto();

  bool auto_wrap, old_auto_wrap;
  auto_wrap = old_auto_wrap = style_to_use.AutoWrap();

  InlineMinMaxIterator child_iterator(this);

  // Only gets added to the max preffered width once.
  bool added_text_indent = false;
  // Signals the text indent was more negative than the min preferred width
  bool has_remaining_negative_text_indent = false;

  // Always resolve percentages to 0 when calculating preferred logical widths.
  LayoutUnit text_indent =
      MinimumValueForLength(style_to_use.TextIndent(), LayoutUnit());
  LayoutObject* prev_float = nullptr;
  bool is_prev_child_inline_flow = false;
  bool should_break_line_after_text = false;
  while (LayoutObject* child = child_iterator.Next()) {
    auto_wrap = child->IsAtomicInlineLevel()
                    ? child->Parent()->StyleRef().AutoWrap()
                    : child->StyleRef().AutoWrap();

    if (!child->IsBR()) {
      // Step One: determine whether or not we need to go ahead and
      // terminate our current line. Each discrete chunk can become
      // the new min-width, if it is the widest chunk seen so far, and
      // it can also become the max-width.
      //
      // Children fall into three categories:
      // (1) An inline flow object. These objects always have a min/max of 0,
      //     and are included in the iteration solely so that their margins can
      //     be added in.
      //
      // (2) An inline non-text non-flow object, e.g., an inline replaced
      //     element. These objects can always be on a line by themselves, so in
      //     this situation we need to go ahead and break the current line, and
      //     then add in our own margins and min/max width on its own line, and
      //     then terminate the line.
      //
      // (3) A text object. Text runs can have breakable characters at the
      //     start, the middle or the end. They may also lose whitespace off the
      //     front if we're already ignoring whitespace. In order to compute
      //     accurate min-width information, we need three pieces of
      //     information.
      //     (a) the min-width of the first non-breakable run. Should be 0 if
      //         the text string starts with whitespace.
      //     (b) the min-width of the last non-breakable run. Should be 0 if the
      //         text string ends with whitespace.
      //     (c) the min/max width of the string (trimmed for whitespace).
      //
      // If the text string starts with whitespace, then we need to go ahead and
      // terminate our current line (unless we're already in a whitespace
      // stripping mode.
      //
      // If the text string has a breakable character in the middle, but didn't
      // start with whitespace, then we add the width of the first non-breakable
      // run and then end the current line. We then need to use the intermediate
      // min/max width values (if any of them are larger than our current
      // min/max). We then look at the width of the last non-breakable run and
      // use that to start a new line (unless we end in whitespace).
      LayoutUnit child_min;
      LayoutUnit child_max;

      if (!child->IsText()) {
        if (child->IsBox() &&
            ToLayoutBox(child)->NeedsPreferredWidthsRecalculation()) {
          // We don't really know whether the containing block of this child
          // did change or is going to change size. However, this is our only
          // opportunity to make sure that it gets its min/max widths
          // calculated.
          child->SetPreferredLogicalWidthsDirty();
        }

        // Case (1) and (2). Inline replaced and inline flow elements.
        if (child->IsLayoutInline()) {
          AdjustMinMaxForInlineFlow(child, child_iterator.end_of_inline,
                                    child_min, child_max);
          inline_min += child_min;
          inline_max += child_max;
          child->ClearPreferredLogicalWidthsDirty();
        } else {
          AdjustMarginForInlineReplaced(child, child_min, child_max);
        }
      }

      if (!child->IsLayoutInline() && !child->IsText()) {
        // Case (2). Inline replaced elements and floats.
        // Go ahead and terminate the current line as far as
        // minwidth is concerned.
        LayoutUnit child_min_preferred_logical_width,
            child_max_preferred_logical_width;
        ComputeChildPreferredLogicalWidths(*child,
                                           child_min_preferred_logical_width,
                                           child_max_preferred_logical_width);
        child_min += child_min_preferred_logical_width;
        child_max += child_max_preferred_logical_width;

        bool clear_previous_float;
        if (child->IsFloating()) {
          if (prev_float) {
            EFloat f = prev_float->StyleRef().Floating(style_to_use);
            EClear c = child->StyleRef().Clear(style_to_use);
            clear_previous_float =
                ((f == EFloat::kLeft &&
                  (c == EClear::kBoth || c == EClear::kLeft)) ||
                 (f == EFloat::kRight &&
                  (c == EClear::kBoth || c == EClear::kRight)));
          } else {
            clear_previous_float = false;
          }
          prev_float = child;
        } else {
          clear_previous_float = false;
        }

        bool can_break_replaced_element =
            !child->IsImage() || allow_images_to_break;
        if ((can_break_replaced_element && (auto_wrap || old_auto_wrap) &&
             (!is_prev_child_inline_flow || should_break_line_after_text)) ||
            clear_previous_float) {
          min_logical_width = std::max(min_logical_width, inline_min);
          inline_min = LayoutUnit();
        }

        // If we're supposed to clear the previous float, then terminate
        // maxwidth as well.
        if (clear_previous_float) {
          max_logical_width = std::max(max_logical_width, inline_max);
          inline_max = LayoutUnit();
        }

        // Add in text-indent. This is added in only once.
        if (!added_text_indent && !child->IsFloating()) {
          child_min += text_indent;
          child_max += text_indent;

          if (child_min < LayoutUnit())
            text_indent = child_min;
          else
            added_text_indent = true;
        }

        // Add our width to the max.
        inline_max += std::max(LayoutUnit(), child_max);

        if (!auto_wrap || !can_break_replaced_element ||
            (is_prev_child_inline_flow && !should_break_line_after_text)) {
          if (child->IsFloating())
            min_logical_width = std::max(min_logical_width, child_min);
          else
            inline_min += child_min;
        } else {
          // Now check our line.
          min_logical_width = std::max(min_logical_width, child_min);

          // Now start a new line.
          inline_min = LayoutUnit();
        }

        if (auto_wrap && can_break_replaced_element &&
            is_prev_child_inline_flow) {
          min_logical_width = std::max(min_logical_width, inline_min);
          inline_min = LayoutUnit();
        }

        // We are no longer stripping whitespace at the start of
        // a line.
        if (!child->IsFloating()) {
          strip_front_spaces = false;
          trailing_space_child = nullptr;
        }
      } else if (child->IsText()) {
        // Case (3). Text.
        LayoutText* t = ToLayoutText(child);

        if (t->IsWordBreak()) {
          min_logical_width = std::max(min_logical_width, inline_min);
          inline_min = LayoutUnit();
          continue;
        }

        // Determine if we have a breakable character. Pass in
        // whether or not we should ignore any spaces at the front
        // of the string. If those are going to be stripped out,
        // then they shouldn't be considered in the breakable char
        // check.
        bool has_breakable_char, has_break;
        LayoutUnit first_line_min_width, last_line_min_width;
        bool has_breakable_start, has_breakable_end;
        LayoutUnit first_line_max_width, last_line_max_width;
        t->TrimmedPrefWidths(
            inline_max, first_line_min_width, has_breakable_start,
            last_line_min_width, has_breakable_end, has_breakable_char,
            has_break, first_line_max_width, last_line_max_width, child_min,
            child_max, strip_front_spaces, style_to_use.Direction());

        // This text object will not be laid out, but it may still provide a
        // breaking opportunity.
        if (!has_break && !child_max) {
          if (auto_wrap && (has_breakable_start || has_breakable_end)) {
            min_logical_width = std::max(min_logical_width, inline_min);
            inline_min = LayoutUnit();
          }
          continue;
        }

        if (strip_front_spaces)
          trailing_space_child = child;
        else
          trailing_space_child = nullptr;

        // Add in text-indent. This is added in only once.
        LayoutUnit ti;
        if (!added_text_indent || has_remaining_negative_text_indent) {
          ti = text_indent;
          child_min += ti;
          first_line_min_width += ti;

          // It the text indent negative and larger than the child minimum, we
          // re-use the remainder in future minimum calculations, but using the
          // negative value again on the maximum will lead to under-counting the
          // max pref width.
          if (!added_text_indent) {
            child_max += ti;
            first_line_max_width += ti;
            added_text_indent = true;
          }

          if (child_min < LayoutUnit()) {
            text_indent = child_min;
            has_remaining_negative_text_indent = true;
          }
        }

        // If we have no breakable characters at all,
        // then this is the easy case. We add ourselves to the current
        // min and max and continue.
        if (!has_breakable_char) {
          inline_min += child_min;
        } else {
          if (has_breakable_start) {
            min_logical_width = std::max(min_logical_width, inline_min);
          } else {
            inline_min += first_line_min_width;
            min_logical_width = std::max(min_logical_width, inline_min);
            child_min -= ti;
          }

          inline_min = child_min;

          if (has_breakable_end) {
            min_logical_width = std::max(min_logical_width, inline_min);
            inline_min = LayoutUnit();
            should_break_line_after_text = false;
          } else {
            min_logical_width = std::max(min_logical_width, inline_min);
            inline_min = last_line_min_width;
            should_break_line_after_text = true;
          }
        }

        if (has_break) {
          inline_max += first_line_max_width;
          max_logical_width = std::max(max_logical_width, inline_max);
          max_logical_width = std::max(max_logical_width, child_max);
          inline_max = last_line_max_width;
          added_text_indent = true;
        } else {
          inline_max += std::max(LayoutUnit(), child_max);
        }
      }

      // Ignore spaces after a list marker.
      if (child->IsListMarkerIncludingNG())
        strip_front_spaces = true;
    } else {
      min_logical_width = std::max(min_logical_width, inline_min);
      max_logical_width = std::max(max_logical_width, inline_max);
      inline_min = inline_max = LayoutUnit();
      strip_front_spaces = true;
      trailing_space_child = nullptr;
      added_text_indent = true;
    }

    if (!child->IsText() && child->IsLayoutInline())
      is_prev_child_inline_flow = true;
    else
      is_prev_child_inline_flow = false;

    old_auto_wrap = auto_wrap;
  }

  if (style_to_use.CollapseWhiteSpace())
    StripTrailingSpace(inline_max, inline_min, trailing_space_child);

  min_logical_width = std::max(min_logical_width, inline_min);
  max_logical_width = std::max(max_logical_width, inline_max);
}

static bool IsInlineWithOutlineAndContinuation(const LayoutObject& o) {
  return o.IsLayoutInline() && o.StyleRef().HasOutline() &&
         !o.IsElementContinuation() && ToLayoutInline(o).Continuation();
}

bool LayoutBlockFlow::ShouldTruncateOverflowingText() const {
  const LayoutObject* object_to_check = this;
  if (IsAnonymousBlock()) {
    const LayoutObject* parent = Parent();
    if (!parent || !parent->BehavesLikeBlockContainer())
      return false;
    object_to_check = parent;
  }
  return object_to_check->HasOverflowClip() &&
         object_to_check->StyleRef().TextOverflow() != ETextOverflow::kClip;
}

DISABLE_CFI_PERF
void LayoutBlockFlow::LayoutInlineChildren(bool relayout_children,
                                           LayoutUnit after_edge) {
  // Figure out if we should clear out our line boxes.
  // FIXME: Handle resize eventually!
  bool is_full_layout =
      !FirstLineBox() || SelfNeedsLayout() || relayout_children;
  LineLayoutState layout_state(is_full_layout);

  if (is_full_layout) {
    // Ensure the old line boxes will be erased.
    if (FirstLineBox())
      SetShouldDoFullPaintInvalidation();
    LineBoxes()->DeleteLineBoxes();
  } else if (const LayoutState* box_state = View()->GetLayoutState()) {
    // We'll attempt to keep the line boxes that we have, but we may need to
    // add, change or remove pagination struts in front of them.
    if (box_state->IsPaginated() || box_state->PaginationStateChanged())
      layout_state.SetNeedsPaginationStrutRecalculation();
  }

  if (FirstChild()) {
    // In full layout mode, clear the line boxes of children upfront. Otherwise,
    // siblings can run into stale root lineboxes during layout. Then layout
    // the replaced elements later. In partial layout mode, line boxes are not
    // deleted and only dirtied. In that case, we can layout the replaced
    // elements at the same time.
    Vector<LayoutBox*> atomic_inline_children;
    for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.AtEnd();
         walker.Advance()) {
      LayoutObject* o = walker.Current().GetLayoutObject();

      // Layout may change LayoutInline's LinesBoundingBox() which affects
      // MaskClip.
      if (o->IsLayoutInline() && o->HasMask())
        o->SetNeedsPaintPropertyUpdate();

      if (!layout_state.HasInlineChild() && o->IsInline())
        layout_state.SetHasInlineChild(true);

      if (o->IsAtomicInlineLevel() || o->IsFloating() ||
          o->IsOutOfFlowPositioned()) {
        LayoutBox* box = ToLayoutBox(o);
        box->SetShouldCheckForPaintInvalidation();

        UpdateBlockChildDirtyBitsBeforeLayout(relayout_children, *box);

        if (o->IsOutOfFlowPositioned()) {
          o->ContainingBlock()->InsertPositionedObject(box);
        } else if (o->IsFloating()) {
          layout_state.Floats().push_back(FloatWithRect(box));
          if (box->NeedsLayout()) {
            // Be sure to at least mark the first line affected by the float as
            // dirty, so that the float gets relaid out. Otherwise we'll miss
            // it. After float layout, if it turns out that it changed size,
            // any lines after this line will be deleted and relaid out.
            DirtyLinesFromChangedChild(box, kMarkOnlyThis);
          }
        } else if (is_full_layout || o->NeedsLayout()) {
          // Atomic inline.
          DCHECK(o->IsAtomicInlineLevel());
          box->DirtyLineBoxes(is_full_layout);
          // In full layout mode, defer laying out inline chlidren (adding any
          // |InlineBox|es to |LineBoxes()|) to after all calls to
          // |DirtyLineBoxesForObject()| is done.
          if (is_full_layout)
            atomic_inline_children.push_back(box);
          else
            o->LayoutIfNeeded();
        }
      } else if (o->IsText() ||
                 (o->IsLayoutInline() && !walker.AtEndOfInline())) {
        if (!o->IsText())
          ToLayoutInline(o)->UpdateAlwaysCreateLineBoxes(
              layout_state.IsFullLayout());
        if (layout_state.IsFullLayout() || o->SelfNeedsLayout()) {
          // In full layout mode, line boxes are deleted at the beginning of
          // this function. It is critical to keep them empty here, because
          // |DirtyLineBoxesForObject()| can destroy |InlineTextBox| without
          // unlinking them from |LineBoxes()|.
          DCHECK(!is_full_layout || !LineBoxes()->First());
          DirtyLineBoxesForObject(o, layout_state.IsFullLayout());
        }
        o->ClearNeedsLayout();
      }

      if (IsInlineWithOutlineAndContinuation(*o))
        SetContainsInlineWithOutlineAndContinuation(true);
    }

    // Now all |DirtyLineBoxesForObject()| is done. We can safely start
    // adding |InlineBox|es to |LineBoxes()|.
    DCHECK(!is_full_layout || !LineBoxes()->First());
    for (LayoutBox* atomic_inline_child : atomic_inline_children) {
      atomic_inline_child->LayoutIfNeeded();
#if DCHECK_IS_ON()
      // |LayoutIfNeeded| should not mark itself and its ancestors to
      // |NeedsLayout|.
      for (const LayoutObject* parent = atomic_inline_child;
           parent && parent != this; parent = parent->Parent()) {
        DCHECK(!parent->SelfNeedsLayout());
        DCHECK(!parent->NeedsLayout() ||
               parent->LayoutBlockedByDisplayLock(
                   DisplayLockLifecycleTarget::kChildren));
      }
#endif
    }

    LayoutRunsAndFloats(layout_state);
  }

  // Expand the last line to accommodate Ruby and emphasis marks.
  int last_line_annotations_adjustment = 0;
  if (LastRootBox()) {
    LayoutUnit lowest_allowed_position =
        std::max(LastRootBox()->LineBottom(), LogicalHeight() + PaddingAfter());
    if (!StyleRef().IsFlippedLinesWritingMode())
      last_line_annotations_adjustment =
          LastRootBox()
              ->ComputeUnderAnnotationAdjustment(lowest_allowed_position)
              .ToInt();
    else
      last_line_annotations_adjustment =
          LastRootBox()
              ->ComputeOverAnnotationAdjustment(lowest_allowed_position)
              .ToInt();
  }

  // Now add in the bottom border/padding.
  SetLogicalHeight(LogicalHeight() + last_line_annotations_adjustment +
                   after_edge);

  if (!FirstLineBox() && HasLineIfEmpty())
    SetLogicalHeight(
        LogicalHeight() +
        LineHeight(true,
                   IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine,
                   kPositionOfInteriorLineBoxes));

  if (ShouldTruncateOverflowingText())
    CheckLinesForTextOverflow();

  // Ensure the new line boxes will be painted.
  if (is_full_layout && FirstLineBox())
    SetShouldDoFullPaintInvalidation();
}

RootInlineBox* LayoutBlockFlow::DetermineStartPosition(
    LineLayoutState& layout_state,
    InlineBidiResolver& resolver) {
  RootInlineBox* curr = nullptr;
  RootInlineBox* last = nullptr;
  RootInlineBox* first_line_box_with_break_and_clearance = nullptr;

  // FIXME: This entire float-checking block needs to be broken into a new
  // function.
  if (!layout_state.IsFullLayout()) {
    // Paginate all of the clean lines.
    bool recalculate_struts = layout_state.NeedsPaginationStrutRecalculation();
    LayoutUnit pagination_delta;
    for (curr = FirstRootBox(); curr && !curr->IsDirty();
         curr = curr->NextRootBox()) {
      if (recalculate_struts) {
        pagination_delta -= curr->PaginationStrut();
        AdjustLinePositionForPagination(*curr, pagination_delta);
        if (pagination_delta) {
          if (ContainsFloats() || !layout_state.Floats().IsEmpty()) {
            // FIXME: Do better eventually.  For now if we ever shift because of
            // pagination and floats are present just go to a full layout.
            layout_state.MarkForFullLayout();
            break;
          }
          curr->MoveInBlockDirection(pagination_delta);
        }
      }

      // If the linebox breaks cleanly and with clearance then dirty from at
      // least this point onwards so that we can clear the correct floats
      // without difficulty.
      if (!first_line_box_with_break_and_clearance &&
          LineBoxHasBRWithClearance(curr))
        first_line_box_with_break_and_clearance = curr;

      if (layout_state.IsFullLayout())
        break;
    }
  }

  if (layout_state.IsFullLayout()) {
    // If we encountered a new float and have inline children, mark ourself to
    // force us to issue paint invalidations.
    if (layout_state.HasInlineChild() && !SelfNeedsLayout()) {
      SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kFloatDescendantChanged, kMarkOnlyThis);
    }

    DeleteLineBoxTree();
    curr = nullptr;
    DCHECK(!FirstLineBox());
    DCHECK(!LastLineBox());
  } else {
    if (first_line_box_with_break_and_clearance)
      curr = first_line_box_with_break_and_clearance;
    if (curr) {
      // We have a dirty line.
      if (RootInlineBox* prev_root_box = curr->PrevRootBox()) {
        // We have a previous line.
        if (!prev_root_box->EndsWithBreak() || !prev_root_box->LineBreakObj() ||
            (prev_root_box->LineBreakObj().IsText() &&
             prev_root_box->LineBreakPos() >=
                 ToLayoutText(prev_root_box->LineBreakObj().GetLayoutObject())
                     ->TextLength())) {
          // The previous line didn't break cleanly or broke at a newline
          // that has been deleted, so treat it as dirty too.
          curr = prev_root_box;
        }
      }
    } else {
      // No dirty lines were found.
      // If the last line didn't break cleanly, treat it as dirty.
      if (LastRootBox() && !LastRootBox()->EndsWithBreak())
        curr = LastRootBox();
    }

    // If we have no dirty lines, then last is just the last root box.
    last = curr ? curr->PrevRootBox() : LastRootBox();
  }

  unsigned num_clean_floats = 0;
  if (!layout_state.Floats().IsEmpty()) {
    // Restore floats from clean lines.
    RootInlineBox* line = FirstRootBox();
    while (line != curr) {
      if (Vector<LayoutBox*>* clean_line_floats = line->FloatsPtr()) {
        for (auto* box : *clean_line_floats) {
          FloatingObject* floating_object = InsertFloatingObject(*box);
          DCHECK(!floating_object->OriginatingLine());
          floating_object->SetOriginatingLine(line);
          LayoutUnit logical_top =
              LogicalTopForChild(*box) - MarginBeforeForChild(*box);
          PlaceNewFloats(logical_top);
          DCHECK_EQ(layout_state.Floats()[num_clean_floats].object, box);
          num_clean_floats++;
        }
      }
      line = line->NextRootBox();
    }
  }
  layout_state.SetFloatIndex(num_clean_floats);

  layout_state.GetLineInfo().SetFirstLine(!last);
  layout_state.GetLineInfo().SetPreviousLineBrokeCleanly(!last ||
                                                         last->EndsWithBreak());

  if (last) {
    SetLogicalHeight(last->LineBottomWithLeading());
    InlineIterator iter = InlineIterator(LineLayoutBlockFlow(this),
                                         LineLayoutItem(last->LineBreakObj()),
                                         last->LineBreakPos());
    resolver.SetPosition(iter, NumberOfIsolateAncestors(iter));
    resolver.SetStatus(last->LineBreakBidiStatus());
  } else {
    TextDirection direction = StyleRef().Direction();
    if (StyleRef().GetUnicodeBidi() == UnicodeBidi::kPlaintext)
      direction = DeterminePlaintextDirectionality(LineLayoutItem(this));
    resolver.SetStatus(
        BidiStatus(direction, IsOverride(StyleRef().GetUnicodeBidi())));
    InlineIterator iter = InlineIterator(
        LineLayoutBlockFlow(this),
        BidiFirstSkippingEmptyInlines(LineLayoutBlockFlow(this),
                                      resolver.Runs(), &resolver),
        0);
    resolver.SetPosition(iter, NumberOfIsolateAncestors(iter));
  }
  return curr;
}

bool LayoutBlockFlow::LineBoxHasBRWithClearance(RootInlineBox* curr) {
  // If the linebox breaks cleanly and with clearance then dirty from at least
  // this point onwards so that we can clear the correct floats without
  // difficulty.
  if (!curr->EndsWithBreak())
    return false;
  InlineBox* last_box = StyleRef().IsLeftToRightDirection()
                            ? curr->LastLeafChild()
                            : curr->FirstLeafChild();
  return last_box && last_box->GetLineLayoutItem().IsBR() &&
         last_box->GetLineLayoutItem().StyleRef().HasClear();
}

void LayoutBlockFlow::DetermineEndPosition(LineLayoutState& layout_state,
                                           RootInlineBox* start_line,
                                           InlineIterator& clean_line_start,
                                           BidiStatus& clean_line_bidi_status) {
  DCHECK(!layout_state.EndLine());
  RootInlineBox* last = nullptr;
  bool previous_was_clean = false;
  for (RootInlineBox* curr = start_line->NextRootBox(); curr;
       curr = curr->NextRootBox()) {
    if (!curr->IsDirty() && LineBoxHasBRWithClearance(curr))
      return;

    // A line is considered clean when it's not marked dirty, AND it either
    // doesn't contain floats, or follows another clean line. The legacy line
    // layout engine has issues with handling floats at line boundaries,
    // potentially resulting in a float belonging to two different lines,
    // causing all kinds of misery.
    if (curr->IsDirty() || (curr->FloatsPtr() && !previous_was_clean)) {
      last = nullptr;
      previous_was_clean = false;
    } else {
      if (!last)
        last = curr;
      previous_was_clean = true;
    }
  }

  if (!last)
    return;

  // At this point, |last| is the first line in a run of clean lines that ends
  // with the last line in the block.

  RootInlineBox* prev = last->PrevRootBox();
  clean_line_start =
      InlineIterator(LineLayoutItem(this), LineLayoutItem(prev->LineBreakObj()),
                     prev->LineBreakPos());
  clean_line_bidi_status = prev->LineBreakBidiStatus();
  layout_state.SetEndLineLogicalTop(prev->LineBottomWithLeading());

  for (RootInlineBox* line = last; line; line = line->NextRootBox())
    line->ExtractLine();  // Disconnect all line boxes from their layout objects
                          // while preserving their connections to one another.

  layout_state.SetEndLine(last);
}

bool LayoutBlockFlow::CheckPaginationAndFloatsAtEndLine(
    LineLayoutState& layout_state) {
  if (!floating_objects_ || !layout_state.EndLine())
    return true;

  LayoutUnit line_delta = LogicalHeight() - layout_state.EndLineLogicalTop();

  if (layout_state.NeedsPaginationStrutRecalculation()) {
    // Check all lines from here to the end, and see if the hypothetical new
    // position for the lines will result
    // in a different available line width.
    for (RootInlineBox* line_box = layout_state.EndLine(); line_box;
         line_box = line_box->NextRootBox()) {
      // This isn't the real move we're going to do, so don't update the line
      // box's pagination strut yet.
      LayoutUnit old_pagination_strut = line_box->PaginationStrut();
      line_delta -= old_pagination_strut;
      AdjustLinePositionForPagination(*line_box, line_delta);
      line_box->SetPaginationStrut(old_pagination_strut);
    }
  }
  if (!line_delta)
    return true;

  // See if any floats end in the range along which we want to shift the lines
  // vertically.
  LayoutUnit logical_top =
      std::min(LogicalHeight(), layout_state.EndLineLogicalTop());

  RootInlineBox* last_line = layout_state.EndLine();
  while (RootInlineBox* next_line = last_line->NextRootBox())
    last_line = next_line;

  LayoutUnit logical_bottom =
      last_line->LineBottomWithLeading() + AbsoluteValue(line_delta);

  const FloatingObjectSet& floating_object_set = floating_objects_->Set();
  FloatingObjectSetIterator end = floating_object_set.end();
  for (FloatingObjectSetIterator it = floating_object_set.begin(); it != end;
       ++it) {
    const FloatingObject& floating_object = *it->get();
    if (LogicalBottomForFloat(floating_object) >= logical_top &&
        LogicalBottomForFloat(floating_object) < logical_bottom)
      return false;
  }

  return true;
}

bool LayoutBlockFlow::MatchedEndLine(LineLayoutState& layout_state,
                                     const InlineBidiResolver& resolver,
                                     const InlineIterator& end_line_start,
                                     const BidiStatus& end_line_status) {
  if (resolver.GetPosition() == end_line_start) {
    if (resolver.Status() != end_line_status)
      return false;

    return CheckPaginationAndFloatsAtEndLine(layout_state);
  }

  // The first clean line doesn't match, but we can check a handful of following
  // lines to try to match back up.
  // The # of lines we're willing to match against.
  constexpr int kNumLines = 8;
  RootInlineBox* original_end_line = layout_state.EndLine();
  RootInlineBox* line = original_end_line;
  for (int i = 0; i < kNumLines && line; i++, line = line->NextRootBox()) {
    if (line->LineBreakObj() == resolver.GetPosition().GetLineLayoutItem() &&
        line->LineBreakPos() == resolver.GetPosition().Offset()) {
      // We have a match.
      if (line->LineBreakBidiStatus() != resolver.Status())
        return false;  // ...but the bidi state doesn't match.

      bool matched = false;
      RootInlineBox* result = line->NextRootBox();
      layout_state.SetEndLine(result);
      if (result) {
        layout_state.SetEndLineLogicalTop(line->LineBottomWithLeading());
        matched = CheckPaginationAndFloatsAtEndLine(layout_state);
      }

      // Now delete the lines that we failed to sync.
      DeleteLineRange(layout_state, original_end_line, result);
      return matched;
    }
  }

  return false;
}

bool LayoutBlockFlow::GeneratesLineBoxesForInlineChild(LayoutObject* inline_obj)

{
  DCHECK_EQ(inline_obj->Parent(), this);

  InlineIterator it(LineLayoutBlockFlow(this), LineLayoutItem(inline_obj), 0);
  // FIXME: We should pass correct value for WhitespacePosition.
  while (!it.AtEnd() && !RequiresLineBox(it))
    it.Increment();

  return !it.AtEnd();
}

void LayoutBlockFlow::AddVisualOverflowFromInlineChildren() {
  LayoutUnit end_padding = HasOverflowClip() ? PaddingEnd() : LayoutUnit();
  // FIXME: Need to find another way to do this, since scrollbars could show
  // when we don't want them to.
  if (HasOverflowClip() && !end_padding && GetNode() &&
      IsRootEditableElement(*GetNode()) && StyleRef().IsLeftToRightDirection())
    end_padding = LayoutUnit(1);

  if (const NGPaintFragment* paint_fragment = PaintFragment()) {
    for (const NGPaintFragment* child : paint_fragment->Children()) {
      if (child->HasSelfPaintingLayer())
        continue;
      PhysicalRect child_rect = child->InkOverflow();
      if (!child_rect.IsEmpty()) {
        child_rect.offset += child->Offset();
        AddContentsVisualOverflow(child_rect);
      }
    }
  } else if (const NGFragmentItems* items = FragmentItems()) {
    for (NGInlineCursor cursor(*items); cursor; cursor.MoveToNextSibling()) {
      const NGFragmentItem* child = cursor.CurrentItem();
      DCHECK(child);
      if (child->HasSelfPaintingLayer())
        continue;
      PhysicalRect child_rect = child->InkOverflow();
      if (!child_rect.IsEmpty()) {
        child_rect.offset += child->Offset();
        AddContentsVisualOverflow(child_rect);
      }
    }
  } else {
    for (RootInlineBox* curr = FirstRootBox(); curr;
         curr = curr->NextRootBox()) {
      LayoutRect visual_overflow =
          curr->VisualOverflowRect(curr->LineTop(), curr->LineBottom());
      AddContentsVisualOverflow(visual_overflow);
    }
  }

  if (!ContainsInlineWithOutlineAndContinuation())
    return;

  // Add outline rects of continuations of descendant inlines into visual
  // overflow of this block.
  PhysicalRect outline_bounds_of_all_continuations;
  for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.AtEnd();
       walker.Advance()) {
    const LayoutObject& o = *walker.Current().GetLayoutObject();
    if (!IsInlineWithOutlineAndContinuation(o))
      continue;

    Vector<PhysicalRect> outline_rects;
    ToLayoutInline(o).AddOutlineRectsForContinuations(
        outline_rects, PhysicalOffset(),
        o.OutlineRectsShouldIncludeBlockVisualOverflow());
    if (!outline_rects.IsEmpty()) {
      PhysicalRect outline_bounds = UnionRectEvenIfEmpty(outline_rects);
      outline_bounds.Inflate(LayoutUnit(o.StyleRef().OutlineOutsetExtent()));
      outline_bounds_of_all_continuations.Unite(outline_bounds);
    }
  }
  AddContentsVisualOverflow(outline_bounds_of_all_continuations);
}

void LayoutBlockFlow::AddLayoutOverflowFromInlineChildren() {
  LayoutUnit end_padding = HasOverflowClip() ? PaddingEnd() : LayoutUnit();
  // FIXME: Need to find another way to do this, since scrollbars could show
  // when we don't want them to.
  if (HasOverflowClip() && !end_padding && GetNode() &&
      IsRootEditableElement(*GetNode()) && StyleRef().IsLeftToRightDirection())
    end_padding = LayoutUnit(1);
  for (RootInlineBox* curr = FirstRootBox(); curr; curr = curr->NextRootBox())
    AddLayoutOverflow(curr->PaddedLayoutOverflowRect(end_padding));
}

void LayoutBlockFlow::DeleteEllipsisLineBoxes() {
  ETextAlign text_align = StyleRef().GetTextAlign();
  IndentTextOrNot indent_text = kIndentText;
  for (RootInlineBox* curr = FirstRootBox(); curr; curr = curr->NextRootBox()) {
    if (curr->HasEllipsisBox()) {
      curr->ClearTruncation();

      // Shift the line back where it belongs if we cannot accommodate an
      // ellipsis.
      LayoutUnit logical_left =
          LogicalLeftOffsetForLine(curr->LineTop(), indent_text);
      LayoutUnit available_logical_width =
          LogicalRightOffsetForLine(curr->LineTop(), kDoNotIndentText) -
          logical_left;
      LayoutUnit total_logical_width = curr->LogicalWidth();
      UpdateLogicalWidthForAlignment(text_align, curr, nullptr, logical_left,
                                     total_logical_width,
                                     available_logical_width, 0);

      curr->MoveInInlineDirection(logical_left - curr->LogicalLeft());
    }
    ClearTruncationOnAtomicInlines(curr);
    indent_text = kDoNotIndentText;
  }
}

void LayoutBlockFlow::ClearTruncationOnAtomicInlines(RootInlineBox* root) {
  bool ltr = StyleRef().IsLeftToRightDirection();
  InlineBox* first_child = ltr ? root->LastChild() : root->FirstChild();
  for (InlineBox* box = first_child; box;
       box = ltr ? box->PrevOnLine() : box->NextOnLine()) {
    if (!box->GetLineLayoutItem().IsAtomicInlineLevel() ||
        !box->GetLineLayoutItem().IsLayoutBlockFlow()) {
      continue;
    }

    if (!box->GetLineLayoutItem().IsTruncated())
      return;
    box->GetLineLayoutItem().SetIsTruncated(false);
  }
}

void LayoutBlockFlow::CheckLinesForTextOverflow() {
  // Determine the width of the ellipsis using the current font.
  const Font& font = StyleRef().GetFont();

  const size_t kFullStopStringLength = 3;
  const UChar kFullStopString[] = {kFullstopCharacter, kFullstopCharacter,
                                   kFullstopCharacter};
  DEFINE_STATIC_LOCAL(AtomicString, fullstop_character_str,
                      (kFullStopString, kFullStopStringLength));
  AtomicString selected_ellipsis_str(&kHorizontalEllipsisCharacter, 1);

  const Font& first_line_font = FirstLineStyle()->GetFont();
  // FIXME: We should probably not hard-code the direction here.
  // https://crbug.com/333004
  TextDirection ellipsis_direction = TextDirection::kLtr;
  float first_line_ellipsis_width = 0;
  float ellipsis_width = 0;

  // As per CSS3 http://www.w3.org/TR/2003/CR-css3-text-20030514/ sequence of
  // three Full Stops (002E) can be used.
  const SimpleFontData* font_data = first_line_font.PrimaryFont();
  DCHECK(font_data);
  if (font_data && font_data->GlyphForCharacter(kHorizontalEllipsisCharacter)) {
    first_line_ellipsis_width = first_line_font.Width(
        ConstructTextRun(first_line_font, &kHorizontalEllipsisCharacter, 1,
                         *FirstLineStyle(), ellipsis_direction));
  } else {
    selected_ellipsis_str = fullstop_character_str;
    first_line_ellipsis_width = first_line_font.Width(ConstructTextRun(
        first_line_font, kFullStopString, kFullStopStringLength,
        *FirstLineStyle(), ellipsis_direction));
  }
  ellipsis_width = (font == first_line_font) ? first_line_ellipsis_width : 0;

  if (!ellipsis_width) {
    const SimpleFontData* font_data = font.PrimaryFont();
    DCHECK(font_data);
    if (font_data &&
        font_data->GlyphForCharacter(kHorizontalEllipsisCharacter)) {
      ellipsis_width =
          font.Width(ConstructTextRun(font, &kHorizontalEllipsisCharacter, 1,
                                      StyleRef(), ellipsis_direction));
    } else {
      selected_ellipsis_str = fullstop_character_str;
      ellipsis_width = font.Width(
          ConstructTextRun(font, kFullStopString, kFullStopStringLength,
                           StyleRef(), ellipsis_direction));
    }
  }

  // For LTR text truncation, we want to get the right edge of our padding box,
  // and then we want to see if the right edge of a line box exceeds that.
  // For RTL, we use the left edge of the padding box and check the left edge of
  // the line box to see if it is less Include the scrollbar for overflow
  // blocks, which means we want to use "contentWidth()".
  bool ltr = StyleRef().IsLeftToRightDirection();
  ETextAlign text_align = StyleRef().GetTextAlign();
  IndentTextOrNot indent_text = kIndentText;
  for (RootInlineBox* curr = FirstRootBox(); curr; curr = curr->NextRootBox()) {
    LayoutUnit block_right_edge =
        LogicalRightOffsetForLine(curr->LineTop(), indent_text);
    LayoutUnit block_left_edge =
        LogicalLeftOffsetForLine(curr->LineTop(), indent_text);
    LayoutUnit line_box_edge = ltr ? curr->LogicalRight() : curr->LogicalLeft();
    if ((ltr && line_box_edge > block_right_edge) ||
        (!ltr && line_box_edge < block_left_edge)) {
      // This line spills out of our box in the appropriate direction. Now we
      // need to see if the line can be truncated.  In order for truncation to
      // be possible, the line must have sufficient space to accommodate our
      // truncation string, and no replaced elements (images, tables) can
      // overlap the ellipsis space.

      LayoutUnit width(indent_text == kIndentText ? first_line_ellipsis_width
                                                  : ellipsis_width);
      LayoutUnit block_edge = ltr ? block_right_edge : block_left_edge;
      InlineBox* box_truncation_starts_at = nullptr;
      if (curr->LineCanAccommodateEllipsis(ltr, block_edge, line_box_edge,
                                           width)) {
        LayoutUnit total_logical_width = curr->PlaceEllipsis(
            selected_ellipsis_str, ltr, block_left_edge, block_right_edge,
            width, LayoutUnit(), &box_truncation_starts_at);
        // We are only interested in the delta from the base position.
        LayoutUnit logical_left;
        LayoutUnit available_logical_width = block_right_edge - block_left_edge;
        UpdateLogicalWidthForAlignment(text_align, curr, nullptr, logical_left,
                                       total_logical_width,
                                       available_logical_width, 0);
        if (ltr)
          curr->MoveInInlineDirection(logical_left);
        else
          curr->MoveInInlineDirection(
              logical_left - (available_logical_width - total_logical_width));
      }
      TryPlacingEllipsisOnAtomicInlines(
          curr, LogicalRightOffsetForContent(), LogicalLeftOffsetForContent(),
          width, selected_ellipsis_str, box_truncation_starts_at);
    }

    indent_text = kDoNotIndentText;
  }
}

void LayoutBlockFlow::TryPlacingEllipsisOnAtomicInlines(
    RootInlineBox* root,
    LayoutUnit block_right_edge,
    LayoutUnit block_left_edge,
    LayoutUnit ellipsis_width,
    const AtomicString& selected_ellipsis_str,
    InlineBox* box_truncation_starts_at) {
  bool found_box = box_truncation_starts_at ? true : false;
  bool ltr = StyleRef().IsLeftToRightDirection();
  LayoutUnit logical_left_offset = block_left_edge;

  // Each atomic inline block (e.g. a <span>) inside a blockflow is managed by
  // an InlineBox that allows us to access the lineboxes that live inside the
  // atomic inline block.
  InlineBox* first_child = box_truncation_starts_at
                               ? box_truncation_starts_at
                               : (ltr ? root->FirstChild() : root->LastChild());
  for (InlineBox* box = first_child; box;
       box = ltr ? box->NextOnLine() : box->PrevOnLine()) {
    if (!box->GetLineLayoutItem().IsAtomicInlineLevel() ||
        !box->GetLineLayoutItem().IsLayoutBlockFlow()) {
      if (box->GetLineLayoutItem().IsText())
        logical_left_offset += box->LogicalWidth();
      continue;
    }

    if (found_box) {
      box->GetLineLayoutItem().SetIsTruncated(true);
      continue;
    }

    RootInlineBox* first_root_box =
        LineLayoutBlockFlow(box->GetLineLayoutItem()).FirstRootBox();
    if (!first_root_box)
      continue;

    bool placed_ellipsis = false;
    // Move the right edge of the block in so that we can test it against the
    // width of the root line boxes.  We don't resize or move the linebox to
    // respect text-align because it is the final one of a sequence on the line.
    if (ltr) {
      for (RootInlineBox* curr = first_root_box; curr;
           curr = curr->NextRootBox()) {
        LayoutUnit curr_logical_left =
            logical_left_offset + curr->LogicalLeft();
        LayoutUnit ellipsis_edge =
            curr_logical_left + curr->LogicalWidth() + ellipsis_width;
        if (ellipsis_edge <= block_right_edge)
          continue;
        InlineBox* truncation_box = nullptr;
        curr->PlaceEllipsis(selected_ellipsis_str, ltr, block_left_edge,
                            block_right_edge, ellipsis_width,
                            logical_left_offset, &truncation_box);
        placed_ellipsis = true;
      }
    } else {
      LayoutUnit max_root_box_width;
      for (RootInlineBox* curr = first_root_box; curr;
           curr = curr->NextRootBox()) {
        LayoutUnit ellipsis_edge =
            box->LogicalLeft() + curr->LogicalLeft() - ellipsis_width;
        if (ellipsis_edge >= block_left_edge)
          continue;
        // Root boxes can vary in width so move our offset out to allow
        // comparison with the right hand edge of the block.
        LayoutUnit logical_left_offset = box->LogicalLeft();
        max_root_box_width =
            std::max<LayoutUnit>(curr->LogicalWidth(), max_root_box_width);
        if (logical_left_offset < 0)
          logical_left_offset += max_root_box_width - curr->LogicalWidth();
        InlineBox* truncation_box = nullptr;
        curr->PlaceEllipsis(selected_ellipsis_str, ltr, block_left_edge,
                            block_right_edge, ellipsis_width,
                            logical_left_offset, &truncation_box);
        placed_ellipsis = true;
      }
    }
    found_box |= placed_ellipsis;
    logical_left_offset += box->LogicalWidth();
  }
}

void LayoutBlockFlow::MarkLinesDirtyInBlockRange(LayoutUnit logical_top,
                                                 LayoutUnit logical_bottom,
                                                 RootInlineBox* highest) {
  if (logical_top >= logical_bottom)
    return;

  RootInlineBox* lowest_dirty_line = LastRootBox();
  RootInlineBox* after_lowest = lowest_dirty_line;
  while (lowest_dirty_line &&
         lowest_dirty_line->LineBottomWithLeading() >= logical_bottom &&
         logical_bottom < LayoutUnit::Max()) {
    after_lowest = lowest_dirty_line;
    lowest_dirty_line = lowest_dirty_line->PrevRootBox();
  }

  while (after_lowest && after_lowest != highest &&
         (after_lowest->LineBottomWithLeading() >= logical_top ||
          after_lowest->LineBottomWithLeading() < LayoutUnit())) {
    after_lowest->MarkDirty();
    after_lowest = after_lowest->PrevRootBox();
  }
}

LayoutUnit LayoutBlockFlow::StartAlignedOffsetForLine(
    LayoutUnit position,
    IndentTextOrNot indent_text) {
  ETextAlign text_align = StyleRef().GetTextAlign();

  bool apply_indent_text;
  switch (text_align) {  // FIXME: Handle TAEND here
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      apply_indent_text = StyleRef().IsLeftToRightDirection();
      break;
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      apply_indent_text = !StyleRef().IsLeftToRightDirection();
      break;
    case ETextAlign::kStart:
      apply_indent_text = true;
      break;
    default:
      apply_indent_text = false;
  }

  if (apply_indent_text)
    return StartOffsetForLine(position, indent_text);

  // updateLogicalWidthForAlignment() handles the direction of the block so no
  // need to consider it here
  LayoutUnit total_logical_width;
  LayoutUnit logical_left =
      LogicalLeftOffsetForLine(LogicalHeight(), kDoNotIndentText);
  LayoutUnit available_logical_width =
      LogicalRightOffsetForLine(LogicalHeight(), kDoNotIndentText) -
      logical_left;
  UpdateLogicalWidthForAlignment(text_align, nullptr, nullptr, logical_left,
                                 total_logical_width, available_logical_width,
                                 0);

  if (!StyleRef().IsLeftToRightDirection())
    return LogicalWidth() - logical_left;
  return logical_left;
}

void LayoutBlockFlow::SetShouldDoFullPaintInvalidationForFirstLine() {
  DCHECK(ChildrenInline());
  if (RootInlineBox* first_root_box = FirstRootBox())
    first_root_box->SetShouldDoFullPaintInvalidationForFirstLine();
  else if (const NGPaintFragment* paint_fragment = PaintFragment())
    paint_fragment->SetShouldDoFullPaintInvalidationForFirstLine();
}

bool LayoutBlockFlow::PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
  // LayoutBlockFlow is in charge of paint invalidation of the first line.
  if (FirstLineBox())
    return false;

  return LayoutBlock::PaintedOutputOfObjectHasNoEffectRegardlessOfSize();
}

}  // namespace blink
