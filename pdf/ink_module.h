// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_MODULE_H_
#define PDF_INK_MODULE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/ink/ink_stroke_input.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

class SkCanvas;

namespace blink {
class WebInputEvent;
class WebMouseEvent;
}  // namespace blink

namespace chrome_pdf {

class InkStroke;

class InkModule {
 public:
  InkModule();
  InkModule(const InkModule&) = delete;
  InkModule& operator=(const InkModule&) = delete;
  ~InkModule();

  bool enabled() const { return enabled_; }

  // Draws `ink_strokes_` and `ink_inputs_` into `canvas`.
  void Draw(SkCanvas& canvas);

  // Returns whether the event was handled or not.
  bool HandleInputEvent(const blink::WebInputEvent& event);

  // Returns whether the message was handled or not.
  bool OnMessage(const base::Value::Dict& message);

 private:
  bool OnMouseDown(const blink::WebMouseEvent& event);
  bool OnMouseUp(const blink::WebMouseEvent& event);
  bool OnMouseMove(const blink::WebMouseEvent& event);

  void HandleSetAnnotationModeMessage(const base::Value::Dict& message);

  bool enabled_ = false;

  // Set when InkModule is in the middle of drawing a stroke.
  std::optional<base::Time> ink_start_time_;
  std::vector<InkStrokeInput> ink_inputs_;
  std::vector<std::unique_ptr<InkStroke>> ink_strokes_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_MODULE_H_
