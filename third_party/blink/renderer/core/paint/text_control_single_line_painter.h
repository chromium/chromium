// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_CONTROL_SINGLE_LINE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_CONTROL_SINGLE_LINE_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutTextControlSingleLine;
struct PaintInfo;

class TextControlSingleLinePainter {
  STACK_ALLOCATED();

 public:
  TextControlSingleLinePainter(const LayoutTextControlSingleLine& text_control)
      : text_control_(text_control) {}
  void Paint(const PaintInfo&);

 private:
  const LayoutTextControlSingleLine& text_control_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_CONTROL_SINGLE_LINE_PAINTER_H_
