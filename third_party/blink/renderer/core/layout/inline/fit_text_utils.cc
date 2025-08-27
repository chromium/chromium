// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/fit_text_utils.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
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
               bool is_scaled_inline_only,
               std::optional<float> limit,
               LineInfo& line_info) {
  bool should_scale_line_height = false;
  for (auto& item : *line_info.MutableResults()) {
    if (item.item->Type() != InlineItem::kText) {
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
    item.fit_text_scale->is_scaled_inline_only = is_scaled_inline_only;
    if (item.fit_text_scale->scale != 1.0f) {
      should_scale_line_height = true;
    }
  }
  return !is_scaled_inline_only && should_scale_line_height;
}

ShapeResult* ShapeForFit(const InlineItemResult& item,
                         const HarfBuzzShaper& shaper,
                         const Font& font,
                         const InlineItemSegments* segments) {
  ShapeOptions options;  // TODO(crbug.com/417306102): Pass correct options.
  if (segments) {
    return segments->ShapeText(&shaper, &font, item.item->Direction(),
                               item.StartOffset(), item.EndOffset(),
                               item.item->Index(), options);
  }
  RunSegmenter::RunSegmenterRange range = item.item->CreateRunSegmenterRange();
  range.end = item.item->EndOffset();
  return shaper.Shape(&font, item.item->Direction(), item.item->StartOffset(),
                      item.item->EndOffset(), range, options);
}

}  // namespace

bool ShouldApplyFitText(const InlineNode node) {
  if (!RuntimeEnabledFeatures::CssFitWidthTextEnabled()) {
    return false;
  }
  const ComputedStyle& style = node.Style();
  bool apply_text_grow = style.TextGrow().Target() != FitTextTarget::kNone;
  bool apply_text_shrink = style.TextShrink().Target() != FitTextTarget::kNone;
  if (!apply_text_grow && !apply_text_shrink) {
    return false;
  }
  if (node.HasFloats() || node.HasInitialLetterBox() || node.HasRuby()) {
    if (apply_text_grow) {
      AddConsoleMessage(
          node, ConsoleMessage::Level::kInfo,
          "Disable `text-grow` due to `float`, `initial-letter`, or "
          "ruby annotations.");
      apply_text_grow = false;
    }
    if (apply_text_shrink) {
      AddConsoleMessage(node, ConsoleMessage::Level::kInfo,
                        "Disable `text-shrink` due to `float`, "
                        "`initial-letter`, or ruby annotations.");
      apply_text_shrink = false;
    }
  }

  if (style.TextGrow().Target() == FitTextTarget::kConsistent) {
    AddConsoleMessage(node, ConsoleMessage::Level::kInfo,
                      "`text-grow: consistent` is not implemented yet.");
    apply_text_grow = false;
  }
  if (style.TextShrink().Target() == FitTextTarget::kConsistent) {
    AddConsoleMessage(node, ConsoleMessage::Level::kInfo,
                      "`text-shrink: consistent` is not implemented yet.");
    apply_text_shrink = false;
  }

  return apply_text_grow || apply_text_shrink;
}

float MeasurePerBlockScale(const InlineNode node,
                           const PhysicalFragment& fragment,
                           LayoutUnit available_width) {
  const auto* box_fragment = DynamicTo<PhysicalBoxFragment>(fragment);
  if (!box_fragment) {
    return 1.0f;
  }
  const auto* items = box_fragment->Items();
  if (!items) {
    return 1.0f;
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

  const FitText& fit_text =
      is_grow ? node.Style().TextGrow() : node.Style().TextShrink();
  if (fit_text.Target() != FitTextTarget::kConsistent) {
    return 1.0f;
  }
  for (InlineCursor cursor(*box_fragment, *items); cursor;
       cursor.MoveToNextSkippingChildren()) {
    if (!cursor.Current().IsLineBox()) {
      continue;
    }
    LayoutUnit remaining_space =
        available_width -
        ToLogicalSize(cursor.Current().Size(), writing_mode).inline_size;
    if (remaining_space.Abs() < epsilon) {
      continue;
    }
    LayoutUnit flexible_total_size;
    for (InlineCursor descendants = cursor.CursorForDescendants(); descendants;
         descendants.MoveToNextInlineLeaf()) {
      const auto& current = descendants.Current();
      if (!current.IsText()) {
        continue;
      }
      // TODO(crbug.com/417306102): Handle font-size + letter-spacing cases
      // like LineFitter::FitLine().
      flexible_total_size +=
          ToLogicalSize(current.Size(), writing_mode).inline_size;
    }
    if (!flexible_total_size ||
        remaining_space + flexible_total_size <= LayoutUnit()) {
      continue;
    }
    float scale = (remaining_space + flexible_total_size) / flexible_total_size;
    // TODO(crbug.com/417306102): Respect to fit_text.SizeLimit().
    minimum_scale = std::min(minimum_scale, scale);
  }
  return std::isfinite(minimum_scale) ? minimum_scale : 1.0f;
}

LineFitter::LineFitter(const InlineNode node, LineInfo* line_info)
    : node_(node),
      line_info_(*line_info),
      items_data_(node_.ItemsData(line_info->UseFirstLineStyle())),
      shaper_(items_data_.text_content),
      spacing_(items_data_.text_content),
      device_pixel_ratio_(node.GetDocument().GetFrame()->DevicePixelRatio()),
      epsilon_(2.0 * device_pixel_ratio_) {}

bool LineFitter::FitLine() {
  LayoutUnit original_width = line_info_.Width();
  LayoutUnit container_width = line_info_.AvailableWidth();
  LayoutUnit diff = container_width - original_width;
  if (diff.Abs() < epsilon_) {
    return false;
  }
  const FitText& text_grow = node_.Style().TextGrow();
  const FitText& text_shrink = node_.Style().TextShrink();
  bool apply_text_grow = text_grow.Target() == FitTextTarget::kPerLine;
  bool apply_text_shrink = text_shrink.Target() == FitTextTarget::kPerLine;
  if ((diff > LayoutUnit() && !apply_text_grow) ||
      (diff < LayoutUnit() && !apply_text_shrink)) {
    return false;
  }
  const bool is_grow = diff > LayoutUnit();
  const FitText& fit_text = is_grow ? text_grow : text_shrink;

  // Measure the static parts and the flexible parts in the items.
  LayoutUnit static_total_size;
  LayoutUnit flexible_total_size;
  // TODO(crbug.com/4173061029): Apply TextAutoSpace as well as letter-spacing
  // and word-spacing.
  for (auto& item : *line_info_.MutableResults()) {
    if (item.item->Type() == InlineItem::kText) {
      if (fit_text.Method() == FitTextMethod::kFontSize &&
          spacing_.SetSpacing(item.item->Style()->GetFontDescription())) {
        ShapeResult* nospacing_shape =
            ShapeForFit(item, shaper_, *item.item->Style()->GetFont(),
                        items_data_.segments.get());
        LayoutUnit size = nospacing_shape->SnappedWidth().ClampNegativeToZero();
        flexible_total_size += size;
        static_total_size += item.inline_size - size;
      } else {
        flexible_total_size += item.inline_size;
      }
    } else {
      static_total_size += item.inline_size;
    }
  }
  if (flexible_total_size <= 0) {
    return false;
  }

  float scale_factor =
      (container_width - static_total_size) / flexible_total_size;
  auto limit = fit_text.SizeLimit();
  if (!is_grow) {
    if (const auto* settings = node_.GetDocument().GetSettings()) {
      if (int min_size = settings->GetMinimumFontSize(); min_size > 0) {
        float physical_min = min_size * device_pixel_ratio_;
        limit = limit ? std::max(*limit, physical_min) : physical_min;
      }
    }
  }

  switch (fit_text.Method()) {
    case FitTextMethod::kScale:
      return ScaleLine(is_grow, scale_factor,
                       /* is_scaled_inline_only */ false, limit, line_info_);

    case FitTextMethod::kScaleInline:
      return ScaleLine(is_grow, scale_factor,
                       /* is_scaled_inline_only */ true, limit, line_info_);

    case FitTextMethod::kFontSize: {
      flexible_total_size = LayoutUnit();
      bool restricted = false;
      for (auto& item : *line_info_.MutableResults()) {
        if (item.item->Type() != InlineItem::kText) {
          continue;
        }
        float item_scale = scale_factor;
        if (limit) {
          if (is_grow) {
            float max_scale = *limit / item.item->Style()->ComputedFontSize();
            item_scale = std::min(scale_factor, max_scale);
          } else {
            float min_scale = *limit / item.item->Style()->ComputedFontSize();
            item_scale = std::max(scale_factor, min_scale);
          }
          if (item_scale != scale_factor) {
            restricted = true;
          }
        }
        const Font& font = *item.item->Style()->GetFont();
        FontDescription scaled_desc(font.GetFontDescription());
        scaled_desc.SetComputedSize(font.GetFontDescription().ComputedSize() *
                                    item_scale);
        Font* scaled_font =
            MakeGarbageCollected<Font>(scaled_desc, font.GetFontSelector());
        ShapeResult* shape_result = ShapeForFit(item, shaper_, *scaled_font,
                                                items_data_.segments.get());
        LayoutUnit size_without_spacing =
            shape_result->SnappedWidth().ClampNegativeToZero();
        if (spacing_.SetSpacing(scaled_desc)) {
          shape_result->ApplySpacing(spacing_);
          item.inline_size = shape_result->SnappedWidth().ClampNegativeToZero();
        } else {
          item.inline_size = size_without_spacing;
        }
        item.shape_result = ShapeResultView::Create(shape_result);
        if (!item.fit_text_scale) {
          item.fit_text_scale = MakeGarbageCollected<FitTextScale>();
        }
        item.fit_text_scale->font = scaled_font;
        item.fit_text_scale->scale = 1.0f;
        item.fit_text_scale->is_scaled_inline_only = false;
        flexible_total_size += size_without_spacing;
      }
      // Final adjustment by paint-time scaling. We skip it if font-size
      // scaling for an item was restricted by specifying a minimum or maximum
      // value.
      if (!restricted &&
          (container_width - line_info_.ComputeWidth()).Abs() >= epsilon_) {
        scale_factor =
            (container_width - static_total_size) / flexible_total_size;
        ScaleLine(is_grow, scale_factor, /* is_scaled_inline_only */ false,
                  limit, line_info_);
      }
      return true;
    }

    case FitTextMethod::kLetterSpacing:
      AddConsoleMessage(
          node_, ConsoleMessage::Level::kInfo,
          StrCat({"`text-", is_grow ? StringView("grow") : StringView("shrink"),
                  ": ... letter-spacing` is not implemented yet."}));
      break;
  }
  return false;
}

}  // namespace blink
