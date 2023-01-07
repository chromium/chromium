// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_INLINE_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_INLINE_TEXT_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"

namespace blink {

class LineLayoutSVGInlineText : public LineLayoutText {
 public:
  explicit LineLayoutSVGInlineText(LayoutSVGInlineText* layout_svg_inline_text)
      : LineLayoutText(layout_svg_inline_text) {}

  explicit LineLayoutSVGInlineText(const LineLayoutItem& item)
      : LineLayoutText(item) {
    SECURITY_DCHECK(!item || item.IsSVGInlineText());
  }

  explicit LineLayoutSVGInlineText(std::nullptr_t) : LineLayoutText(nullptr) {}

  LineLayoutSVGInlineText() = default;

  const Vector<SVGTextMetrics>& MetricsList() const {
    return ToSVGInlineText()->MetricsList();
  }

  SVGCharacterDataMap& CharacterDataMap() {
    return ToSVGInlineText()->CharacterDataMap();
  }

  bool CharacterStartsNewTextChunk(int position) const {
    return ToSVGInlineText()->CharacterStartsNewTextChunk(position);
  }

  float ScalingFactor() const { return ToSVGInlineText()->ScalingFactor(); }

  const Font& ScaledFont() const { return ToSVGInlineText()->ScaledFont(); }

 private:
  LayoutSVGInlineText* ToSVGInlineText() {
    return To<LayoutSVGInlineText>(GetLayoutObject());
  }

  const LayoutSVGInlineText* ToSVGInlineText() const {
    return To<LayoutSVGInlineText>(GetLayoutObject());
  }
};

class SVGInlineTextMetricsIterator {
  DISALLOW_NEW();

 public:
  SVGInlineTextMetricsIterator() { Reset(LineLayoutSVGInlineText()); }
  explicit SVGInlineTextMetricsIterator(
      LineLayoutSVGInlineText text_line_layout) {
    Reset(text_line_layout);
  }

  void AdvanceToTextStart(LineLayoutSVGInlineText text_line_layout,
                          unsigned start_character_offset) {
    DCHECK(text_line_layout);
    if (!text_line_layout_ || text_line_layout_ != text_line_layout) {
      Reset(text_line_layout);
      DCHECK(!MetricsList().empty());
    }

    if (character_offset_ == start_character_offset)
      return;

    // TODO(fs): We could walk backwards through the metrics list in these
    // cases.
    if (character_offset_ > start_character_offset)
      Reset(text_line_layout);

    while (character_offset_ < start_character_offset)
      Next();
    DCHECK_EQ(character_offset_, start_character_offset);
  }

  void Next() {
    character_offset_ += Metrics().length();
    DCHECK_LE(character_offset_, text_line_layout_.length());
    DCHECK_LT(metrics_list_offset_, MetricsList().size());
    ++metrics_list_offset_;
  }

  const SVGTextMetrics& Metrics() const {
    DCHECK(text_line_layout_);
    DCHECK_LT(metrics_list_offset_, MetricsList().size());
    return MetricsList()[metrics_list_offset_];
  }
  const Vector<SVGTextMetrics>& MetricsList() const {
    return text_line_layout_.MetricsList();
  }
  unsigned MetricsListOffset() const { return metrics_list_offset_; }
  unsigned CharacterOffset() const { return character_offset_; }
  bool IsAtEnd() const { return metrics_list_offset_ == MetricsList().size(); }

 private:
  void Reset(LineLayoutSVGInlineText text_line_layout) {
    text_line_layout_ = text_line_layout;
    character_offset_ = 0;
    metrics_list_offset_ = 0;
  }

  LineLayoutSVGInlineText text_line_layout_;
  unsigned metrics_list_offset_;
  unsigned character_offset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_SVG_INLINE_TEXT_H_
