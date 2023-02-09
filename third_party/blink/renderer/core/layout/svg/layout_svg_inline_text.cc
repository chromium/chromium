/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"

#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/font_size_functions.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/text/bidi_character_run.h"
#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/text/text_run_iterator.h"

namespace blink {

// Turn tabs, newlines and carriage returns into spaces. In the future this
// should be removed in favor of letting the generic white-space code handle
// this.
static scoped_refptr<StringImpl> NormalizeWhitespace(
    scoped_refptr<StringImpl> string) {
  scoped_refptr<StringImpl> new_string = string->Replace('\t', ' ');
  new_string = new_string->Replace('\n', ' ');
  new_string = new_string->Replace('\r', ' ');
  return new_string;
}

LayoutSVGInlineText::LayoutSVGInlineText(Node* n,
                                         scoped_refptr<StringImpl> string)
    : LayoutText(n, NormalizeWhitespace(std::move(string))),
      scaling_factor_(1) {}

void LayoutSVGInlineText::TextDidChange() {
  NOT_DESTROYED();
  SetTextInternal(NormalizeWhitespace(GetText().Impl()));
  LayoutText::TextDidChange();
  LayoutSVGText::NotifySubtreeStructureChanged(
      this, layout_invalidation_reason::kTextChanged);

  if (StyleRef().UsedUserModify() != EUserModify::kReadOnly)
    UseCounter::Count(GetDocument(), WebFeature::kSVGTextEdited);
}

void LayoutSVGInlineText::StyleDidChange(StyleDifference diff,
                                         const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutText::StyleDidChange(diff, old_style);
  UpdateScaledFont();

  const bool new_collapse = StyleRef().CollapseWhiteSpace();
  const bool old_collapse = old_style && old_style->CollapseWhiteSpace();
  if (old_collapse != new_collapse) {
    ForceSetText(OriginalText());
    return;
  }

  if (!diff.NeedsFullLayout())
    return;

  // The text metrics may be influenced by style changes.
  if (LayoutSVGBlock* text_or_ng_text =
          LayoutSVGText::LocateLayoutSVGTextAncestor(this)) {
    if (auto* text_layout_object = DynamicTo<LayoutSVGText>(text_or_ng_text))
      text_layout_object->SetNeedsTextMetricsUpdate();
    else
      To<LayoutNGSVGText>(text_or_ng_text)->SetNeedsTextMetricsUpdate();
    text_or_ng_text->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kStyleChange);
  }
}

bool LayoutSVGInlineText::IsFontFallbackValid() const {
  return LayoutText::IsFontFallbackValid() && ScaledFont().IsFallbackValid();
}

void LayoutSVGInlineText::InvalidateSubtreeLayoutForFontUpdates() {
  NOT_DESTROYED();
  if (!IsFontFallbackValid()) {
    LayoutSVGText::NotifySubtreeStructureChanged(
        this, layout_invalidation_reason::kFontsChanged);
  }
  LayoutText::InvalidateSubtreeLayoutForFontUpdates();
}

InlineTextBox* LayoutSVGInlineText::CreateTextBox(int start, uint16_t length) {
  NOT_DESTROYED();
  InlineTextBox* box = MakeGarbageCollected<SVGInlineTextBox>(
      LineLayoutItem(this), start, length);
  box->SetHasVirtualLogicalHeight();
  return box;
}

LayoutRect LayoutSVGInlineText::LocalCaretRect(const InlineBox* box,
                                               int caret_offset,
                                               LayoutUnit*) const {
  NOT_DESTROYED();
  if (!box || !box->IsInlineTextBox())
    return LayoutRect();

  const auto* text_box = To<InlineTextBox>(box);
  if (static_cast<unsigned>(caret_offset) < text_box->Start() ||
      static_cast<unsigned>(caret_offset) > text_box->Start() + text_box->Len())
    return LayoutRect();

  // Use the edge of the selection rect to determine the caret rect.
  if (static_cast<unsigned>(caret_offset) <
      text_box->Start() + text_box->Len()) {
    LayoutRect rect =
        text_box->LocalSelectionRect(caret_offset, caret_offset + 1);
    LayoutUnit x = box->IsLeftToRightDirection() ? rect.X() : rect.MaxX();
    return LayoutRect(x, rect.Y(), GetFrameView()->CaretWidth(), rect.Height());
  }

  LayoutRect rect =
      text_box->LocalSelectionRect(caret_offset - 1, caret_offset);
  LayoutUnit x = box->IsLeftToRightDirection() ? rect.MaxX() : rect.X();
  return LayoutRect(x, rect.Y(), GetFrameView()->CaretWidth(), rect.Height());
}

gfx::RectF LayoutSVGInlineText::FloatLinesBoundingBox() const {
  NOT_DESTROYED();
  gfx::RectF bounding_box;
  for (InlineTextBox* box : TextBoxes())
    bounding_box.Union(gfx::RectF(box->FrameRect()));
  return bounding_box;
}

PhysicalRect LayoutSVGInlineText::PhysicalLinesBoundingBox() const {
  NOT_DESTROYED();
  return PhysicalRect::EnclosingRect(FloatLinesBoundingBox());
}

bool LayoutSVGInlineText::CharacterStartsNewTextChunk(int position) const {
  NOT_DESTROYED();
  DCHECK_GE(position, 0);
  DCHECK_LT(position, static_cast<int>(TextLength()));

  // Each <textPath> element starts a new text chunk, regardless of any x/y
  // values.
  if (!position && Parent()->IsSVGTextPath() && !PreviousSibling())
    return true;

  const SVGCharacterDataMap::const_iterator it =
      character_data_map_.find(static_cast<unsigned>(position + 1));
  if (it == character_data_map_.end())
    return false;

  return it->value.HasX() || it->value.HasY();
}

gfx::RectF LayoutSVGInlineText::ObjectBoundingBox() const {
  NOT_DESTROYED();
  if (!IsInLayoutNGInlineFormattingContext())
    return FloatLinesBoundingBox();

  gfx::RectF bounds;
  NGInlineCursor cursor;
  cursor.MoveTo(*this);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem& item = *cursor.CurrentItem();
    if (item.Type() == NGFragmentItem::kSvgText)
      bounds.Union(cursor.Current().ObjectBoundingBox(cursor));
  }
  return bounds;
}

PositionWithAffinity LayoutSVGInlineText::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  if (IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveTo(*this);
    NGInlineCursor last_hit_cursor;
    PhysicalOffset last_hit_transformed_point;
    LayoutUnit closest_distance = LayoutUnit::Max();
    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      PhysicalOffset transformed_point =
          cursor.CurrentItem()->MapPointInContainer(point);
      PhysicalRect item_rect = cursor.Current().RectInContainerFragment();
      LayoutUnit distance;
      if (!item_rect.Contains(transformed_point) ||
          !cursor.PositionForPointInChild(transformed_point))
        distance = item_rect.SquaredDistanceTo(transformed_point);
      // Intentionally apply '<=', not '<', because we'd like to choose a later
      // item.
      if (distance <= closest_distance) {
        closest_distance = distance;
        last_hit_cursor = cursor;
        last_hit_transformed_point = transformed_point;
      }
    }
    if (last_hit_cursor) {
      auto position_with_affinity =
          last_hit_cursor.PositionForPointInChild(last_hit_transformed_point);
      // Note: Due by Bidi adjustment, |position_with_affinity| isn't relative
      // to this.
      return AdjustForEditingBoundary(position_with_affinity);
    }
    return CreatePositionWithAffinity(0);
  }

  if (!HasInlineFragments() || !TextLength())
    return CreatePositionWithAffinity(0);

  DCHECK(scaling_factor_);

  const SimpleFontData* font_data = scaled_font_.PrimaryFont();
  DCHECK(font_data);
  float baseline =
      font_data ? font_data->GetFontMetrics().FloatAscent() / scaling_factor_
                : 0;

  LayoutBlock* containing_block = ContainingBlock();
  DCHECK(containing_block);

  // Map local point to absolute point, as the character origins stored in the
  // text fragments use absolute coordinates.
  gfx::PointF absolute_point(point);
  absolute_point +=
      gfx::PointF(containing_block->Location()).OffsetFromOrigin();

  double closest_distance = std::numeric_limits<double>::max();
  float position_in_fragment = 0;
  const SVGTextFragment* closest_distance_fragment = nullptr;
  SVGInlineTextBox* closest_distance_box = nullptr;

  for (InlineTextBox* box : TextBoxes()) {
    auto* text_box = DynamicTo<SVGInlineTextBox>(box);
    if (!text_box)
      continue;

    for (const SVGTextFragment& fragment : text_box->TextFragments()) {
      gfx::RectF fragment_rect = fragment.BoundingBox(baseline);

      double distance =
          (fragment_rect.ClosestPoint(absolute_point) - absolute_point)
              .LengthSquared();
      if (distance <= closest_distance) {
        closest_distance = distance;
        closest_distance_box = text_box;
        closest_distance_fragment = &fragment;
        // TODO(fs): This only works (reasonably) well for text with trivial
        // transformations. For improved fidelity in the other cases we ought
        // to apply the inverse transformation for the fragment and then map
        // against the (untransformed) fragment rect.
        position_in_fragment = fragment.is_vertical
                                   ? absolute_point.y() - fragment_rect.y()
                                   : absolute_point.x() - fragment_rect.x();
      }
    }
  }

  if (!closest_distance_fragment)
    return CreatePositionWithAffinity(0);

  int offset = closest_distance_box->OffsetForPositionInFragment(
      *closest_distance_fragment, position_in_fragment);
  return CreatePositionWithAffinity(offset + closest_distance_box->Start(),
                                    offset > 0
                                        ? TextAffinity::kUpstreamIfPossible
                                        : TextAffinity::kDownstream);
}

namespace {

inline bool IsValidSurrogatePair(const TextRun& run, unsigned index) {
  if (!U16_IS_LEAD(run[index]))
    return false;
  if (index + 1 >= run.length())
    return false;
  return U16_IS_TRAIL(run[index + 1]);
}

unsigned CountCodePoints(const TextRun& run,
                         unsigned index,
                         unsigned end_index) {
  unsigned num_codepoints = 0;
  while (index < end_index) {
    index += IsValidSurrogatePair(run, index) ? 2 : 1;
    num_codepoints++;
  }
  return num_codepoints;
}

TextRun ConstructTextRun(LayoutSVGInlineText& text,
                         unsigned position,
                         unsigned length,
                         TextDirection text_direction) {
  const ComputedStyle& style = text.StyleRef();

  TextRun run(
      // characters, will be set below if non-zero.
      static_cast<const LChar*>(nullptr),
      0,  // length, will be set below if non-zero.
      0,  // xPos, only relevant with allowTabs=true
      0,  // padding, only relevant for justified text, not relevant for SVG
      TextRun::kAllowTrailingExpansion, text_direction,
      IsOverride(style.GetUnicodeBidi()) /* directionalOverride */);

  if (length) {
    if (text.Is8Bit())
      run.SetText(text.Characters8() + position, length);
    else
      run.SetText(text.Characters16() + position, length);
  }

  // We handle letter & word spacing ourselves.
  run.DisableSpacing();

  // Propagate the maximum length of the characters buffer to the TextRun, even
  // when we're only processing a substring.
  run.SetCharactersLength(text.TextLength() - position);
  DCHECK_GE(run.CharactersLength(), run.length());
  return run;
}

// TODO(pdr): We only have per-glyph data so we need to synthesize per-grapheme
// data. E.g., if 'fi' is shaped into a single glyph, we do not know the 'i'
// position. The code below synthesizes an average glyph width when characters
// share a single position. This will incorrectly split combining diacritics.
// See: https://crbug.com/473476.
void SynthesizeGraphemeWidths(const TextRun& run,
                              Vector<CharacterRange>& ranges) {
  unsigned distribute_count = 0;
  for (int range_index = static_cast<int>(ranges.size()) - 1; range_index >= 0;
       --range_index) {
    CharacterRange& current_range = ranges[range_index];
    if (current_range.Width() == 0) {
      distribute_count++;
      continue;
    }
    if (distribute_count == 0)
      continue;
    distribute_count++;

    // Distribute the width evenly among the code points.
    const unsigned distribute_end = range_index + distribute_count;
    unsigned num_codepoints = CountCodePoints(run, range_index, distribute_end);
    DCHECK_GT(num_codepoints, 0u);
    float new_width = current_range.Width() / num_codepoints;

    float last_end_position = current_range.start;
    unsigned distribute_index = range_index;
    do {
      CharacterRange& range = ranges[distribute_index];
      range.start = last_end_position;
      range.end = last_end_position + new_width;
      last_end_position = range.end;
      distribute_index += IsValidSurrogatePair(run, distribute_index) ? 2 : 1;
    } while (distribute_index < distribute_end);

    distribute_count = 0;
  }
}

}  // namespace

void LayoutSVGInlineText::AddMetricsFromRun(
    const TextRun& run,
    bool& last_character_was_white_space) {
  NOT_DESTROYED();
  Vector<CharacterRange> char_ranges =
      ScaledFont().IndividualCharacterRanges(run);
  SynthesizeGraphemeWidths(run, char_ranges);

  const SimpleFontData* font_data = ScaledFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  const float cached_font_height =
      font_data->GetFontMetrics().FloatHeight() / scaling_factor_;
  const bool preserve_white_space =
      StyleRef().WhiteSpace() == EWhiteSpace::kPre;
  const unsigned run_length = run.length();

  // TODO(pdr): Character-based iteration is ambiguous and error-prone. It
  // should be unified under a single concept. See: https://crbug.com/593570
  unsigned character_index = 0;
  while (character_index < run_length) {
    bool current_character_is_white_space = run[character_index] == ' ';
    if (!preserve_white_space && last_character_was_white_space &&
        current_character_is_white_space) {
      metrics_.push_back(SVGTextMetrics(SVGTextMetrics::kSkippedSpaceMetrics));
      character_index++;
      continue;
    }

    unsigned length = IsValidSurrogatePair(run, character_index) ? 2 : 1;
    float width = char_ranges[character_index].Width() / scaling_factor_;

    metrics_.push_back(SVGTextMetrics(length, width, cached_font_height));

    last_character_was_white_space = current_character_is_white_space;
    character_index += length;
  }
}

void LayoutSVGInlineText::UpdateMetricsList(
    bool& last_character_was_white_space) {
  NOT_DESTROYED();
  metrics_.clear();

  if (!TextLength())
    return;

  TextRun run =
      ConstructTextRun(*this, 0, TextLength(), StyleRef().Direction());
  BidiResolver<TextRunIterator, BidiCharacterRun> bidi_resolver;
  BidiRunList<BidiCharacterRun>& bidi_runs = bidi_resolver.Runs();
  bool bidi_override = IsOverride(StyleRef().GetUnicodeBidi());
  BidiStatus status(TextDirection::kLtr, bidi_override);
  if (run.Is8Bit() || bidi_override) {
    WTF::unicode::CharDirection direction = WTF::unicode::kLeftToRight;
    // If BiDi override is in effect, use the specified direction.
    if (bidi_override && !StyleRef().IsLeftToRightDirection())
      direction = WTF::unicode::kRightToLeft;
    bidi_runs.AddRun(new BidiCharacterRun(
        status.context->Override(), status.context->Level(), 0,
        run.CharactersLength(), direction, status.context->Dir()));
  } else {
    status.last = status.last_strong = WTF::unicode::kOtherNeutral;
    bidi_resolver.SetStatus(status);
    bidi_resolver.SetPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));
    const bool kHardLineBreak = false;
    const bool kReorderRuns = false;
    bidi_resolver.CreateBidiRunsForLine(TextRunIterator(&run, run.length()),
                                        kNoVisualOverride, kHardLineBreak,
                                        kReorderRuns);
  }

  for (const BidiCharacterRun* bidi_run = bidi_runs.FirstRun(); bidi_run;
       bidi_run = bidi_run->Next()) {
    TextRun sub_run = ConstructTextRun(*this, bidi_run->Start(),
                                       bidi_run->Stop() - bidi_run->Start(),
                                       bidi_run->Direction());
    AddMetricsFromRun(sub_run, last_character_was_white_space);
  }

  bidi_resolver.Runs().DeleteRuns();
}

void LayoutSVGInlineText::UpdateScaledFont() {
  NOT_DESTROYED();
  ComputeNewScaledFontForStyle(*this, scaling_factor_, scaled_font_);
}

void LayoutSVGInlineText::ComputeNewScaledFontForStyle(
    const LayoutObject& layout_object,
    float& scaling_factor,
    Font& scaled_font) {
  const ComputedStyle& style = layout_object.StyleRef();

  // Alter font-size to the right on-screen value to avoid scaling the glyphs
  // themselves, except when GeometricPrecision is specified.
  scaling_factor =
      SVGLayoutSupport::CalculateScreenFontSizeScalingFactor(&layout_object);
  if (!scaling_factor) {
    scaling_factor = 1;
    scaled_font = style.GetFont();
    return;
  }

  const FontDescription& unscaled_font_description = style.GetFontDescription();
  if (unscaled_font_description.TextRendering() == kGeometricPrecision)
    scaling_factor = 1;

  Document& document = layout_object.GetDocument();
  float scaled_font_size = FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
      &document, scaling_factor, unscaled_font_description.IsAbsoluteSize(),
      unscaled_font_description.SpecifiedSize(), kDoNotApplyMinimumForFontSize);
  if (scaled_font_size == unscaled_font_description.ComputedSize()) {
    scaled_font = style.GetFont();
    return;
  }

  FontDescription font_description = unscaled_font_description;
  font_description.SetComputedSize(scaled_font_size);
  const float zoom = style.EffectiveZoom();
  font_description.SetLetterSpacing(font_description.LetterSpacing() *
                                    scaling_factor / zoom);
  font_description.SetWordSpacing(font_description.WordSpacing() *
                                  scaling_factor / zoom);

  scaled_font =
      Font(font_description, document.GetStyleEngine().GetFontSelector());
}

PhysicalRect LayoutSVGInlineText::VisualRectInDocument(
    VisualRectFlags flags) const {
  NOT_DESTROYED();
  return Parent()->VisualRectInDocument(flags);
}

gfx::RectF LayoutSVGInlineText::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  return Parent()->VisualRectInLocalSVGCoordinates();
}

}  // namespace blink
