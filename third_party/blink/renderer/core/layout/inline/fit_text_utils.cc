// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/fit_text_utils.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/inline/fit_text_scale.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

// Show a console message with ConsoleMessage::Source::kRendering and
// discard_duplicates==true.
void AddConsoleMessage(const InlineNode node,
                       ConsoleMessage::Level level,
                       const String& message) {
  node.GetDocument().AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(ConsoleMessage::Source::kRendering,
                                           level, message),
      /* discard_duplicates */ true);
}

// Returns true if LogicalLineBuilder needs to scale line-height.
bool ScaleLine(bool is_grow,
               float scale_factor,
               std::optional<float> limit,
               LineInfo& line_info) {
  bool should_scale_line_height = false;
  LayoutUnit inline_size = line_info.TextIndent();
  for (auto& item : *line_info.MutableResults()) {
    if (item.item->Type() != InlineItem::kText &&
        item.item->TextType() != TextItemType::kForcedLineBreak) {
      inline_size += item.inline_size;
      continue;
    }
    if (!item.fit_text_scale) {
      item.fit_text_scale = MakeGarbageCollected<FitTextScale>();
    }
    if (limit) {
      if (is_grow) {
        float max_scale = *limit / item.item->Style()->ComputedFontSize();
        item.fit_text_scale->scale = std::min(scale_factor, max_scale);
      } else {
        float min_scale = *limit / item.item->Style()->ComputedFontSize();
        item.fit_text_scale->scale = std::max(scale_factor, min_scale);
      }
    } else {
      item.fit_text_scale->scale = scale_factor;
    }
    if (item.fit_text_scale->scale != 1.0f) {
      should_scale_line_height = true;
    }
    inline_size += item.inline_size * item.fit_text_scale->scale;
  }
  line_info.SetWidth(line_info.AvailableWidth(), inline_size);
  line_info.SetTextFitScale(scale_factor);
  return should_scale_line_height;
}

ShapeResult* ShapeForFit(const InlineItem& item,
                         unsigned start_offset,
                         unsigned end_offset,
                         const HarfBuzzShaper& shaper,
                         const Font& font,
                         const InlineItemSegments* segments,
                         bool is_start_of_paragraph) {
  ShapeOptions options;
  if (is_start_of_paragraph) {
    options.is_line_start = true;
    const String& text_content = shaper.GetText();
    options.han_kerning_start =
        start_offset < text_content.length() &&
        ShouldTrimStartOfParagraph(
            font.GetFontDescription().GetTextSpacingTrim()) &&
        Character::MaybeHanKerningOpen(text_content[start_offset]);
  }

  ShapeResult* result = nullptr;
  if (segments) {
    result = segments->ShapeText(&shaper, &font, item.Direction(), start_offset,
                                 end_offset, item.Index(), options);
  } else {
    RunSegmenter::RunSegmenterRange range = item.CreateRunSegmenterRange();
    range.end = end_offset;
    result = shaper.Shape(&font, item.Direction(), start_offset, end_offset,
                          range, options);
  }

  // Apply text-autospace. We add TextAutoSpaceInlineSize() of the new font
  // to positions where the original ShapeResult has auto spacing.
  const ShapeResult* original = item.TextShapeResult();
  if (original) {
    original->EnsurePositionData();
    Vector<OffsetWithSpacing, 16> offsets;
    float spacing = font.TextAutoSpaceInlineSize();
    for (unsigned i = start_offset; i < end_offset; ++i) {
      if (original->HasAutoSpacingAfter(i)) {
        offsets.push_back(OffsetWithSpacing{i + 1, spacing});
      }
    }
    if (!offsets.empty()) {
      result->ApplyTextAutoSpacing(offsets);
    }
  }

  return result;
}

std::optional<float> MinimumSize(bool is_grow, const InlineNode node) {
  if (!is_grow) {
    return node.MinimumFontPhysicalSize();
  }
  return std::nullopt;
}

// Returns true if FontDescription has non-zero non-percentage spacing.
bool HasFixedSpacing(const FontDescription& font_description) {
  const Length& letter_spacing = font_description.ComputedLetterSpacing();
  if (letter_spacing.IsFixed() && !letter_spacing.IsZero()) {
    return true;
  }
  const Length& word_spacing = font_description.ComputedWordSpacing();
  if (word_spacing.IsFixed() && !word_spacing.IsZero()) {
    return true;
  }
  return false;
}

// Returns a copy of FontDescription with fixed spacing set to zero.
FontDescription PercentageSpacingDescription(const FontDescription& desc) {
  FontDescription copy = desc;
  const Length& letter_spacing = desc.ComputedLetterSpacing();
  if (letter_spacing.IsFixed() && !letter_spacing.IsZero()) {
    copy.SetLetterSpacing(Length::Fixed(0.0f));
  }
  const Length& word_spacing = desc.ComputedWordSpacing();
  if (word_spacing.IsFixed() && !word_spacing.IsZero()) {
    copy.SetWordSpacing(Length::Fixed(0.0f));
  }
  return copy;
}

// A helper for font-size scaling.
FontDescription ScaledFontDescription(const Font& font,
                                      float scale_factor,
                                      std::optional<float> limit,
                                      bool& restricted) {
  float item_scale = scale_factor;
  float original_size = font.GetFontDescription().ComputedSize();
  if (limit) {
    if (item_scale > 1.0f) {
      item_scale = std::min(scale_factor, *limit / original_size);
    } else {
      item_scale = std::max(scale_factor, *limit / original_size);
    }
    if (item_scale != scale_factor) {
      restricted = true;
    }
  }
  FontDescription scaled_desc(font.GetFontDescription());
  scaled_desc.SetComputedSize(original_size * item_scale);
  return scaled_desc;
}

float ComputeAdditionalPaintTimeScale(const InlineItemsData& items_data,
                                      LayoutUnit available_width,
                                      LayoutUnit epsilon,
                                      WritingMode writing_mode,
                                      HarfBuzzShaper& shaper,
                                      ShapeResultSpacing& spacing,
                                      const InlineCursor& line,
                                      bool is_start_of_paragraph,
                                      float scale,
                                      std::optional<float> limit,
                                      LayoutUnit static_total_size) {
  LayoutUnit flexible_total_size;
  for (InlineCursor descendants = line.CursorForDescendants(); descendants;
       descendants.MoveToNextInlineLeaf()) {
    const auto& current = descendants.Current();
    if (!current.IsText() || !current.TextShapeResult()) {
      continue;
    }
    const Font& font = *current.Style().GetFont();
    bool restricted = false;
    FontDescription scaled_desc =
        ScaledFontDescription(font, scale, limit, restricted);
    if (restricted) {
      // We won't apply additional scale if font-size is restricted by `limit`.
      return 1.0f;
    }
    Font* scaled_font =
        MakeGarbageCollected<Font>(scaled_desc, font.GetFontSelector());
    auto iter = std::ranges::find_if(
        items_data.items, [&](const Member<InlineItem>& item) {
          return item->StartOffset() <= current.TextStartOffset() &&
                 current.TextEndOffset() <= item->EndOffset();
        });
    CHECK_NE(iter, items_data.items.end());
    ShapeResult* shape_result = ShapeForFit(
        **iter, current.TextStartOffset(), current.TextEndOffset(), shaper,
        *scaled_font, items_data.segments.get(), is_start_of_paragraph);
    is_start_of_paragraph = false;
    if (spacing.SetSpacing(scaled_desc)) {
      shape_result->ApplySpacing(spacing);
    }
    flexible_total_size += shape_result->SnappedWidth().ClampNegativeToZero();
  }
  LayoutUnit remaining_space =
      available_width - (flexible_total_size + static_total_size);
  return remaining_space.Abs() >= epsilon
             ? (flexible_total_size + remaining_space).ToFloat() /
                   flexible_total_size.ToFloat()
             : 1.0f;
}

}  // namespace

bool ShouldApplyFitText(const InlineNode node) {
  if (!RuntimeEnabledFeatures::CssTextFitEnabled()) {
    return false;
  }
  const ComputedStyle& style = node.Style();
  FitTextType fit_type = style.TextFit().Type();
  if (fit_type == FitTextType::kNone) {
    return false;
  }
  if (node.HasFloats() || node.HasInitialLetterBox() || node.HasRuby()) {
    AddConsoleMessage(node, ConsoleMessage::Level::kInfo,
                      "Disable `text-fit` due to `float`, `initial-letter`, or "
                      "ruby annotations.");
    return false;
  }

  return true;
}

ParagraphScale MeasurePerBlockScale(const InlineNode node,
                                    const PhysicalFragment& fragment,
                                    LayoutUnit available_width) {
  const auto* box_fragment = DynamicTo<PhysicalBoxFragment>(fragment);
  if (!box_fragment) {
    return ParagraphScale();
  }
  const auto* items = box_fragment->Items();
  if (!items) {
    return ParagraphScale();
  }

  LayoutUnit epsilon(2.0 * node.GetDocument().GetFrame()->DevicePixelRatio());
  float minimum_scale = std::numeric_limits<float>::infinity();
  const WritingMode writing_mode = box_fragment->Style().GetWritingMode();

  // Determine text-grow or text-shrink.
  bool is_grow = true;
  {
    InlineCursor cursor(*box_fragment, *items);
    for (; cursor; cursor.MoveToNextSkippingChildren()) {
      if (!cursor.Current().IsLineBox()) {
        continue;
      }
      LayoutUnit remaining_space =
          available_width -
          ToLogicalSize(cursor.Current().Size(), writing_mode).inline_size;
      if (remaining_space.Abs() < epsilon) {
        continue;
      }
      if (remaining_space < LayoutUnit()) {
        is_grow = false;
        break;
      }
    }
  }

  const FitText& fit_text = node.Style().TextFit();
  if (fit_text.Target() != FitTextTarget::kConsistent) {
    return ParagraphScale();
  }
  float additional_paint_time_scale = 1.0f;
  bool is_next_start_of_paragraph = true;
  for (InlineCursor cursor(*box_fragment, *items); cursor;
       cursor.MoveToNextSkippingChildren()) {
    if (!cursor.Current().IsLineBox()) {
      continue;
    }
    bool is_start_of_paragraph = is_next_start_of_paragraph;
    is_next_start_of_paragraph = false;
    LayoutUnit remaining_space =
        available_width -
        ToLogicalSize(cursor.Current().Size(), writing_mode).inline_size;
    if (remaining_space.Abs() < epsilon) {
      continue;
    }
    LayoutUnit flexible_total_size;
    LayoutUnit flexible_total_size_including_letter_spacing;
    const InlineItemsData& items_data =
        node.ItemsData(cursor.CurrentItem()->UsesFirstLineStyle());
    HarfBuzzShaper shaper(items_data.text_content);
    ShapeResultSpacing spacing(items_data.text_content);
    bool did_reshape = false;
    const auto limit = MinimumSize(is_grow, node);
    for (InlineCursor descendants = cursor.CursorForDescendants(); descendants;
         descendants.MoveToNextInlineLeaf()) {
      const auto& current = descendants.Current();
      if (current.IsLineBreak()) {
        is_next_start_of_paragraph = true;
        continue;
      }
      if (!current.IsText() || !current.TextShapeResult()) {
        continue;
      }
      const ComputedStyle& style = current->Style();
      if (HasFixedSpacing(style.GetFontDescription())) {
        unsigned start = current.TextStartOffset();
        unsigned end = current.TextEndOffset();
        auto iter = std::ranges::find_if(
            items_data.items, [&](const Member<InlineItem>& item) {
              return item->StartOffset() <= start && end <= item->EndOffset();
            });
        CHECK_NE(iter, items_data.items.end());
        ShapeResult* nospacing_shape =
            ShapeForFit(**iter, start, end, shaper, *style.GetFont(),
                        items_data.segments.get(), is_start_of_paragraph);
        is_start_of_paragraph = false;
        did_reshape = true;
        if (spacing.SetSpacing(
                PercentageSpacingDescription(style.GetFontDescription()))) {
          nospacing_shape->ApplySpacing(spacing);
        }
        flexible_total_size +=
            nospacing_shape->SnappedWidth().ClampNegativeToZero();
      } else {
        flexible_total_size +=
            ToLogicalSize(current.Size(), writing_mode).inline_size;
      }
      flexible_total_size_including_letter_spacing +=
          ToLogicalSize(current.Size(), writing_mode).inline_size;
    }
    if (!flexible_total_size ||
        remaining_space + flexible_total_size <= LayoutUnit()) {
      continue;
    }
    float scale = (remaining_space + flexible_total_size).ToFloat() /
                  flexible_total_size.ToFloat();
    if (scale < minimum_scale) {
      minimum_scale = scale;
      if (fit_text.Method() == FitTextMethod::kFontSize || did_reshape) {
        // This value should exclude letter-spacing because
        // ComputeAdditionalPaintTimeScale() is for the paint-time scaling,
        // which scales letter-spacing.
        LayoutUnit static_total_size =
            available_width - remaining_space -
            flexible_total_size_including_letter_spacing;
        additional_paint_time_scale = ComputeAdditionalPaintTimeScale(
            items_data, available_width, epsilon, writing_mode, shaper, spacing,
            cursor, is_start_of_paragraph, scale, limit, static_total_size);
      }
    }
  }
  if (std::isfinite(minimum_scale)) {
    if (fit_text.ScaleFactorLimit().has_value()) {
      float limit = *fit_text.ScaleFactorLimit();
      minimum_scale = fit_text.Type() == FitTextType::kGrow
                          ? std::min(minimum_scale, std::max(limit, 1.0f))
                          : std::max(minimum_scale, std::min(limit, 1.0f));
    }
    return {minimum_scale, additional_paint_time_scale};
  } else {
    return {1.0f, additional_paint_time_scale};
  }
}

LineFitter::LineFitter(const InlineNode node, LineInfo* line_info)
    : node_(node),
      line_info_(*line_info),
      items_data_(node_.ItemsData(line_info->UseFirstLineStyle())),
      shaper_(items_data_.text_content),
      spacing_(items_data_.text_content),
      device_pixel_ratio_(node.GetDocument().GetFrame()->DevicePixelRatio()),
      epsilon_(2.0 * device_pixel_ratio_) {}

float LineFitter::MeasureScale() {
  LayoutUnit original_width = line_info_.Width();
  LayoutUnit container_width = line_info_.AvailableWidth();
  LayoutUnit diff = container_width - original_width;
  if (diff.Abs() < epsilon_) {
    return 1.0f;
  }
  const FitText& fit_text = node_.Style().TextFit();
  const FitTextTarget target = fit_text.Target();
  bool apply_text_grow = fit_text.Type() == FitTextType::kGrow &&
                         target != FitTextTarget::kConsistent;
  bool apply_text_shrink = fit_text.Type() == FitTextType::kShrink &&
                           target != FitTextTarget::kConsistent;
  if ((diff > LayoutUnit() && !apply_text_grow) ||
      (diff < LayoutUnit() && !apply_text_shrink)) {
    return 1.0f;
  }

  if (target == FitTextTarget::kPerLine && line_info_.IsLastLine()) {
    return 1.0f;
  }

  // Measure the static parts and the flexible parts in the items.
  LayoutUnit static_total_size;
  LayoutUnit flexible_total_size;
  bool is_first_text = true;
  // TODO(crbug.com/4173061029): Apply TextAutoSpace as well as letter-spacing
  // and word-spacing.
  for (auto& item : *line_info_.MutableResults()) {
    if (item.item->Type() == InlineItem::kText) {
      if (HasFixedSpacing(item.item->Style()->GetFontDescription())) {
        ShapeResult* nospacing_shape = ShapeForFit(
            *item.item, item.StartOffset(), item.EndOffset(), shaper_,
            *item.item->Style()->GetFont(), items_data_.segments.get(),
            line_info_.IsStartOfParagraph() && is_first_text);
        if (spacing_.SetSpacing(PercentageSpacingDescription(
                item.item->Style()->GetFontDescription()))) {
          nospacing_shape->ApplySpacing(spacing_);
        }
        LayoutUnit size = nospacing_shape->SnappedWidth().ClampNegativeToZero();
        flexible_total_size += size;
        static_total_size += item.inline_size - size;
      } else {
        flexible_total_size += item.inline_size;
      }
      is_first_text = false;
    } else {
      static_total_size += item.inline_size;
    }
  }
  if (flexible_total_size <= 0) {
    return std::numeric_limits<float>::infinity();
  }

  float scale_factor = (container_width - static_total_size).ToFloat() /
                       flexible_total_size.ToFloat();
  if (fit_text.ScaleFactorLimit().has_value()) {
    float limit = *fit_text.ScaleFactorLimit();
    return fit_text.Type() == FitTextType::kGrow
               ? std::min(scale_factor, std::max(limit, 1.0f))
               : std::max(scale_factor, std::min(limit, 1.0f));
  }
  return scale_factor;
}

bool LineFitter::FitLine(float scale_factor,
                         std::optional<float> additional_paint_time_scale) {
  const bool is_grow = scale_factor > 1.0f;
  const FitText& fit_text = node_.Style().TextFit();
  auto limit = MinimumSize(is_grow, node_);

  bool has_fixed_spacing = false;
  for (auto& item : *line_info_.MutableResults()) {
    if (item.item->Type() == InlineItem::kText &&
        HasFixedSpacing(item.item->Style()->GetFontDescription())) {
      has_fixed_spacing = true;
      break;
    }
  }

  if (fit_text.Method() == FitTextMethod::kScale && !has_fixed_spacing) {
    return ScaleLine(is_grow, scale_factor, limit, line_info_);
  }

  LayoutUnit static_total_size;
  LayoutUnit flexible_total_size;
  bool restricted = false;
  bool is_first_text = true;
  for (auto& item : *line_info_.MutableResults()) {
    if (item.item->Type() != InlineItem::kText) {
      if (item.item->IsForcedLineBreak()) {
        const Font& font = *item.item->Style()->GetFont();
        Font* scaled_font = MakeGarbageCollected<Font>(
            ScaledFontDescription(font, scale_factor, limit, restricted),
            font.GetFontSelector());
        if (!item.fit_text_scale) {
          item.fit_text_scale = MakeGarbageCollected<FitTextScale>();
        }
        item.fit_text_scale->font = scaled_font;
        item.fit_text_scale->scale = 1.0f;
      }
      static_total_size += item.inline_size;
      continue;
    }
    const Font& font = *item.item->Style()->GetFont();
    FontDescription scaled_desc =
        ScaledFontDescription(font, scale_factor, limit, restricted);
    Font* scaled_font =
        MakeGarbageCollected<Font>(scaled_desc, font.GetFontSelector());
    ShapeResult* shape_result =
        ShapeForFit(*item.item, item.StartOffset(), item.EndOffset(), shaper_,
                    *scaled_font, items_data_.segments.get(),
                    line_info_.IsStartOfParagraph() && is_first_text);
    is_first_text = false;
    LayoutUnit size_without_spacing =
        shape_result->SnappedWidth().ClampNegativeToZero();
    if (spacing_.SetSpacing(scaled_desc)) {
      shape_result->ApplySpacing(spacing_);
      item.inline_size = shape_result->SnappedWidth().ClampNegativeToZero();
      static_total_size += item.inline_size - size_without_spacing;
    } else {
      item.inline_size = size_without_spacing;
    }
    item.shape_result = ShapeResultView::Create(shape_result);
    if (!item.fit_text_scale) {
      item.fit_text_scale = MakeGarbageCollected<FitTextScale>();
    }
    item.fit_text_scale->font = scaled_font;
    item.fit_text_scale->scale = 1.0f;
    flexible_total_size += size_without_spacing;
  }
  // Final adjustment by paint-time scaling. We skip it if font-size
  // scaling for an item was restricted by specifying a minimum or maximum
  // value.
  if (!restricted) {
    if (additional_paint_time_scale) {
      // FitTextTarget::kConsistent case:
      if (*additional_paint_time_scale != 1.0f) {
        ScaleLine(is_grow, *additional_paint_time_scale, limit, line_info_);
      }
    } else {
      // FitTextTarget::kPerLine case:
      LayoutUnit container_width = line_info_.AvailableWidth();
      if ((container_width - line_info_.ComputeWidth()).Abs() >= epsilon_) {
        scale_factor = (container_width - static_total_size).ToFloat() /
                       flexible_total_size.ToFloat();
        ScaleLine(is_grow, scale_factor, limit, line_info_);
      }
    }
  }
  line_info_.SetWidth(line_info_.AvailableWidth(), line_info_.ComputeWidth());
  line_info_.SetTextFitScale(scale_factor);
  return true;
}

bool LineFitter::MeasureAndFitLine() {
  float scale_factor = MeasureScale();
  return std::isfinite(scale_factor) && scale_factor != 1.0f &&
         FitLine(scale_factor, std::nullopt);
}

}  // namespace blink
