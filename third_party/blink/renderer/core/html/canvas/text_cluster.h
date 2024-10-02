// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_TEXT_CLUSTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_TEXT_CLUSTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class TextMetrics;

class CORE_EXPORT TextCluster final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  TextCluster(const String& text,
              double x,
              double y,
              unsigned begin,
              unsigned end,
              TextAlign align,
              TextBaseline baseline,
              TextMetrics& text_metrics);
  static TextCluster* Create(const String& text,
                             double x,
                             double y,
                             unsigned begin,
                             unsigned end,
                             TextAlign align,
                             TextBaseline baseline,
                             TextMetrics& text_metrics);

  const String& text() const { return text_; }
  double x() const { return x_; }
  double y() const { return y_; }
  unsigned begin() const { return begin_; }
  unsigned end() const { return end_; }
  const String align() const { return TextAlignName(align_); }
  const String baseline() const { return TextBaselineName(baseline_); }
  const Member<TextMetrics> textMetrics() const { return text_metrics_; }

  void setX(double x) { x_ = x; }
  void setY(double y) { y_ = y; }
  void setBegin(unsigned begin) { begin_ = begin; }
  void setEnd(unsigned end) { end_ = end; }

  void OffsetPosition(double x_offset, double y_offset);
  void OffsetCharacters(unsigned offset);

  void Trace(Visitor*) const override;

 private:
  const String text_;
  double x_ = 0.0;
  double y_ = 0.0;
  unsigned begin_ = 0;
  unsigned end_ = 0;
  const TextAlign align_;
  const TextBaseline baseline_;
  const Member<TextMetrics> text_metrics_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_TEXT_CLUSTER_H_
