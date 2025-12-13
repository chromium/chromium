// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"

#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result_ruby_column.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

InlineItemResult::InlineItemResult(const InlineItem& item,
                                   unsigned index,
                                   const TextOffsetRange& text_offset,
                                   bool break_anywhere_if_overflow,
                                   bool should_create_line_box,
                                   bool has_unpositioned_floats)
    : item(item),
      item_index(index),
      text_offset(text_offset),
      break_anywhere_if_overflow(break_anywhere_if_overflow),
      should_create_line_box(should_create_line_box),
      has_unpositioned_floats(has_unpositioned_floats) {}

void InlineItemResult::ShapeHyphen() {
  DCHECK(!hyphen);
  DCHECK(item);
  DCHECK(item->Style());
  hyphen.Shape(*item->Style());
}

#if DCHECK_IS_ON()
void InlineItemResult::CheckConsistency(bool allow_null_shape_result) const {
  DCHECK(item);
  text_offset.AssertValid();
  DCHECK_GE(text_offset.start, item->StartOffset());
  // InlineItemResult for kOpenRubyColumn contains multiple InlineItem
  // instances. text_offset.end and item->EndOffset() are different.
  if (item->Type() == InlineItem::kOpenRubyColumn) {
    return;
  }
  DCHECK_LE(text_offset.end, item->EndOffset());
  if (item->Type() == InlineItem::kText) {
    if (!Length()) {
      // Empty text item should not have a `shape_result`.
      DCHECK(!shape_result);
      return;
    }
    if (allow_null_shape_result && !shape_result)
      return;
    DCHECK(shape_result);
    DCHECK_EQ(Length(), shape_result->NumCharacters());
    DCHECK_EQ(StartOffset(), shape_result->StartIndex());
    DCHECK_EQ(EndOffset(), shape_result->EndIndex());
  }
}
#endif

void InlineItemResult::Trace(Visitor* visitor) const {
  visitor->Trace(item);
  visitor->Trace(shape_result);
  visitor->Trace(hyphen);
  visitor->Trace(layout_result);
  visitor->Trace(ruby_column);
  visitor->Trace(positioned_float);
  visitor->Trace(exclusion_space_before_position_float);
  visitor->Trace(fit_text_scale);
}

String InlineItemResult::ToString(const String& ifc_text_content,
                                  const String& indent) const {
  // Unlike InlineItem::ToString(), this shows associated text precisely, and
  // shows kOpenRubyColumn structure.
  StringBuilder builder;
  builder.Append(indent);
  builder.Append("InlineItemResult ");
  builder.Append(item->InlineItemTypeToString(item->Type()));
  builder.Append(" ");
  if (item->Type() == InlineItem::kText) {
    builder.Append(
        ifc_text_content.Substring(TextOffset().start, TextOffset().Length())
            .EncodeForDebugging());
  } else if (IsRubyColumn()) {
    if (item->GetLayoutObject()) {
      builder.Append(item->GetLayoutObject()->ToString());
    } else {
      builder.Append("(anonymous)");
    }
    builder.Append(", base_line: [\n");
    String child_indent = StrCat({indent, "\t"});
    for (const auto& r : ruby_column->base_line.Results()) {
      builder.Append(r.ToString(ifc_text_content, child_indent));
      builder.Append("\n");
    }
    for (wtf_size_t i = 0; i < ruby_column->annotation_line_list.size(); ++i) {
      builder.Append(indent);
      builder.Append("], annotation_line_list[");
      builder.AppendNumber(i);
      builder.Append("]: [\n");
      for (const auto& r : ruby_column->annotation_line_list[i].Results()) {
        builder.Append(r.ToString(ifc_text_content, child_indent));
        builder.Append("\n");
      }
    }
    builder.Append(indent);
    builder.Append("]");
  } else if (item->GetLayoutObject()) {
    builder.Append(item->GetLayoutObject()->ToString());
  }
  return builder.ToString();
}

FitTextBlockScale FindTextScaleInternal(const InlineItemResults& line_items,
                                        wtf_size_t start_index,
                                        wtf_size_t initial_nesting_level) {
  wtf_size_t level = initial_nesting_level;
  for (wtf_size_t i = start_index; i < line_items.size(); ++i) {
    auto item_type = line_items[i].item->Type();
    if (item_type == InlineItem::kOpenTag) {
      ++level;
    } else if (item_type == InlineItem::kCloseTag) {
      if (level == 0) {
        break;
      }
      --level;
    } else if (item_type == InlineItem::kText) {
      if (level == 0) {
        if (const auto* fit_text_scale = line_items[i].fit_text_scale.Get()) {
          float paint_scale = fit_text_scale->scale;
          float font_scale = 1.0f;
          const Font* scaled_font = fit_text_scale->font;
          if (scaled_font) {
            font_scale = scaled_font->GetFontDescription().ComputedSize() /
                         line_items[i]
                             .item->Style()
                             ->GetFontDescription()
                             .ComputedSize();
          }
          return {paint_scale, font_scale * paint_scale, scaled_font};
        }
        break;
      }
    }
  }
  return {1.0f, 1.0f, nullptr};
}

}  // namespace blink
