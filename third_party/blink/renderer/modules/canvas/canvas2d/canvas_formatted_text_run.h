// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_RUN_H_

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MODULES_EXPORT CanvasFormattedTextRun final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(CanvasFormattedTextRun, Dispose);

 public:
  static CanvasFormattedTextRun* Create(ExecutionContext* execution_context,
                                        const String text) {
    return MakeGarbageCollected<CanvasFormattedTextRun>(execution_context,
                                                        text);
  }

  CanvasFormattedTextRun(ExecutionContext*, const String text);
  CanvasFormattedTextRun(const CanvasFormattedTextRun&) = delete;
  CanvasFormattedTextRun& operator=(const CanvasFormattedTextRun&) = delete;

  String text() const { return text_; }
  void setText(const String text) { text_ = text; }

  unsigned length() const { return text_.length(); }

  LayoutText* GetLayoutObject() { return layout_text_; }

  void Trace(Visitor* visitor) const override;

  void Dispose();

 private:
  String text_;

  LayoutText* layout_text_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_RUN_H_
