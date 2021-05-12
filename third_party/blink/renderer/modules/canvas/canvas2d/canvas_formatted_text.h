// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_formatted_text_run.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/text/bidi_text_run.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run_iterator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutBlockFlow;
class FontDescription;
class Document;

class MODULES_EXPORT CanvasFormattedText final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(CanvasFormattedText, Dispose);

 public:
  static CanvasFormattedText* Create(ExecutionContext* execution_context) {
    return MakeGarbageCollected<CanvasFormattedText>(execution_context);
  }

  static CanvasFormattedText* Create(ExecutionContext* execution_context,
                                     const String text);

  static CanvasFormattedText* Create(ExecutionContext* execution_context,
                                     CanvasFormattedTextRun* run,
                                     ExceptionState& exception_state) {
    CanvasFormattedText* canvas_formatted_text =
        MakeGarbageCollected<CanvasFormattedText>(execution_context);
    canvas_formatted_text->appendRun(run, exception_state);
    return canvas_formatted_text;
  }

  explicit CanvasFormattedText(ExecutionContext* execution_context);
  CanvasFormattedText(const CanvasFormattedText&) = delete;
  CanvasFormattedText& operator=(const CanvasFormattedText&) = delete;

  void Trace(Visitor* visitor) const override;

  unsigned length() const { return text_runs_.size(); }

  bool CheckRunsIndexBound(uint32_t index,
                           ExceptionState* exception_state) const {
    if (index >= text_runs_.size()) {
      if (exception_state) {
        exception_state->ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            ExceptionMessages::IndexExceedsMaximumBound("index", index,
                                                        text_runs_.size()));
      }
      return false;
    }
    return true;
  }

  bool CheckRunIsNotParented(CanvasFormattedTextRun* run,
                             ExceptionState* exception_state) const {
    if (run->GetLayoutObject() && run->GetLayoutObject()->Parent()) {
      if (exception_state) {
        exception_state->ThrowDOMException(
            DOMExceptionCode::kInvalidModificationError,
            "The run is already a part of a formatted text. Remove it from "
            "that formatted text before insertion.");
      }
      return false;
    }
    return true;
  }

  CanvasFormattedTextRun* getRun(unsigned index,
                                 ExceptionState& exception_state) const {
    if (!CheckRunsIndexBound(index, &exception_state))
      return nullptr;
    return text_runs_[index];
  }

  CanvasFormattedTextRun* appendRun(CanvasFormattedTextRun* run,
                                    ExceptionState& exception_state);

  CanvasFormattedTextRun* setRun(unsigned index,
                                 CanvasFormattedTextRun* run,
                                 ExceptionState& exception_state);

  CanvasFormattedTextRun* insertRun(unsigned index,
                                    CanvasFormattedTextRun* run,
                                    ExceptionState& exception_state);

  void deleteRun(unsigned index, ExceptionState& exception_state) {
    deleteRun(index, 1, exception_state);
  }

  void deleteRun(unsigned index,
                 unsigned length,
                 ExceptionState& exception_state);

  LayoutBlockFlow* GetLayoutBlock(Document& document,
                                  const FontDescription& defaultFont);

  sk_sp<PaintRecord> PaintFormattedText(Document& document,
                                        const FontDescription& font,
                                        double x,
                                        double y,
                                        double wrap_width,
                                        FloatRect& bounds);

  void Dispose();

 private:
  HeapVector<Member<CanvasFormattedTextRun>> text_runs_;
  LayoutBlockFlow* block_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_H_
