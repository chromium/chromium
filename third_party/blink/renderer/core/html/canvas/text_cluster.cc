// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_cluster.h"

#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"

namespace blink {
TextCluster::TextCluster(const String& text,
                         double x,
                         double y,
                         unsigned begin,
                         unsigned end,
                         TextAlign align,
                         TextBaseline baseline,
                         TextMetrics& text_metrics)
    : text_(text),
      x_(x),
      y_(y),
      begin_(begin),
      end_(end),
      align_(align),
      baseline_(baseline),
      text_metrics_(text_metrics) {}

TextCluster* TextCluster::Create(const String& text,
                                 double x,
                                 double y,
                                 unsigned begin,
                                 unsigned end,
                                 TextAlign align,
                                 TextBaseline baseline,
                                 TextMetrics& text_metrics) {
  return MakeGarbageCollected<TextCluster>(text, x, y, begin, end, align,
                                           baseline, text_metrics);
}

void TextCluster::Trace(Visitor* visitor) const {
  visitor->Trace(text_metrics_);
  ScriptWrappable::Trace(visitor);
}

void TextCluster::OffsetPosition(double x_offset, double y_offset) {
  x_ += x_offset;
  y_ += y_offset;
}

void TextCluster::OffsetCharacters(unsigned offset) {
  begin_ += offset;
  end_ += offset;
}
}  // namespace blink
