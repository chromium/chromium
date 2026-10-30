// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_align.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_baseline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_text_cluster_options.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/text_cluster.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

constexpr int kHangingAsPercentOfAscent = 80;

float TextMetrics::GetFontBaseline(
    const V8CanvasTextBaseline::Enum text_baseline,
    const SimpleFontData& font_data) {
  FontMetrics font_metrics = font_data.GetFontMetrics();
  switch (text_baseline) {
    case V8CanvasTextBaseline::Enum::kTop:
      return font_data.NormalizedTypoAscent().ToFloat();
    case V8CanvasTextBaseline::Enum::kHanging:
      if (font_metrics.HangingBaseline().has_value()) {
        return font_metrics.HangingBaseline().value();
      }
      // According to
      // http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
      // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of
      // the ascender height"
      return font_metrics.FloatAscent(FontBaseline::kAlphabetic,
                                      FontMetrics::ApplyBaselineTable(true)) *
             kHangingAsPercentOfAscent / 100.0;
    case V8CanvasTextBaseline::Enum::kIdeographic:
      if (font_metrics.IdeographicBaseline().has_value()) {
        return font_metrics.IdeographicBaseline().value();
      }
      return -font_metrics.FloatDescent(FontBaseline::kAlphabetic,
                                        FontMetrics::ApplyBaselineTable(true));
    case V8CanvasTextBaseline::Enum::kBottom:
      return -font_data.NormalizedTypoDescent().ToFloat();
    case V8CanvasTextBaseline::Enum::kMiddle: {
      const FontHeight metrics = font_data.NormalizedTypoAscentAndDescent();
      return (metrics.ascent.ToFloat() - metrics.descent.ToFloat()) / 2.0f;
    }
    case V8CanvasTextBaseline::Enum::kAlphabetic:
      if (font_metrics.AlphabeticBaseline().has_value()) {
        return font_metrics.AlphabeticBaseline().value();
      }
      return 0;
    default:
      // Do nothing.
      return 0;
  }
}

void TextMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(baselines_);
  visitor->Trace(shaped_text_);
  ScriptWrappable::Trace(visitor);
}

TextMetrics::TextMetrics()
    : baselines_(Baselines::Create()),
      shaped_text_(MakeGarbageCollected<ShapedText>()) {}

TextMetrics::TextMetrics(const Font* font,
                         const TextDirection& direction,
                         const V8CanvasTextBaseline::Enum baseline,
                         const V8CanvasTextAlign::Enum align,
                         const String& text,
                         PlainTextPainter& text_painter)
    : baselines_(Baselines::Create()),
      shaped_text_(MakeGarbageCollected<ShapedText>(font,
                                                    direction,
                                                    baseline,
                                                    align,
                                                    text,
                                                    text_painter)) {
  Update(font, baseline);
}

void TextMetrics::Update(const Font* font,
                         const V8CanvasTextBaseline::Enum baseline) {
  const SimpleFontData* font_data = font->PrimaryFont();
  if (!font_data)
    return;

  width_ = shaped_text_->width();
  const gfx::RectF& glyph_bounds = shaped_text_->GlyphBounds();
  const float text_align_dx = shaped_text_->TextAlignDx();
  actual_bounding_box_left_ = -glyph_bounds.x() + text_align_dx;
  actual_bounding_box_right_ = glyph_bounds.right() - text_align_dx;

  // y direction
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const float ascent = font_metrics.FloatAscent(
      FontBaseline::kAlphabetic, FontMetrics::ApplyBaselineTable(true));
  const float descent = font_metrics.FloatDescent(
      FontBaseline::kAlphabetic, FontMetrics::ApplyBaselineTable(true));
  baseline_y = GetFontBaseline(baseline, *font_data);
  font_bounding_box_ascent_ = ascent - baseline_y;
  font_bounding_box_descent_ = descent + baseline_y;
  actual_bounding_box_ascent_ = -glyph_bounds.y() - baseline_y;
  actual_bounding_box_descent_ = glyph_bounds.bottom() + baseline_y;
  // TODO(kojii): We use normalized sTypoAscent/Descent here, but this should be
  // revisited when the spec evolves.
  const FontHeight normalized_typo_metrics =
      font_data->NormalizedTypoAscentAndDescent();
  em_height_ascent_ = normalized_typo_metrics.ascent - baseline_y;
  em_height_descent_ = normalized_typo_metrics.descent + baseline_y;

  // Setting baselines:
  if (font_metrics.AlphabeticBaseline().has_value()) {
    baselines_->setAlphabetic(font_metrics.AlphabeticBaseline().value() -
                              baseline_y);
  } else {
    baselines_->setAlphabetic(-baseline_y);
  }

  if (font_metrics.HangingBaseline().has_value()) {
    baselines_->setHanging(font_metrics.HangingBaseline().value() - baseline_y);
  } else {
    baselines_->setHanging(ascent * kHangingAsPercentOfAscent / 100.0f -
                           baseline_y);
  }

  if (font_metrics.IdeographicBaseline().has_value()) {
    baselines_->setIdeographic(font_metrics.IdeographicBaseline().value() -
                               baseline_y);
  } else {
    baselines_->setIdeographic(-descent - baseline_y);
  }
}

const HeapVector<Member<DOMRectReadOnly>> TextMetrics::getSelectionRects(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  return shaped_text_->getSelectionRects(start, end, exception_state);
}

DOMRectReadOnly* TextMetrics::getActualBoundingBox(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  return shaped_text_->getActualBoundingBox(start, end, exception_state);
}

unsigned TextMetrics::getIndexFromOffset(double x) {
  return shaped_text_->getIndexFromOffset(x);
}

namespace {
float getTextAlignDelta(float width,
                        const V8CanvasTextAlign::Enum text_align,
                        const TextDirection& direction) {
  switch (text_align) {
    case V8CanvasTextAlign::Enum::kRight:
      return width;
    case V8CanvasTextAlign::Enum::kCenter:
      return width / 2.0f;
    case V8CanvasTextAlign::Enum::kLeft:
      return 0;
    case V8CanvasTextAlign::Enum::kStart:
      if (IsLtr(direction)) {
        return 0;
      }
      return width;
    case V8CanvasTextAlign::Enum::kEnd:
      if (IsLtr(direction)) {
        return width;
      }
      return 0;
  }
}

float getTextBaselineDelta(float baseline,
                           const V8CanvasTextBaseline::Enum text_baseline,
                           const SimpleFontData& font_data) {
  float new_baseline = TextMetrics::GetFontBaseline(text_baseline, font_data);
  return baseline - new_baseline;
}

struct TextClusterCallbackContext {
  unsigned start_index_;
  float x_position_;
  float width_;

  void Trace(Visitor* visitor) const {}
};
}  // namespace

HeapVector<Member<TextCluster>> TextMetrics::getTextClusters(
    const TextClusterOptions* options) {
  return getTextClustersImpl(0, shaped_text_->GetText().length(), options,
                             /*exception_state=*/nullptr);
}

HeapVector<Member<TextCluster>> TextMetrics::getTextClusters(
    uint32_t start,
    uint32_t end,
    const TextClusterOptions* options,
    ExceptionState& exception_state) {
  return getTextClustersImpl(start, end, options, &exception_state);
}

HeapVector<Member<TextCluster>> TextMetrics::getTextClustersImpl(
    uint32_t start,
    uint32_t end,
    const TextClusterOptions* options,
    ExceptionState* exception_state) {
  HeapVector<Member<TextCluster>> minimal_clusters, clusters_for_range;
  const String& text = shaped_text_->GetText();
  const TextDirection direction = shaped_text_->Direction();
  const float text_align_dx = shaped_text_->TextAlignDx();
  const Font* font = shaped_text_->GetFont();
  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start >= text.length() || end > text.length()) {
    const bool is_start = start >= text.length();
    CHECK(exception_state != nullptr);
    exception_state->ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound(
            is_start ? "start" : "end", is_start ? start : end, text.length()));
    return clusters_for_range;
  }

  V8CanvasTextAlign::Enum cluster_text_align(shaped_text_->ContextTextAlign());
  V8CanvasTextBaseline::Enum cluster_text_baseline(
      shaped_text_->ContextTextBaseline());
  if (options) {
    if (options->hasAlign()) {
      cluster_text_align = options->align().AsEnum();
    }
    if (options->hasBaseline()) {
      cluster_text_baseline = options->baseline().AsEnum();
    }
  }

  for (const auto& run_with_offset : shaped_text_->RunsWithOffset()) {
    HeapVector<TextClusterCallbackContext> clusters_for_run;

    run_with_offset.shape_result_->ForEachGraphemeClusters(
        StringView(run_with_offset.text_), run_with_offset.x_position_, 0,
        run_with_offset.num_characters_, 0,
        [](void* context, unsigned character_index, float total_advance,
           unsigned graphemes_in_cluster, float cluster_advance,
           CanvasRotationInVertical rotation) {
          auto* clusters =
              static_cast<HeapVector<TextClusterCallbackContext>*>(context);
          TextClusterCallbackContext cluster = {.start_index_ = character_index,
                                                .x_position_ = total_advance,
                                                .width_ = cluster_advance};
          clusters->push_back(cluster);
        },
        &clusters_for_run);

    std::sort(clusters_for_run.begin(), clusters_for_run.end(),
              [](TextClusterCallbackContext a, TextClusterCallbackContext b) {
                return a.start_index_ < b.start_index_;
              });

    for (wtf_size_t i = 0; i < clusters_for_run.size(); i++) {
      TextCluster* text_cluster;
      if (i + 1 < clusters_for_run.size()) {
        text_cluster = TextCluster::Create(
            text, clusters_for_run[i].x_position_, 0,
            clusters_for_run[i].start_index_,
            clusters_for_run[i + 1].start_index_, cluster_text_align,
            cluster_text_baseline, *this);
      } else {
        text_cluster = TextCluster::Create(
            text, clusters_for_run[i].x_position_, 0,
            clusters_for_run[i].start_index_, run_with_offset.num_characters_,
            cluster_text_align, cluster_text_baseline, *this);
      }
      text_cluster->OffsetCharacters(run_with_offset.character_offset_);
      text_cluster->OffsetPosition(
          getTextAlignDelta(clusters_for_run[i].width_, cluster_text_align,
                            direction),
          getTextBaselineDelta(baseline_y, cluster_text_baseline,
                               *font->PrimaryFont()));
      text_cluster->OffsetPosition(-text_align_dx, 0);
      minimal_clusters.push_back(text_cluster);
    }
  }

  for (const auto& cluster : minimal_clusters) {
    if (cluster->end() <= start or end <= cluster->start()) {
      continue;
    }
    clusters_for_range.push_back(cluster);
  }
  return clusters_for_range;
}

}  // namespace blink
